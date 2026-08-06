/** Helper types of the public reading API, and color construction.

Small but public things: the SerializedData container, the BlocksTree node, the
BlocksTreeRoot per-thread root and the color helpers. All of these ship in the
installed headers, so they are part of the contract with the user even though the
core itself barely calls them.
*/

#include <catch2/catch_test_macros.hpp>

#include "support/prof_fixture.h"

#include <easy/details/profiler_colors.h>
#include <easy/profiler.h>
#include <easy/reader.h>

#include <cstring>
#include <string>
#include <utility>

using easy_test::LoadedProfile;

//////////////////////////////////////////////////////////////////////////
// Colors

TEST_CASE("Building a color from components", "[colors]")
{
    using profiler::colors::color;

    REQUIRE(color(0xff, 0x00, 0x00) == 0xffff0000u);
    REQUIRE(color(0x00, 0xff, 0x00) == 0xff00ff00u);
    REQUIRE(color(0x00, 0x00, 0xff) == 0xff0000ffu);

    // Explicit alpha.
    REQUIRE(color(0x12, 0x34, 0x56, 0x78) == 0x78123456u);

    // Fully transparent.
    REQUIRE(color(0xff, 0xff, 0xff, 0x00) == 0x00ffffffu);
}

TEST_CASE("Replacing alpha with an 8-bit value", "[colors]")
{
    using profiler::colors::modify_alpha8;

    REQUIRE(modify_alpha8(0xff123456u, 0x00) == 0x00123456u);
    REQUIRE(modify_alpha8(0x00123456u, 0xff) == 0xff123456u);
    REQUIRE(modify_alpha8(0xaabbccddu, 0x11) == 0x11bbccddu);

    // The color components are left alone.
    REQUIRE((modify_alpha8(profiler::colors::Red, 0x40) & 0x00ffffffu) ==
            (profiler::colors::Red & 0x00ffffffu));
}

TEST_CASE("Replacing alpha from another color", "[colors]")
{
    using profiler::colors::modify_alpha32;

    // Only the top 8 bits of the second argument are used.
    REQUIRE(modify_alpha32(0xff123456u, 0x00ffffffu) == 0x00123456u);
    REQUIRE(modify_alpha32(0x00123456u, 0xff000000u) == 0xff123456u);

    REQUIRE(modify_alpha32(0x00aabbccu, 0x80ffffffu) ==
            modify_alpha32(0x00aabbccu, 0x80000000u));
}

TEST_CASE("Color construction agrees with alpha replacement", "[colors]")
{
    using profiler::colors::color;
    using profiler::colors::modify_alpha8;

    const profiler::color_t opaque = color(0x10, 0x20, 0x30);
    REQUIRE(modify_alpha8(opaque, 0x80) == color(0x10, 0x20, 0x30, 0x80));
}

//////////////////////////////////////////////////////////////////////////
// SerializedData

TEST_CASE("A fresh SerializedData is empty", "[reader][types]")
{
    profiler::SerializedData data;

    REQUIRE(data.empty());
    REQUIRE(data.size() == 0);
    REQUIRE(data.data() == nullptr);
}

TEST_CASE("set allocates a buffer of the requested size", "[reader][types]")
{
    profiler::SerializedData data;

    data.set(128);
    REQUIRE_FALSE(data.empty());
    REQUIRE(data.size() == 128);
    REQUIRE(data.data() != nullptr);

    // Indexing returns an offset from the start of the buffer.
    REQUIRE(data[0] == data.data());
    REQUIRE(data[64] == data.data() + 64);

    const profiler::SerializedData& constant = data;
    REQUIRE(constant.data() == data.data());
    REQUIRE(constant[16] == data.data() + 16);
}

TEST_CASE("set with size zero releases the buffer", "[reader][types]")
{
    profiler::SerializedData data;
    data.set(64);
    REQUIRE_FALSE(data.empty());

    data.set(0);
    REQUIRE(data.empty());
    REQUIRE(data.size() == 0);
    REQUIRE(data.data() == nullptr);
}

TEST_CASE("clear releases the buffer", "[reader][types]")
{
    profiler::SerializedData data;
    data.set(256);
    REQUIRE(data.size() == 256);

    data.clear();
    REQUIRE(data.empty());
    REQUIRE(data.size() == 0);
}

TEST_CASE("extend grows the buffer and keeps its content", "[reader][types]")
{
    profiler::SerializedData data;
    data.set(16);
    std::memset(data.data(), 0xAB, 16);

    data.extend(16);

    REQUIRE(data.size() == 32);
    for (int i = 0; i < 16; ++i)
        REQUIRE(static_cast<unsigned char>(data.data()[i]) == 0xAB);
}

TEST_CASE("extend works on an empty buffer", "[reader][types]")
{
    profiler::SerializedData data;
    REQUIRE(data.empty());

    data.extend(32);

    REQUIRE(data.size() == 32);
    REQUIRE(data.data() != nullptr);
}

TEST_CASE("swap exchanges the buffers", "[reader][types]")
{
    profiler::SerializedData first;
    profiler::SerializedData second;

    first.set(8);
    std::memset(first.data(), 0x11, 8);

    second.set(16);
    std::memset(second.data(), 0x22, 16);

    first.swap(second);

    REQUIRE(first.size() == 16);
    REQUIRE(second.size() == 8);
    REQUIRE(static_cast<unsigned char>(first.data()[0]) == 0x22);
    REQUIRE(static_cast<unsigned char>(second.data()[0]) == 0x11);
}

TEST_CASE("Moving transfers buffer ownership", "[reader][types]")
{
    profiler::SerializedData source;
    source.set(64);
    std::memset(source.data(), 0x33, 64);
    const char* originalBuffer = source.data();

    profiler::SerializedData moved(std::move(source));

    REQUIRE(moved.size() == 64);
    REQUIRE(moved.data() == originalBuffer);
    REQUIRE(source.empty());
    REQUIRE(source.data() == nullptr);

    profiler::SerializedData target;
    target = std::move(moved);

    REQUIRE(target.size() == 64);
    REQUIRE(target.data() == originalBuffer);
    REQUIRE(moved.empty());
}

//////////////////////////////////////////////////////////////////////////
// BlocksTree and BlocksTreeRoot

TEST_CASE("A fresh BlocksTree is empty", "[reader][types]")
{
    profiler::BlocksTree tree;

    REQUIRE(tree.node == nullptr);
    REQUIRE(tree.children.empty());
    REQUIRE(tree.depth == 0);
    REQUIRE(tree.per_parent_stats == nullptr);
    REQUIRE(tree.per_frame_stats == nullptr);
    REQUIRE(tree.per_thread_stats == nullptr);
}

TEST_CASE("Moving a BlocksTree carries its children", "[reader][types]")
{
    profiler::BlocksTree source;
    source.children.push_back(1);
    source.children.push_back(2);
    source.depth = 3;

    profiler::BlocksTree moved(std::move(source));

    REQUIRE(moved.children.size() == 2);
    REQUIRE(moved.children[0] == 1);
    REQUIRE(moved.children[1] == 2);
    REQUIRE(moved.depth == 3);

    profiler::BlocksTree target;
    target = std::move(moved);

    REQUIRE(target.children.size() == 2);
    REQUIRE(target.depth == 3);
}

TEST_CASE("BlocksTree comparison rests on the start time", "[reader][types]")
{
    // Nodes without data are incomparable: the operator returns false both ways.
    profiler::BlocksTree empty;
    profiler::BlocksTree other;

    REQUIRE_FALSE(empty < other);
    REQUIRE_FALSE(other < empty);
}

TEST_CASE("Comparing nodes of a real trace", "[reader][types]")
{
    LoadedProfile profile;
    easy_test::capture(profile, [] {
        { EASY_BLOCK("cmp_first"); }
        { EASY_BLOCK("cmp_second"); }
    });

    REQUIRE(profile.readerLog.empty());

    const long first = easy_test::findBlockIndex(profile, "cmp_first");
    const long second = easy_test::findBlockIndex(profile, "cmp_second");
    REQUIRE(first >= 0);
    REQUIRE(second >= 0);

    const profiler::BlocksTree& a = profile.blocks[static_cast<size_t>(first)];
    const profiler::BlocksTree& b = profile.blocks[static_cast<size_t>(second)];

    REQUIRE(a < b);
    REQUIRE_FALSE(b < a);

    // shrink_to_fit does not change the content.
    const size_t childrenBefore = a.children.size();
    const_cast<profiler::BlocksTree&>(a).shrink_to_fit();
    REQUIRE(a.children.size() == childrenBefore);
}

TEST_CASE("A fresh BlocksTreeRoot is empty", "[reader][types]")
{
    profiler::BlocksTreeRoot root;

    REQUIRE(root.children.empty());
    REQUIRE(root.sync.empty());
    REQUIRE(root.events.empty());
    REQUIRE(root.thread_id == 0);
    REQUIRE(root.profiled_time == 0);
    REQUIRE(root.wait_time == 0);
    REQUIRE(root.frames_number == 0);
    REQUIRE(root.blocks_number == 0);
    REQUIRE(root.depth == 0);
    REQUIRE_FALSE(root.got_name());
}

TEST_CASE("The thread name in BlocksTreeRoot", "[reader][types]")
{
    profiler::BlocksTreeRoot root;
    REQUIRE_FALSE(root.got_name());

    root.thread_name = "worker";
    REQUIRE(root.got_name());
    REQUIRE(std::string(root.name()) == "worker");
}

TEST_CASE("Roots compare by thread id", "[reader][types]")
{
    profiler::BlocksTreeRoot first;
    profiler::BlocksTreeRoot second;

    first.thread_id = 10;
    second.thread_id = 20;

    REQUIRE(first < second);
    REQUIRE_FALSE(second < first);
}

TEST_CASE("Moving a BlocksTreeRoot carries its state", "[reader][types]")
{
    profiler::BlocksTreeRoot source;
    source.thread_id = 42;
    source.thread_name = "movable_thread";
    source.children.push_back(7);
    source.sync.push_back(8);
    source.blocks_number = 3;
    source.depth = 2;

    profiler::BlocksTreeRoot moved(std::move(source));

    REQUIRE(moved.thread_id == 42);
    REQUIRE(std::string(moved.name()) == "movable_thread");
    REQUIRE(moved.children.size() == 1);
    REQUIRE(moved.sync.size() == 1);
    REQUIRE(moved.blocks_number == 3);
    REQUIRE(moved.depth == 2);

    profiler::BlocksTreeRoot target;
    target = std::move(moved);

    REQUIRE(target.thread_id == 42);
    REQUIRE(std::string(target.name()) == "movable_thread");
    REQUIRE(target.children.size() == 1);
}

TEST_CASE("BlockStatistics computes an average", "[reader][types]")
{
    profiler::BlockStatistics stats(1000, 5, 3);

    REQUIRE(stats.total_duration == 1000);
    REQUIRE(stats.calls_number == 1);
    REQUIRE(stats.min_duration_block == 5);
    REQUIRE(stats.max_duration_block == 5);
    REQUIRE(stats.parent_block == 3);
    REQUIRE(stats.average_duration() == 1000);

    stats.total_duration += 3000;
    stats.calls_number += 3;
    REQUIRE(stats.average_duration() == 1000);
}

TEST_CASE("A bookmark holds a position, a color and text", "[reader][types]")
{
    profiler::Bookmark bookmark;
    bookmark.pos = 123456;
    bookmark.color = profiler::colors::Red;
    bookmark.text = "slowdown here";

    REQUIRE(bookmark.pos == 123456);
    REQUIRE(bookmark.color == profiler::colors::Red);
    REQUIRE(bookmark.text == "slowdown here");

    // Base size of a bookmark record in the file.
    REQUIRE(profiler::Bookmark::BaseSize ==
            sizeof(profiler::timestamp_t) + sizeof(profiler::color_t) + 1);
}
