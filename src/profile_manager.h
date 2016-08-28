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

#ifdef WIN32
#include <Windows.h>
#else
#include <thread>
#endif

inline uint32_t getCurrentThreadId()
{
#ifdef WIN32
	return (uint32_t)::GetCurrentThreadId();
#else
	thread_local static uint32_t _id = (uint32_t)std::hash<std::thread::id>()(std::this_thread::get_id());
	return _id;
#endif
}

namespace profiler { class SerializedBlock; }

//////////////////////////////////////////////////////////////////////////

template <class T, uint16_t N>
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

        m_size = 0;
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

class ThreadStorage
{
    typedef std::stack<std::reference_wrapper<profiler::Block> > stack_of_blocks_t;
    typedef std::vector<profiler::SerializedBlock*> serialized_list_t;

    chunk_allocator<char, 1024> m_allocator;

public:

    stack_of_blocks_t         openedList;
    serialized_list_t         closedList;
    uint64_t          usedMemorySize = 0;
    bool                   named = false;

    void store(const profiler::Block& _block);
    void clearClosed();
};

//////////////////////////////////////////////////////////////////////////

class ProfileManager
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
};

#endif
