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
#include "profile_manager.h"
#include "profiler/serialized_block.h"
#include "profiler/easy_net.h"

#include "easy_socket.h"
#include "event_trace_win.h"

#include <thread>
#include <string.h>

#include <fstream>

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

using namespace profiler;

#ifdef _WIN32
extern decltype(LARGE_INTEGER::QuadPart) CPU_FREQUENCY;
#endif

//auto& MANAGER = ProfileManager::instance();
#define MANAGER ProfileManager::instance()

extern "C" {

    PROFILER_API block_id_t registerDescription(const char* _name, const char* _filename, int _line, block_type_t _block_type, color_t _color)
    {
        return MANAGER.addBlockDescriptor(_name, _filename, _line, _block_type, _color);
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
    _used_mem += sizeof(profiler::SerializedBlockDescriptor) + strlen(_name) + strlen(_filename) + 2;
}

//////////////////////////////////////////////////////////////////////////

void ThreadStorage::storeBlock(const profiler::Block& block)
{
    auto name_length = static_cast<uint16_t>(strlen(block.name()));
    auto size = static_cast<uint16_t>(sizeof(BaseBlockData) + name_length + 1);
    auto data = blocks.alloc.allocate(size);
    ::new (static_cast<void*>(data)) SerializedBlock(block, name_length);
    blocks.usedMemorySize += size;
    blocks.closedList.emplace_back(reinterpret_cast<SerializedBlock*>(data));
}

void ThreadStorage::storeCSwitch(const profiler::Block& block)
{
    auto name_length = static_cast<uint16_t>(strlen(block.name()));
    auto size = static_cast<uint16_t>(sizeof(BaseBlockData) + name_length + 1);
    auto data = sync.alloc.allocate(size);
    ::new (static_cast<void*>(data)) SerializedBlock(block, name_length);
    sync.usedMemorySize += size;
    sync.closedList.emplace_back(reinterpret_cast<SerializedBlock*>(data));
}

void ThreadStorage::clearClosed()
{
    blocks.clearClosed();
    sync.clearClosed();
}

//////////////////////////////////////////////////////////////////////////

EASY_THREAD_LOCAL static ::ThreadStorage* THREAD_STORAGE = nullptr;

// #ifdef _WIN32
// LPTOP_LEVEL_EXCEPTION_FILTER PREVIOUS_FILTER = NULL;
// LONG WINAPI easyTopLevelExceptionFilter(struct _EXCEPTION_POINTERS* ExceptionInfo)
// {
//     std::ofstream testexp("TEST_EXP.txt", std::fstream::binary);
//     testexp.write("APPLICATION CRASHED!", strlen("APPLICATION CRASHED!"));
// 
//     EasyEventTracer::instance().disable();
//     if (PREVIOUS_FILTER)
//         return PREVIOUS_FILTER(ExceptionInfo);
//     return EXCEPTION_CONTINUE_SEARCH;
// }
// #endif

ProfileManager::ProfileManager()
{
// #ifdef _WIN32
//     PREVIOUS_FILTER = SetUnhandledExceptionFilter(easyTopLevelExceptionFilter);
    // #endif

    m_stopListen = ATOMIC_VAR_INIT(false);
}

ProfileManager::~ProfileManager()
{
    stopListenSignalToCapture();
    if(m_listenThread.joinable()){
        m_listenThread.join();
    }
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

    if (THREAD_STORAGE == nullptr)
        THREAD_STORAGE = &threadStorage(getCurrentThreadId());

    if (!_block.isFinished())
        THREAD_STORAGE->blocks.openedList.emplace(_block);
    else
        THREAD_STORAGE->storeBlock(_block);
}

void ProfileManager::beginContextSwitch(profiler::thread_id_t _thread_id, profiler::timestamp_t _time, profiler::block_id_t _id)
{
    auto ts = findThreadStorage(_thread_id);
    if (ts != nullptr)
        ts->sync.openedList.emplace(_time, profiler::BLOCK_TYPE_CONTEXT_SWITCH, _id, "");
}

void ProfileManager::storeContextSwitch(profiler::thread_id_t _thread_id, profiler::timestamp_t _time, profiler::block_id_t _id)
{
    auto ts = findThreadStorage(_thread_id);
    if (ts != nullptr)
    {
        profiler::Block lastBlock(_time, profiler::BLOCK_TYPE_CONTEXT_SWITCH, _id, "");
        lastBlock.finish(_time);
        ts->storeCSwitch(lastBlock);
    }
}

void ProfileManager::endBlock()
{
    if (!m_isEnabled)
        return;

    if (THREAD_STORAGE->blocks.openedList.empty())
        return;

    Block& lastBlock = THREAD_STORAGE->blocks.openedList.top();
    if (!lastBlock.isFinished())
        lastBlock.finish();

    THREAD_STORAGE->storeBlock(lastBlock);
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

void ProfileManager::startListenSignalToCapture()
{
    if(!m_isAlreadyListened)
    {
        m_stopListen.store(false);
        m_listenThread  = std::thread(&ProfileManager::startListen, this);
        m_isAlreadyListened = true;

    }
}

void ProfileManager::stopListenSignalToCapture()
{
    m_stopListen.store(true);
    m_isAlreadyListened = false;
}

void ProfileManager::setEnabled(bool isEnable)
{
    m_isEnabled = isEnable;
}

//////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////

#define STORE_CSWITCHES_SEPARATELY

uint32_t ProfileManager::dumpBlocksToStream(StreamWriter& of)
{
    const bool wasEnabled = m_isEnabled;
    if (wasEnabled)
        ::profiler::setEnabled(false);

#ifndef _WIN32
    uint64_t timestamp;
    uint32_t thread_from, thread_to;

    std::ifstream infile(m_csInfoFilename.c_str());

    if(infile.is_open())
    {
        static const auto desc = addBlockDescriptor("OS.ContextSwitch", __FILE__, __LINE__, ::profiler::BLOCK_TYPE_CONTEXT_SWITCH, ::profiler::colors::White);
        while (infile >> timestamp >> thread_from >> thread_to)
        {
            beginContextSwitch(thread_from, timestamp, desc);
            endContextSwitch(thread_to, timestamp);
        }
    }

#endif
    uint64_t usedMemorySize = 0;
    uint32_t blocks_number = 0;
    for (const auto& thread_storage : m_threads)
    {
        const auto& t = thread_storage.second;
        usedMemorySize += t.blocks.usedMemorySize + t.sync.usedMemorySize;
        blocks_number += static_cast<uint32_t>(t.blocks.closedList.size()) + static_cast<uint32_t>(t.sync.closedList.size());
    }

#ifdef _WIN32
    of.write(CPU_FREQUENCY);
#else
    of.write(0LL);
#endif

    of.write(blocks_number);
    of.write(usedMemorySize);
    of.write(static_cast<uint32_t>(m_descriptors.size()));
    of.write(m_usedMemorySize);

    for (const auto& descriptor : m_descriptors)
    {
        const auto name_size = static_cast<uint16_t>(strlen(descriptor.name()) + 1);
        const auto filename_size = static_cast<uint16_t>(strlen(descriptor.file()) + 1);
        const auto size = static_cast<uint16_t>(sizeof(profiler::SerializedBlockDescriptor) + name_size + filename_size);

        of.write(size);
        of.write<profiler::BaseBlockDescriptor>(descriptor);
        of.write(name_size);
        of.write(descriptor.name(), name_size);
        of.write(descriptor.file(), filename_size);
    }

    for (auto& thread_storage : m_threads)
    {
        auto& t = thread_storage.second;

        of.write(thread_storage.first);
#ifdef STORE_CSWITCHES_SEPARATELY
        of.write(static_cast<uint32_t>(t.blocks.closedList.size()));
#else
        of.write(static_cast<uint32_t>(t.blocks.closedList.size()) + static_cast<uint32_t>(t.sync.closedList.size()));
        uint32_t i = 0;
#endif

        for (auto b : t.blocks.closedList)
        {
#ifndef STORE_CSWITCHES_SEPARATELY
            if (i < t.sync.closedList.size())
            {
                auto s = t.sync.closedList[i];
                if (s->end() <= b->end())// || s->begin() >= b->begin())
                //if (((s->end() <= b->end() && s->end() >= b->begin()) || (s->begin() >= b->begin() && s->begin() <= b->end())))
                {
                    if (s->m_begin < b->m_begin)
                        s->m_begin = b->m_begin;
                    if (s->m_end > b->m_end)
                        s->m_end = b->m_end;
                    of.writeBlock(s);
                    ++i;
                }
            }
#endif

            of.writeBlock(b);
        }

#ifdef STORE_CSWITCHES_SEPARATELY
        of.write(static_cast<uint32_t>(t.sync.closedList.size()));
        for (auto b : t.sync.closedList)
        {
#else
        for (; i < t.sync.closedList.size(); ++i)
        {
            auto b = t.sync.closedList[i];
#endif

            of.writeBlock(b);
        }

        t.clearClosed();
    }

//     if (wasEnabled)
//         ::profiler::setEnabled(true);

    return blocks_number;
}

uint32_t ProfileManager::dumpBlocksToFile(const char* filename)
{
    StreamWriter os;
    auto res =  dumpBlocksToStream(os);



    std::ofstream of(filename, std::fstream::binary);
    of << os.stream().str();

    return res;
}

const char* ProfileManager::setThreadName(const char* name, const char* filename, const char* _funcname, int line)
{
    if (THREAD_STORAGE == nullptr)
    {
        THREAD_STORAGE = &threadStorage(getCurrentThreadId());
    }

    if (!THREAD_STORAGE->named)
    {
        const auto id = addBlockDescriptor(_funcname, filename, line, profiler::BLOCK_TYPE_THREAD_SIGN, profiler::colors::Black);
        THREAD_STORAGE->storeBlock(profiler::Block(profiler::BLOCK_TYPE_THREAD_SIGN, id, name));
        THREAD_STORAGE->name = name;
        THREAD_STORAGE->named = true;
    }

    return THREAD_STORAGE->name.c_str();
}

//////////////////////////////////////////////////////////////////////////

/*
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>

*/

void ProfileManager::startListen()
{

    EasySocket socket;

    socket.setAddress("192.224.4.115",28077);
    while(!m_stopListen.load() && (socket.connect() < 0 ) ) {}

    profiler::net::Message replyMessage(profiler::net::MESSAGE_TYPE_REPLY_START_CAPTURING);

    char buffer[256] = {};
    //bzero(buffer,256);
    while(!m_stopListen.load())
    {
        auto bytes = socket.read(buffer,255);

        char *buf = &buffer[0];

        if (bytes == 0)
        {
            while (!m_stopListen.load() && (socket.connect() < 0)) {}
            continue;
        }

        if(bytes > 0)
        {
            profiler::net::Message* message = (profiler::net::Message*)buf;
            if(!message->isEasyNetMessage()){
                continue;
            }

            switch (message->type) {
                case profiler::net::MESSAGE_TYPE_REQUEST_START_CAPTURE:
                {
                    printf ("RECEIVED MESSAGE_TYPE_REQUEST_START_CAPTURE\n");
                    profiler::setEnabled(true);

                    replyMessage.type = profiler::net::MESSAGE_TYPE_REPLY_START_CAPTURING;
                    socket.write(&replyMessage,sizeof(replyMessage));
                }
                    break;
                case profiler::net::MESSAGE_TYPE_REQUEST_STOP_CAPTURE:
                {
                    printf ("RECEIVED MESSAGE_TYPE_REQUEST_STOP_CAPTURE\n");
                    profiler::setEnabled(false);

                    replyMessage.type = profiler::net::MESSAGE_TYPE_REPLY_PREPARE_BLOCKS;
                    auto send_bytes = socket.write(&replyMessage,sizeof(replyMessage));


                    profiler::net::DataMessage dm;
                    StreamWriter os;
                    dumpBlocksToStream(os);
                    dm.size = (uint32_t)os.stream().str().length();

                    int packet_size = int(sizeof(dm))+int(dm.size);

                    char *sendbuf = new char[packet_size];

                    memset(sendbuf,0,packet_size);
                    memcpy(sendbuf,&dm,sizeof(dm));
                    memcpy(sendbuf + sizeof(dm),os.stream().str().c_str(),dm.size);

                    send_bytes = socket.write(sendbuf,packet_size);


                    /*std::string tempfilename = "test_snd.prof";
                    std::ofstream of(tempfilename, std::fstream::binary);
                    of.write((const char*)os.stream().str().c_str(), dm.size);
                    of.close();*/

                    delete [] sendbuf;
                    //std::this_thread::sleep_for(std::chrono::seconds(2));
                    replyMessage.type = profiler::net::MESSAGE_TYPE_REPLY_END_SEND_BLOCKS;
                    //send_bytes = socket.write(&replyMessage,sizeof(replyMessage));
                }
                    break;
                default:
                    break;
            }


            //nn_freemsg (buf);
        }

    }

}
