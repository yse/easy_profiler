/** The reader's thread pool when the system refuses a thread.

std::thread's constructor throws when a thread cannot be created, and the only
way to produce that on demand is to interpose pthread_create. The interposer
lives in support/thread_failure_shim.cpp, a shared library linked into this
binary alone: it replaces the symbol for the whole process, so this test gets
a target of its own.
*/

#include <catch2/catch_test_macros.hpp>

#include "support/prof_fixture.h"
#include "support/thread_failure_shim.h"

#include <easy/profiler.h>

#include <string>
#include <thread>
#include <vector>

using easy_test::LoadedProfile;

TEST_CASE("A reader task is withdrawn when its worker thread cannot be created",
          "[threads][reader][regression]")
{
    // DEFECT: async() queued the task and created the worker afterwards. When
    // std::thread threw, the exception left async() before it had returned the
    // future, so nothing could ever wait for that task - while a worker
    // already running was free to pick it up and run it against the statistics
    // map the unwinding had destroyed. ASan reported stack-use-after-scope.
    //
    // The median pass is only split across threads above 1000 statistics
    // entries, which sets the descriptor count below.
    //
    // The pool queues min(hardware_concurrency(), entries) tasks and asks for a
    // worker only while it has fewer threads than queued tasks. With one CPU it
    // queues one task and never asks twice; with two, the second request races
    // the first worker draining the queue. Below three CPUs the refusal - and so
    // the defect - is unreachable, and asserting it would fail a correct tree on
    // a single-core runner.
    const unsigned int cpus = std::thread::hardware_concurrency();
    if (cpus < 3)
    {
        WARN("skipped: " << cpus << " CPU(s) reported, the pool never asks for a second worker");
        return;
    }

    easy_test::drainPending();
    easy_test::TempFile file("thread_create_failure");

    // Large enough that the task left on the worker is still walking its slice
    // of the map while the unwinding frees it. With a few hundred entries per
    // task the withdrawn task sometimes finished first and the defect hid.
    const int descriptorCount = 20000;
    std::vector<std::string> names;
    names.reserve(descriptorCount);

    profiler::setEnabled(true);
    const profiler::timestamp_t base = profiler::timeNow();
    for (int i = 0; i < descriptorCount; ++i)
    {
        names.push_back("thread_failure_" + std::to_string(i));
        const auto* descriptor = profiler::registerDescription(
            profiler::ON, names.back().c_str(), names.back().c_str(), __FILE__, __LINE__,
            profiler::BlockType::Block, profiler::colors::Red, true);

        // Two samples per id so every entry has a median to compute.
        const auto begin = base + static_cast<profiler::timestamp_t>(i) * 10;
        profiler::storeBlock(descriptor, "", begin, begin + 5);
        profiler::storeBlock(descriptor, "", begin + 100000, begin + 100009);
    }
    profiler::setEnabled(false);
    REQUIRE(profiler::dumpBlocksToFile(file.c_str()) == descriptorCount * 2);

    // One thread is allowed through, so a task is already in flight on a worker
    // when the pool is refused its second one.
    easy_test_refuse_threads_after(1);
    LoadedProfile refused;
    easy_test::loadProfile(file.path(), refused, true);
    const int observed = easy_test_observed_thread_creations();
    const int refusals = easy_test_refused_thread_creations();
    easy_test_refuse_threads_after(-1);

    // The interposer has to have seen the pool's requests, or nothing below
    // tests the defect: the pool asks once for the worker that is let through
    // and once for the one that is refused.
    REQUIRE(observed == 2);
    REQUIRE(refusals == 1);

    REQUIRE(refused.blocksRead == 0);
    REQUIRE_FALSE(refused.readerLog.empty());

    LoadedProfile accepted;
    easy_test::loadProfile(file.path(), accepted, true);
    INFO("reader log: " << accepted.readerLog);
    REQUIRE(accepted.readerLog.empty());
    REQUIRE(accepted.blocksRead == descriptorCount * 2);
}
