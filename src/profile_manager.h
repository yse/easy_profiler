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

#ifndef ___PROFILER____MANAGER____H______
#define ___PROFILER____MANAGER____H______

#include "profiler/profiler.h"

#include "spin_lock.h"

#include <stack>
#include <map>
#include <list>
#include <set>
#include <vector>
#include <functional>

//////////////////////////////////////////////////////////////////////////

#ifdef _WIN32
#include <Windows.h>
#else
#include <thread>
#endif

inline uint32_t getCurrentThreadId()
{
#ifdef _WIN32
	return (uint32_t)::GetCurrentThreadId();
#else
	thread_local static uint32_t _id = (uint32_t)std::hash<std::thread::id>()(std::this_thread::get_id());
	return _id;
#endif
}

namespace profiler { class SerializedBlock; }

//////////////////////////////////////////////////////////////////////////

template <class T, const uint16_t N>
class chunk_allocator final
{
    struct chunk { T data[N]; };

    std::list<chunk> m_chunks;
    uint16_t           m_size;

public:

    chunk_allocator() : m_size(0)
    {
        m_chunks.emplace_back();
    }

    T* allocate(uint16_t n)
    {
        if (m_size + n <= N)
        {
            T* data = m_chunks.back().data + m_size;
            m_size += n;
            return data;
        }

        m_size = n;
        m_chunks.emplace_back();
        return m_chunks.back().data;
    }

    void clear()
    {
        m_size = 0;
        m_chunks.clear();
        m_chunks.emplace_back();
    }
};

//////////////////////////////////////////////////////////////////////////

const uint16_t SIZEOF_CSWITCH = sizeof(profiler::BaseBlockData) + 1;

class ThreadStorage final
{
    typedef std::vector<profiler::SerializedBlock*> serialized_list_t;

    template <class T, const uint16_t N>
    struct BlocksList final
    {
        typedef std::stack<T> stack_of_blocks_t;

        chunk_allocator<char, N>       alloc;
        stack_of_blocks_t         openedList;
        serialized_list_t         closedList;
        uint64_t          usedMemorySize = 0;

        void clearClosed() {
            serialized_list_t().swap(closedList);
            alloc.clear();
            usedMemorySize = 0;
        }
    };

public:

    BlocksList<std::reference_wrapper<profiler::Block>, SIZEOF_CSWITCH * (uint16_t)1024U> blocks;
    BlocksList<profiler::Block, SIZEOF_CSWITCH * (uint16_t)128U>                            sync;
    bool named = false;

    void storeBlock(const profiler::Block& _block);
    void storeCSwitch(const profiler::Block& _block);
    void clearClosed();
};

//////////////////////////////////////////////////////////////////////////

class ProfileManager final
{
    friend profiler::StaticBlockDescriptor;

    ProfileManager();
    ProfileManager(const ProfileManager& p) = delete;
    ProfileManager& operator=(const ProfileManager&) = delete;

    typedef profiler::guard_lock<profiler::spin_lock> guard_lock_t;
    typedef std::map<profiler::thread_id_t, ThreadStorage> map_of_threads_stacks;
    typedef std::vector<profiler::BlockDescriptor> block_descriptors_t;

    map_of_threads_stacks     m_threads;
    block_descriptors_t   m_descriptors;
    uint64_t       m_usedMemorySize = 0;
    profiler::spin_lock          m_spin;
    profiler::spin_lock    m_storedSpin;
    bool            m_isEnabled = false;

public:

    static ProfileManager& instance();
	~ProfileManager();

    void beginBlock(profiler::Block& _block);
	void endBlock();
	void setEnabled(bool isEnable);
    uint32_t dumpBlocksToFile(const char* filename);
    void setThreadName(const char* name, const char* filename, const char* _funcname, int line);

    void _cswitchBeginBlock(profiler::timestamp_t _time, profiler::block_id_t _id, profiler::thread_id_t _thread_id);
    void _cswitchEndBlock(profiler::thread_id_t _thread_id, profiler::timestamp_t _endtime);

private:

    template <class ... TArgs>
    uint32_t addBlockDescriptor(TArgs ... _args)
    {
        guard_lock_t lock(m_storedSpin);
        const auto id = static_cast<uint32_t>(m_descriptors.size());
        m_descriptors.emplace_back(m_usedMemorySize, _args...);
        return id;
    }

    ThreadStorage& threadStorage(profiler::thread_id_t _thread_id)
    {
        guard_lock_t lock(m_spin);
        return m_threads[_thread_id];
    }

    ThreadStorage* _threadStorage(profiler::thread_id_t _thread_id)
    {
        guard_lock_t lock(m_spin);
        auto it = m_threads.find(_thread_id);
        return it != m_threads.end() ? &it->second : nullptr;
    }
};

#endif
