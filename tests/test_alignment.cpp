/** alignment_helpers.h — unaligned memory access.

The allocator writes a 2-byte length prefix before every record, so payloads are
almost never aligned. The whole format rests on unaligned loads and stores being
correct, which is why every function is exercised at each byte offset rather than
at offset zero only.
*/

#include <catch2/catch_test_macros.hpp>

#include "alignment_helpers.h"

#include <cstdint>
#include <cstring>
#include <vector>

namespace {

struct ScratchBuffer
{
    alignas(16) unsigned char bytes[64];

    ScratchBuffer() { std::memset(bytes, 0xCD, sizeof(bytes)); }

    void* at(size_t offset) { return bytes + offset; }
};

} // namespace

TEST_CASE("is_aligned detects alignment", "[alignment]")
{
    ScratchBuffer buffer;

    REQUIRE(is_aligned<2>(buffer.at(0)));
    REQUIRE(is_aligned<4>(buffer.at(0)));
    REQUIRE(is_aligned<8>(buffer.at(0)));

    REQUIRE_FALSE(is_aligned<2>(buffer.at(1)));
    REQUIRE_FALSE(is_aligned<4>(buffer.at(2)));
    REQUIRE(is_aligned<2>(buffer.at(2)));
    REQUIRE_FALSE(is_aligned<8>(buffer.at(4)));
    REQUIRE(is_aligned<4>(buffer.at(4)));
}

TEST_CASE("unaligned_store16/load16 at every offset", "[alignment]")
{
    for (size_t offset = 0; offset < 16; ++offset)
    {
        ScratchBuffer buffer;
        const uint16_t written = static_cast<uint16_t>(0xBEEF);

        unaligned_store16(buffer.at(offset), written);

        uint16_t viaReturn = unaligned_load16<uint16_t>(buffer.at(offset));
        REQUIRE(viaReturn == written);

        uint16_t viaOutParam = 0;
        unaligned_load16(buffer.at(offset), &viaOutParam);
        REQUIRE(viaOutParam == written);

        if (offset > 0)
            REQUIRE(buffer.bytes[offset - 1] == 0xCD);
        REQUIRE(buffer.bytes[offset + 2] == 0xCD);
    }
}

TEST_CASE("unaligned_store32/load32 at every offset", "[alignment]")
{
    for (size_t offset = 0; offset < 16; ++offset)
    {
        ScratchBuffer buffer;
        const uint32_t written = 0xDEADBEEFu;

        unaligned_store32(buffer.at(offset), written);
        REQUIRE(unaligned_load32<uint32_t>(buffer.at(offset)) == written);

        uint32_t viaOutParam = 0;
        unaligned_load32(buffer.at(offset), &viaOutParam);
        REQUIRE(viaOutParam == written);

        if (offset > 0)
            REQUIRE(buffer.bytes[offset - 1] == 0xCD);
        REQUIRE(buffer.bytes[offset + 4] == 0xCD);
    }
}

TEST_CASE("unaligned_store64/load64 at every offset", "[alignment]")
{
    for (size_t offset = 0; offset < 16; ++offset)
    {
        ScratchBuffer buffer;
        const uint64_t written = 0x0123456789ABCDEFull;

        unaligned_store64(buffer.at(offset), written);
        REQUIRE(unaligned_load64<uint64_t>(buffer.at(offset)) == written);

        uint64_t viaOutParam = 0;
        unaligned_load64(buffer.at(offset), &viaOutParam);
        REQUIRE(viaOutParam == written);

        if (offset > 0)
            REQUIRE(buffer.bytes[offset - 1] == 0xCD);
        REQUIRE(buffer.bytes[offset + 8] == 0xCD);
    }
}

TEST_CASE("unaligned_zero clears exactly its own width", "[alignment]")
{
    SECTION("zero16")
    {
        for (size_t offset = 0; offset < 16; ++offset)
        {
            ScratchBuffer buffer;
            unaligned_zero16(buffer.at(offset));
            REQUIRE(buffer.bytes[offset] == 0);
            REQUIRE(buffer.bytes[offset + 1] == 0);
            REQUIRE(buffer.bytes[offset + 2] == 0xCD);
        }
    }

    SECTION("zero32")
    {
        for (size_t offset = 0; offset < 16; ++offset)
        {
            ScratchBuffer buffer;
            unaligned_zero32(buffer.at(offset));
            for (size_t i = 0; i < 4; ++i)
                REQUIRE(buffer.bytes[offset + i] == 0);
            REQUIRE(buffer.bytes[offset + 4] == 0xCD);
        }
    }

    SECTION("zero64")
    {
        for (size_t offset = 0; offset < 16; ++offset)
        {
            ScratchBuffer buffer;
            unaligned_zero64(buffer.at(offset));
            for (size_t i = 0; i < 8; ++i)
                REQUIRE(buffer.bytes[offset + i] == 0);
            REQUIRE(buffer.bytes[offset + 8] == 0xCD);
        }
    }
}

TEST_CASE("Signed types survive a round-trip", "[alignment]")
{
    ScratchBuffer buffer;

    const int16_t negative16 = -12345;
    unaligned_store16(buffer.at(3), negative16);
    REQUIRE(unaligned_load16<int16_t>(buffer.at(3)) == negative16);

    const int32_t negative32 = -1234567890;
    unaligned_store32(buffer.at(5), negative32);
    REQUIRE(unaligned_load32<int32_t>(buffer.at(5)) == negative32);

    const int64_t negative64 = -1234567890123456789ll;
    unaligned_store64(buffer.at(7), negative64);
    REQUIRE(unaligned_load64<int64_t>(buffer.at(7)) == negative64);
}

TEST_CASE("Adjacent records do not overlap", "[alignment]")
{
    // The layout chunk_allocator produces: back-to-back pairs of
    // 16-bit length prefix followed by payload.
    ScratchBuffer buffer;

    unaligned_store16(buffer.at(0), static_cast<uint16_t>(4));
    unaligned_store32(buffer.at(2), 0x11223344u);
    unaligned_store16(buffer.at(6), static_cast<uint16_t>(8));
    unaligned_store64(buffer.at(8), 0xAABBCCDDEEFF0011ull);

    REQUIRE(unaligned_load16<uint16_t>(buffer.at(0)) == 4);
    REQUIRE(unaligned_load32<uint32_t>(buffer.at(2)) == 0x11223344u);
    REQUIRE(unaligned_load16<uint16_t>(buffer.at(6)) == 8);
    REQUIRE(unaligned_load64<uint64_t>(buffer.at(8)) == 0xAABBCCDDEEFF0011ull);
}
