/*
 * */

#include "thread_pool.h"
#include <algorithm>

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
