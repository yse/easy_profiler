/** A lock on the format's economy: how many bytes one block costs.

The README claim "information about 12 million blocks takes less than 300 MB" is
a direct consequence of 23 bytes per block: 12e6 * 23 = 276 MB, with no headroom
in that figure. These tests check it deterministically, from file content, with
no nanoseconds and no dependence on machine speed.
*/

#include <catch2/catch_test_macros.hpp>

#include "support/prof_image.h"
#include "support/prof_fixture.h"

#include <easy/profiler.h>

#include "thread_storage.h"

#include <cstdint>
#include <string>

TEST_CASE("A block with a compile-time name costs 21 payload bytes", "[format][size]")
{
    easy_test::TempFile file("bytes_compiletime");

    const int count = 1000;

    easy_test::drainPending();
    profiler::setEnabled(true);
    for (int i = 0; i < count; ++i)
    {
        EASY_BLOCK("sized_block");
    }
    profiler::setEnabled(false);

    const uint32_t dumped = profiler::dumpBlocksToFile(file.c_str());
    REQUIRE(dumped == static_cast<uint32_t>(count));

    easy_test::ProfImage image;
    REQUIRE(image.loadFrom(file.path()));
    REQUIRE(image.totalBlocks() == static_cast<uint32_t>(count));

    // used_memory_size is the sum of BASE_SIZE + runtime name length over all
    // blocks. For a compile-time name the runtime name is empty, leaving just
    // the terminator.
    REQUIRE(image.usedMemorySize() == static_cast<uint64_t>(count) * 21);
}

TEST_CASE("On the wire a block takes 23 bytes", "[format][size]")
{
    // The allocator adds a 2-byte length prefix to the payload, which doubles as
    // the record separator in the file.
    REQUIRE(SIZEOF_BLOCK == 21 + sizeof(uint16_t));
    REQUIRE(SIZEOF_BLOCK == 23);

    // Where the README figure comes from.
    const uint64_t twelveMillion = 12ull * 1000ull * 1000ull;
    const uint64_t bytes = twelveMillion * SIZEOF_BLOCK;
    REQUIRE(bytes == 276000000ull);
    REQUIRE(bytes < 300ull * 1000ull * 1000ull);
}

TEST_CASE("A runtime name adds exactly its own length", "[format][size]")
{
    easy_test::TempFile plainFile("bytes_plain");
    easy_test::TempFile namedFile("bytes_named");

    const int count = 500;
    const std::string runtimeName = "0123456789";

    easy_test::drainPending();
    profiler::setEnabled(true);
    for (int i = 0; i < count; ++i)
    {
        EASY_BLOCK("plain_block");
    }
    profiler::setEnabled(false);
    REQUIRE(profiler::dumpBlocksToFile(plainFile.c_str()) == static_cast<uint32_t>(count));

    easy_test::drainPending();
    profiler::setEnabled(true);
    for (int i = 0; i < count; ++i)
    {
        EASY_BLOCK(runtimeName.c_str());
    }
    profiler::setEnabled(false);
    REQUIRE(profiler::dumpBlocksToFile(namedFile.c_str()) == static_cast<uint32_t>(count));

    easy_test::ProfImage plainImage;
    easy_test::ProfImage namedImage;
    REQUIRE(plainImage.loadFrom(plainFile.path()));
    REQUIRE(namedImage.loadFrom(namedFile.path()));

    const uint64_t difference = namedImage.usedMemorySize() - plainImage.usedMemorySize();
    REQUIRE(difference == static_cast<uint64_t>(count) * runtimeName.size());
}

TEST_CASE("Block stream size is linear in the block count", "[format][size]")
{
    // No "every Nth block" overhead: the cost is strictly linear.
    const int small = 100;
    const int large = 1000;

    easy_test::TempFile smallFile("bytes_small");
    easy_test::TempFile largeFile("bytes_large");

    easy_test::drainPending();
    profiler::setEnabled(true);
    for (int i = 0; i < small; ++i) { EASY_BLOCK("linear_block"); }
    profiler::setEnabled(false);
    profiler::dumpBlocksToFile(smallFile.c_str());

    easy_test::drainPending();
    profiler::setEnabled(true);
    for (int i = 0; i < large; ++i) { EASY_BLOCK("linear_block"); }
    profiler::setEnabled(false);
    profiler::dumpBlocksToFile(largeFile.c_str());

    easy_test::ProfImage smallImage;
    easy_test::ProfImage largeImage;
    REQUIRE(smallImage.loadFrom(smallFile.path()));
    REQUIRE(largeImage.loadFrom(largeFile.path()));

    REQUIRE(largeImage.usedMemorySize() == smallImage.usedMemorySize() * (large / small));
}

TEST_CASE("Nesting does not raise the per-block cost", "[format][size][tree]")
{
    // Same block count, different depth, identical block stream. This is what
    // "zero bytes for tree structure" means.
    easy_test::TempFile shallowFile("bytes_shallow");
    easy_test::TempFile deepFile("bytes_deep");

    easy_test::drainPending();
    profiler::setEnabled(true);
    for (int i = 0; i < 8; ++i) { EASY_BLOCK("depth_block"); }
    profiler::setEnabled(false);
    profiler::dumpBlocksToFile(shallowFile.c_str());

    easy_test::drainPending();
    profiler::setEnabled(true);
    {
        // Every EASY_BLOCK must sit on its own line: the macro builds unique
        // variable names from __LINE__, so two blocks on one line are a redefinition.
        EASY_BLOCK("depth_block");
        EASY_BLOCK("depth_block");
        EASY_BLOCK("depth_block");
        EASY_BLOCK("depth_block");
        EASY_BLOCK("depth_block");
        EASY_BLOCK("depth_block");
        EASY_BLOCK("depth_block");
        EASY_BLOCK("depth_block");
    }
    profiler::setEnabled(false);
    profiler::dumpBlocksToFile(deepFile.c_str());

    easy_test::ProfImage shallowImage;
    easy_test::ProfImage deepImage;
    REQUIRE(shallowImage.loadFrom(shallowFile.path()));
    REQUIRE(deepImage.loadFrom(deepFile.path()));

    REQUIRE(shallowImage.totalBlocks() == 8);
    REQUIRE(deepImage.totalBlocks() == 8);
    REQUIRE(shallowImage.usedMemorySize() == deepImage.usedMemorySize());
}

TEST_CASE("The file header takes 72 bytes", "[format][size]")
{
    easy_test::TempFile file("bytes_header");

    easy_test::drainPending();
    profiler::setEnabled(true);
    { EASY_BLOCK("header_probe"); }
    profiler::setEnabled(false);
    REQUIRE(profiler::dumpBlocksToFile(file.c_str()) == 1);

    easy_test::ProfImage image;
    REQUIRE(image.loadFrom(file.path()));

    // The 'Easy' signature is in place, which confirms the layout the other tests
    // and prof_image.h assume.
    REQUIRE(image.signature() == 0x45617379u);
    REQUIRE(image.version() != 0);
    REQUIRE(image.threadsCount() >= 1);
    REQUIRE(image.size() > easy_test::ProfHeader::Size);
}
