/** spin_lock — the core's synchronization primitive.

ProfileManager guards thread registration, the descriptor table and the dump path
with spin locks (profile_manager.h:110-112). The profiler has to stay cheap, so
an atomic_flag with busy waiting is used instead of a mutex; the correctness of
that mutual exclusion was never checked.
*/

#include <catch2/catch_test_macros.hpp>

#include "spin_lock.h"

#include <atomic>
#include <thread>
#include <vector>

TEST_CASE("The lock can be taken and released", "[spinlock]")
{
    profiler::spin_lock lock;

    lock.lock();
    lock.unlock();

    lock.lock();
    lock.unlock();

    SUCCEED("sequential lock and unlock do not block");
}

TEST_CASE("guard_lock releases the lock in its destructor", "[spinlock]")
{
    profiler::spin_lock lock;

    {
        profiler::guard_lock<profiler::spin_lock> guard(lock);
    }

    // Had the destructor not released the lock, this would hang forever.
    lock.lock();
    lock.unlock();

    SUCCEED("guard_lock released the lock");
}

TEST_CASE("guard_lock allows an early release", "[spinlock]")
{
    profiler::spin_lock lock;

    {
        profiler::guard_lock<profiler::spin_lock> guard(lock);
        guard.unlock();

        // After an explicit unlock the destructor must not release again, so the
        // lock is already free and can be taken right here.
        lock.lock();
        lock.unlock();
    }

    lock.lock();
    lock.unlock();

    SUCCEED("no double release happened");
}

TEST_CASE("Mutual exclusion under thread contention", "[spinlock][threads]")
{
    profiler::spin_lock lock;

    // Deliberately non-atomic: without mutual exclusion increments would be lost.
    long long counter = 0;

    const int threadCount = 4;
    // Enough contention to exercise the lock repeatedly without turning a
    // scheduled ThreadSanitizer run into a timeout on slower CI workers.
    const int perThread = 1000;

    std::vector<std::thread> workers;
    workers.reserve(threadCount);

    for (int t = 0; t < threadCount; ++t)
    {
        workers.emplace_back([&lock, &counter] {
            for (int i = 0; i < perThread; ++i)
            {
                profiler::guard_lock<profiler::spin_lock> guard(lock);
                ++counter;
            }
        });
    }

    for (std::thread& worker : workers)
        worker.join();

    REQUIRE(counter == static_cast<long long>(threadCount) * perThread);
}

TEST_CASE("The critical section never runs concurrently", "[spinlock][threads]")
{
    profiler::spin_lock lock;

    std::atomic<int> insideNow(0);
    std::atomic<int> maxObserved(0);
    std::atomic<bool> violated(false);

    const int threadCount = 6;
    const int iterations = 5000;

    std::vector<std::thread> workers;
    workers.reserve(threadCount);

    for (int t = 0; t < threadCount; ++t)
    {
        workers.emplace_back([&] {
            for (int i = 0; i < iterations; ++i)
            {
                profiler::guard_lock<profiler::spin_lock> guard(lock);

                const int current = insideNow.fetch_add(1) + 1;
                if (current > 1)
                    violated.store(true);

                int previousMax = maxObserved.load();
                while (current > previousMax &&
                       !maxObserved.compare_exchange_weak(previousMax, current))
                {
                }

                insideNow.fetch_sub(1);
            }
        });
    }

    for (std::thread& worker : workers)
        worker.join();

    REQUIRE_FALSE(violated.load());
    REQUIRE(maxObserved.load() == 1);
    REQUIRE(insideNow.load() == 0);
}

TEST_CASE("Independent locks do not interfere", "[spinlock][threads]")
{
    profiler::spin_lock first;
    profiler::spin_lock second;

    // Holding one lock must not block the other — otherwise the spin lock would
    // be global rather than per-object.
    first.lock();

    std::atomic<bool> acquiredSecond(false);
    std::thread worker([&second, &acquiredSecond] {
        profiler::guard_lock<profiler::spin_lock> guard(second);
        acquiredSecond.store(true);
    });
    worker.join();

    REQUIRE(acquiredSecond.load());

    first.unlock();
}
