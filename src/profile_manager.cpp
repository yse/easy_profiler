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
#include <fstream>
#include "profile_manager.h"
#include "profiler/serialized_block.h"
#include "profiler/easy_net.h"
#include "profiler/easy_socket.h"
#include "event_trace_win.h"
#include "current_time.h"

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

using namespace profiler;

const uint32_t PROFILER_SIGNATURE = ('E' << 24) | ('a' << 16) | ('s' << 8) | 'y';
const uint8_t FORCE_ON_FLAG = profiler::FORCE_ON & ~profiler::ON;

//auto& MANAGER = ProfileManager::instance();
#define MANAGER ProfileManager::instance()

EASY_THREAD_LOCAL static ::ThreadStorage* THREAD_STORAGE = nullptr;

#ifdef _WIN32
decltype(LARGE_INTEGER::QuadPart) CPU_FREQUENCY = ([](){ LARGE_INTEGER freq; QueryPerformanceFrequency(&freq); return freq.QuadPart; })();
#endif

//////////////////////////////////////////////////////////////////////////

extern "C" {

    PROFILER_API const BaseBlockDescriptor* registerDescription(EasyBlockStatus _status, const char* _autogenUniqueId, const char* _name, const char* _filename, int _line, block_type_t _block_type, color_t _color)
    {
        return MANAGER.addBlockDescriptor(_status, _autogenUniqueId, _name, _filename, _line, _block_type, _color);
    }

    PROFILER_API void endBlock()
    {
        MANAGER.endBlock();
    }

    PROFILER_API void setEnabled(bool isEnable)
    {
        MANAGER.setEnabled(isEnable);
    }

    PROFILER_API void storeEvent(const BaseBlockDescriptor* _desc, const char* _runtimeName)
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

    PROFILER_API const char* registerThread(const char* name, ThreadGuard& threadGuard)
    {
        return MANAGER.registerThread(name, threadGuard);
    }

    PROFILER_API void setEventTracingEnabled(bool _isEnable)
    {
        MANAGER.setEventTracingEnabled(_isEnable);
    }

#ifdef _WIN32
    PROFILER_API void setLowPriorityEventTracing(bool _isLowPriority)
    {
        EasyEventTracer::instance().setLowPriority(_isLowPriority);
    }
#else
    PROFILER_API void setLowPriorityEventTracing(bool) { }
#endif

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

    PROFILER_API void   startListenSignalToCapture()
    {
        return MANAGER.startListenSignalToCapture();
    }

    PROFILER_API void   stopListenSignalToCapture()
    {
        return MANAGER.stopListenSignalToCapture();
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

BaseBlockDescriptor::BaseBlockDescriptor(block_id_t _id, EasyBlockStatus _status, int _line, block_type_t _block_type, color_t _color)
    : m_id(_id)
    , m_line(_line)
    , m_type(_block_type)
    , m_color(_color)
    , m_status(_status)
{

}

//////////////////////////////////////////////////////////////////////////

#ifndef EASY_BLOCK_DESC_FULL_COPY
# define EASY_BLOCK_DESC_FULL_COPY 0
#endif

#if EASY_BLOCK_DESC_FULL_COPY == 0
# define EASY_BLOCK_DESC_STRING const char*
# define EASY_BLOCK_DESC_STRING_LEN(s) static_cast<uint16_t>(strlen(s) + 1)
# define EASY_BLOCK_DESC_STRING_VAL(s) s
#else
# define EASY_BLOCK_DESC_STRING std::string
# define EASY_BLOCK_DESC_STRING_LEN(s) static_cast<uint16_t>(s.size() + 1)
# define EASY_BLOCK_DESC_STRING_VAL(s) s.c_str()
#endif

class BlockDescriptor : public BaseBlockDescriptor
{
    friend ProfileManager;

    EASY_BLOCK_DESC_STRING     m_name; ///< Static name of all blocks of the same type (blocks can have dynamic name) which is, in pair with descriptor id, a unique block identifier
    EASY_BLOCK_DESC_STRING m_filename; ///< Source file name where this block is declared
    uint16_t                   m_size; ///< Used memory size

public:

    BlockDescriptor(block_id_t _id, EasyBlockStatus _status, const char* _name, const char* _filename, int _line, block_type_t _block_type, color_t _color)
        : BaseBlockDescriptor(_id, _status, _line, _block_type, _color)
        , m_name(_name)
        , m_filename(_filename)
        , m_size(static_cast<uint16_t>(sizeof(profiler::SerializedBlockDescriptor) + strlen(_name) + strlen(_filename) + 2))
    {
    }

    const char* name() const {
        return EASY_BLOCK_DESC_STRING_VAL(m_name);
    }

    const char* filename() const {
        return EASY_BLOCK_DESC_STRING_VAL(m_filename);
    }

    uint16_t nameSize() const {
        return EASY_BLOCK_DESC_STRING_LEN(m_name);
    }

    uint16_t filenameSize() const {
        return EASY_BLOCK_DESC_STRING_LEN(m_filename);
    }

}; // END of class BlockDescriptor.

//////////////////////////////////////////////////////////////////////////

void ThreadStorage::storeBlock(const profiler::Block& block)
{
#if EASY_MEASURE_STORAGE_EXPAND != 0
    EASY_LOCAL_STATIC_PTR(const BaseBlockDescriptor*, desc,\
        MANAGER.addBlockDescriptor(EASY_STORAGE_EXPAND_ENABLED ? profiler::ON : profiler::OFF, EASY_UNIQUE_LINE_ID, "EasyProfiler.ExpandStorage",\
            __FILE__, __LINE__, profiler::BLOCK_TYPE_BLOCK, profiler::colors::White));

    EASY_THREAD_LOCAL static profiler::timestamp_t beginTime = 0ULL;
    EASY_THREAD_LOCAL static profiler::timestamp_t endTime = 0ULL;
#endif

    auto name_length = static_cast<uint16_t>(strlen(block.name()));
    auto size = static_cast<uint16_t>(sizeof(BaseBlockData) + name_length + 1);

#if EASY_MEASURE_STORAGE_EXPAND != 0
    const bool expanded = (desc->m_status & profiler::ON) && blocks.closedList.need_expand(size);
    if (expanded) beginTime = getCurrentTime();
#endif

    auto data = blocks.closedList.allocate(size);

#if EASY_MEASURE_STORAGE_EXPAND != 0
    if (expanded) endTime = getCurrentTime();
#endif

    ::new (data) SerializedBlock(block, name_length);
    blocks.usedMemorySize += size;

#if EASY_MEASURE_STORAGE_EXPAND != 0
    if (expanded)
    {
        profiler::Block b(beginTime, desc->id(), "");
        b.finish(endTime);

        size = static_cast<uint16_t>(sizeof(BaseBlockData) + 1);
        data = blocks.closedList.allocate(size);
        ::new (data) SerializedBlock(b, 0);
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

ThreadGuard::~ThreadGuard()
{
    if (m_id != 0 && THREAD_STORAGE != nullptr && THREAD_STORAGE->id == m_id)
    {
        EASY_EVENT("ThreadFinished", profiler::colors::Dark);
        THREAD_STORAGE->expired.store(true, std::memory_order_release);
        THREAD_STORAGE = nullptr;
    }
}

//////////////////////////////////////////////////////////////////////////

ProfileManager::ProfileManager()
{
    m_isEnabled = ATOMIC_VAR_INIT(false);
    m_isEventTracingEnabled = ATOMIC_VAR_INIT(EASY_EVENT_TRACING_ENABLED);
    m_stopListen = ATOMIC_VAR_INIT(false);
}

ProfileManager::~ProfileManager()
{
    stopListenSignalToCapture();

    if (m_listenThread.joinable())
        m_listenThread.join();

    for (auto desc : m_descriptors)
    {
        if (desc != nullptr)
            delete desc;
    }
}

#ifndef EASY_MAGIC_STATIC_CPP11
class ProfileManagerInstance {
    friend ProfileManager;
    ProfileManager instance;
} PROFILE_MANAGER;
#endif

ProfileManager& ProfileManager::instance()
{
#ifndef EASY_MAGIC_STATIC_CPP11
    return PROFILE_MANAGER.instance;
#else
    ///C++11 makes possible to create Singleton without any warry about thread-safeness
    ///http://preshing.com/20130930/double-checked-locking-is-fixed-in-cpp11/
    static ProfileManager profileManager;
    return profileManager;
#endif
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

const BaseBlockDescriptor* ProfileManager::addBlockDescriptor(EasyBlockStatus _defaultStatus,
                                                        const char* _autogenUniqueId,
                                                        const char* _name,
                                                        const char* _filename,
                                                        int _line,
                                                        block_type_t _block_type,
                                                        color_t _color)
{
    guard_lock_t lock(m_storedSpin);

    descriptors_map_t::key_type key(_autogenUniqueId);
    auto it = m_descriptorsMap.find(key);
    if (it != m_descriptorsMap.end())
        return m_descriptors[it->second];

    auto desc = new BlockDescriptor(static_cast<block_id_t>(m_descriptors.size()), _defaultStatus, _name, _filename, _line, _block_type, _color);
    m_usedMemorySize += desc->m_size;
    m_descriptors.emplace_back(desc);
    m_descriptorsMap.emplace(key, desc->id());

    return desc;
}

void ProfileManager::storeBlock(const profiler::BaseBlockDescriptor* _desc, const char* _runtimeName)
{
    if (!m_isEnabled.load(std::memory_order_acquire) || !(_desc->m_status & profiler::ON))
        return;

    if (THREAD_STORAGE == nullptr)
        THREAD_STORAGE = &threadStorage(getCurrentThreadId());

#if EASY_ENABLE_BLOCK_STATUS != 0
    if (!THREAD_STORAGE->allowChildren)
        return;
#endif

    profiler::Block b(_desc, _runtimeName);
    b.start();
    b.m_end = b.m_begin;

    THREAD_STORAGE->storeBlock(b);
}

void ProfileManager::beginBlock(Block& _block)
{
    if (!m_isEnabled.load(std::memory_order_acquire))
        return;

    if (THREAD_STORAGE == nullptr)
        THREAD_STORAGE = &threadStorage(getCurrentThreadId());

#if EASY_ENABLE_BLOCK_STATUS != 0
    if (THREAD_STORAGE->allowChildren)
    {
#endif
        if (_block.m_status & profiler::ON)
            _block.start();
#if EASY_ENABLE_BLOCK_STATUS != 0
        THREAD_STORAGE->allowChildren = !(_block.m_status & profiler::OFF_RECURSIVE);
    } 
    else if (_block.m_status & FORCE_ON_FLAG)
    {
        _block.start();
        _block.m_status = profiler::FORCE_ON_WITHOUT_CHILDREN;
    }
    else
    {
        _block.m_status = profiler::OFF_RECURSIVE;
    }
#endif

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
    if (!m_isEnabled.load(std::memory_order_acquire))
        return;

    if (THREAD_STORAGE->blocks.openedList.empty())
        return;

    Block& lastBlock = THREAD_STORAGE->blocks.openedList.top();
    if (lastBlock.m_status & profiler::ON)
    {
        if (!lastBlock.finished())
            lastBlock.finish();
        THREAD_STORAGE->storeBlock(lastBlock);
    }
    else
    {
        lastBlock.m_end = lastBlock.m_begin; // this is to restrict endBlock() call inside ~Block()
    }

    THREAD_STORAGE->blocks.openedList.pop();

#if EASY_ENABLE_BLOCK_STATUS != 0
    THREAD_STORAGE->allowChildren = THREAD_STORAGE->blocks.openedList.empty() || !(lastBlock.m_status & profiler::OFF_RECURSIVE);
#endif
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
    auto time = getCurrentTime();
    const bool prev = m_isEnabled.exchange(isEnable, std::memory_order_release);
    if (prev == isEnable)
        return;

    if (isEnable)
    {
        m_beginTime = time;

#ifdef _WIN32
        if (m_isEventTracingEnabled.load(std::memory_order_acquire))
            EasyEventTracer::instance().enable(true);
#endif
    }
    else
    {
        m_endTime = time;

#ifdef _WIN32
        EasyEventTracer::instance().disable();
#endif
    }
}

void ProfileManager::setEventTracingEnabled(bool _isEnable)
{
    m_isEventTracingEnabled.store(_isEnable, std::memory_order_release);
}

//////////////////////////////////////////////////////////////////////////

uint32_t ProfileManager::dumpBlocksToStream(profiler::OStream& _outputStream)
{
    const bool wasEnabled = m_isEnabled.load(std::memory_order_acquire);

#ifndef _WIN32
    const bool eventTracingEnabled = m_isEventTracingEnabled.load(std::memory_order_acquire);
#endif

    if (wasEnabled)
        ::profiler::setEnabled(false);


    // This is to make sure that no new descriptors or new threads will be
    // added until we finish sending data.
    guard_lock_t lock1(m_storedSpin);
    guard_lock_t lock2(m_spin);
    // This is the only place using both spins, so no dead-lock will occur


    // Wait for some time to be sure that all operations which began before setEnabled(false) will be finished.
    // This is much better than inserting spin-lock or atomic variable check into each store operation.
    std::this_thread::sleep_for(std::chrono::milliseconds(20));
    // TODO: think about better solution because this one is not 100% safe...


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
    for (auto it = m_threads.begin(), end = m_threads.end(); it != end;)
    {
        const auto& t = it->second;
        const uint32_t num = static_cast<uint32_t>(t.blocks.closedList.size()) + static_cast<uint32_t>(t.sync.closedList.size());

        if (t.expired.load(std::memory_order_acquire) && num == 0) {
            // Thread has been finished and contains no profiled information.
            // Remove it now.
            m_threads.erase(it++);
            continue;
        }

        usedMemorySize += t.blocks.usedMemorySize + t.sync.usedMemorySize;
        blocks_number += num;
        ++it;
    }

    // Write profiler signature and version
    _outputStream.write(PROFILER_SIGNATURE);
    _outputStream.write(profiler::EASY_FULL_VERSION);

    // Write CPU frequency to let GUI calculate real time value from CPU clocks
#ifdef _WIN32
    _outputStream.write(CPU_FREQUENCY);
#else

#if !defined(USE_STD_CHRONO)
    double g_TicksPerNanoSec;
    struct timespec begints, endts;
    uint64_t begin = 0, end = 0;
    clock_gettime(CLOCK_MONOTONIC, &begints);
    begin = getCurrentTime();
    volatile uint64_t i;
    for (i = 0; i < 100000000; i++); /* must be CPU intensive */
    end = getCurrentTime();
    clock_gettime(CLOCK_MONOTONIC, &endts);
    struct timespec tmpts;
    const int NANO_SECONDS_IN_SEC = 1000000000;
    tmpts.tv_sec = endts.tv_sec - begints.tv_sec;
    tmpts.tv_nsec = endts.tv_nsec - begints.tv_nsec;
    if (tmpts.tv_nsec < 0) {
        tmpts.tv_sec--;
        tmpts.tv_nsec += NANO_SECONDS_IN_SEC;
    }

    uint64_t nsecElapsed = tmpts.tv_sec * 1000000000LL + tmpts.tv_nsec;
    g_TicksPerNanoSec = (double)(end - begin)/(double)nsecElapsed;



    int64_t cpu_frequency = int(g_TicksPerNanoSec*1000000);
     _outputStream.write(cpu_frequency*1000LL);
#else
    _outputStream.write(0LL);
#endif
#endif

    // Write begin and end time
    _outputStream.write(m_beginTime);
    _outputStream.write(m_endTime);

    // Write blocks number and used memory size
    _outputStream.write(blocks_number);
    _outputStream.write(usedMemorySize);
    _outputStream.write(static_cast<uint32_t>(m_descriptors.size()));
    _outputStream.write(m_usedMemorySize);

    // Write block descriptors
    for (const auto descriptor : m_descriptors)
    {
        const auto name_size = descriptor->nameSize();
        const auto filename_size = descriptor->filenameSize();
        const auto size = static_cast<uint16_t>(sizeof(profiler::SerializedBlockDescriptor) + name_size + filename_size);

        _outputStream.write(size);
        _outputStream.write<profiler::BaseBlockDescriptor>(*descriptor);
        _outputStream.write(name_size);
        _outputStream.write(descriptor->name(), name_size);
        _outputStream.write(descriptor->filename(), filename_size);
    }

    // Write blocks and context switch events for each thread
    for (auto it = m_threads.begin(), end = m_threads.end(); it != end;)
    {
        auto& t = it->second;

        _outputStream.write(it->first);

        const auto name_size = static_cast<uint16_t>(t.name.size() + 1);
        _outputStream.write(name_size);
        _outputStream.write(name_size > 1 ? t.name.c_str() : "", name_size);

        _outputStream.write(t.sync.closedList.size());
        if (!t.sync.closedList.empty())
            t.sync.closedList.serialize(_outputStream);

        _outputStream.write(t.blocks.closedList.size());
        if (!t.blocks.closedList.empty())
            t.blocks.closedList.serialize(_outputStream);

        t.clearClosed();
        t.blocks.openedList.clear();
        t.sync.openedList.clear();

        if (t.expired.load(std::memory_order_acquire))
            m_threads.erase(it++); // Remove expired thread after writing all profiled information
        else
            ++it;
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

const char* ProfileManager::registerThread(const char* name, ThreadGuard& threadGuard)
{
    const bool isNewThread = THREAD_STORAGE == nullptr;
    if (isNewThread)
    {
        const auto id = getCurrentThreadId();
        THREAD_STORAGE = &threadStorage(id);
        threadGuard.m_id = THREAD_STORAGE->id = id;
    }

    if (!THREAD_STORAGE->named)
    {
        if (!isNewThread) {
            threadGuard.m_id = THREAD_STORAGE->id = getCurrentThreadId();
        }

        THREAD_STORAGE->named = true;
        THREAD_STORAGE->name = name;
    }

    return THREAD_STORAGE->name.c_str();
}

void ProfileManager::setBlockStatus(block_id_t _id, EasyBlockStatus _status)
{
    if (m_isEnabled.load(std::memory_order_acquire))
        return; // Changing blocks statuses is restricted while profile session is active

    guard_lock_t lock(m_storedSpin);
    if (_id < m_descriptors.size())
    {
        auto desc = m_descriptors[_id];
        lock.unlock();
        desc->m_status = _status;
    }
}

void ProfileManager::startListenSignalToCapture()
{
    if (!m_isAlreadyListened)
    {
        m_stopListen.store(false, std::memory_order_release);
        m_listenThread = std::thread(&ProfileManager::listen, this);
        m_isAlreadyListened = true;
    }
}

void ProfileManager::stopListenSignalToCapture()
{
    m_stopListen.store(true, std::memory_order_release);
    m_isAlreadyListened = false;
}

//////////////////////////////////////////////////////////////////////////

//#define EASY_DEBUG_NET_PRINT

void ProfileManager::listen()
{
    EASY_THREAD("EasyProfiler.Listen");

    EasySocket socket;
    profiler::net::Message replyMessage(profiler::net::MESSAGE_TYPE_REPLY_START_CAPTURING);

    socket.bind(profiler::DEFAULT_PORT);
    int bytes = 0;
    while (!m_stopListen.load(std::memory_order_acquire))
    {
        bool hasConnect = false;

        socket.listen();
        socket.accept();

        EASY_EVENT("ClientConnected", profiler::colors::White, profiler::OFF);
        hasConnect = true;

#ifdef EASY_DEBUG_NET_PRINT
        printf("Client Accepted!\n");
#endif

        replyMessage.type = profiler::net::MESSAGE_TYPE_ACCEPTED_CONNECTION;
        bytes = socket.send(&replyMessage, sizeof(replyMessage));

        hasConnect = bytes > 0;

        while (hasConnect && !m_stopListen.load(std::memory_order_acquire))
        {
            char buffer[256] = {};

            bytes = socket.receive(buffer, 255);

            hasConnect = bytes > 0;

            char *buf = &buffer[0];

            if (bytes > 0)
            {
                profiler::net::Message* message = (profiler::net::Message*)buf;
                if (!message->isEasyNetMessage()){
                    continue;
                }

                switch (message->type)
                {
                    case profiler::net::MESSAGE_TYPE_REQUEST_START_CAPTURE:
                    {
#ifdef EASY_DEBUG_NET_PRINT
                        printf("RECEIVED MESSAGE_TYPE_REQUEST_START_CAPTURE\n");
#endif

                        ProfileManager::setEnabled(true);
                        EASY_EVENT("StartCapture", profiler::colors::Green, profiler::OFF);

                        replyMessage.type = profiler::net::MESSAGE_TYPE_REPLY_START_CAPTURING;
                        bytes = socket.send(&replyMessage, sizeof(replyMessage));
                        hasConnect = bytes > 0;

                        break;
                    }

                    case profiler::net::MESSAGE_TYPE_REQUEST_STOP_CAPTURE:
                    {
#ifdef EASY_DEBUG_NET_PRINT
                        printf("RECEIVED MESSAGE_TYPE_REQUEST_STOP_CAPTURE\n");
#endif

                        EASY_EVENT("StopCapture", profiler::colors::Red, profiler::OFF);
                        ProfileManager::setEnabled(false);

                        //TODO
                        //if connection aborted - ignore this part

                        profiler::net::DataMessage dm;
                        profiler::OStream os;
                        dumpBlocksToStream(os);
                        dm.size = (uint32_t)os.stream().str().length();

                        int packet_size = int(sizeof(dm)) + int(dm.size);

                        char *sendbuf = new char[packet_size];

                        memset(sendbuf, 0, packet_size);
                        memcpy(sendbuf, &dm, sizeof(dm));
                        memcpy(sendbuf + sizeof(dm), os.stream().str().c_str(), dm.size);

                        bytes = socket.send(sendbuf, packet_size);
                        hasConnect = bytes > 0;

                        /*std::string tempfilename = "test_snd.prof";
                        std::ofstream of(tempfilename, std::fstream::binary);
                        of.write((const char*)os.stream().str().c_str(), dm.size);
                        of.close();*/

                        delete[] sendbuf;

                        replyMessage.type = profiler::net::MESSAGE_TYPE_REPLY_BLOCKS_END;
                        bytes = socket.send(&replyMessage, sizeof(replyMessage));
                        hasConnect = bytes > 0;

                        break;
                    }

                    case profiler::net::MESSAGE_TYPE_REQUEST_BLOCKS_DESCRIPTION:
                    {
#ifdef EASY_DEBUG_NET_PRINT
                        printf("RECEIVED MESSAGE_TYPE_REQUEST_BLOCKS_DESCRIPTION\n");
#endif

                        profiler::OStream os;

                        // Write profiler signature and version
                        os.write(PROFILER_SIGNATURE);
                        os.write(profiler::EASY_FULL_VERSION);

                        // Write block descriptors
                        m_storedSpin.lock();
                        os.write(static_cast<uint32_t>(m_descriptors.size()));
                        os.write(m_usedMemorySize);
                        for (const auto descriptor : m_descriptors)
                        {
                            const auto name_size = descriptor->nameSize();
                            const auto filename_size = descriptor->filenameSize();
                            const auto size = static_cast<uint16_t>(sizeof(profiler::SerializedBlockDescriptor) + name_size + filename_size);

                            os.write(size);
                            os.write<profiler::BaseBlockDescriptor>(*descriptor);
                            os.write(name_size);
                            os.write(descriptor->name(), name_size);
                            os.write(descriptor->filename(), filename_size);
                        }
                        m_storedSpin.unlock();
                        // END of Write block descriptors.

                        profiler::net::DataMessage dm((uint32_t)os.stream().str().length(), profiler::net::MESSAGE_TYPE_REPLY_BLOCKS_DESCRIPTION);
                        int packet_size = int(sizeof(dm)) + int(dm.size);

                        char *sendbuf = new char[packet_size];

                        memset(sendbuf, 0, packet_size);
                        memcpy(sendbuf, &dm, sizeof(dm));
                        memcpy(sendbuf + sizeof(dm), os.stream().str().c_str(), dm.size);

                        bytes = socket.send(sendbuf, packet_size);
                        hasConnect = bytes > 0;

                        delete[] sendbuf;

                        replyMessage.type = profiler::net::MESSAGE_TYPE_REPLY_BLOCKS_DESCRIPTION_END;
                        bytes = socket.send(&replyMessage, sizeof(replyMessage));
                        hasConnect = bytes > 0;

                        break;
                    }

                    case profiler::net::MESSAGE_TYPE_EDIT_BLOCK_STATUS:
                    {
#ifdef EASY_DEBUG_NET_PRINT
                        printf("RECEIVED MESSAGE_TYPE_EDIT_BLOCK_STATUS\n");
#endif

                        auto data = reinterpret_cast<const profiler::net::BlockStatusMessage*>(message);
                        setBlockStatus(data->id, static_cast<::profiler::EasyBlockStatus>(data->status));

                        break;
                    }

                    default:
                        break;
                }

                //nn_freemsg (buf);
            }
        }

        

    }

}
//////////////////////////////////////////////////////////////////////////

