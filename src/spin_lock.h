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

#ifndef ________SPIN_LOCK__________H______
#define ________SPIN_LOCK__________H______

#include <atomic>

namespace profiler{

	class spin_lock
	{
		std::atomic_flag m_lock;
	public:
		void lock()
		{
			while (m_lock.test_and_set(std::memory_order_acquire)){}
		}

		void unlock()
		{
			m_lock.clear(std::memory_order_release);
		}

		spin_lock(){
			m_lock.clear();
		}
	};

	template<class T>
	class guard_lock
	{
		T&  m_mutex;
		bool m_isLocked = false;
	public:
		guard_lock(T& m) :m_mutex(m){
			m_mutex.lock();
			m_isLocked = true;
		}
		~guard_lock(){
			this->release();
		}
		inline void release(){
			if (m_isLocked){
				m_mutex.unlock();
				m_isLocked = false;
			}

		}
	};
}

#endif
