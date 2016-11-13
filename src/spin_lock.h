/**
Lightweight profiler library for c++
Copyright(C) 2016  Sergey Yagovtsev, Victor Zarubkin


Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.


GNU General Public License Usage
Alternatively, this file may be used under the terms of the GNU
General Public License as published by the Free Software Foundation,
either version 3 of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.If not, see <http://www.gnu.org/licenses/>.
**/

#ifndef EASY_PROFILER__SPIN_LOCK__________H______
#define EASY_PROFILER__SPIN_LOCK__________H______

#define EASY_USE_CRITICAL_SECTION // Use CRITICAL_SECTION instead of std::atomic_flag

#if defined(_WIN32) && defined(EASY_USE_CRITICAL_SECTION)
#include <Windows.h>
#else
#include <atomic>
#endif

namespace profiler {

#if defined(_WIN32) && defined(EASY_USE_CRITICAL_SECTION)
    // std::atomic_flag on Windows works slower than critical section, so we will use it instead of std::atomic_flag...
    // By the way, Windows critical sections are slower than std::atomic_flag on Unix.
    class spin_lock { CRITICAL_SECTION m_lock; public:

        void lock() {
            EnterCriticalSection(&m_lock);
        }

        void unlock() {
            LeaveCriticalSection(&m_lock);
        }

        spin_lock() {
            InitializeCriticalSection(&m_lock);
        }

        ~spin_lock() {
            DeleteCriticalSection(&m_lock);
        }
    };
#else
    // std::atomic_flag on Unix works fine and very fast (almost instant!)
    class spin_lock { ::std::atomic_flag m_lock; public:

        void lock() {
            while (m_lock.test_and_set(::std::memory_order_acquire));
        }

        void unlock() {
            m_lock.clear(::std::memory_order_release);
        }

        spin_lock() {
            m_lock.clear();
        }
    };
#endif

    template <class T>
    class guard_lock
    {
        T& m_lock;
        bool m_isLocked = false;

    public:

        explicit guard_lock(T& m) : m_lock(m) {
            m_lock.lock();
            m_isLocked = true;
        }

        ~guard_lock() {
            unlock();
        }

        inline void unlock() {
            if (m_isLocked) {
                m_lock.unlock();
                m_isLocked = false;
            }
        }
    };

} // END of namespace profiler.

#ifdef EASY_USE_CRITICAL_SECTION
# undef EASY_USE_CRITICAL_SECTION
#endif

#endif // EASY_PROFILER__SPIN_LOCK__________H______
