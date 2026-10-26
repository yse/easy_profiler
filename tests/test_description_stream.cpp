/** Direct tests for the standalone descriptor-stream reader.

The network protocol sends descriptors separately from captured blocks.  A
valid stream is assembled from a real profile so the test follows the same
public data model as the GUI without duplicating descriptor construction.
*/

#include <catch2/catch_test_macros.hpp>

#include "support/prof_fixture.h"

#include <easy/arbitrary_value.h>
#include <easy/profiler.h>
#include <easy/reader.h>

#include <atomic>
#include <cstdint>
#include <cstring>
#include <sstream>
#include <string>

namespace {

constexpr size_t DescriptionHeaderSize = sizeof(uint32_t) * 3U + sizeof(uint64_t);
constexpr size_t FirstDescriptorSizeOffset = DescriptionHeaderSize;
constexpr size_t FirstDescriptorDataOffset = DescriptionHeaderSize + sizeof(uint16_t);

template <class T>
void append(std::string& output, const T& value)
{
    output.append(reinterpret_cast<const char*>(&value), sizeof(value));
}

std::string makeDescriptionStream(const easy_test::LoadedProfile& profile)
{
    std::string output;
    const uint32_t signature = 0x45617379u;
    const uint32_t version = profiler::formatVersion();
    const uint32_t count = profile.descriptorsCount;

    uint64_t memorySize = 0;
    for (uint32_t i = 0; i < count; ++i)
    {
        const profiler::SerializedBlockDescriptor* descriptor = profile.descriptors[i];
        REQUIRE(descriptor != nullptr);
        memorySize += sizeof(profiler::SerializedBlockDescriptor) +
                      std::strlen(descriptor->name()) +
                      std::strlen(descriptor->file()) + 2U;
    }

    append(output, signature);
    append(output, version);
    append(output, count);
    append(output, memorySize);

    for (uint32_t i = 0; i < count; ++i)
    {
        const profiler::SerializedBlockDescriptor* descriptor = profile.descriptors[i];
        const uint16_t size = static_cast<uint16_t>(
            sizeof(profiler::SerializedBlockDescriptor) +
            std::strlen(descriptor->name()) +
            std::strlen(descriptor->file()) + 2U);
        append(output, size);
        output.append(descriptor->data(), size);
    }

    return output;
}

void captureDescriptions(easy_test::LoadedProfile& profile)
{
    easy_test::capture(profile, [] {
        EASY_BLOCK("description_stream_block");
        EASY_VALUE("description_stream_value", static_cast<int32_t>(17));
    });
}

bool readDescriptionStream(const std::string& bytes,
                           std::atomic<int>& progress,
                           profiler::SerializedData& storage,
                           profiler::descriptors_list_t& descriptors,
                           std::string& error)
{
    std::istringstream input(bytes, std::ios::in | std::ios::binary);
    std::ostringstream log;
    const bool result = readDescriptionsFromStream(progress, input, storage, descriptors, log);
    error = log.str();
    return result;
}

template <class T>
void overwrite(std::string& bytes, size_t offset, T value)
{
    REQUIRE(offset + sizeof(value) <= bytes.size());
    std::memcpy(&bytes[offset], &value, sizeof(value));
}

void requireInvalidDescriptor(std::string bytes)
{
    profiler::SerializedData storage;
    profiler::descriptors_list_t descriptors;
    std::atomic<int> progress(0);
    std::string error;

    REQUIRE_FALSE(readDescriptionStream(bytes, progress, storage, descriptors, error));
    REQUIRE(error.find("Invalid") != std::string::npos);
}

} // namespace

TEST_CASE("A standalone description stream reads back", "[reader][descriptions][format]")
{
    easy_test::LoadedProfile source;
    captureDescriptions(source);
    REQUIRE(source.readerLog.empty());

    profiler::SerializedData storage;
    profiler::descriptors_list_t descriptors;
    std::atomic<int> progress(0);
    std::string error;

    REQUIRE(readDescriptionStream(makeDescriptionStream(source), progress,
                                  storage, descriptors, error));
    REQUIRE(error.empty());
    REQUIRE(progress.load() == 100);
    REQUIRE(descriptors.size() == source.descriptorsCount);
    REQUIRE(storage.size() == source.serializedDescriptors.size());
    REQUIRE(std::strcmp(descriptors.back()->name(), source.descriptors.back()->name()) == 0);
}

TEST_CASE("A description stream whose size becomes unknown after the header is rejected",
          "[reader][descriptions][robustness]")
{
    // Seekable while the reader sizes the stream, unseekable from then on. A
    // non-seekable stream is buffered up front, so only this shape reaches the
    // branch, and the declared size must then not be trusted for allocation.
    class SeekableOnceBuffer : public std::stringbuf
    {
    public:
        explicit SeekableOnceBuffer(const std::string& bytes)
            : std::stringbuf(bytes, std::ios::in | std::ios::binary), m_endSeeks(0) {}

    protected:
        pos_type seekoff(off_type offset, std::ios_base::seekdir direction,
                         std::ios_base::openmode mode) override
        {
            if (direction == std::ios_base::end && ++m_endSeeks > 1)
                return pos_type(off_type(-1));
            return std::stringbuf::seekoff(offset, direction, mode);
        }

    private:
        int m_endSeeks;
    };

    easy_test::LoadedProfile source;
    captureDescriptions(source);
    std::string bytes = makeDescriptionStream(source);
    overwrite<uint64_t>(bytes, sizeof(uint32_t) * 3U, static_cast<uint64_t>(1) << 40);

    SeekableOnceBuffer buffer(bytes);
    std::istream input(&buffer);
    profiler::SerializedData storage;
    profiler::descriptors_list_t descriptors;
    std::atomic<int> progress(0);
    std::ostringstream log;

    REQUIRE_FALSE(readDescriptionsFromStream(progress, input, storage, descriptors, log));
    REQUIRE(storage.size() == 0);
    REQUIRE(log.str().find("size is unavailable") != std::string::npos);
}

TEST_CASE("A truncated standalone description stream is rejected", "[reader][descriptions][robustness]")
{
    easy_test::LoadedProfile source;
    captureDescriptions(source);
    std::string bytes = makeDescriptionStream(source);
    REQUIRE(bytes.size() > 4);
    bytes.resize(bytes.size() - 4);

    profiler::SerializedData storage;
    profiler::descriptors_list_t descriptors;
    std::atomic<int> progress(0);
    std::string error;

    REQUIRE_FALSE(readDescriptionStream(bytes, progress, storage, descriptors, error));
    REQUIRE_FALSE(error.empty());
}

TEST_CASE("Malformed standalone description headers are rejected", "[reader][descriptions][robustness]")
{
    easy_test::LoadedProfile source;
    captureDescriptions(source);
    std::string bytes = makeDescriptionStream(source);

    SECTION("wrong signature")
    {
        overwrite<uint32_t>(bytes, 0, 0xdeadbeefU);
    }
    SECTION("incompatible version")
    {
        overwrite<uint32_t>(bytes, sizeof(uint32_t), 0U);
    }
    SECTION("zero descriptor count")
    {
        overwrite<uint32_t>(bytes, sizeof(uint32_t) * 2U, 0U);
    }
    SECTION("zero descriptor memory")
    {
        overwrite<uint64_t>(bytes, sizeof(uint32_t) * 3U, 0U);
    }
    SECTION("descriptor count exceeds available records")
    {
        overwrite<uint32_t>(bytes, sizeof(uint32_t) * 2U,
                            std::numeric_limits<uint32_t>::max());
    }
    SECTION("descriptor memory exceeds available bytes")
    {
        overwrite<uint64_t>(bytes, sizeof(uint32_t) * 3U,
                            std::numeric_limits<uint64_t>::max());
    }

    profiler::SerializedData storage;
    profiler::descriptors_list_t descriptors;
    std::atomic<int> progress(0);
    std::string error;

    REQUIRE_FALSE(readDescriptionStream(bytes, progress, storage, descriptors, error));
    REQUIRE_FALSE(error.empty());
}

TEST_CASE("A mismatched standalone descriptor id is rejected", "[reader][descriptions][robustness]")
{
    easy_test::LoadedProfile source;
    captureDescriptions(source);
    std::string bytes = makeDescriptionStream(source);

    const size_t firstDescriptor = FirstDescriptorDataOffset;
    REQUIRE(bytes.size() >= firstDescriptor + sizeof(profiler::block_id_t));
    const profiler::block_id_t wrongId = 1;
    std::memcpy(&bytes[firstDescriptor], &wrongId, sizeof(wrongId));

    profiler::SerializedData storage;
    profiler::descriptors_list_t descriptors;
    std::atomic<int> progress(0);
    std::string error;

    REQUIRE_FALSE(readDescriptionStream(bytes, progress, storage, descriptors, error));
    REQUIRE(error.find("id") != std::string::npos);
}

TEST_CASE("An oversized standalone descriptor record is rejected", "[reader][descriptions][robustness]")
{
    easy_test::LoadedProfile source;
    captureDescriptions(source);
    std::string bytes = makeDescriptionStream(source);

    const uint16_t oversized = 0xffffU;
    std::memcpy(&bytes[FirstDescriptorSizeOffset], &oversized, sizeof(oversized));

    profiler::SerializedData storage;
    profiler::descriptors_list_t descriptors;
    std::atomic<int> progress(0);
    std::string error;

    REQUIRE_FALSE(readDescriptionStream(bytes, progress, storage, descriptors, error));
    REQUIRE_FALSE(error.empty());
}

TEST_CASE("A zero-sized standalone descriptor record is rejected", "[reader][descriptions][robustness]")
{
    easy_test::LoadedProfile source;
    captureDescriptions(source);
    std::string bytes = makeDescriptionStream(source);
    overwrite<uint16_t>(bytes, FirstDescriptorSizeOffset, 0U);

    profiler::SerializedData storage;
    profiler::descriptors_list_t descriptors;
    std::atomic<int> progress(0);
    std::string error;

    REQUIRE_FALSE(readDescriptionStream(bytes, progress, storage, descriptors, error));
    REQUIRE(error.find("Zero descriptor size") != std::string::npos);
}

TEST_CASE("Standalone descriptor count must consume the declared memory", "[reader][descriptions][robustness]")
{
    easy_test::LoadedProfile source;
    captureDescriptions(source);
    REQUIRE(source.descriptorsCount > 1);
    std::string bytes = makeDescriptionStream(source);
    overwrite<uint32_t>(bytes, sizeof(uint32_t) * 2U, source.descriptorsCount - 1U);

    profiler::SerializedData storage;
    profiler::descriptors_list_t descriptors;
    std::atomic<int> progress(0);
    std::string error;

    REQUIRE_FALSE(readDescriptionStream(bytes, progress, storage, descriptors, error));
    REQUIRE(error.find("memory size") != std::string::npos);
}

TEST_CASE("Standalone descriptors reject unknown block types", "[reader][descriptions][robustness]")
{
    easy_test::LoadedProfile source;
    captureDescriptions(source);
    std::string bytes = makeDescriptionStream(source);

    const size_t typeOffset = sizeof(profiler::block_id_t) + sizeof(int32_t) +
                              sizeof(profiler::color_t);
    overwrite<uint8_t>(bytes, FirstDescriptorDataOffset + typeOffset, 0xffU);

    requireInvalidDescriptor(bytes);
}

TEST_CASE("Standalone descriptors reject unknown block statuses", "[reader][descriptions][robustness]")
{
    easy_test::LoadedProfile source;
    captureDescriptions(source);
    std::string bytes = makeDescriptionStream(source);

    const size_t statusOffset = sizeof(profiler::block_id_t) + sizeof(int32_t) +
                                sizeof(profiler::color_t) + sizeof(profiler::block_type_t);
    overwrite<uint8_t>(bytes, FirstDescriptorDataOffset + statusOffset, 0xffU);

    requireInvalidDescriptor(bytes);
}

TEST_CASE("Standalone descriptors require a non-empty name", "[reader][descriptions][robustness]")
{
    easy_test::LoadedProfile source;
    captureDescriptions(source);
    std::string bytes = makeDescriptionStream(source);

    const size_t nameLengthOffset = FirstDescriptorDataOffset +
                                    sizeof(profiler::BaseBlockDescriptor);
    overwrite<uint16_t>(bytes, nameLengthOffset, 0U);

    requireInvalidDescriptor(bytes);
}

TEST_CASE("Standalone descriptor names cannot consume the filename", "[reader][descriptions][robustness]")
{
    easy_test::LoadedProfile source;
    captureDescriptions(source);
    std::string bytes = makeDescriptionStream(source);

    uint16_t recordSize = 0;
    std::memcpy(&recordSize, &bytes[FirstDescriptorSizeOffset], sizeof(recordSize));
    REQUIRE(recordSize > sizeof(profiler::SerializedBlockDescriptor));
    const uint16_t stringsSize = static_cast<uint16_t>(
        recordSize - sizeof(profiler::SerializedBlockDescriptor));
    const size_t nameLengthOffset = FirstDescriptorDataOffset +
                                    sizeof(profiler::BaseBlockDescriptor);
    overwrite<uint16_t>(bytes, nameLengthOffset, stringsSize);

    requireInvalidDescriptor(bytes);
}

TEST_CASE("Standalone descriptor names must be terminated", "[reader][descriptions][robustness]")
{
    easy_test::LoadedProfile source;
    captureDescriptions(source);
    std::string bytes = makeDescriptionStream(source);

    const size_t nameLengthOffset = FirstDescriptorDataOffset +
                                    sizeof(profiler::BaseBlockDescriptor);
    uint16_t nameLength = 0;
    std::memcpy(&nameLength, &bytes[nameLengthOffset], sizeof(nameLength));
    REQUIRE(nameLength > 0);
    const size_t terminator = FirstDescriptorDataOffset +
                              sizeof(profiler::SerializedBlockDescriptor) + nameLength - 1U;
    overwrite<uint8_t>(bytes, terminator, static_cast<uint8_t>('x'));

    requireInvalidDescriptor(bytes);
}

TEST_CASE("Standalone descriptor filenames must be terminated", "[reader][descriptions][robustness]")
{
    easy_test::LoadedProfile source;
    captureDescriptions(source);
    std::string bytes = makeDescriptionStream(source);

    uint16_t recordSize = 0;
    std::memcpy(&recordSize, &bytes[FirstDescriptorSizeOffset], sizeof(recordSize));
    REQUIRE(recordSize > sizeof(profiler::SerializedBlockDescriptor));
    const size_t terminator = FirstDescriptorDataOffset + recordSize - 1U;
    overwrite<uint8_t>(bytes, terminator, static_cast<uint8_t>('x'));

    requireInvalidDescriptor(bytes);
}

TEST_CASE("Standalone description reading honours an existing cancellation", "[reader][descriptions][cancel]")
{
    easy_test::LoadedProfile source;
    captureDescriptions(source);

    profiler::SerializedData storage;
    profiler::descriptors_list_t descriptors;
    std::atomic<int> progress(-1);
    std::string error;

    REQUIRE_FALSE(readDescriptionStream(makeDescriptionStream(source), progress,
                                        storage, descriptors, error));
    REQUIRE(error.find("interrupted") != std::string::npos);
    REQUIRE(storage.empty());
    REQUIRE(descriptors.empty());
}
