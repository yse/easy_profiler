/** Locks on the layout of the format's structures.

A compact .prof is a stated competitive property of the project: 23 bytes per
block, of which the tree structure costs nothing. Those numbers rest entirely on
packed structures (#pragma pack(1)) and on the variable-length part sitting
inline right behind the fixed one.

Any change to these sizes changes the file format and breaks read compatibility.
These tests exist so such a change cannot be made by accident.
*/

#include <catch2/catch_test_macros.hpp>

#include <easy/details/profiler_public_types.h>
#include <easy/serialized_block.h>

#include "thread_storage.h"

#include <cstddef>

TEST_CASE("Fixed sizes of the format structures", "[layout][format]")
{
    SECTION("Event — two timestamps, no padding holes")
    {
        REQUIRE(sizeof(profiler::Event) == 16);
        REQUIRE(sizeof(profiler::timestamp_t) == 8);
    }

    SECTION("BaseBlockData — Event plus the descriptor id")
    {
        // 8 (begin) + 8 (end) + 4 (id) = 20, not padded up to 24.
        REQUIRE(sizeof(profiler::BaseBlockData) == 20);
        REQUIRE(sizeof(profiler::BaseBlockData) == sizeof(profiler::Event) + sizeof(profiler::block_id_t));
    }

    SECTION("BaseBlockDescriptor")
    {
        // id(4) + line(4) + color(4) + type(1) + status(1) = 14, not padded up to 16.
        REQUIRE(sizeof(profiler::BaseBlockDescriptor) == 14);
    }

    SECTION("SerializedBlockDescriptor — descriptor plus the name length")
    {
        REQUIRE(sizeof(profiler::SerializedBlockDescriptor) ==
                sizeof(profiler::BaseBlockDescriptor) + sizeof(uint16_t));
        REQUIRE(sizeof(profiler::SerializedBlockDescriptor) == 16);
    }

    SECTION("CSwitchEvent — Event plus the thread id")
    {
        REQUIRE(sizeof(profiler::CSwitchEvent) == 24);
        REQUIRE(sizeof(profiler::thread_id_t) == 8);
    }

    SECTION("ArbitraryValue")
    {
        REQUIRE(sizeof(profiler::ArbitraryValue) == 34);
    }
}

TEST_CASE("A block costs 23 bytes on the wire", "[layout][format]")
{
    // thread_storage.h:97 defines SIZEOF_BLOCK as
    //     sizeof(BaseBlockData) + 1 (empty-name terminator) + sizeof(uint16_t) (length prefix)
    // This constant is where the README claim "12M blocks < 300 MB" comes from:
    // 12e6 * 23 = 276 MB.
    REQUIRE(SIZEOF_BLOCK == 23);
    REQUIRE(SIZEOF_BLOCK == sizeof(profiler::BaseBlockData) + 1U + sizeof(uint16_t));

    REQUIRE(SIZEOF_CSWITCH == sizeof(profiler::CSwitchEvent) + 1U + sizeof(uint16_t));
    REQUIRE(SIZEOF_CSWITCH == 27);
}

TEST_CASE("The chunk size fits into 16-bit offsets", "[layout][allocator]")
{
    // The allocator addresses a position inside a chunk with a uint16_t
    // (m_chunkOffset), so the chunk size must stay strictly below 65536.
    REQUIRE(static_cast<int>(SIZEOF_BLOCK) * static_cast<int>(BLOCKS_IN_CHUNK) < 65536);
    REQUIRE(static_cast<int>(SIZEOF_CSWITCH) * static_cast<int>(BLOCKS_IN_CHUNK) < 65536);

    REQUIRE(BLOCKS_IN_CHUNK == 128U);
    REQUIRE(BLOCK_CHUNK_SIZE >= SIZEOF_BLOCK * BLOCKS_IN_CHUNK);
    REQUIRE(CSWITCH_CHUNK_SIZE >= SIZEOF_CSWITCH * BLOCKS_IN_CHUNK);
    REQUIRE(CSWITCH_CHUNK_SIZE < 65536U);
}

TEST_CASE("The variable part follows the fixed part immediately", "[layout][format]")
{
    // SerializedBlock::name() returns data() + sizeof(BaseBlockData): no pointer,
    // no offset table — the name is inline. Checked through the layout rather
    // than by constructing an object, whose constructor is private.
    REQUIRE(sizeof(profiler::SerializedBlock) == sizeof(profiler::BaseBlockData));

    // In a descriptor the name and the file path are adjacent, separated only by
    // m_nameLength.
    REQUIRE(sizeof(profiler::SerializedBlockDescriptor) - sizeof(profiler::BaseBlockDescriptor)
            == sizeof(uint16_t));
}

TEST_CASE("Upper bound on block data size", "[layout]")
{
    REQUIRE(profiler::MAX_BLOCK_DATA_SIZE == 2048 + 512 + 256);
    REQUIRE(profiler::MAX_BLOCK_DATA_SIZE == 2816);
}
