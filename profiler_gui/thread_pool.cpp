/************************************************************************
* file name         : thread_pool.cpp
* ----------------- :
* creation time     : 2018/01/28
* author            : Victor Zarubkin
* email             : v.s.zarubkin@gmail.com
* ----------------- :
* description       : The file contains implementation of ThreadPool.
* ----------------- :
* license           : Lightweight profiler library for c++
*                   : Copyright(C) 2016-2018  Sergey Yagovtsev, Victor Zarubkin
*                   :
*                   : Licensed under either of
*                   :     * MIT license (LICENSE.MIT or http://opensource.org/licenses/MIT)
*                   :     * Apache License, Version 2.0, (LICENSE.APACHE or http://www.apache.org/licenses/LICENSE-2.0)
*                   : at your option.
*                   :
*                   : The MIT License
*                   :
*                   : Permission is hereby granted, free of charge, to any person obtaining a copy
*                   : of this software and associated documentation files (the "Software"), to deal
*                   : in the Software without restriction, including without limitation the rights
*                   : to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies
*                   : of the Software, and to permit persons to whom the Software is furnished
*                   : to do so, subject to the following conditions:
*                   :
*                   : The above copyright notice and this permission notice shall be included in all
*                   : copies or substantial portions of the Software.
*                   :
*                   : THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED,
*                   : INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR
*                   : PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE
*                   : LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
*                   : TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE
*                   : USE OR OTHER DEALINGS IN THE SOFTWARE.
*                   :
*                   : The Apache License, Version 2.0 (the "License")
*                   :
*                   : You may not use this file except in compliance with the License.
*                   : You may obtain a copy of the License at
*                   :
*                   : http://www.apache.org/licenses/LICENSE-2.0
*                   :
*                   : Unless required by applicable law or agreed to in writing, software
*                   : distributed under the License is distributed on an "AS IS" BASIS,
*                   : WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
*                   : See the License for the specific language governing permissions and
*                   : limitations under the License.
************************************************************************/

#include "thread_pool.h"
#include <algorithm>

#ifdef _MSC_VER
// std::back_inserter is defined in <iterator> for Visual C++ ...
#include <iterator>
#endif

ThreadPool& ThreadPool::instance()
{
    static ThreadPool pool;
    return pool;
}

ThreadPool::ThreadPool()
{
    const auto threadsCount = std::max(std::thread::hardware_concurrency(), 2U);

    m_threads.reserve(threadsCount);
    std::generate_n(std::back_inserter(m_threads), threadsCount, [this] {
        return std::thread(&ThreadPool::work, this);
    });
}

ThreadPool::~ThreadPool()
{
    m_interrupt.store(true, std::memory_order_release);
    m_cv.notify_all();
    for (auto& thread : m_threads)
        thread.join();
}

void ThreadPool::enqueue(ThreadPoolTask& task)
{
    m_mutex.lock();
    m_tasks.emplace_back(task);
    m_mutex.unlock();
    m_cv.notify_one();
}

void ThreadPool::dequeue(ThreadPoolTask& task)
{
    const std::lock_guard<std::mutex> lock(m_mutex);

    if (task.status() != TaskStatus::Enqueued)
        return;

    for (auto it = m_tasks.begin(); it != m_tasks.end(); ++it)
    {
        if (&it->get() == &task)
        {
            m_tasks.erase(it);
            break;
        }
    }
}

void ThreadPool::work()
{
    while (true)
    {
        std::unique_lock<std::mutex> lock(m_mutex);
        m_cv.wait(lock, [this] { return !m_tasks.empty() || m_interrupt.load(std::memory_order_acquire); });

        if (m_interrupt.load(std::memory_order_acquire))
            break;

        if (m_tasks.empty())
            continue;

        auto& task = m_tasks.front().get();
        task.setStatus(TaskStatus::Processing);
        m_tasks.pop_front();

        lock.unlock();

        task.execute();
    }
}
