/**
Lightweight profiler library for c++
Copyright(C) 2016  Sergey Yagovtsev

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

class ProfileManager
{
	ProfileManager();
	ProfileManager(const ProfileManager& p) = delete;
	ProfileManager& operator=(const ProfileManager&) = delete;
    static ProfileManager m_profileManager;

	bool m_isEnabled = false;

	typedef std::stack<profiler::Block*> stack_of_blocks_t;
	typedef std::map<size_t, stack_of_blocks_t> map_of_threads_stacks;

	map_of_threads_stacks m_openedBracketsMap;

	profiler::spin_lock m_spin;
	profiler::spin_lock m_storedSpin;
	typedef profiler::guard_lock<profiler::spin_lock> guard_lock_t;

	void _internalInsertBlock(profiler::Block* _block);

	typedef std::list<profiler::SerilizedBlock*> serialized_list_t;
	serialized_list_t m_blocks;

	
public:
    static ProfileManager& instance();
	~ProfileManager();
	void beginBlock(profiler::Block* _block);
	void endBlock();
	void setEnabled(bool isEnable);
	unsigned int dumpBlocksToFile(const char* filename);
};

#endif
