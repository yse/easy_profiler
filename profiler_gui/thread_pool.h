/**
 * */

#ifndef EASY_PROFILER_THREAD_POOL_H
#define EASY_PROFILER_THREAD_POOL_H

#include "thread_pool_task.h"
#include <vector>
#include <deque>
#include <condition_variable>
#include <thread>

class ThreadPool EASY_FINAL
{
    friend ThreadPoolTask;

    std::vector<std::thread> m_threads;
    std::deque<std::reference_wrapper<ThreadPoolTask> > m_tasks;
    std::mutex m_mutex;
    std::condition_variable m_cv;
    std::atomic_bool m_interrupt;

    ThreadPool();

public:

    ~ThreadPool();

    static ThreadPool& instance();

private:

    void enqueue(ThreadPoolTask& task);
    void dequeue(ThreadPoolTask& task);
    void work();

}; // end of class ThreadPool.

#endif //EASY_PROFILER_THREAD_POOL_H
