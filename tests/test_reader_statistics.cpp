/** Statistics gathering on read — the reader's second half.

fillTreesFromFile takes a gather_statistics flag. With it on, besides
reconstructing the tree the reader computes three statistics sets per block:
within the parent, within the frame and within the whole thread. That work is
parallelized through its own thread pool and followed by a median pass.

This half of the code is called only from the GUI and used to be covered by
nothing, even though it produces the numbers the user reads in the tables.
*/

#include <catch2/catch_test_macros.hpp>

#include "support/prof_fixture.h"

#include <easy/arbitrary_value.h>
#include <easy/profiler.h>
#include <easy/reader.h>

#include <string>
#include <thread>
#include <vector>

using easy_test::LoadedProfile;

namespace {

/** A workload with repeated blocks; without repeats the statistics degenerate. */
void repeatedWorkload(int repeats)
{
    EASY_BLOCK("stats_root");
    for (int i = 0; i < repeats; ++i)
    {
        EASY_BLOCK("stats_repeated");
        volatile int sink = 0;
        for (int k = 0; k < 200 * (i + 1); ++k)
            sink = sink + k;
        (void)sink;
    }
}

} // namespace

TEST_CASE("Without the flag no statistics are computed", "[statistics][reader]")
{
    LoadedProfile profile;
    easy_test::capture(profile, [] { repeatedWorkload(4); }, /*gatherStatistics*/ false);

    REQUIRE(profile.readerLog.empty());

    const long root = easy_test::findBlockIndex(profile, "stats_root");
    REQUIRE(root >= 0);

    REQUIRE(profile.blocks[static_cast<size_t>(root)].per_thread_stats == nullptr);
    REQUIRE(profile.blocks[static_cast<size_t>(root)].per_frame_stats == nullptr);
    REQUIRE(profile.blocks[static_cast<size_t>(root)].per_parent_stats == nullptr);
}

TEST_CASE("With the flag the statistics are populated", "[statistics][reader]")
{
    LoadedProfile profile;
    easy_test::capture(profile, [] { repeatedWorkload(6); }, /*gatherStatistics*/ true);

    REQUIRE(profile.readerLog.empty());

    const long root = easy_test::findBlockIndex(profile, "stats_root");
    REQUIRE(root >= 0);

    const profiler::BlocksTree& rootTree = profile.blocks[static_cast<size_t>(root)];

    REQUIRE(rootTree.per_thread_stats != nullptr);
    REQUIRE(rootTree.per_thread_stats->calls_number >= 1);

    REQUIRE(rootTree.children.size() == 6);
    for (size_t i = 0; i < rootTree.children.size(); ++i)
    {
        const profiler::BlocksTree& child = profile.blocks[rootTree.children[i]];
        REQUIRE(child.per_parent_stats != nullptr);
        REQUIRE(child.per_thread_stats != nullptr);
    }
}

TEST_CASE("The call count covers every repetition", "[statistics][reader]")
{
    constexpr int repeats = 10;

    LoadedProfile profile;
    easy_test::capture(profile, [] { repeatedWorkload(repeats); }, true);

    REQUIRE(profile.readerLog.empty());

    const long root = easy_test::findBlockIndex(profile, "stats_root");
    REQUIRE(root >= 0);

    const profiler::BlocksTree& rootTree = profile.blocks[static_cast<size_t>(root)];
    REQUIRE(rootTree.children.size() == static_cast<size_t>(repeats));

    // All repetitions share one descriptor, so the per-parent statistics object
    // is shared and its calls_number equals the repeat count.
    const profiler::BlocksTree& firstChild = profile.blocks[rootTree.children[0]];
    REQUIRE(firstChild.per_parent_stats != nullptr);
    REQUIRE(firstChild.per_parent_stats->calls_number == static_cast<profiler::calls_number_t>(repeats));

    for (size_t i = 1; i < rootTree.children.size(); ++i)
    {
        const profiler::BlocksTree& child = profile.blocks[rootTree.children[i]];
        REQUIRE(child.per_parent_stats == firstChild.per_parent_stats);
    }
}

TEST_CASE("Total duration is never below any single one", "[statistics][reader]")
{
    LoadedProfile profile;
    easy_test::capture(profile, [] { repeatedWorkload(8); }, true);

    REQUIRE(profile.readerLog.empty());

    const long root = easy_test::findBlockIndex(profile, "stats_root");
    REQUIRE(root >= 0);

    const profiler::BlocksTree& rootTree = profile.blocks[static_cast<size_t>(root)];
    const profiler::BlockStatistics* stats = profile.blocks[rootTree.children[0]].per_parent_stats;
    REQUIRE(stats != nullptr);

    profiler::timestamp_t sum = 0;
    for (size_t i = 0; i < rootTree.children.size(); ++i)
        sum += profile.blocks[rootTree.children[i]].node->duration();

    REQUIRE(stats->total_duration == sum);
    REQUIRE(stats->average_duration() == sum / stats->calls_number);
}

TEST_CASE("The min and max duration blocks are identified correctly", "[statistics][reader]")
{
    // repeatedWorkload makes every next repetition longer than the previous, so
    // the minimum is expected first and the maximum last.
    LoadedProfile profile;
    easy_test::capture(profile, [] { repeatedWorkload(8); }, true);

    REQUIRE(profile.readerLog.empty());

    const long root = easy_test::findBlockIndex(profile, "stats_root");
    REQUIRE(root >= 0);

    const profiler::BlocksTree& rootTree = profile.blocks[static_cast<size_t>(root)];
    const profiler::BlockStatistics* stats = profile.blocks[rootTree.children[0]].per_parent_stats;
    REQUIRE(stats != nullptr);

    const profiler::timestamp_t minDuration =
        profile.blocks[stats->min_duration_block].node->duration();
    const profiler::timestamp_t maxDuration =
        profile.blocks[stats->max_duration_block].node->duration();

    REQUIRE(minDuration <= maxDuration);

    for (size_t i = 0; i < rootTree.children.size(); ++i)
    {
        const profiler::timestamp_t duration = profile.blocks[rootTree.children[i]].node->duration();
        REQUIRE(duration >= minDuration);
        REQUIRE(duration <= maxDuration);
    }
}

TEST_CASE("The median lies between the minimum and the maximum", "[statistics][reader]")
{
    // Medians are computed in a separate pass after statistics collection
    // (calculate_medians / calculate_medians_async in reader.cpp).
    LoadedProfile profile;
    easy_test::capture(profile, [] { repeatedWorkload(9); }, true);

    REQUIRE(profile.readerLog.empty());

    const long root = easy_test::findBlockIndex(profile, "stats_root");
    REQUIRE(root >= 0);

    const profiler::BlocksTree& rootTree = profile.blocks[static_cast<size_t>(root)];
    const profiler::BlockStatistics* stats = profile.blocks[rootTree.children[0]].per_parent_stats;
    REQUIRE(stats != nullptr);

    const profiler::timestamp_t minDuration =
        profile.blocks[stats->min_duration_block].node->duration();
    const profiler::timestamp_t maxDuration =
        profile.blocks[stats->max_duration_block].node->duration();

    REQUIRE(stats->median_duration >= minDuration);
    REQUIRE(stats->median_duration <= maxDuration);
}

TEST_CASE("Children duration does not exceed the parent duration", "[statistics][reader]")
{
    LoadedProfile profile;
    easy_test::capture(profile, [] { repeatedWorkload(5); }, true);

    REQUIRE(profile.readerLog.empty());

    const long root = easy_test::findBlockIndex(profile, "stats_root");
    REQUIRE(root >= 0);

    const profiler::BlocksTree& rootTree = profile.blocks[static_cast<size_t>(root)];
    REQUIRE(rootTree.per_thread_stats != nullptr);

    // total_children_duration sums the durations of the direct children.
    REQUIRE(rootTree.per_thread_stats->total_children_duration <= rootTree.node->duration());
}

TEST_CASE("Statistics on a multithreaded trace", "[statistics][reader][threads]")
{
    // The reader parallelizes the computation per trace thread, so multithreaded
    // input exercises a different path than single-threaded input.
    LoadedProfile profile;

    easy_test::capture(profile, [] {
        std::vector<std::thread> workers;
        for (int t = 0; t < 4; ++t)
        {
            workers.emplace_back([] {
                EASY_THREAD_SCOPE("stats_worker");
                repeatedWorkload(5);
            });
        }
        for (std::thread& worker : workers)
            worker.join();
    }, true);

    REQUIRE(profile.readerLog.empty());
    REQUIRE(profile.threadedTrees.size() >= 4);

    size_t threadsWithStats = 0;
    for (const auto& entry : profile.threadedTrees)
    {
        const profiler::BlocksTreeRoot& root = entry.second;
        if (root.children.empty())
            continue;

        bool allHaveStats = true;
        for (size_t i = 0; i < root.children.size(); ++i)
        {
            if (profile.blocks[root.children[i]].per_thread_stats == nullptr)
                allHaveStats = false;
        }

        if (allHaveStats)
            ++threadsWithStats;
    }

    REQUIRE(threadsWithStats >= 4);
}

TEST_CASE("A deep tree with statistics", "[statistics][reader]")
{
    LoadedProfile profile;

    easy_test::capture(profile, [] {
        EASY_BLOCK("deep_stats_root");
        for (int outer = 0; outer < 4; ++outer)
        {
            EASY_BLOCK("deep_stats_mid");
            for (int inner = 0; inner < 3; ++inner)
            {
                EASY_BLOCK("deep_stats_leaf");
            }
        }
    }, true);

    REQUIRE(profile.readerLog.empty());

    const long root = easy_test::findBlockIndex(profile, "deep_stats_root");
    REQUIRE(root >= 0);

    const profiler::BlocksTree& rootTree = profile.blocks[static_cast<size_t>(root)];
    REQUIRE(rootTree.per_thread_stats != nullptr);
    REQUIRE(rootTree.children.size() == 4);

    const profiler::BlocksTree& mid = profile.blocks[rootTree.children[0]];
    REQUIRE(mid.per_parent_stats != nullptr);
    REQUIRE(mid.per_parent_stats->calls_number == 4);
    REQUIRE(mid.children.size() == 3);

    const profiler::BlocksTree& leaf = profile.blocks[mid.children[0]];
    REQUIRE(leaf.per_parent_stats != nullptr);
    REQUIRE(leaf.per_parent_stats->calls_number == 3);
}

TEST_CASE("Subtree depth is filled in", "[statistics][reader]")
{
    LoadedProfile profile;

    easy_test::capture(profile, [] {
        EASY_BLOCK("depth_stats_1");
        EASY_BLOCK("depth_stats_2");
        EASY_BLOCK("depth_stats_3");
    }, true);

    REQUIRE(profile.readerLog.empty());

    const long outer = easy_test::findBlockIndex(profile, "depth_stats_1");
    const long inner = easy_test::findBlockIndex(profile, "depth_stats_3");
    REQUIRE(outer >= 0);
    REQUIRE(inner >= 0);

    REQUIRE(profile.blocks[static_cast<size_t>(outer)].depth >
            profile.blocks[static_cast<size_t>(inner)].depth);
    REQUIRE(profile.blocks[static_cast<size_t>(inner)].depth == 0);
}

TEST_CASE("Statistics on a trace containing values", "[statistics][reader][values]")
{
    // Values are tree records too, and the statistics pass must not trip on them.
    LoadedProfile profile;

    easy_test::capture(profile, [] {
        EASY_BLOCK("stats_with_values");
        for (int i = 0; i < 5; ++i)
        {
            EASY_BLOCK("stats_value_host");
            EASY_VALUE("stats_value", static_cast<int32_t>(i));
        }
    }, true);

    REQUIRE(profile.readerLog.empty());
    REQUIRE(easy_test::findValue(profile, "stats_value") != nullptr);

    const long root = easy_test::findBlockIndex(profile, "stats_with_values");
    REQUIRE(root >= 0);
    REQUIRE(profile.blocks[static_cast<size_t>(root)].per_thread_stats != nullptr);
}

TEST_CASE("A wide statistics table is processed by the reader worker pool", "[statistics][reader][threads]")
{
    constexpr int eventsCount = 1100;
    LoadedProfile profile;

    easy_test::capture(profile, [] {
        for (int i = 0; i < eventsCount; ++i)
        {
            const std::string name = "wide_stats_event_" + std::to_string(i);
            EASY_EVENT(name);
        }
    }, true);

    REQUIRE(profile.readerLog.empty());
    REQUIRE(profile.blocksRead == static_cast<profiler::block_index_t>(eventsCount));

    size_t eventsWithStats = 0;
    for (const profiler::BlocksTree& block : profile.blocks)
    {
        if (block.per_thread_stats != nullptr)
            ++eventsWithStats;
    }
    REQUIRE(eventsWithStats == static_cast<size_t>(eventsCount));
}
