/** chunk_allocator — the core storage mechanism.

The allocator implements "the in-memory representation is the on-disk
representation": a block is constructed in its final form inside a chunk, and a
dump is a byte-for-byte write of the chunks into a stream. Hence the 2-byte
length prefix, which doubles as the record separator in the file and as the
allocator's own bookkeeping; a zero prefix marks the end of data in a chunk.

Record order after serialize() must match insertion order: chunks form a stack
(each one remembers the previous), and serialize() unwinds the list via invert().
A mistake here would scramble block order in every trace.
*/

#include <catch2/catch_test_macros.hpp>

#include "chunk_allocator.h"

#include <cstdint>
#include <cstring>
#include <sstream>
#include <string>
#include <vector>

namespace {

/** Parse a stream produced by chunk_allocator::serialize().

The stream is a concatenation of [uint16 length][payload] records. A zero length
never appears: serialize() stops walking a chunk as soon as it hits one.
*/
std::vector<std::string> parseRecords(const std::string& data)
{
    std::vector<std::string> records;
    size_t pos = 0;

    while (pos + sizeof(uint16_t) <= data.size())
    {
        uint16_t length = 0;
        std::memcpy(&length, data.data() + pos, sizeof(uint16_t));
        if (length == 0)
            break;

        pos += sizeof(uint16_t);
        if (pos + length > data.size())
            break;

        records.push_back(std::string(data.data() + pos, length));
        pos += length;
    }

    return records;
}

void fillRecord(void* destination, const std::string& payload)
{
    std::memcpy(destination, payload.data(), payload.size());
}

/** The length prefix sits immediately before the payload. */
uint16_t prefixOf(void* payload)
{
    uint16_t length = 0;
    std::memcpy(&length, static_cast<char*>(payload) - sizeof(uint16_t), sizeof(uint16_t));
    return length;
}

} // namespace

TEST_CASE("A fresh allocator is empty", "[allocator]")
{
    chunk_allocator<1024> allocator;

    REQUIRE(allocator.size() == 0);
    REQUIRE(allocator.empty());
    REQUIRE(allocator.markedSize() == 0);
    REQUIRE(allocator.markedEmpty());
}

TEST_CASE("allocate returns usable memory and counts calls", "[allocator]")
{
    chunk_allocator<1024> allocator;

    void* first = allocator.allocate(8);
    REQUIRE(first != nullptr);
    REQUIRE(allocator.size() == 1);
    REQUIRE_FALSE(allocator.empty());

    void* second = allocator.allocate(8);
    REQUIRE(second != nullptr);
    REQUIRE(second != first);
    REQUIRE(allocator.size() == 2);

    // Records are back-to-back with exactly the length prefix in between.
    const ptrdiff_t distance = static_cast<char*>(second) - static_cast<char*>(first);
    REQUIRE(distance == static_cast<ptrdiff_t>(8 + sizeof(uint16_t)));
}

TEST_CASE("The length prefix is written before the payload", "[allocator]")
{
    chunk_allocator<1024> allocator;

    void* small = allocator.allocate(3);
    REQUIRE(prefixOf(small) == 3);

    void* large = allocator.allocate(97);
    REQUIRE(prefixOf(large) == 97);
}

TEST_CASE("A zero prefix is written past the payload as the chunk end marker", "[allocator]")
{
    chunk_allocator<1024> allocator;

    void* payload = allocator.allocate(16);
    std::memset(payload, 0xAB, 16);

    // Without those two zeroed bytes serialize() would not know where the
    // chunk's data ends.
    uint16_t terminator = 0xFFFF;
    std::memcpy(&terminator, static_cast<char*>(payload) + 16, sizeof(uint16_t));
    REQUIRE(terminator == 0);
}

TEST_CASE("need_expand fires at the chunk boundary", "[allocator]")
{
    chunk_allocator<64> allocator;

    REQUIRE_FALSE(allocator.need_expand(8));

    // Take 60 of 64 bytes: 6 records of 10 bytes each (8 payload + 2 prefix).
    for (int i = 0; i < 6; ++i)
        allocator.allocate(8);

    // 4 bytes left. An 8-byte record needs 10 and does not fit.
    REQUIRE(allocator.need_expand(8));
    // A 2-byte record needs 4 and fits exactly.
    REQUIRE_FALSE(allocator.need_expand(2));
}

TEST_CASE("serialize preserves content and insertion order", "[allocator][format]")
{
    chunk_allocator<1024> allocator;

    const std::vector<std::string> written = {
        "first", "second", "third", "fourth"
    };

    for (const std::string& payload : written)
    {
        void* memory = allocator.allocate(static_cast<uint16_t>(payload.size()));
        fillRecord(memory, payload);
    }

    REQUIRE(allocator.size() == written.size());

    std::ostringstream stream;
    allocator.serialize(stream);

    const std::vector<std::string> readBack = parseRecords(stream.str());
    REQUIRE(readBack == written);
}

TEST_CASE("serialize preserves order across many chunks", "[allocator][format]")
{
    // A tiny chunk forces constant expansion. This is where invert() matters:
    // chunks are stored in reverse.
    chunk_allocator<64> allocator;

    std::vector<std::string> written;
    for (int i = 0; i < 200; ++i)
    {
        std::ostringstream name;
        name << "record-" << i;
        written.push_back(name.str());
    }

    for (const std::string& payload : written)
    {
        void* memory = allocator.allocate(static_cast<uint16_t>(payload.size()));
        fillRecord(memory, payload);
    }

    REQUIRE(allocator.size() == written.size());

    std::ostringstream stream;
    allocator.serialize(stream);

    const std::vector<std::string> readBack = parseRecords(stream.str());
    REQUIRE(readBack.size() == written.size());
    REQUIRE(readBack == written);
}

TEST_CASE("serialize clears the storage", "[allocator]")
{
    chunk_allocator<256> allocator;

    for (int i = 0; i < 5; ++i)
    {
        void* memory = allocator.allocate(4);
        std::memset(memory, i, 4);
    }
    REQUIRE(allocator.size() == 5);

    std::ostringstream stream;
    allocator.serialize(stream);

    REQUIRE(allocator.size() == 0);
    REQUIRE(allocator.empty());

    std::ostringstream second;
    allocator.serialize(second);
    REQUIRE(parseRecords(second.str()).empty());
}

TEST_CASE("A record that exactly fills a chunk", "[allocator]")
{
    chunk_allocator<64> allocator;

    // 62 bytes of payload + 2 bytes of prefix == exactly 64.
    void* memory = allocator.allocate(62);
    REQUIRE(memory != nullptr);
    std::memset(memory, 0x5A, 62);

    REQUIRE(allocator.size() == 1);

    REQUIRE(allocator.need_expand(1));
    void* next = allocator.allocate(4);
    REQUIRE(next != nullptr);
    std::memset(next, 0x77, 4);

    std::ostringstream stream;
    allocator.serialize(stream);

    const std::vector<std::string> records = parseRecords(stream.str());
    REQUIRE(records.size() == 2);
    REQUIRE(records[0] == std::string(62, static_cast<char>(0x5A)));
    REQUIRE(records[1] == std::string(4, 0x77));
}

TEST_CASE("clear resets the counters", "[allocator]")
{
    chunk_allocator<64> allocator;

    for (int i = 0; i < 50; ++i)
        allocator.allocate(8);

    REQUIRE(allocator.size() == 50);

    allocator.clear();

    REQUIRE(allocator.size() == 0);
    REQUIRE(allocator.empty());
    REQUIRE(allocator.markedSize() == 0);

    void* memory = allocator.allocate(8);
    REQUIRE(memory != nullptr);
    REQUIRE(allocator.size() == 1);
}

TEST_CASE("put_mark records a position", "[allocator][mark]")
{
    chunk_allocator<1024> allocator;

    for (int i = 0; i < 3; ++i)
        allocator.allocate(8);

    REQUIRE(allocator.markedSize() == 0);
    REQUIRE(allocator.markedEmpty());

    allocator.put_mark();

    REQUIRE(allocator.markedSize() == 3);
    REQUIRE_FALSE(allocator.markedEmpty());

    // Plain allocations past the mark do not move markedSize.
    allocator.allocate(8);
    REQUIRE(allocator.size() == 4);
    REQUIRE(allocator.markedSize() == 3);
}

TEST_CASE("marked_allocate keeps an independent count", "[allocator][mark]")
{
    chunk_allocator<1024> allocator;

    SECTION("with no preceding mark it behaves like allocate and sets the mark")
    {
        void* memory = allocator.marked_allocate(8);
        REQUIRE(memory != nullptr);
        REQUIRE(allocator.size() == 1);
        REQUIRE(allocator.markedSize() == 1);
    }

    SECTION("after a mark it continues from the marked position")
    {
        allocator.allocate(8);
        allocator.allocate(8);
        allocator.put_mark();
        REQUIRE(allocator.markedSize() == 2);

        void* memory = allocator.marked_allocate(8);
        REQUIRE(memory != nullptr);
        REQUIRE(allocator.markedSize() == 3);
    }
}

TEST_CASE("serialize after put_mark yields the data up to the mark", "[allocator][mark][format]")
{
    chunk_allocator<64> allocator;

    // serialize() walks the chunks, stops at the marked one and uses
    // m_markedChunkOffset as the boundary.
    for (int i = 0; i < 3; ++i)
    {
        void* memory = allocator.allocate(4);
        std::memset(memory, 'a' + i, 4);
    }

    allocator.put_mark();
    REQUIRE(allocator.markedSize() == 3);

    std::ostringstream stream;
    allocator.serialize(stream);

    const std::vector<std::string> records = parseRecords(stream.str());
    REQUIRE(records.size() == 3);
    REQUIRE(records[0] == "aaaa");
    REQUIRE(records[1] == "bbbb");
    REQUIRE(records[2] == "cccc");
}

TEST_CASE("Large records close to the chunk limit", "[allocator]")
{
    chunk_allocator<4096> allocator;

    std::vector<std::string> written;
    for (int i = 0; i < 20; ++i)
        written.push_back(std::string(2000, static_cast<char>('A' + i)));

    for (const std::string& payload : written)
    {
        void* memory = allocator.allocate(static_cast<uint16_t>(payload.size()));
        fillRecord(memory, payload);
    }

    std::ostringstream stream;
    allocator.serialize(stream);

    const std::vector<std::string> readBack = parseRecords(stream.str());
    REQUIRE(readBack == written);
}

TEST_CASE("Interleaved records of varying length", "[allocator][format]")
{
    chunk_allocator<128> allocator;

    std::vector<std::string> written;
    for (int i = 1; i <= 60; ++i)
        written.push_back(std::string(static_cast<size_t>(i % 40) + 1, static_cast<char>('0' + (i % 10))));

    for (const std::string& payload : written)
    {
        void* memory = allocator.allocate(static_cast<uint16_t>(payload.size()));
        fillRecord(memory, payload);
    }

    std::ostringstream stream;
    allocator.serialize(stream);

    REQUIRE(parseRecords(stream.str()) == written);
}
