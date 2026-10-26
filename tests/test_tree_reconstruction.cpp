/** Reconstructing the call tree from the bracket sequence.

This is the format's defining property: the tree structure is NOT stored in the
file at all — no parent pointers, no depth, no child counts. Blocks nest strictly,
so the sequence of their opens and closes is a well-formed bracket sequence; a
block is serialized at the moment it closes, so write order is a post-order walk.
From post-order plus the [begin, end) intervals — which have to be stored
anyway — the tree follows uniquely.

The reconstruction algorithm is reader.cpp:1174-1190: the flat list acts as a
stack of pending siblings, and an arriving block adopts the tail of that list if
it began before the last entry there ended.

A mistake here means wrong topology in every trace with no error message at all,
so topology is asserted exactly rather than by "contains".
*/

#include <catch2/catch_test_macros.hpp>

#include "support/prof_image.h"
#include "support/prof_fixture.h"

#include <easy/profiler.h>

#include "thread_storage.h"

#include <string>
#include <vector>

using easy_test::LoadedProfile;

namespace {

/** Render a subtree as "Parent(Child1,Child2(Grandchild))". */
std::string renderSubtree(const LoadedProfile& profile, profiler::block_index_t index)
{
    std::string result = easy_test::blockName(profile, index);

    const profiler::BlocksTree& entry = profile.blocks[index];
    if (entry.children.empty())
        return result;

    result += '(';
    for (size_t i = 0; i < entry.children.size(); ++i)
    {
        if (i != 0)
            result += ',';
        result += renderSubtree(profile, entry.children[i]);
    }
    result += ')';

    return result;
}

/** Render every root block of the single thread. */
std::string renderTree(const LoadedProfile& profile)
{
    if (profile.threadedTrees.empty())
        return std::string();

    const profiler::BlocksTreeRoot& root = profile.threadedTrees.begin()->second;

    std::string result;
    for (size_t i = 0; i < root.children.size(); ++i)
    {
        if (i != 0)
            result += ',';
        result += renderSubtree(profile, root.children[i]);
    }
    return result;
}

} // namespace

TEST_CASE("A single block", "[tree]")
{
    LoadedProfile profile;

    easy_test::capture(profile, [] {
        EASY_BLOCK("solo");
    });

    REQUIRE(profile.readerLog.empty());
    REQUIRE(renderTree(profile) == "solo");
}

TEST_CASE("Flat siblings do not nest into each other", "[tree]")
{
    LoadedProfile profile;

    easy_test::capture(profile, [] {
        { EASY_BLOCK("flat_a"); }
        { EASY_BLOCK("flat_b"); }
        { EASY_BLOCK("flat_c"); }
    });

    REQUIRE(profile.readerLog.empty());

    // Three roots, none with children. This is exactly where the algorithm would
    // break if it adopted on "block ended earlier" instead of "began earlier".
    REQUIRE(renderTree(profile) == "flat_a,flat_b,flat_c");
}

TEST_CASE("A deep nesting chain", "[tree]")
{
    LoadedProfile profile;

    easy_test::capture(profile, [] {
        EASY_BLOCK("chain_1");
        EASY_BLOCK("chain_2");
        EASY_BLOCK("chain_3");
        EASY_BLOCK("chain_4");
    });

    REQUIRE(profile.readerLog.empty());
    REQUIRE(renderTree(profile) == "chain_1(chain_2(chain_3(chain_4)))");
}

TEST_CASE("Frame shape: a parent with two branches", "[tree]")
{
    // The shape the principle is easiest to explain on:
    //   ( Frame ( Update ( Physics ) ( AI ) ) ( Render ) )
    // Write order, by close time: Physics, AI, Update, Render, Frame.
    LoadedProfile profile;

    easy_test::capture(profile, [] {
        EASY_BLOCK("Frame");
        {
            EASY_BLOCK("Update");
            { EASY_BLOCK("Physics"); }
            { EASY_BLOCK("AI"); }
        }
        {
            EASY_BLOCK("Render");
        }
    });

    REQUIRE(profile.readerLog.empty());
    REQUIRE(renderTree(profile) == "Frame(Update(Physics,AI),Render)");
}

TEST_CASE("Several roots with their own subtrees", "[tree]")
{
    LoadedProfile profile;

    easy_test::capture(profile, [] {
        {
            EASY_BLOCK("root_one");
            { EASY_BLOCK("one_child"); }
        }
        {
            EASY_BLOCK("root_two");
            { EASY_BLOCK("two_child_a"); }
            { EASY_BLOCK("two_child_b"); }
        }
    });

    REQUIRE(profile.readerLog.empty());
    REQUIRE(renderTree(profile) == "root_one(one_child),root_two(two_child_a,two_child_b)");
}

TEST_CASE("An asymmetric tree", "[tree]")
{
    LoadedProfile profile;

    easy_test::capture(profile, [] {
        EASY_BLOCK("asym_root");
        {
            EASY_BLOCK("asym_left");
            {
                EASY_BLOCK("asym_left_deep");
                { EASY_BLOCK("asym_left_deepest"); }
            }
        }
        { EASY_BLOCK("asym_middle"); }
        {
            EASY_BLOCK("asym_right");
            { EASY_BLOCK("asym_right_child"); }
        }
    });

    REQUIRE(profile.readerLog.empty());
    REQUIRE(renderTree(profile) ==
            "asym_root(asym_left(asym_left_deep(asym_left_deepest)),asym_middle,asym_right(asym_right_child))");
}

TEST_CASE("A block in a loop produces siblings, not nesting", "[tree]")
{
    LoadedProfile profile;

    easy_test::capture(profile, [] {
        EASY_BLOCK("loop_parent");
        for (int i = 0; i < 4; ++i)
        {
            EASY_BLOCK("loop_body");
        }
    });

    REQUIRE(profile.readerLog.empty());

    const long parent = easy_test::findBlockIndex(profile, "loop_parent");
    REQUIRE(parent >= 0);

    const std::vector<std::string> children =
        easy_test::childNames(profile, static_cast<profiler::block_index_t>(parent));

    REQUIRE(children.size() == 4);
    for (size_t i = 0; i < children.size(); ++i)
        REQUIRE(children[i] == "loop_body");
}

TEST_CASE("An event inside a block becomes its child", "[tree][events]")
{
    LoadedProfile profile;

    easy_test::capture(profile, [] {
        EASY_BLOCK("event_parent");
        EASY_EVENT("event_marker");
    });

    REQUIRE(profile.readerLog.empty());
    REQUIRE(renderTree(profile) == "event_parent(event_marker)");
}

TEST_CASE("An event between blocks does not swallow its neighbours", "[tree][events]")
{
    LoadedProfile profile;

    easy_test::capture(profile, [] {
        EASY_BLOCK("marker_host");
        { EASY_BLOCK("before_marker"); }
        EASY_EVENT("standalone_marker");
        { EASY_BLOCK("after_marker"); }
    });

    REQUIRE(profile.readerLog.empty());

    // An event has zero duration and cannot adopt its neighbours.
    REQUIRE(renderTree(profile) == "marker_host(before_marker,standalone_marker,after_marker)");
}

TEST_CASE("A non-scoped block nests exactly like a scoped one", "[tree]")
{
    LoadedProfile profile;

    easy_test::capture(profile, [] {
        EASY_NONSCOPED_BLOCK("nonscoped_parent");
        { EASY_BLOCK("nonscoped_kid"); }
        EASY_END_BLOCK;
    });

    REQUIRE(profile.readerLog.empty());
    REQUIRE(renderTree(profile) == "nonscoped_parent(nonscoped_kid)");
}

TEST_CASE("A wide tree of many siblings", "[tree]")
{
    LoadedProfile profile;

    const int width = 50;
    easy_test::capture(profile, [] {
        EASY_BLOCK("wide_parent");
        for (int i = 0; i < width; ++i)
        {
            EASY_BLOCK("wide_child");
        }
    });

    REQUIRE(profile.readerLog.empty());

    const long parent = easy_test::findBlockIndex(profile, "wide_parent");
    REQUIRE(parent >= 0);
    REQUIRE(profile.blocks[static_cast<size_t>(parent)].children.size() == static_cast<size_t>(width));
}

TEST_CASE("A deep tree loses no levels", "[tree]")
{
    LoadedProfile profile;

    struct Recursive
    {
        static void descend(int depth)
        {
            if (depth == 0)
                return;
            EASY_BLOCK("deep_level");
            descend(depth - 1);
        }
    };

    const int depth = 40;
    easy_test::capture(profile, [] { Recursive::descend(depth); });

    REQUIRE(profile.readerLog.empty());

    REQUIRE(profile.threadedTrees.size() == 1);
    const profiler::BlocksTreeRoot& root = profile.threadedTrees.begin()->second;
    REQUIRE(root.children.size() == 1);

    int levels = 0;
    profiler::block_index_t current = root.children[0];
    while (true)
    {
        ++levels;
        const profiler::BlocksTree& entry = profile.blocks[current];
        if (entry.children.empty())
            break;
        REQUIRE(entry.children.size() == 1);
        current = entry.children[0];
    }

    REQUIRE(levels == depth);
}

TEST_CASE("Child intervals lie inside the parent interval", "[tree]")
{
    LoadedProfile profile;

    easy_test::capture(profile, [] {
        EASY_BLOCK("containment_parent");
        { EASY_BLOCK("containment_first"); }
        { EASY_BLOCK("containment_second"); }
    });

    REQUIRE(profile.readerLog.empty());

    const long parentIndex = easy_test::findBlockIndex(profile, "containment_parent");
    REQUIRE(parentIndex >= 0);

    const profiler::BlocksTree& parent = profile.blocks[static_cast<size_t>(parentIndex)];
    REQUIRE(parent.node != nullptr);

    profiler::timestamp_t previousEnd = 0;
    for (size_t i = 0; i < parent.children.size(); ++i)
    {
        const profiler::BlocksTree& child = profile.blocks[parent.children[i]];
        REQUIRE(child.node != nullptr);

        // A child lies wholly inside its parent — otherwise reconstruction would
        // be ambiguous.
        REQUIRE(child.node->begin() >= parent.node->begin());
        REQUIRE(child.node->end() <= parent.node->end());

        // Siblings do not overlap and advance in time.
        REQUIRE(child.node->begin() >= previousEnd);
        previousEnd = child.node->end();
    }
}

TEST_CASE("Tree structure costs nothing in the block stream", "[tree][format]")
{
    // The principle's direct consequence: four identically named blocks occupy
    // exactly the same number of bytes whether they are flat siblings or a nested
    // chain. Nesting costs zero bytes.
    //
    // FILE sizes cannot be compared: the descriptor table is process-wide and
    // never cleared, so every subsequent dump carries all descriptors registered
    // so far and the file grows monotonically from test to test. Hence the header
    // field used_memory_size (offset 40) — the size of the block stream itself.
    easy_test::TempFile flatFile("tree_flat");
    easy_test::TempFile nestedFile("tree_nested");

    easy_test::drainPending();
    profiler::setEnabled(true);
    {
        { EASY_BLOCK("shape_x"); }
        { EASY_BLOCK("shape_x"); }
        { EASY_BLOCK("shape_x"); }
        { EASY_BLOCK("shape_x"); }
    }
    profiler::setEnabled(false);
    const uint32_t flatBlocks = profiler::dumpBlocksToFile(flatFile.c_str());

    easy_test::drainPending();
    profiler::setEnabled(true);
    {
        EASY_BLOCK("shape_x");
        EASY_BLOCK("shape_x");
        EASY_BLOCK("shape_x");
        EASY_BLOCK("shape_x");
    }
    profiler::setEnabled(false);
    const uint32_t nestedBlocks = profiler::dumpBlocksToFile(nestedFile.c_str());

    REQUIRE(flatBlocks == 4);
    REQUIRE(nestedBlocks == 4);

    easy_test::ProfImage flatImage;
    easy_test::ProfImage nestedImage;
    REQUIRE(flatImage.loadFrom(flatFile.path()));
    REQUIRE(nestedImage.loadFrom(nestedFile.path()));

    REQUIRE(flatImage.totalBlocks() == 4);
    REQUIRE(nestedImage.totalBlocks() == 4);

    // There it is: an identically sized block stream for different topologies.
    REQUIRE(flatImage.usedMemorySize() == nestedImage.usedMemorySize());

    // The size equals 4 blocks of 21 bytes.
    //
    // Two quantities must not be confused here:
    //   * used_memory_size in the header counts the record payload only —
    //     ThreadStorage::storeBlock adds BASE_SIZE + nameLength, where
    //     BASE_SIZE == sizeof(BaseBlockData) + 1 == 21 (thread_storage.cpp:180);
    //   * on the wire each record takes 2 bytes more, because the length prefix
    //     is added by the allocator (chunk_allocator.h:202).
    // Hence SIZEOF_BLOCK == 23, the advertised 23 bytes per block in the file.
    REQUIRE(flatImage.usedMemorySize() == 4 * 21);
    REQUIRE(flatImage.usedMemorySize() + 4 * sizeof(uint16_t) == 4 * SIZEOF_BLOCK);
}
