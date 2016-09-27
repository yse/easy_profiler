/**
Lightweight profiler library for c++
Copyright(C) 2016  Sergey Yagovtsev, Victor Zarubkin

This program is free software : you can redistribute it and / or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.If not, see <http://www.gnu.org/licenses/>.
**/

#ifndef EASY_PROFILER____MANAGER____H______
#define EASY_PROFILER____MANAGER____H______

#include "profiler/profiler.h"
#include "profiler/easy_socket.h"
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

#if EASY_ENABLE_ALIGNMENT == 0
# define EASY_ALIGNED(TYPE, VAR, A) TYPE VAR
# define EASY_MALLOC(MEMSIZE, A) malloc(MEMSIZE)
# define EASY_FREE(MEMPTR) free(MEMPTR)
#else
# if defined(_WIN32)
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
    struct chunk { EASY_ALIGNED(int8_t, data[N], 64); chunk* prev = nullptr; };

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
            last = ::new (EASY_MALLOC(sizeof(chunk), 64)) chunk();
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
    profiler::thread_id_t id = 0;
    std::atomic_bool expired;
    bool allowChildren = true;
    bool named = false;

    void storeBlock(const profiler::Block& _block);
    void storeCSwitch(const profiler::Block& _block);
    void clearClosed();

    ThreadStorage()
    {
        expired = ATOMIC_VAR_INIT(false);
    }
};

//////////////////////////////////////////////////////////////////////////

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

#ifdef _WIN32
    typedef std::unordered_map<profiler::hashed_cstr, profiler::block_id_t> descriptors_map_t;
#else
    typedef std::unordered_map<profiler::hashed_stdstring, profiler::block_id_t> descriptors_map_t;
#endif

    map_of_threads_stacks             m_threads;
    block_descriptors_t           m_descriptors;
    descriptors_map_t          m_descriptorsMap;
    uint64_t               m_usedMemorySize = 0;
    profiler::timestamp_t       m_beginTime = 0;
    profiler::timestamp_t         m_endTime = 0;
    profiler::spin_lock                  m_spin;
    profiler::spin_lock            m_storedSpin;
    std::atomic_bool                m_isEnabled;
    std::atomic_bool    m_isEventTracingEnabled;

#ifndef _WIN32
    std::string m_csInfoFilename = "/tmp/cs_profiling_info.log";
#endif

    uint32_t dumpBlocksToStream(profiler::OStream& _outputStream);
    void setBlockStatus(profiler::block_id_t _id, profiler::EasyBlockStatus _status);

    std::thread m_listenThread;
    bool m_isAlreadyListened = false;
    void listen();

    int m_socket = 0;//TODO crossplatform

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
                                                            profiler::color_t _color);

    void storeBlock(const profiler::BaseBlockDescriptor* _desc, const char* _runtimeName);
    void beginBlock(profiler::Block& _block);
    void endBlock();
    void setEnabled(bool isEnable, bool _setTime = true);
    void setEventTracingEnabled(bool _isEnable);
    uint32_t dumpBlocksToFile(const char* filename);
    const char* registerThread(const char* name, profiler::ThreadGuard& threadGuard);

#ifndef _WIN32
    void setContextSwitchLogFilename(const char* name)
    {
        m_csInfoFilename = name;
    }

    const char* getContextSwitchLogFilename() const
    {
        return m_csInfoFilename.c_str();
    }
#endif

    void beginContextSwitch(profiler::thread_id_t _thread_id, profiler::timestamp_t _time, profiler::thread_id_t _target_thread_id, const char* _target_process, bool _lockSpin = true);
    void storeContextSwitch(profiler::thread_id_t _thread_id, profiler::timestamp_t _time, profiler::thread_id_t _target_thread_id, bool _lockSpin = true);
    void endContextSwitch(profiler::thread_id_t _thread_id, profiler::timestamp_t _endtime, bool _lockSpin = true);
    void startListenSignalToCapture();
    void stopListenSignalToCapture();

private:

    void storeBlockForce(const profiler::BaseBlockDescriptor* _desc, const char* _runtimeName, ::profiler::timestamp_t& _timestamp);
    void storeBlockForce2(const profiler::BaseBlockDescriptor* _desc, const char* _runtimeName, ::profiler::timestamp_t _timestamp);

    ThreadStorage& threadStorage(profiler::thread_id_t _thread_id);
    ThreadStorage* _findThreadStorage(profiler::thread_id_t _thread_id);

    ThreadStorage* findThreadStorage(profiler::thread_id_t _thread_id)
    {
        guard_lock_t lock(m_spin);
        return _findThreadStorage(_thread_id);
    }
};

#endif // EASY_PROFILER____MANAGER____H______
