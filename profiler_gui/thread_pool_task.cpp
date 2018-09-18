/************************************************************************
* file name         : thread_pool_task.cpp
* ----------------- :
* creation time     : 2018/01/28
* author            : Victor Zarubkin
* email             : v.s.zarubkin@gmail.com
* ----------------- :
* description       : The file contains implementation of ThreadPoolTask.
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

#include "thread_pool_task.h"
#include "thread_pool.h"

ThreadPoolTask::ThreadPoolTask() : m_func([]{}), m_interrupt(nullptr)
{
    m_status = static_cast<int8_t>(TaskStatus::Finished);
}

ThreadPoolTask::~ThreadPoolTask()
{
    dequeue();
}

void ThreadPoolTask::enqueue(std::function<void()>&& func, std::atomic_bool& interruptFlag)
{
    dequeue();
    setStatus(TaskStatus::Enqueued);

    m_interrupt = & interruptFlag;
    m_interrupt->store(false, std::memory_order_release);
    m_func = std::move(func);

    ThreadPool::instance().enqueue(*this);
}

void ThreadPoolTask::dequeue()
{
    if (m_interrupt == nullptr || status() == TaskStatus::Finished)
        return;

    m_interrupt->store(true, std::memory_order_release);

    ThreadPool::instance().dequeue(*this);

    // wait for finish
    m_mutex.lock();
    setStatus(TaskStatus::Finished);
    m_mutex.unlock();

    m_interrupt->store(false, std::memory_order_release);
}

TaskStatus ThreadPoolTask::status() const
{
    return static_cast<TaskStatus>(m_status.load(std::memory_order_acquire));
}

void ThreadPoolTask::execute()
{
    const std::lock_guard<std::mutex> guard(m_mutex);

    if (status() == TaskStatus::Finished || m_interrupt->load(std::memory_order_acquire))
        return;

    m_func();
    setStatus(TaskStatus::Finished);
}

void ThreadPoolTask::setStatus(TaskStatus status)
{
    m_status.store(static_cast<int8_t>(status), std::memory_order_release);
}
