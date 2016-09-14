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

    PROFILER_API const BaseBlockDescriptor& registerDescription(bool _enabled, const char* _autogenUniqueId, const char* _name, const char* _filename, int _line, block_type_t _block_type, color_t _color)
    {
        return MANAGER.addBlockDescriptor(_enabled, _autogenUniqueId, _name, _filename, _line, _block_type, _color);
    }

    PROFILER_API void endBlock()
    {
        MANAGER.endBlock();
    }

    PROFILER_API void setEnabled(bool isEnable)
    {
        MANAGER.setEnabled(isEnable);
    }

    PROFILER_API void storeEvent(const BaseBlockDescriptor& _desc, const char* _runtimeName)
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

    PROFILER_API const char* registerThread(const char* name)//, const char* filename, const char* _funcname, int line)
    {
        return MANAGER.registerThread(name);// , filename, _funcname, line);
    }

#ifndef _WIN32
    PROFILER_API void setContextSwitchLogFilename(const char* name)
    {
        return MANAGER.setContextSwitchLogFilename(name);
    }

    PROFILER_API const char* getContextSwitchLogFilename()
    {
        return MANAGER.getContextSwitchLogFilename();
    }
#endif

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
    , m_pEnable(nullptr)
    , m_expired(false)
{
    _used_mem += sizeof(profiler::SerializedBlockDescriptor) + strlen(_name) + strlen(_filename) + 2;
}

BlockDescriptor::BlockDescriptor(uint64_t& _used_mem, bool _enabled, const char* _name, const char* _filename, int _line, block_type_t _block_type, color_t _color)
    : BaseBlockDescriptor(0, _enabled, _line, _block_type, _color)
    , m_name(_name)
    , m_filename(_filename)
    , m_pEnable(nullptr)
    , m_expired(false)
{
    _used_mem += sizeof(profiler::SerializedBlockDescriptor) + strlen(_name) + strlen(_filename) + 2;
}

BlockDescRef::~BlockDescRef()
{
    MANAGER.markExpired(m_desc.id());
}

//////////////////////////////////////////////////////////////////////////

void ThreadStorage::storeBlock(const profiler::Block& block)
{
#if EASY_MEASURE_STORAGE_EXPAND != 0
    static const auto desc = MANAGER.addBlockDescriptor(EASY_STORAGE_EXPAND_ENABLED, EASY_UNIQUE_LINE_ID, "EasyProfiler.ExpandStorage", __FILE__, __LINE__, profiler::BLOCK_TYPE_BLOCK, profiler::colors::White);
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
    m_isEnabled = ATOMIC_VAR_INIT(false);
    m_isEventTracingEnabled = ATOMIC_VAR_INIT(EASY_EVENT_TRACING_ENABLED);
}

ProfileManager::~ProfileManager()
{
    //dumpBlocksToFile("test.prof");
    for (auto desc : m_descriptors)
    {
        if (desc != nullptr)
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
    m_descriptors[_id]->m_expired = true;
}

ThreadStorage& ProfileManager::threadStorage(profiler::thread_id_t _thread_id)
{
    guard_lock_t lock(m_spin);
    return m_threads[_thread_id];
}

ThreadStorage* ProfileManager::_findThreadStorage(profiler::thread_id_t _thread_id)
{
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

void ProfileManager::beginContextSwitch(profiler::thread_id_t _thread_id, profiler::timestamp_t _time, profiler::thread_id_t _target_thread_id, const char* _target_process, bool _lockSpin)
{
    auto ts = _lockSpin ? findThreadStorage(_thread_id) : _findThreadStorage(_thread_id);
    if (ts != nullptr)
        // Dirty hack: _target_thread_id will be written to the field "block_id_t m_id"
        // and will be available calling method id().
        ts->sync.openedList.emplace(_time, _target_thread_id, _target_process);
}

void ProfileManager::storeContextSwitch(profiler::thread_id_t _thread_id, profiler::timestamp_t _time, profiler::thread_id_t _target_thread_id, bool _lockSpin)
{
    auto ts = _lockSpin ? findThreadStorage(_thread_id) : _findThreadStorage(_thread_id);
    if (ts != nullptr)
    {
        profiler::Block b(_time, _target_thread_id, "");
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

void ProfileManager::endContextSwitch(profiler::thread_id_t _thread_id, profiler::timestamp_t _endtime, bool _lockSpin)
{
    auto ts = _lockSpin ? findThreadStorage(_thread_id) : _findThreadStorage(_thread_id);
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

#ifdef _WIN32
    if (isEnable) {
        if (m_isEventTracingEnabled)
            EasyEventTracer::instance().enable(true);
    } else {
        EasyEventTracer::instance().disable();
    }
#endif
}

//////////////////////////////////////////////////////////////////////////

uint32_t ProfileManager::dumpBlocksToStream(profiler::OStream& _outputStream)
{
    const bool wasEnabled = m_isEnabled;
    const bool eventTracingEnabled = m_isEventTracingEnabled;
    if (wasEnabled)
        ::profiler::setEnabled(false);


    // This is to make sure that no new descriptors or new threads will be
    // added until we finish sending data.
    guard_lock_t lock1(m_storedSpin);
    guard_lock_t lock2(m_spin);
    // This is the only place using both spins, so no dead-lock will occur


#ifndef _WIN32
    if (eventTracingEnabled)
    {
        // Read thread context switch events from temporary file

        uint64_t timestamp = 0;
        uint32_t thread_from = 0, thread_to = 0;

        std::ifstream infile(m_csInfoFilename.c_str());
        if(infile.is_open()) {
            while (infile >> timestamp >> thread_from >> thread_to) {
                beginContextSwitch(thread_from, timestamp, thread_to, "", false);
                endContextSwitch(thread_to, timestamp, false);
            }
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
        if (descriptor == nullptr)
            continue;

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

        const auto name_size = static_cast<uint16_t>(t.name.size() + 1);
        _outputStream.write(name_size);
        _outputStream.write(name_size > 1 ? t.name.c_str() : "", name_size);

        _outputStream.write(t.sync.closedList.size());
        t.sync.closedList.serialize(_outputStream);

        _outputStream.write(t.blocks.closedList.size());
        if (!t.blocks.closedList.empty())
            t.blocks.closedList.serialize(_outputStream);

        t.clearClosed();
        t.blocks.openedList.clear();
        t.sync.openedList.clear();
    }

    // Remove all expired block descriptors (descriptor may become expired if it's .dll/.so have been unloaded during application execution)
    for (auto& desc : m_descriptors)
    {
        if (desc == nullptr || !desc->m_expired)
            continue;

        delete desc;
        desc = nullptr;
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

const char* ProfileManager::registerThread(const char* name)
{
    if (THREAD_STORAGE == nullptr)
    {
        THREAD_STORAGE = &threadStorage(getCurrentThreadId());
    }

    if (!THREAD_STORAGE->named)
    {
        THREAD_STORAGE->named = true;
        THREAD_STORAGE->name = name;
    }

    return THREAD_STORAGE->name.c_str();
}

void ProfileManager::setBlockEnabled(profiler::block_id_t _id, const profiler::hashed_stdstring& _key, bool _enabled)
{
    guard_lock_t lock(m_storedSpin);

    auto desc = m_descriptors[_id];
    if (desc != nullptr)
    {
        lock.unlock();

        *desc->m_pEnable = _enabled;
        desc->m_enabled = _enabled;
    }
    else
    {
#ifdef _WIN32
        blocks_enable_status_t::key_type key(_key.c_str(), _key.size(), _key.hcode());
        m_blocksEnableStatus[key] = _enabled;
#else
        m_blocksEnableStatus[_key] = _enabled;
#endif
    }
}

//////////////////////////////////////////////////////////////////////////

