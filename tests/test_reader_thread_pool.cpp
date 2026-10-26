/** The reader's thread pool across many loads.

ReaderThreadPool lives for one fillTreesFromStream call. The hang it used to
have surfaced once in thousands of loads, so this is a loop test rather than a
single load. It sits in the thread target for that target's explicit CTest
timeout, and carries its own watchdog so a wedge is reported with the load
count instead of waiting for CTest.
*/

#include <catch2/catch_test_macros.hpp>

#include "support/prof_fixture.h"

#include <easy/profiler.h>

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdio>
#include <cstdlib>
#include <mutex>
#include <thread>
#include <vector>

using easy_test::LoadedProfile;

namespace {

class Watchdog
{
public:
    Watchdog(std::chrono::seconds limit, const std::atomic<int>& loads)
        : m_done(false)
        , m_thread([this, limit, &loads] {
            std::unique_lock<std::mutex> lock(m_mutex);
            if (!m_cv.wait_for(lock, limit, [this] { return m_done; }))
            {
                std::fprintf(stderr, "the reader wedged after %d loads\n", loads.load());
                std::abort();
            }
        })
    {
    }

    ~Watchdog()
    {
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            m_done = true;
        }
        m_cv.notify_all();
        m_thread.join();
    }

private:
    std::mutex              m_mutex;
    std::condition_variable m_cv;
    bool                    m_done;
    std::thread             m_thread;
};

} // namespace

TEST_CASE("Loading a profile repeatedly never hangs in the reader thread pool",
          "[threads][reader][regression]")
{
    // DEFECT: ~ReaderThreadPool set its stop flag outside the mutex guarding
    // the workers' condition-variable predicate. A worker that had evaluated
    // the predicate but not yet entered wait() missed notify_all() and blocked
    // forever, and join() in the destructor never returned, inside
    // fillTreesFromStream. Against the unfixed pool this profile shape wedged
    // within 15 to 141 loads in every run.
    //
    // Statistics gathering is what submits work to the pool, one task per
    // thread tree, and every worker races the destructor independently, hence
    // eight threads rather than one.
    easy_test::drainPending();
    easy_test::TempFile file("thread_pool_loop");

    profiler::setEnabled(true);
    {
        std::vector<std::thread> workers;
        for (int i = 0; i < 8; ++i)
        {
            workers.emplace_back([] {
                EASY_THREAD_SCOPE("pool_loop_worker");
                EASY_BLOCK("pool_loop_outer");
                { EASY_BLOCK("pool_loop_inner"); }
            });
        }
        for (auto& worker : workers)
            worker.join();
    }
    profiler::setEnabled(false);
    REQUIRE(profiler::dumpBlocksToFile(file.c_str()) == 24);

    std::atomic<int> loads(0);
    Watchdog watchdog(std::chrono::seconds(45), loads);

    for (; loads < 2000; ++loads)
    {
        LoadedProfile profile;
        easy_test::loadProfile(file.path(), profile, true);
        INFO("load " << loads.load() << ", reader log: " << profile.readerLog);
        REQUIRE(profile.readerLog.empty());

        // Counted by name: in the single-process coverage runner the main
        // thread's storage from earlier cases is dumped too, as a ninth tree.
        size_t workerTrees = 0;
        for (const auto& tree : profile.threadedTrees)
        {
            if (tree.second.thread_name == "pool_loop_worker")
                ++workerTrees;
        }
        REQUIRE(workerTrees == 8);
    }
}
