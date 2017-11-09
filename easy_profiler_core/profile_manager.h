/**
Lightweight profiler library for c++
Copyright(C) 2016-2017  Sergey Yagovtsev, Victor Zarubkin

Licensed under either of
    * MIT license (LICENSE.MIT or http://opensource.org/licenses/MIT)
    * Apache License, Version 2.0, (LICENSE.APACHE or http://www.apache.org/licenses/LICENSE-2.0)
at your option.

The MIT License
    Permission is hereby granted, free of charge, to any person obtaining a copy
    of this software and associated documentation files (the "Software"), to deal
    in the Software without restriction, including without limitation the rights 
    to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies 
    of the Software, and to permit persons to whom the Software is furnished 
    to do so, subject to the following conditions:

    The above copyright notice and this permission notice shall be included in all 
    copies or substantial portions of the Software.

    THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, 
    INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR 
    PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE 
    LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, 
    TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE 
    USE OR OTHER DEALINGS IN THE SOFTWARE.


The Apache License, Version 2.0 (the "License");
    You may not use this file except in compliance with the License.
    You may obtain a copy of the License at

    http://www.apache.org/licenses/LICENSE-2.0

    Unless required by applicable law or agreed to in writing, software
    distributed under the License is distributed on an "AS IS" BASIS,
    WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
    See the License for the specific language governing permissions and
    limitations under the License.

**/

#ifndef EASY_PROFILER_MANAGER_H
#define EASY_PROFILER_MANAGER_H

#include <easy/details/profiler_public_types.h>

#ifdef _WIN32
// Do not move this include to other place!
// It should be included before Windows.h which is included in spin_lock.h
# include <easy/easy_socket.h>
#endif // _WIN32

#include "spin_lock.h"
#include "outstream.h"
#include "hashed_cstr.h"
#include "thread_storage.h"

#include <map>
#include <vector>
#include <unordered_map>
#include <thread>
#include <atomic>
#include <type_traits>

//////////////////////////////////////////////////////////////////////////

typedef uint64_t processid_t;

class BlockDescriptor;

namespace profiler { class ValueId; }

class ProfileManager
{
#ifndef EASY_MAGIC_STATIC_AVAILABLE
    friend class ProfileManagerInstance;
#endif

    ProfileManager();

    typedef profiler::guard_lock<profiler::spin_lock> guard_lock_t;
    typedef std::map<profiler::thread_id_t, ThreadStorage> map_of_threads_stacks;
    typedef std::vector<BlockDescriptor*> block_descriptors_t;

#ifdef EASY_PROFILER_HASHED_CSTR_DEFINED
    typedef std::unordered_map<profiler::hashed_cstr, profiler::block_id_t> descriptors_map_t;
#else
    typedef std::unordered_map<profiler::hashed_stdstring, profiler::block_id_t> descriptors_map_t;
#endif

    const processid_t                     m_processId;

    map_of_threads_stacks                   m_threads;
    block_descriptors_t                 m_descriptors;
    descriptors_map_t                m_descriptorsMap;
    uint64_t                         m_usedMemorySize;
    profiler::timestamp_t                 m_beginTime;
    profiler::timestamp_t                   m_endTime;
    std::atomic<profiler::timestamp_t>     m_frameMax;
    std::atomic<profiler::timestamp_t>     m_frameAvg;
    std::atomic<profiler::timestamp_t>     m_frameCur;
    profiler::spin_lock                        m_spin;
    profiler::spin_lock                  m_storedSpin;
    profiler::spin_lock                    m_dumpSpin;
    std::atomic<profiler::thread_id_t> m_mainThreadId;
    std::atomic<char>                m_profilerStatus;
    std::atomic_bool          m_isEventTracingEnabled;
    std::atomic_bool             m_isAlreadyListening;
    std::atomic_bool                  m_frameMaxReset;
    std::atomic_bool                  m_frameAvgReset;
    std::atomic_bool                    m_stopDumping;

    std::string m_csInfoFilename = "/tmp/cs_profiling_info.log";

    uint32_t dumpBlocksToStream(profiler::OStream& _outputStream, bool _lockSpin, bool _async);
    void setBlockStatus(profiler::block_id_t _id, profiler::EasyBlockStatus _status);

    std::thread m_listenThread;
    void listen(uint16_t _port);

    std::atomic_bool m_stopListen;

public:

    ProfileManager(const ProfileManager&)              = delete;
    ProfileManager(ProfileManager&&)                   = delete;
    ProfileManager& operator = (const ProfileManager&) = delete;
    ProfileManager& operator = (ProfileManager&&)      = delete;

    static ProfileManager& instance();
    ~ProfileManager();

    const profiler::BaseBlockDescriptor* addBlockDescriptor(profiler::EasyBlockStatus _defaultStatus,
                                                            const char* _autogenUniqueId,
                                                            const char* _name,
                                                            const char* _filename,
                                                            int _line,
                                                            profiler::block_type_t _block_type,
                                                            profiler::color_t _color,
                                                            bool _copyName = false);

    void storeValue(const profiler::BaseBlockDescriptor* _desc, profiler::DataType _type, const void* _data, size_t _size, bool _isArray, profiler::ValueId _vin);
    bool storeBlock(const profiler::BaseBlockDescriptor* _desc, const char* _runtimeName);
    bool storeBlock(const profiler::BaseBlockDescriptor* _desc, const char* _runtimeName, profiler::timestamp_t _beginTime, profiler::timestamp_t _endTime);
    void beginBlock(profiler::Block& _block);
    void beginNonScopedBlock(const profiler::BaseBlockDescriptor* _desc, const char* _runtimeName);
    void endBlock();
    profiler::timestamp_t maxFrameDuration();
    profiler::timestamp_t avgFrameDuration();
    profiler::timestamp_t curFrameDuration() const;
    void setEnabled(bool isEnable);
    bool isEnabled() const;
    void setEventTracingEnabled(bool _isEnable);
    bool isEventTracingEnabled() const;
    uint32_t dumpBlocksToFile(const char* filename);
    const char* registerThread(const char* name, profiler::ThreadGuard& threadGuard);
    const char* registerThread(const char* name);

    void setContextSwitchLogFilename(const char* name)
    {
        m_csInfoFilename = name;
    }

    const char* getContextSwitchLogFilename() const
    {
        return m_csInfoFilename.c_str();
    }

    void beginContextSwitch(profiler::thread_id_t _thread_id, profiler::timestamp_t _time, profiler::thread_id_t _target_thread_id, const char* _target_process, bool _lockSpin = true);
    void endContextSwitch(profiler::thread_id_t _thread_id, processid_t _process_id, profiler::timestamp_t _endtime, bool _lockSpin = true);
    void startListen(uint16_t _port);
    void stopListen();
    bool isListening() const;

private:

    void registerThread();

    void beginFrame();
    void endFrame();

    void enableEventTracer();
    void disableEventTracer();

    static char checkThreadExpired(ThreadStorage& _registeredThread);

    void storeBlockForce(const profiler::BaseBlockDescriptor* _desc, const char* _runtimeName, ::profiler::timestamp_t& _timestamp);
    void storeBlockForce2(const profiler::BaseBlockDescriptor* _desc, const char* _runtimeName, ::profiler::timestamp_t _timestamp);
    void storeBlockForce2(ThreadStorage& _registeredThread, const profiler::BaseBlockDescriptor* _desc, const char* _runtimeName, ::profiler::timestamp_t _timestamp);

    ThreadStorage& _threadStorage(profiler::thread_id_t _thread_id);
    ThreadStorage* _findThreadStorage(profiler::thread_id_t _thread_id);

    inline ThreadStorage& threadStorage(profiler::thread_id_t _thread_id)
    {
        guard_lock_t lock(m_spin);
        return _threadStorage(_thread_id);
    }

    inline ThreadStorage* findThreadStorage(profiler::thread_id_t _thread_id)
    {
        guard_lock_t lock(m_spin);
        return _findThreadStorage(_thread_id);
    }

}; // END of class ProfileManager.

//////////////////////////////////////////////////////////////////////////

#endif // EASY_PROFILER_MANAGER_H
