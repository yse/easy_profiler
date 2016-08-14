#include "profile_manager.h"

#include <thread>
#include <string.h>

#include <fstream>

using namespace profiler;

auto& MANAGER = ProfileManager::instance();

extern "C"{

    void PROFILER_API endBlock()
    {
        MANAGER.endBlock();
    }

    void PROFILER_API setEnabled(bool isEnable)
    {
        MANAGER.setEnabled(isEnable);
    }
    void PROFILER_API beginBlock(Block* _block)
	{
        MANAGER.beginBlock(_block);
	}

	unsigned int PROFILER_API dumpBlocksToFile(const char* filename)
	{
        return MANAGER.dumpBlocksToFile(filename);
	}

	void PROFILER_API setThreadName(const char* name)
	{
        return MANAGER.setThreadName(name);
	}
}

SerializedBlock* SerializedBlock::create(const Block* block, uint64_t& memory_size)
{
    auto name_length = static_cast<uint16_t>(strlen(block->getName()));
    auto size = static_cast<uint16_t>(sizeof(BaseBlockData) + name_length + 1);
    auto data = ::new char[size];
    ::new (static_cast<void*>(data)) SerializedBlock(block, name_length);
    memory_size += size;
    return reinterpret_cast<SerializedBlock*>(data);
}

void SerializedBlock::destroy(SerializedBlock* that)
{
    ::delete[] reinterpret_cast<char*>(that);
}

SerializedBlock::SerializedBlock(const Block* block, uint16_t name_length)
    : BaseBlockData(*block)
{
    auto name = const_cast<char*>(getName());
    strncpy(name, block->getName(), name_length);
    name[name_length] = 0;
}

//////////////////////////////////////////////////////////////////////////

BlockSourceInfo::BlockSourceInfo(const char* _filename, int _linenumber) : m_id(MANAGER.addSource(_filename, _linenumber))
{

}

SourceBlock::SourceBlock(const char* _filename, int _line) : m_filename(_filename), m_line(_line)
{

}

//////////////////////////////////////////////////////////////////////////

ProfileManager::ProfileManager()
{

}

ProfileManager::~ProfileManager()
{
	//dumpBlocksToFile("test.prof");
}

ProfileManager& ProfileManager::instance()
{
    ///C++11 makes possible to create Singleton without any warry about thread-safeness
    ///http://preshing.com/20130930/double-checked-locking-is-fixed-in-cpp11/
    static ProfileManager m_profileManager;
    return m_profileManager;
}

void ProfileManager::beginBlock(Block* _block)
{
	if (!m_isEnabled)
		return;
	if (BLOCK_TYPE_BLOCK == _block->getType()){
		guard_lock_t lock(m_spin);
		m_openedBracketsMap[_block->getThreadId()].push(_block);
	}
	else{
		_internalInsertBlock(_block);
	}
	
}
void ProfileManager::endBlock()
{

	if (!m_isEnabled)
		return;

	uint32_t threadId = getCurrentThreadId();
	
	guard_lock_t lock(m_spin);
	auto& stackOfOpenedBlocks = m_openedBracketsMap[threadId];

	if (stackOfOpenedBlocks.empty())
		return;

	Block* lastBlock = stackOfOpenedBlocks.top();

	if (lastBlock && !lastBlock->isFinished()){
		lastBlock->finish();
	}
	_internalInsertBlock(lastBlock);
	stackOfOpenedBlocks.pop();
}

void ProfileManager::setEnabled(bool isEnable)
{
	m_isEnabled = isEnable;
}

void ProfileManager::_internalInsertBlock(profiler::Block* _block)
{
	guard_lock_t lock(m_storedSpin);
    m_blocks.emplace_back(SerializedBlock::create(_block, m_blocksMemorySize));
}

unsigned int ProfileManager::dumpBlocksToFile(const char* filename)
{
    ::std::ofstream of(filename, std::fstream::binary);

    auto blocks_number = static_cast<uint32_t>(m_blocks.size());
    //of.write((const char*)&blocks_number, sizeof(uint32_t));
    of.write((const char*)&m_blocksMemorySize, sizeof(uint64_t));

    for (auto b : m_blocks)
    {
        auto sz = static_cast<uint16_t>(sizeof(BaseBlockData) + strlen(b->getName()) + 1);

        of.write((const char*)&sz, sizeof(uint16_t));
        of.write(b->data(), sz);

        SerializedBlock::destroy(b);
    }

    m_blocksMemorySize = 0;
    m_blocks.clear();

    return blocks_number;
}

void ProfileManager::setThreadName(const char* name)
{
    auto current_thread_id = getCurrentThreadId();

    guard_lock_t lock(m_storedSpin);
    auto find_it = m_namedThreades.find(current_thread_id);
    if (find_it != m_namedThreades.end())
        return;

    profiler::Block block(name, current_thread_id, 0, profiler::BLOCK_TYPE_THREAD_SIGN);
    m_blocks.emplace_back(SerializedBlock::create(&block, m_blocksMemorySize));
    m_namedThreades.insert(current_thread_id);
}

//////////////////////////////////////////////////////////////////////////

unsigned int ProfileManager::addSource(const char* _filename, int _line)
{
    guard_lock_t lock(m_storedSpin);
    const auto id = static_cast<unsigned int>(m_sources.size());
    m_sources.emplace_back(_filename, _line);
    return id;
}

//////////////////////////////////////////////////////////////////////////

