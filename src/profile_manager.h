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
#include "spin_lock.h"
#include "outstream.h"
//#include "hashed_cstr.h"
#include <map>
#include <list>
#include <vector>
#include <string>
#include <functional>

//////////////////////////////////////////////////////////////////////////

#ifdef _WIN32
#include <Windows.h>
#else
#include <thread>
#include <sys/types.h>
#include <unistd.h>
#include <sys/syscall.h>
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

namespace profiler { class SerializedBlock; }

//////////////////////////////////////////////////////////////////////////

//#define EASY_ENABLE_ALIGNMENT

#ifndef EASY_ENABLE_ALIGNMENT
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
class chunk_allocator final
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

            if (m_shift < N)
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
            do {
                const uint16_t size = sizeof(uint16_t) + *(uint16_t*)data;
                _outputStream.write((const char*)data, size);
                data = data + size;
                i += size;
            } while (i < N && *(uint16_t*)data != 0);
            current = current->prev;
        } while (current != nullptr);

        clear();
    }
};

//////////////////////////////////////////////////////////////////////////

const uint16_t SIZEOF_CSWITCH = sizeof(profiler::BaseBlockData) + 1 + sizeof(uint16_t);

typedef std::vector<profiler::SerializedBlock*> serialized_list_t;

template <class T, const uint16_t N>
struct BlocksList final
{
    BlocksList() = default;

    class Stack final {
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


class ThreadStorage final
{
public:

    BlocksList<std::reference_wrapper<profiler::Block>, SIZEOF_CSWITCH * (uint16_t)128U> blocks;
    BlocksList<profiler::Block, SIZEOF_CSWITCH * (uint16_t)128U>                           sync;
    std::string name;
    bool named = false;

    void storeBlock(const profiler::Block& _block);
    void storeCSwitch(const profiler::Block& _block);
    void clearClosed();

    ThreadStorage() = default;
};

//////////////////////////////////////////////////////////////////////////

class ProfileManager final
{
    friend profiler::BlockDescRef;

    ProfileManager();
    ProfileManager(const ProfileManager& p) = delete;
    ProfileManager& operator=(const ProfileManager&) = delete;

    typedef profiler::guard_lock<profiler::spin_lock> guard_lock_t;
    typedef std::map<profiler::thread_id_t, ThreadStorage> map_of_threads_stacks;
    typedef std::vector<profiler::BlockDescriptor*> block_descriptors_t;
    typedef std::vector<profiler::block_id_t> expired_ids_t;

    map_of_threads_stacks          m_threads;
    block_descriptors_t        m_descriptors;
    expired_ids_t       m_expiredDescriptors;
    uint64_t            m_usedMemorySize = 0;
    profiler::spin_lock               m_spin;
    profiler::spin_lock         m_storedSpin;
    profiler::block_id_t     m_idCounter = 0;
    bool                 m_isEnabled = false;

    std::string m_csInfoFilename = "/tmp/cs_profiling_info.log";

    uint32_t dumpBlocksToStream(profiler::OStream& _outputStream);

public:

    static ProfileManager& instance();
    ~ProfileManager();

    template <class ... TArgs>
    const profiler::BaseBlockDescriptor& addBlockDescriptor(bool _enabledByDefault, TArgs ... _args)
    {
        auto desc = new profiler::BlockDescriptor(m_usedMemorySize, _enabledByDefault, _args...);

        guard_lock_t lock(m_storedSpin);
        desc->setId(m_idCounter++);
        m_descriptors.emplace_back(desc);

        return *desc;
    }

    void storeBlock(const profiler::BaseBlockDescriptor& _desc, const char* _runtimeName);
    void beginBlock(profiler::Block& _block);
    void endBlock();
    void setEnabled(bool isEnable);
    uint32_t dumpBlocksToFile(const char* filename);
    const char* setThreadName(const char* name, const char* filename, const char* _funcname, int line);

    void setContextSwitchLogFilename(const char* name)
    {
        m_csInfoFilename = name;
    }

    const char* getContextSwitchLogFilename() const
    {
        return m_csInfoFilename.c_str();
    }

    void beginContextSwitch(profiler::thread_id_t _thread_id, profiler::timestamp_t _time, profiler::block_id_t _id);
    void storeContextSwitch(profiler::thread_id_t _thread_id, profiler::timestamp_t _time, profiler::block_id_t _id);
    void endContextSwitch(profiler::thread_id_t _thread_id, profiler::timestamp_t _endtime);

private:

    void markExpired(profiler::block_id_t _id);
    ThreadStorage& threadStorage(profiler::thread_id_t _thread_id);
    ThreadStorage* findThreadStorage(profiler::thread_id_t _thread_id);
};

#endif // EASY_PROFILER____MANAGER____H______
