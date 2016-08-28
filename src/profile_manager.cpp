#include "profile_manager.h"
#include "profiler/serialized_block.h"

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
    void PROFILER_API beginBlock(Block& _block)
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

SerializedBlock::SerializedBlock(const Block& block, uint16_t name_length)
    : BaseBlockData(block)
{
    auto pName = const_cast<char*>(name());
    if (name_length) strncpy(pName, block.name(), name_length);
    pName[name_length] = 0;
}

//////////////////////////////////////////////////////////////////////////

StaticBlockDescriptor::StaticBlockDescriptor(const char* _name, const char* _filename, int _line, block_type_t _block_type, color_t _color)
    : m_id(MANAGER.addBlockDescriptor(_name, _filename, _line, _block_type, _color))
{

}

BaseBlockDescriptor::BaseBlockDescriptor(int _line, block_type_t _block_type, color_t _color)
    : m_line(_line)
    , m_type(_block_type)
    , m_color(_color)
{

}

BlockDescriptor::BlockDescriptor(uint64_t& _used_mem, const char* _name, const char* _filename, int _line, block_type_t _block_type, color_t _color)
    : BaseBlockDescriptor(_line, _block_type, _color)
    , m_name(_name)
    , m_filename(_filename)
{
    _used_mem += sizeof(profiler::BaseBlockDescriptor) + strlen(_name) + strlen(_filename) + 2;
}

//////////////////////////////////////////////////////////////////////////

void ThreadStorage::store(const profiler::Block& block)
{
    auto name_length = static_cast<uint16_t>(strlen(block.name()));
    auto size = static_cast<uint16_t>(sizeof(BaseBlockData) + name_length + 1);
    auto data = m_allocator.allocate(size);
    ::new (static_cast<void*>(data)) SerializedBlock(block, name_length);
    usedMemorySize += size;
    closedList.emplace_back(reinterpret_cast<SerializedBlock*>(data));
}

void ThreadStorage::clearClosed()
{
    serialized_list_t().swap(closedList);
    m_allocator.clear();
    usedMemorySize = 0;
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

void ProfileManager::beginBlock(Block& _block)
{
    if (!m_isEnabled)
        return;

    auto& thread_storage = threadStorage(getCurrentThreadId());

    if (!_block.isFinished())
        thread_storage.openedList.emplace(_block);
    else
        thread_storage.store(_block);
}

void ProfileManager::endBlock()
{
    if (!m_isEnabled)
        return;

    auto& thread_storage = threadStorage(getCurrentThreadId());
    if (thread_storage.openedList.empty())
        return;

    Block& lastBlock = thread_storage.openedList.top();
    if (!lastBlock.isFinished())
        lastBlock.finish();

    thread_storage.store(lastBlock);
    thread_storage.openedList.pop();
}

void ProfileManager::setEnabled(bool isEnable)
{
	m_isEnabled = isEnable;
}

//////////////////////////////////////////////////////////////////////////

class FileWriter final
{
    std::ofstream m_file;

public:

    FileWriter(const char* _filename) : m_file(_filename, std::fstream::binary) { }

    template <typename T> void write(const char* _data, T _size) {
        m_file.write(_data, _size);
    }

    template <class T> void write(const T& _data) {
        m_file.write((const char*)&_data, sizeof(T));
    }
};

//////////////////////////////////////////////////////////////////////////

uint32_t ProfileManager::dumpBlocksToFile(const char* filename)
{
    FileWriter of(filename);

    uint64_t usedMemorySize = 0;
    uint32_t blocks_number = 0;
    for (const auto& thread_storage : m_threads)
    {
        usedMemorySize += thread_storage.second.usedMemorySize;
        blocks_number += static_cast<uint32_t>(thread_storage.second.closedList.size());
    }

    of.write(blocks_number);
    of.write(usedMemorySize);
    of.write(static_cast<uint32_t>(m_descriptors.size()));
    of.write(m_usedMemorySize);

    for (const auto& descriptor : m_descriptors)
    {
        const auto name_size = static_cast<uint16_t>(strlen(descriptor.name()) + 1);
        const auto filename_size = static_cast<uint16_t>(strlen(descriptor.file()) + 1);
        const auto size = static_cast<uint16_t>(sizeof(profiler::BaseBlockDescriptor) + name_size + filename_size + sizeof(uint16_t));

        of.write(size);
        of.write<profiler::BaseBlockDescriptor>(descriptor);
        of.write(name_size);
        of.write(descriptor.name(), name_size);
        of.write(descriptor.file(), filename_size);
    }

    for (auto& thread_storage : m_threads)
    {
        of.write(thread_storage.first);
        of.write(static_cast<uint32_t>(thread_storage.second.closedList.size()));

        for (auto b : thread_storage.second.closedList)
        {
            auto sz = static_cast<uint16_t>(sizeof(BaseBlockData) + strlen(b->name()) + 1);
            of.write(sz);
            of.write(b->data(), sz);
        }

        thread_storage.second.clearClosed();
    }

    return blocks_number;
}

void ProfileManager::setThreadName(const char* name)
{
    auto& thread_storage = threadStorage(getCurrentThreadId());
    if (thread_storage.named)
        return;

    const auto id = addBlockDescriptor("ThreadName", __FILE__, __LINE__, profiler::BLOCK_TYPE_THREAD_SIGN, profiler::colors::Random);
    thread_storage.store(profiler::Block(nullptr, profiler::BLOCK_TYPE_THREAD_SIGN, id, name));
    thread_storage.named = true;
}

//////////////////////////////////////////////////////////////////////////

