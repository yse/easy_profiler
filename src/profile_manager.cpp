/************************************************************************
* file name         : profile_manager.cpp
* ----------------- :
* creation time     : 2016/02/16
* authors           : Sergey Yagovtsev, Victor Zarubkin
* emails            : yse.sey@gmail.com, v.s.zarubkin@gmail.com
* ----------------- :
* description       : The file contains implementation of Profile manager and implement access c-function
*                   :
* license           : Lightweight profiler library for c++
*                   : Copyright(C) 2016  Sergey Yagovtsev, Victor Zarubkin
*                   :
*                   : This program is free software : you can redistribute it and / or modify
*                   : it under the terms of the GNU General Public License as published by
*                   : the Free Software Foundation, either version 3 of the License, or
*                   : (at your option) any later version.
*                   :
*                   : This program is distributed in the hope that it will be useful,
*                   : but WITHOUT ANY WARRANTY; without even the implied warranty of
*                   : MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.See the
*                   : GNU General Public License for more details.
*                   :
*                   : You should have received a copy of the GNU General Public License
*                   : along with this program.If not, see <http://www.gnu.org/licenses/>.
************************************************************************/

#include <algorithm>
#include <thread>
#include <fstream>
#include "profiler/serialized_block.h"
#include "profile_manager.h"
#include "event_trace_win.h"

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

using namespace profiler;

#ifdef _WIN32
extern decltype(LARGE_INTEGER::QuadPart) CPU_FREQUENCY;
#endif

extern timestamp_t getCurrentTime();

//auto& MANAGER = ProfileManager::instance();
#define MANAGER ProfileManager::instance()

extern "C" {

    PROFILER_API const BaseBlockDescriptor& registerDescription(bool _enabled, const char* _name, const char* _filename, int _line, block_type_t _block_type, color_t _color)
    {
        return MANAGER.addBlockDescriptor(_enabled, _name, _filename, _line, _block_type, _color);
    }

    PROFILER_API void endBlock()
    {
        MANAGER.endBlock();
    }

    PROFILER_API void setEnabled(bool isEnable)
    {
        MANAGER.setEnabled(isEnable);
#ifdef _WIN32
        if (isEnable)
            EasyEventTracer::instance().enable(true);
        else
            EasyEventTracer::instance().disable();
#endif
    }

    PROFILER_API void storeBlock(const BaseBlockDescriptor& _desc, const char* _runtimeName)
    {
        MANAGER.storeBlock(_desc, _runtimeName);
    }

    PROFILER_API void beginBlock(Block& _block)
    {
        MANAGER.beginBlock(_block);
    }

    PROFILER_API uint32_t dumpBlocksToFile(const char* filename)
    {
        return MANAGER.dumpBlocksToFile(filename);
    }

    PROFILER_API const char* setThreadName(const char* name, const char* filename, const char* _funcname, int line)
    {
        return MANAGER.setThreadName(name, filename, _funcname, line);
    }

    PROFILER_API void setContextSwitchLogFilename(const char* name)
    {
        return MANAGER.setContextSwitchLogFilename(name);
    }

    PROFILER_API const char* getContextSwitchLogFilename()
    {
        return MANAGER.getContextSwitchLogFilename();
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

BaseBlockDescriptor::BaseBlockDescriptor(block_id_t _id, bool _enabled, int _line, block_type_t _block_type, color_t _color)
    : m_id(_id)
    , m_line(_line)
    , m_type(_block_type)
    , m_color(_color)
    , m_enabled(_enabled)
{

}

BlockDescriptor::BlockDescriptor(uint64_t& _used_mem, block_id_t _id, bool _enabled, const char* _name, const char* _filename, int _line, block_type_t _block_type, color_t _color)
    : BaseBlockDescriptor(_id, _enabled, _line, _block_type, _color)
    , m_name(_name)
    , m_filename(_filename)
{
    _used_mem += sizeof(profiler::SerializedBlockDescriptor) + strlen(_name) + strlen(_filename) + 2;
}

BlockDescriptor::BlockDescriptor(uint64_t& _used_mem, bool _enabled, const char* _name, const char* _filename, int _line, block_type_t _block_type, color_t _color)
    : BaseBlockDescriptor(0, _enabled, _line, _block_type, _color)
    , m_name(_name)
    , m_filename(_filename)
{
    _used_mem += sizeof(profiler::SerializedBlockDescriptor) + strlen(_name) + strlen(_filename) + 2;
}

void BlockDescriptor::setId(block_id_t _id)
{
    m_id = _id;
}

BlockDescRef::~BlockDescRef()
{
    MANAGER.markExpired(m_desc.id());
}

//////////////////////////////////////////////////////////////////////////

void ThreadStorage::storeBlock(const profiler::Block& block)
{
#if EASY_MEASURE_STORAGE_EXPAND != 0
    static const auto desc = MANAGER.addBlockDescriptor(EASY_STORAGE_EXPAND_ENABLED, "EasyProfiler.ExpandStorage", __FILE__, __LINE__, profiler::BLOCK_TYPE_BLOCK, profiler::colors::White);
#endif

    auto name_length = static_cast<uint16_t>(strlen(block.name()));
    auto size = static_cast<uint16_t>(sizeof(BaseBlockData) + name_length + 1);

#if EASY_MEASURE_STORAGE_EXPAND != 0
    const bool expanded = desc.enabled() && blocks.closedList.need_expand(size);
    profiler::Block b(0ULL, desc.id(), "");
    if (expanded) b.start();
#endif

    auto data = blocks.closedList.allocate(size);

#if EASY_MEASURE_STORAGE_EXPAND != 0
    if (expanded) b.finish();
#endif

    ::new (data) SerializedBlock(block, name_length);
    blocks.usedMemorySize += size;

#if EASY_MEASURE_STORAGE_EXPAND != 0
    if (expanded)
    {
        name_length = 0;
        size = static_cast<uint16_t>(sizeof(BaseBlockData) + 1);
        data = blocks.closedList.allocate(size);
        ::new (data) SerializedBlock(b, name_length);
        blocks.usedMemorySize += size;
    }
#endif
}

void ThreadStorage::storeCSwitch(const profiler::Block& block)
{
    auto name_length = static_cast<uint16_t>(strlen(block.name()));
    auto size = static_cast<uint16_t>(sizeof(BaseBlockData) + name_length + 1);
    auto data = sync.closedList.allocate(size);
    ::new (data) SerializedBlock(block, name_length);
    sync.usedMemorySize += size;
}

void ThreadStorage::clearClosed()
{
    blocks.clearClosed();
    sync.clearClosed();
}

//////////////////////////////////////////////////////////////////////////

EASY_THREAD_LOCAL static ::ThreadStorage* THREAD_STORAGE = nullptr;

ProfileManager::ProfileManager()
{
    m_expiredDescriptors.reserve(1024U);
}

ProfileManager::~ProfileManager()
{
    //dumpBlocksToFile("test.prof");
    for (auto desc : m_descriptors)
    {
        delete desc;
    }
}

ProfileManager& ProfileManager::instance()
{
    ///C++11 makes possible to create Singleton without any warry about thread-safeness
    ///http://preshing.com/20130930/double-checked-locking-is-fixed-in-cpp11/
    static ProfileManager m_profileManager;
    return m_profileManager;
}

void ProfileManager::markExpired(profiler::block_id_t _id)
{
    // Mark block descriptor as expired (descriptor may become expired if it's .dll/.so have been unloaded during application execution).
    // We can not delete this descriptor now, because we need to send/write all collected data first.

    guard_lock_t lock(m_storedSpin);
    m_expiredDescriptors.push_back(_id);
}

ThreadStorage& ProfileManager::threadStorage(profiler::thread_id_t _thread_id)
{
    guard_lock_t lock(m_spin);
    return m_threads[_thread_id];
}

ThreadStorage* ProfileManager::findThreadStorage(profiler::thread_id_t _thread_id)
{
    guard_lock_t lock(m_spin);
    auto it = m_threads.find(_thread_id);
    return it != m_threads.end() ? &it->second : nullptr;
}

void ProfileManager::storeBlock(const profiler::BaseBlockDescriptor& _desc, const char* _runtimeName)
{
    if (!m_isEnabled || !_desc.enabled())
        return;

    profiler::Block b(_desc, _runtimeName);
    b.finish(b.begin());

    if (THREAD_STORAGE == nullptr)
        THREAD_STORAGE = &threadStorage(getCurrentThreadId());

    THREAD_STORAGE->storeBlock(b);
}

void ProfileManager::beginBlock(Block& _block)
{
    if (!m_isEnabled)
        return;

    if (THREAD_STORAGE == nullptr)
        THREAD_STORAGE = &threadStorage(getCurrentThreadId());

    THREAD_STORAGE->blocks.openedList.emplace(_block);
}

void ProfileManager::beginContextSwitch(profiler::thread_id_t _thread_id, profiler::timestamp_t _time, profiler::block_id_t _id)
{
    auto ts = findThreadStorage(_thread_id);
    if (ts != nullptr)
        ts->sync.openedList.emplace(_time, _id, "");
}

void ProfileManager::storeContextSwitch(profiler::thread_id_t _thread_id, profiler::timestamp_t _time, profiler::block_id_t _id)
{
    auto ts = findThreadStorage(_thread_id);
    if (ts != nullptr)
    {
        profiler::Block b(_time, _id, "");
        b.finish(_time);
        ts->storeCSwitch(b);
    }
}

void ProfileManager::endBlock()
{
    if (!m_isEnabled)
        return;

    if (THREAD_STORAGE->blocks.openedList.empty())
        return;

    Block& lastBlock = THREAD_STORAGE->blocks.openedList.top();
    if (lastBlock.enabled())
    {
        if (!lastBlock.finished())
            lastBlock.finish();
        THREAD_STORAGE->storeBlock(lastBlock);
    }

    THREAD_STORAGE->blocks.openedList.pop();
}

void ProfileManager::endContextSwitch(profiler::thread_id_t _thread_id, profiler::timestamp_t _endtime)
{
    auto ts = findThreadStorage(_thread_id);
    if (ts == nullptr || ts->sync.openedList.empty())
        return;

    Block& lastBlock = ts->sync.openedList.top();
    lastBlock.finish(_endtime);

    ts->storeCSwitch(lastBlock);
    ts->sync.openedList.pop();
}

void ProfileManager::setEnabled(bool isEnable)
{
    m_isEnabled = isEnable;
}

//////////////////////////////////////////////////////////////////////////

uint32_t ProfileManager::dumpBlocksToStream(profiler::OStream& _outputStream)
{
    const bool wasEnabled = m_isEnabled;
    if (wasEnabled)
        ::profiler::setEnabled(false);


#ifndef _WIN32
    // Read thread context switch events from temporary file

    uint64_t timestamp;
    uint32_t thread_from, thread_to;

    std::ifstream infile(m_csInfoFilename.c_str());

    if(infile.is_open())
    {
        static const auto& desc = addBlockDescriptor(true, "OS.ContextSwitch", __FILE__, __LINE__, profiler::BLOCK_TYPE_CONTEXT_SWITCH, profiler::colors::White);
        while (infile >> timestamp >> thread_from >> thread_to)
        {
            beginContextSwitch(thread_from, timestamp, desc.id());
            endContextSwitch(thread_to, timestamp);
        }
    }
#endif

    // Calculate used memory total size and total blocks number
    uint64_t usedMemorySize = 0;
    uint32_t blocks_number = 0;
    for (const auto& thread_storage : m_threads)
    {
        const auto& t = thread_storage.second;
        usedMemorySize += t.blocks.usedMemorySize + t.sync.usedMemorySize;
        blocks_number += static_cast<uint32_t>(t.blocks.closedList.size()) + static_cast<uint32_t>(t.sync.closedList.size());
    }

    // Write CPU frequency to let GUI calculate real time value from CPU clocks
#ifdef _WIN32
    _outputStream.write(CPU_FREQUENCY);
#else
    _outputStream.write(0LL);
#endif

    // Write blocks number and used memory size
    _outputStream.write(blocks_number);
    _outputStream.write(usedMemorySize);
    _outputStream.write(static_cast<uint32_t>(m_descriptors.size()));
    _outputStream.write(m_usedMemorySize);

    // Write block descriptors
    for (const auto descriptor : m_descriptors)
    {
        const auto name_size = static_cast<uint16_t>(strlen(descriptor->name()) + 1);
        const auto filename_size = static_cast<uint16_t>(strlen(descriptor->file()) + 1);
        const auto size = static_cast<uint16_t>(sizeof(profiler::SerializedBlockDescriptor) + name_size + filename_size);

        _outputStream.write(size);
        _outputStream.write<profiler::BaseBlockDescriptor>(*descriptor);
        _outputStream.write(name_size);
        _outputStream.write(descriptor->name(), name_size);
        _outputStream.write(descriptor->file(), filename_size);
    }

    // Write blocks and context switch events for each thread
    for (auto& thread_storage : m_threads)
    {
        auto& t = thread_storage.second;

        _outputStream.write(thread_storage.first);

        _outputStream.write(t.sync.closedList.size());
        t.sync.closedList.serialize(_outputStream);

        _outputStream.write(t.blocks.closedList.size());
        t.blocks.closedList.serialize(_outputStream);

        t.clearClosed();
    }

    if (!m_expiredDescriptors.empty())
    {
        // Remove all expired block descriptors (descriptor may become expired if it's .dll/.so have been unloaded during application execution)

        std::sort(m_expiredDescriptors.begin(), m_expiredDescriptors.end());
        for (auto it = m_expiredDescriptors.rbegin(), rend = m_expiredDescriptors.rend(); it != rend; ++it)
        {
            auto id = *it;
            delete m_descriptors[id];
            m_descriptors.erase(m_descriptors.begin() + id);
        }
        m_expiredDescriptors.clear();
    }

    //if (wasEnabled)
    //    ::profiler::setEnabled(true);

    return blocks_number;
}

uint32_t ProfileManager::dumpBlocksToFile(const char* _filename)
{
    profiler::OStream outputStream;
    const auto blocksNumber = dumpBlocksToStream(outputStream);

    std::ofstream of(_filename, std::fstream::binary);
    of << outputStream.stream().str();

    return blocksNumber;
}

const char* ProfileManager::setThreadName(const char* name, const char* filename, const char* _funcname, int line)
{
    if (THREAD_STORAGE == nullptr)
    {
        THREAD_STORAGE = &threadStorage(getCurrentThreadId());
    }

    if (!THREAD_STORAGE->named)
    {
        const auto& desc = addBlockDescriptor(true, _funcname, filename, line, profiler::BLOCK_TYPE_THREAD_SIGN, profiler::colors::Black);

        profiler::Block b(desc, name);
        b.finish(b.begin());

        THREAD_STORAGE->storeBlock(b);
        THREAD_STORAGE->name = name;
        THREAD_STORAGE->named = true;
    }

    return THREAD_STORAGE->name.c_str();
}

//////////////////////////////////////////////////////////////////////////

