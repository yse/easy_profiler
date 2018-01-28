/**
 * */

#include "thread_pool_task.h"
#include "thread_pool.h"

ThreadPoolTask::ThreadPoolTask() : m_func([]{}), m_interrupt(nullptr)
{
    m_status = ATOMIC_VAR_INIT(static_cast<int8_t>(TaskStatus::Finished));
}

ThreadPoolTask::~ThreadPoolTask()
{
    dequeue();
}

void ThreadPoolTask::enqueue(std::function<void()>&& func, std::atomic_bool& interruptFlag)
{
    dequeue();

    m_interrupt = & interruptFlag;
    m_interrupt->store(false, std::memory_order_release);
    m_func = std::move(func);

    setStatus(TaskStatus::Enqueued);
    ThreadPool::instance().enqueue(*this);
}

void ThreadPoolTask::dequeue()
{
    if (m_interrupt == nullptr)
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
