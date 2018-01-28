/*
 * */

#ifndef EASY_PROFILER_THREAD_POOL_TASK_H
#define EASY_PROFILER_THREAD_POOL_TASK_H

#include <easy/details/easy_compiler_support.h>
#include <functional>
#include <atomic>
#include <mutex>

enum class TaskStatus : int8_t
{
    Enqueued,
    Processing,
    Finished,
};

class ThreadPoolTask EASY_FINAL
{
    friend class ThreadPool;

    std::function<void()>   m_func;
    std::mutex             m_mutex;
    std::atomic<int8_t>   m_status;
    std::atomic_bool*  m_interrupt;

public:

    ThreadPoolTask(const ThreadPoolTask&) = delete;
    ThreadPoolTask(ThreadPoolTask&&) = delete;

    ThreadPoolTask();
    ~ThreadPoolTask();

    void enqueue(std::function<void()>&& func, std::atomic_bool& interruptFlag);
    void dequeue();

private:

    void execute();

    TaskStatus status() const;
    void setStatus(TaskStatus status);
};

#endif //EASY_PROFILER_THREAD_POOL_TASK_H
