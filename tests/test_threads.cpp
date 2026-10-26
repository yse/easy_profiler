/** Multithreaded capture.

Every thread keeps its own ThreadStorage with its own chunks, so the hot path
carries no cross-thread coordination at all - that is needed only when a thread
registers and when a dump happens. These tests check that nothing gets lost in
the process and that threads do not mix up each other's blocks.
*/

#include <catch2/catch_test_macros.hpp>

#include "support/prof_fixture.h"

#include <easy/arbitrary_value.h>
#include <easy/profiler.h>

#include <atomic>
#include <string>
#include <thread>
#include <vector>

using easy_test::LoadedProfile;

TEST_CASE("registerThread returns the thread name", "[threads]")
{
    std::string reported;

    std::thread worker([&reported] {
        const char* name = profiler::registerThread("named_worker");
        reported = (name != nullptr) ? name : "";
    });
    worker.join();

    REQUIRE(reported == "named_worker");
}

TEST_CASE("The first name registration wins", "[threads]")
{
    std::string firstName;
    std::string secondName;

    std::thread worker([&] {
        const char* first = profiler::registerThread("original_name");
        firstName = (first != nullptr) ? first : "";

        // Registering again does not rename the thread:
        // "Only first call of registerThread() for the current thread will have an effect".
        const char* second = profiler::registerThread("attempted_rename");
        secondName = (second != nullptr) ? second : "";
    });
    worker.join();

    REQUIRE(firstName == "original_name");
    REQUIRE(secondName == "original_name");
}

TEST_CASE("Blocks from several threads are not lost", "[threads]")
{
    LoadedProfile profile;

    const int threadCount = 4;
    const int blocksPerThread = 250;

    easy_test::capture(profile, [=] {
        std::vector<std::thread> workers;
        workers.reserve(threadCount);

        for (int t = 0; t < threadCount; ++t)
        {
            workers.emplace_back([=] {
                EASY_THREAD_SCOPE("capture_worker");
                for (int i = 0; i < blocksPerThread; ++i)
                {
                    EASY_BLOCK("worker_block");
                }
            });
        }

        for (std::thread& worker : workers)
            worker.join();
    });

    REQUIRE(profile.readerLog.empty());

    // Not a single block lost. Counting own blocks rather than the grand total:
    // EASY_THREAD_SCOPE adds one "ThreadFinished" event per thread
    // (profile_manager.cpp:243), so the total is higher by threadCount.
    REQUIRE(easy_test::countBlocksNamed(profile, "worker_block")
            == static_cast<size_t>(threadCount * blocksPerThread));

    REQUIRE(profile.blocksDumped
            == static_cast<uint32_t>(threadCount * blocksPerThread + threadCount));
    REQUIRE(profile.blocksRead == profile.blocksDumped);
}

TEST_CASE("Each thread gets its own subtree", "[threads]")
{
    LoadedProfile profile;

    const int threadCount = 3;

    easy_test::capture(profile, [=] {
        std::vector<std::thread> workers;
        workers.reserve(threadCount);

        for (int t = 0; t < threadCount; ++t)
        {
            workers.emplace_back([] {
                EASY_THREAD_SCOPE("subtree_worker");
                EASY_BLOCK("subtree_root");
                { EASY_BLOCK("subtree_child"); }
            });
        }

        for (std::thread& worker : workers)
            worker.join();
    });

    REQUIRE(profile.readerLog.empty());

    REQUIRE(profile.threadedTrees.size() >= static_cast<size_t>(threadCount));

    // Every worker thread has a subtree_root with one child. A thread may hold
    // two roots: EASY_THREAD_SCOPE adds a top-level "ThreadFinished" event.
    int matchedThreads = 0;
    for (const auto& entry : profile.threadedTrees)
    {
        const profiler::BlocksTreeRoot& root = entry.second;
        if (root.children.empty())
            continue;

        const profiler::block_index_t rootIndex = root.children[0];
        if (std::string(easy_test::blockName(profile, rootIndex)) != "subtree_root")
            continue;

        REQUIRE(profile.blocks[rootIndex].children.size() == 1);
        REQUIRE(std::string(easy_test::blockName(profile, profile.blocks[rootIndex].children[0]))
                == "subtree_child");
        ++matchedThreads;
    }

    REQUIRE(matchedThreads == threadCount);
}

TEST_CASE("Nesting inside a thread does not mix with other threads", "[threads]")
{
    LoadedProfile profile;

    easy_test::capture(profile, [] {
        std::thread deep([] {
            EASY_THREAD_SCOPE("deep_worker");
            EASY_BLOCK("deep_a");
            EASY_BLOCK("deep_b");
            EASY_BLOCK("deep_c");
        });

        std::thread flat([] {
            EASY_THREAD_SCOPE("flat_worker");
            { EASY_BLOCK("flat_a"); }
            { EASY_BLOCK("flat_b"); }
        });

        deep.join();
        flat.join();
    });

    REQUIRE(profile.readerLog.empty());

    const long deepA = easy_test::findBlockIndex(profile, "deep_a");
    REQUIRE(deepA >= 0);
    REQUIRE(profile.blocks[static_cast<size_t>(deepA)].children.size() == 1);

    // The flat thread's blocks stayed siblings and did not end up inside deep_a.
    const std::vector<std::string> deepChildren =
        easy_test::childNames(profile, static_cast<profiler::block_index_t>(deepA));
    REQUIRE(deepChildren.size() == 1);
    REQUIRE(deepChildren[0] == "deep_b");
}

TEST_CASE("A thread that finished before the dump keeps its blocks", "[threads]")
{
    LoadedProfile profile;

    easy_test::capture(profile, [] {
        std::thread shortLived([] {
            EASY_THREAD_SCOPE("short_lived_worker");
            EASY_BLOCK("short_lived_block");
        });
        shortLived.join();
    });

    REQUIRE(profile.readerLog.empty());
    REQUIRE(easy_test::findBlockIndex(profile, "short_lived_block") >= 0);
}

TEST_CASE("A thread that expires after capture stops receives an expiration marker", "[threads][lifecycle]")
{
    easy_test::drainPending();
    std::atomic<bool> payloadStored(false);
    std::atomic<bool> mayExit(false);

    profiler::setEnabled(true);
    std::thread worker([&] {
        EASY_THREAD_SCOPE("expired_marker_worker");
        EASY_EVENT("expired_marker_payload");
        payloadStored.store(true, std::memory_order_release);
        while (!mayExit.load(std::memory_order_acquire))
        {
        }
    });

    while (!payloadStored.load(std::memory_order_acquire))
    {
    }
    profiler::setEnabled(false);
    mayExit.store(true, std::memory_order_release);
    worker.join();

    easy_test::TempFile file("expired_marker");
    REQUIRE(profiler::dumpBlocksToFile(file.c_str()) >= 2);

    LoadedProfile profile;
    easy_test::loadProfile(file.path(), profile);
    INFO(profile.readerLog);
    REQUIRE(profile.readerLog.empty());
    REQUIRE(easy_test::findBlockIndex(profile, "expired_marker_payload") >= 0);
    REQUIRE(easy_test::findBlockIndex(profile, "ThreadExpired") >= 0);
}

TEST_CASE("Many short-lived threads", "[threads]")
{
    LoadedProfile profile;

    const int rounds = 30;

    easy_test::capture(profile, [=] {
        for (int i = 0; i < rounds; ++i)
        {
            std::thread worker([] {
                EASY_THREAD_SCOPE("churn_worker");
                EASY_BLOCK("churn_block");
            });
            worker.join();
        }
    });

    REQUIRE(profile.readerLog.empty());
    REQUIRE(easy_test::countBlocksNamed(profile, "churn_block") == static_cast<size_t>(rounds));
}

TEST_CASE("Concurrent capture and thread registration", "[threads]")
{
    LoadedProfile profile;

    const int threadCount = 8;
    std::atomic<int> ready(0);

    easy_test::capture(profile, [&] {
        std::vector<std::thread> workers;
        workers.reserve(threadCount);

        for (int t = 0; t < threadCount; ++t)
        {
            workers.emplace_back([&] {
                // Start all threads at roughly the same moment so registration
                // and capture overlap.
                ready.fetch_add(1);
                while (ready.load() < threadCount)
                {
                }

                EASY_THREAD_SCOPE("race_worker");
                for (int i = 0; i < 100; ++i)
                {
                    EASY_BLOCK("race_block");
                }
            });
        }

        for (std::thread& worker : workers)
            worker.join();
    });

    REQUIRE(profile.readerLog.empty());
    REQUIRE(easy_test::countBlocksNamed(profile, "race_block")
            == static_cast<size_t>(threadCount * 100));
}

TEST_CASE("Values from different threads do not get mixed up", "[threads][values]")
{
    LoadedProfile profile;

    easy_test::capture(profile, [] {
        std::thread first([] {
            EASY_THREAD_SCOPE("value_worker_one");
            EASY_BLOCK("value_host_one");
            EASY_VALUE("thread_value_one", static_cast<int32_t>(111));
        });

        std::thread second([] {
            EASY_THREAD_SCOPE("value_worker_two");
            EASY_BLOCK("value_host_two");
            EASY_VALUE("thread_value_two", static_cast<int32_t>(222));
        });

        first.join();
        second.join();
    });

    REQUIRE(profile.readerLog.empty());

    const profiler::ArbitraryValue* one = easy_test::findValue(profile, "thread_value_one");
    const profiler::ArbitraryValue* two = easy_test::findValue(profile, "thread_value_two");

    REQUIRE(one != nullptr);
    REQUIRE(two != nullptr);
    REQUIRE(one->toValue<int32_t>()->value() == 111);
    REQUIRE(two->toValue<int32_t>()->value() == 222);
}
