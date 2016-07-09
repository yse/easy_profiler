#include "profile_manager.h"

#include <thread>
#include <string.h>

#include <fstream>

using namespace profiler;

extern "C"{

    void PROFILER_API endBlock()
    {
        ProfileManager::instance().endBlock();
    }

    void PROFILER_API setEnabled(bool isEnable)
    {
        ProfileManager::instance().setEnabled(isEnable);
    }
	void PROFILER_API beginBlock(Block* _block)
	{
		ProfileManager::instance().beginBlock(_block);
	}

	unsigned int PROFILER_API dumpBlocksToFile(const char* filename)
	{
		return ProfileManager::instance().dumpBlocksToFile(filename);
	}
}

SerilizedBlock::SerilizedBlock(Block* block):
		m_size(0),
		m_data(nullptr)
{
	m_size += sizeof(BaseBlockData);
	uint16_t name_len = (uint16_t)strlen(block->getName()) + 1;
	m_size += name_len;

	m_data = new char[m_size];
	memcpy(&m_data[0], block, sizeof(BaseBlockData));
	strncpy(&m_data[sizeof(BaseBlockData)], block->getName(), name_len);
}

SerilizedBlock::SerilizedBlock(uint16_t _size, char* _data) :
		m_size(_size),
		m_data(_data)
{
	//m_data = new char[m_size];
	//memcpy(&m_data[0], _data, m_size);
}

SerilizedBlock::~SerilizedBlock()
{
	if (m_data){
		delete[] m_data;
		m_data = nullptr;
	}
}

SerilizedBlock::SerilizedBlock(const SerilizedBlock& other)
{
	m_size = other.m_size;
	m_data = new char[m_size];
	memcpy(&m_data[0], other.m_data, m_size);
}

SerilizedBlock::SerilizedBlock(SerilizedBlock&& that)
{
	m_size = that.m_size;
	m_data = that.m_data;
	that.m_size = 0;
	that.m_data = nullptr;
}

const BaseBlockData * SerilizedBlock::block() const
{
	return (const BaseBlockData*)m_data;
}

const char* SerilizedBlock::getBlockName() const
{
	return (const char*)&m_data[sizeof(profiler::BaseBlockData)];
}

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
	m_blocks.emplace_back(new SerilizedBlock(_block));
}

unsigned int ProfileManager::dumpBlocksToFile(const char* filename)
{
	std::ofstream of(filename, std::fstream::binary);

	for (auto* b : m_blocks){
		uint16_t sz = b->size();
		of.write((const char*)&sz, sizeof(uint16_t));
		of.write(b->data(), b->size());
		delete b;
	}
	unsigned int size = (unsigned int)m_blocks.size();

	m_blocks.clear();

	return size;
}
