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

#include <easy/profiler.h>
#include <easy/easy_socket.h>

#include "spin_lock.h"
#include "outstream.h"
#include "hashed_cstr.h"

#include <map>
#include <vector>
#include <unordered_map>
#include <thread>
#include <atomic>
//#include <list>

//////////////////////////////////////////////////////////////////////////

#ifdef _WIN32
#include <Windows.h>
#else
#include <sys/types.h>
#include <unistd.h>
#include <sys/syscall.h>
#include <chrono>
#include <time.h>
#endif

inline uint32_t getCurrentThreadId()
{
#ifdef _WIN32
    return (uint32_t)::GetCurrentThreadId();
#else
    EASY_THREAD_LOCAL static const pid_t x = syscall(__NR_gettid);
    EASY_THREAD_LOCAL static const uint32_t _id = (uint32_t)x;//std::hash<std::thread::id>()(std::this_thread::get_id());
    return _id;
#endif
}

namespace profiler {
    
    class SerializedBlock;

    struct do_not_calc_hash {
        template <class T> inline size_t operator()(T _value) const {
            return static_cast<size_t>(_value);
        }
    };

}

//////////////////////////////////////////////////////////////////////////

#ifndef EASY_ENABLE_BLOCK_STATUS
# define EASY_ENABLE_BLOCK_STATUS 1
#endif

#ifndef EASY_ENABLE_ALIGNMENT
# define EASY_ENABLE_ALIGNMENT 0
#endif

#ifndef EASY_ALIGNMENT_SIZE
# define EASY_ALIGNMENT_SIZE 64
#endif


#if EASY_ENABLE_ALIGNMENT == 0
# define EASY_ALIGNED(TYPE, VAR, A) TYPE VAR
# define EASY_MALLOC(MEMSIZE, A) malloc(MEMSIZE)
# define EASY_FREE(MEMPTR) free(MEMPTR)
#else
# if defined(_MSC_VER)
#  define EASY_ALIGNED(TYPE, VAR, A) __declspec(align(A)) TYPE VAR
#  define EASY_MALLOC(MEMSIZE, A) _aligned_malloc(MEMSIZE, A)
#  define EASY_FREE(MEMPTR) _aligned_free(MEMPTR)
# elif defined(__GNUC__)
#  define EASY_ALIGNED(TYPE, VAR, A) TYPE VAR __attribute__(aligned(A))
# else
#  define EASY_ALIGNED(TYPE, VAR, A) TYPE VAR
# endif
#endif

template <const uint16_t N>
class chunk_allocator
{
    struct chunk { EASY_ALIGNED(int8_t, data[N], EASY_ALIGNMENT_SIZE); chunk* prev = nullptr; };

    struct chunk_list
    {
        chunk* last = nullptr;

        ~chunk_list()
        {
            clear();
        }

        void clear()
        {
            do {
                auto p = last;
                last = last->prev;
                EASY_FREE(p);
            } while (last != nullptr);
        }

        chunk& back()
        {
            return *last;
        }

        void emplace_back()
        {
            auto prev = last;
            last = ::new (EASY_MALLOC(sizeof(chunk), EASY_ALIGNMENT_SIZE)) chunk();
            last->prev = prev;
            *(uint16_t*)last->data = 0;
        }

        void invert()
        {
            chunk* next = nullptr;

            while (last->prev != nullptr) {
                auto p = last->prev;
                last->prev = next;
                next = last;
                last = p;
            }

            last->prev = next;
        }
    };

    //typedef std::list<chunk> chunk_list;

    chunk_list m_chunks;
    uint32_t     m_size;
    uint16_t    m_shift;

public:

    chunk_allocator() : m_size(0), m_shift(0)
    {
        m_chunks.emplace_back();
    }

    void* allocate(uint16_t n)
    {
        ++m_size;

        if (!need_expand(n))
        {
            int8_t* data = m_chunks.back().data + m_shift;
            m_shift += n + sizeof(uint16_t);

            *(uint16_t*)data = n;
            data = data + sizeof(uint16_t);

            if (m_shift + 1 < N)
                *(uint16_t*)(data + n) = 0;

            return data;
        }

        m_shift = n + sizeof(uint16_t);
        m_chunks.emplace_back();
        auto data = m_chunks.back().data;

        *(uint16_t*)data = n;
        data = data + sizeof(uint16_t);

        *(uint16_t*)(data + n) = 0;
        return data;
    }

    inline bool need_expand(uint16_t n) const
    {
        return (m_shift + n + sizeof(uint16_t)) > N;
    }

    inline uint32_t size() const
    {
        return m_size;
    }

    inline bool empty() const
    {
        return m_size == 0;
    }

    void clear()
    {
        m_size = 0;
        m_shift = 0;
        m_chunks.clear();
        m_chunks.emplace_back();
    }

    /** Serialize data to stream.

    \warning Data will be cleared after serialization.
    */
    void serialize(profiler::OStream& _outputStream)
    {
        m_chunks.invert();

        auto current = m_chunks.last;
        do {
            const int8_t* data = current->data;
            uint16_t i = 0;
            while (i + 1 < N && *(uint16_t*)data != 0) {
                const uint16_t size = sizeof(uint16_t) + *(uint16_t*)data;
                _outputStream.write((const char*)data, size);
                data = data + size;
                i += size;
            }
            current = current->prev;
        } while (current != nullptr);

        clear();
    }
};

//////////////////////////////////////////////////////////////////////////

const uint16_t SIZEOF_CSWITCH = sizeof(profiler::BaseBlockData) + 1 + sizeof(uint16_t);

typedef std::vector<profiler::SerializedBlock*> serialized_list_t;

template <class T, const uint16_t N>
struct BlocksList
{
    BlocksList() = default;

    class Stack {
        //std::stack<T> m_stack;
        std::vector<T> m_stack;

    public:

        inline void clear() { m_stack.clear(); }
        inline bool empty() const { return m_stack.empty(); }

        inline void emplace(profiler::Block& _block) {
            //m_stack.emplace(_block);
            m_stack.emplace_back(_block);
        }

        inline void emplace(profiler::Block&& _block) {
            //m_stack.emplace(_block);
            m_stack.emplace_back(std::forward<profiler::Block&&>(_block));
        }

        template <class ... TArgs> inline void emplace(TArgs ... _args) {
            //m_stack.emplace(_args);
            m_stack.emplace_back(_args...);
        }

        inline T& top() {
            //return m_stack.top();
            return m_stack.back();
        }

        inline void pop() {
            //m_stack.pop();
            m_stack.pop_back();
        }
    };

    Stack                     openedList;
    chunk_allocator<N>        closedList;
    uint64_t          usedMemorySize = 0;

    void clearClosed() {
        //closedList.clear();
        usedMemorySize = 0;
    }
};

struct ThreadStorage
{
    BlocksList<std::reference_wrapper<profiler::Block>, SIZEOF_CSWITCH * (uint16_t)128U> blocks;
    BlocksList<profiler::Block, SIZEOF_CSWITCH * (uint16_t)128U>                           sync;
    std::string name;

#ifndef _WIN32
    const pthread_t pthread_id;
#endif

    const profiler::thread_id_t id;
    std::atomic<char> expired;
    std::atomic_bool frame; ///< is new frame working
    bool allowChildren;
    bool named;
    bool guarded;

    void storeBlock(const profiler::Block& _block);
    void storeCSwitch(const profiler::Block& _block);
    void clearClosed();
    void popSilent();

    ThreadStorage();
};

//////////////////////////////////////////////////////////////////////////

typedef uint32_t processid_t;

class BlockDescriptor;

class ProfileManager
{
#ifndef EASY_MAGIC_STATIC_CPP11
    friend class ProfileManagerInstance;
#endif

    ProfileManager();
    ProfileManager(const ProfileManager& p) = delete;
    ProfileManager& operator=(const ProfileManager&) = delete;

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

    std::string m_csInfoFilename = "/tmp/cs_profiling_info.log";

    uint32_t dumpBlocksToStream(profiler::OStream& _outputStream, bool _lockSpin);
    void setBlockStatus(profiler::block_id_t _id, profiler::EasyBlockStatus _status);

    std::thread m_listenThread;
    void listen(uint16_t _port);

    std::atomic_bool m_stopListen;

public:

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

    bool storeBlock(const profiler::BaseBlockDescriptor* _desc, const char* _runtimeName);
    void beginBlock(profiler::Block& _block);
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

    void beginFrame();
    void endFrame();

    void enableEventTracer();
    void disableEventTracer();

    char checkThreadExpired(ThreadStorage& _registeredThread);

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
};

#endif // EASY_PROFILER_MANAGER_H
