/** Reader robustness against corrupt and forged .prof files.

The reader is the only parser in the project: both the GUI and profiler_converter
go through the same fillTreesFromFile. Files arrive from elsewhere ("a colleague
sent a trace", "captured on the test rig"), so behaving correctly on damaged
input is a requirement rather than hygiene.

Expected behaviour in every case below: return 0 and write the reason to the log
without taking the process down.

Each TEST_CASE is registered as its own CTest test (catch_discover_tests), so a
crash in one case does not take the others with it.
*/

#include <catch2/catch_test_macros.hpp>

#include "support/prof_image.h"
#include "support/prof_fixture.h"

#include <easy/arbitrary_value.h>
#include <easy/profiler.h>
#include <easy/serialized_block.h>

// profile_manager.h pulls in winsock2.h and has to precede current_thread.h,
// which includes Windows.h, the same order profile_manager.cpp uses.
#include "profile_manager.h"
#include "current_thread.h"

#include <algorithm>
#include <cstring>
#include <fstream>
#include <limits>
#include <sstream>
#include <streambuf>
#include <string>
#include <vector>

using easy_test::LoadedProfile;
using easy_test::ProfHeader;
using easy_test::ProfImage;

namespace {

/** Take a valid .prof image to corrupt afterwards. */
bool makeValidImage(ProfImage& image)
{
    return image.captureFrom([] {
        EASY_BLOCK("robust_root");
        { EASY_BLOCK("robust_child_a"); }
        { EASY_BLOCK("robust_child_b"); }
    });
}

bool makeValidValueImage(ProfImage& image)
{
    return image.captureFrom([] {
        EASY_VALUE("robust_value", int32_t(42));
    });
}

// ProfileManager is an internal class without PROFILER_API; on Windows the
// shared-library build does not export it, so the direct calls below are
// unavailable there.
#ifndef _WIN32
bool makeValidContextSwitchImage(ProfImage& image)
{
    easy_test::drainPending();
    const bool originalTracing = profiler::isEventTracingEnabled();
    profiler::setEventTracingEnabled(false);
    profiler::setEnabled(true);
    { EASY_BLOCK("robust_cswitch_anchor"); }

    ProfileManager& manager = ProfileManager::instance();
    const profiler::thread_id_t thread = getCurrentThreadId();
    const profiler::timestamp_t begin = profiler::timeNow();
    manager.beginContextSwitch(thread, begin, thread, "robust_cswitch_target", true);
    manager.endContextSwitch(thread, 0, begin + 1000U, true);

    profiler::setEnabled(false);
    easy_test::TempFile file("robust_cswitch_builder");
    const bool dumped = profiler::dumpBlocksToFile(file.c_str()) >= 2;
    profiler::setEventTracingEnabled(originalTracing);
    return dumped && image.loadFrom(file.path());
}

#endif // !_WIN32

void readImage(const ProfImage& image, const char* stem, LoadedProfile& out)
{
    easy_test::TempFile file(stem);
    image.writeTo(file.path());
    easy_test::loadProfile(file.path(), out);
}

class NonSeekableBuffer : public std::streambuf
{
public:
    explicit NonSeekableBuffer(std::string& bytes)
    {
        char* begin = bytes.empty() ? nullptr : &bytes[0];
        setg(begin, begin, begin + bytes.size());
    }

protected:
    pos_type seekoff(off_type, std::ios_base::seekdir,
                     std::ios_base::openmode) override
    {
        return pos_type(off_type(-1));
    }

    pos_type seekpos(pos_type, std::ios_base::openmode) override
    {
        return pos_type(off_type(-1));
    }
};

class EndSeekFailBuffer : public std::stringbuf
{
public:
    explicit EndSeekFailBuffer(const std::string& bytes)
        : std::stringbuf(bytes, std::ios::in | std::ios::binary)
    {
    }

protected:
    pos_type seekoff(off_type offset, std::ios_base::seekdir direction,
                     std::ios_base::openmode mode) override
    {
        if (direction == std::ios_base::end)
            return pos_type(off_type(-1));
        return std::stringbuf::seekoff(offset, direction, mode);
    }
};

/** Seekable while the reader sizes the stream, unseekable from then on. */
class SeekableOnceBuffer : public std::stringbuf
{
public:
    explicit SeekableOnceBuffer(const std::string& bytes)
        : std::stringbuf(bytes, std::ios::in | std::ios::binary)
        , m_endSeeks(0)
    {
    }

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

class CancellingBuffer : public std::stringbuf
{
public:
    CancellingBuffer(const std::string& bytes, std::atomic<int>& progress)
        : std::stringbuf(bytes, std::ios::in | std::ios::binary)
        , m_progress(progress)
    {
    }

protected:
    pos_type seekoff(off_type, std::ios_base::seekdir,
                     std::ios_base::openmode) override
    {
        return pos_type(off_type(-1));
    }

    pos_type seekpos(pos_type, std::ios_base::openmode) override
    {
        return pos_type(off_type(-1));
    }

    std::streamsize xsgetn(char* data, std::streamsize count) override
    {
        const std::streamsize result = std::stringbuf::xsgetn(data, count);
        m_progress.store(-1);
        return result;
    }

private:
    std::atomic<int>& m_progress;
};

class FailingReadBuffer : public std::streambuf
{
protected:
    pos_type seekoff(off_type, std::ios_base::seekdir,
                     std::ios_base::openmode) override
    {
        return pos_type(off_type(-1));
    }

    int_type underflow() override
    {
        throw std::ios_base::failure("injected stream failure");
    }
};

class LargeNonSeekableBuffer : public std::streambuf
{
public:
    explicit LargeNonSeekableBuffer(size_t size) : m_remaining(size) {}

protected:
    pos_type seekoff(off_type, std::ios_base::seekdir,
                     std::ios_base::openmode) override
    {
        return pos_type(off_type(-1));
    }

    pos_type seekpos(pos_type, std::ios_base::openmode) override
    {
        return pos_type(off_type(-1));
    }

    std::streamsize xsgetn(char* data, std::streamsize count) override
    {
        const size_t requested = static_cast<size_t>(count);
        const size_t returned = std::min(requested, m_remaining);
        std::memset(data, 0, returned);
        m_remaining -= returned;
        return static_cast<std::streamsize>(returned);
    }

private:
    size_t m_remaining;
};

void readNonSeekable(ProfImage& image, LoadedProfile& out, int& finalProgress)
{
    std::string bytes(image.bytes().begin(), image.bytes().end());
    NonSeekableBuffer buffer(bytes);
    std::istream stream(&buffer);
    std::atomic<int> progress(0);
    std::ostringstream log;

    out.blocksRead = fillTreesFromStream(
        progress, stream, out.beginEndTime, out.serializedBlocks,
        out.serializedDescriptors, out.descriptors, out.blocks,
        out.threadedTrees, out.bookmarks, out.descriptorsCount,
        out.version, out.pid, false, log);
    out.readerLog = log.str();
    finalProgress = progress.load();
}

size_t descriptorPrefixOffset(const ProfImage& image, uint32_t index)
{
    size_t offset = ProfHeader::Size;
    for (uint32_t i = 0; i < index; ++i)
        offset += sizeof(uint16_t) + image.read<uint16_t>(offset);
    return offset;
}

size_t descriptorDataOffset(const ProfImage& image, uint32_t index)
{
    return descriptorPrefixOffset(image, index) + sizeof(uint16_t);
}

size_t firstThreadOffset(const ProfImage& image)
{
    return ProfHeader::Size + image.descriptorsMemorySize() +
           static_cast<size_t>(image.totalDescriptors()) * sizeof(uint16_t);
}

size_t firstThreadNameSizeOffset(const ProfImage& image)
{
    return firstThreadOffset(image) + sizeof(profiler::thread_id_t);
}

size_t firstContextSwitchCountOffset(const ProfImage& image)
{
    const size_t nameSizeOffset = firstThreadNameSizeOffset(image);
    return nameSizeOffset + sizeof(uint16_t) + image.read<uint16_t>(nameSizeOffset);
}

size_t firstBlockCountOffset(const ProfImage& image)
{
    size_t offset = firstContextSwitchCountOffset(image);

    const uint32_t contextSwitches = image.read<uint32_t>(offset);
    offset += sizeof(uint32_t);
    for (uint32_t i = 0; i < contextSwitches; ++i)
        offset += sizeof(uint16_t) + image.read<uint16_t>(offset);
    return offset;
}

size_t firstContextSwitchPrefixOffset(const ProfImage& image)
{
    return firstContextSwitchCountOffset(image) + sizeof(uint32_t);
}

size_t firstBlockPrefixOffset(const ProfImage& image)
{
    return firstBlockCountOffset(image) + sizeof(uint32_t);
}

/** Width of one element of an arbitrary value payload, or 0 for an unknown type.

Mirrors what a consumer has to do to interpret a value: the record stores a byte
count, and the element count follows from the type.
*/
size_t valueElementSize(profiler::DataType type)
{
    switch (type)
    {
        case profiler::DataType::Bool:   return sizeof(bool);
        case profiler::DataType::Char:   return sizeof(char);
        case profiler::DataType::Int8:   return sizeof(int8_t);
        case profiler::DataType::Uint8:  return sizeof(uint8_t);
        case profiler::DataType::Int16:  return sizeof(int16_t);
        case profiler::DataType::Uint16: return sizeof(uint16_t);
        case profiler::DataType::Int32:  return sizeof(int32_t);
        case profiler::DataType::Uint32: return sizeof(uint32_t);
        case profiler::DataType::Int64:  return sizeof(int64_t);
        case profiler::DataType::Uint64: return sizeof(uint64_t);
        case profiler::DataType::Float:  return sizeof(float);
        case profiler::DataType::Double: return sizeof(double);
        case profiler::DataType::String: return sizeof(char);
        default: return 0;
    }
}

} // namespace

TEST_CASE("A missing file", "[robustness]")
{
    LoadedProfile profile;
    easy_test::loadProfile("definitely_missing_file_12345.prof", profile);

    REQUIRE(profile.blocksRead == 0);
    REQUIRE_FALSE(profile.readerLog.empty());
}

TEST_CASE("File reading honours an existing cancellation before opening the file", "[robustness][cancel]")
{
    LoadedProfile profile;
    std::atomic<int> progress(-1);
    std::ostringstream log;

    profile.blocksRead = fillTreesFromFile(
        progress, "definitely_missing_cancelled_file_12345.prof",
        profile.beginEndTime, profile.serializedBlocks, profile.serializedDescriptors,
        profile.descriptors, profile.blocks, profile.threadedTrees, profile.bookmarks,
        profile.descriptorsCount, profile.version, profile.pid, false, log);

    REQUIRE(profile.blocksRead == 0);
    REQUIRE(log.str().find("interrupted") != std::string::npos);
}

TEST_CASE("An empty file", "[robustness]")
{
    easy_test::TempFile file("robust_empty");
    {
        std::ofstream out(file.path().c_str(), std::ios::binary | std::ios::trunc);
    }

    LoadedProfile profile;
    easy_test::loadProfile(file.path(), profile);

    REQUIRE(profile.blocksRead == 0);
    REQUIRE_FALSE(profile.readerLog.empty());
}

TEST_CASE("A file of random garbage", "[robustness]")
{
    easy_test::TempFile file("robust_garbage");
    {
        std::ofstream out(file.path().c_str(), std::ios::binary | std::ios::trunc);
        const std::string garbage(4096, '\xA5');
        out.write(garbage.data(), static_cast<std::streamsize>(garbage.size()));
    }

    LoadedProfile profile;
    easy_test::loadProfile(file.path(), profile);

    REQUIRE(profile.blocksRead == 0);
    REQUIRE_FALSE(profile.readerLog.empty());
}

TEST_CASE("A valid profile reads from a non-seekable stream", "[robustness][stream]")
{
    ProfImage image;
    REQUIRE(makeValidImage(image));

    LoadedProfile profile;
    int progress = 0;
    readNonSeekable(image, profile, progress);

    REQUIRE(profile.readerLog.empty());
    REQUIRE(profile.blocksRead == image.totalBlocks());
    REQUIRE(progress == 100);
}

TEST_CASE("A stream with an unavailable end position is buffered", "[robustness][stream]")
{
    ProfImage image;
    REQUIRE(makeValidImage(image));

    const std::string bytes(image.bytes().begin(), image.bytes().end());
    EndSeekFailBuffer buffer(bytes);
    std::istream stream(&buffer);
    std::atomic<int> progress(0);
    std::ostringstream log;
    LoadedProfile profile;

    profile.blocksRead = fillTreesFromStream(
        progress, stream, profile.beginEndTime, profile.serializedBlocks,
        profile.serializedDescriptors, profile.descriptors, profile.blocks,
        profile.threadedTrees, profile.bookmarks, profile.descriptorsCount,
        profile.version, profile.pid, false, log);

    REQUIRE(log.str().empty());
    REQUIRE(profile.blocksRead == image.totalBlocks());
    REQUIRE(progress.load() == 100);
}

TEST_CASE("A stream whose size becomes unknown after the header is rejected", "[robustness][stream]")
{
    // Non-seekable streams are buffered before the header is read, so this is
    // reached only when a seek that worked at the start fails later. The
    // declared sizes must then not be trusted for allocation.
    ProfImage image;
    REQUIRE(makeValidImage(image));
    image.setDescriptorsMemorySize(static_cast<uint64_t>(1) << 40);

    const std::string bytes(image.bytes().begin(), image.bytes().end());
    SeekableOnceBuffer buffer(bytes);
    std::istream stream(&buffer);
    std::atomic<int> progress(0);
    std::ostringstream log;
    LoadedProfile profile;

    profile.blocksRead = fillTreesFromStream(
        progress, stream, profile.beginEndTime, profile.serializedBlocks,
        profile.serializedDescriptors, profile.descriptors, profile.blocks,
        profile.threadedTrees, profile.bookmarks, profile.descriptorsCount,
        profile.version, profile.pid, false, log);

    REQUIRE(profile.blocksRead == 0);
    REQUIRE(profile.serializedDescriptors.size() == 0);
    REQUIRE(log.str().find("size is unavailable") != std::string::npos);
}

TEST_CASE("Cancelling while a non-seekable stream is buffered stops reading", "[robustness][stream][cancel]")
{
    ProfImage image;
    REQUIRE(makeValidImage(image));

    const std::string bytes(image.bytes().begin(), image.bytes().end());
    std::atomic<int> progress(0);
    CancellingBuffer buffer(bytes, progress);
    std::istream stream(&buffer);
    std::ostringstream log;
    LoadedProfile profile;

    profile.blocksRead = fillTreesFromStream(
        progress, stream, profile.beginEndTime, profile.serializedBlocks,
        profile.serializedDescriptors, profile.descriptors, profile.blocks,
        profile.threadedTrees, profile.bookmarks, profile.descriptorsCount,
        profile.version, profile.pid, false, log);

    REQUIRE(profile.blocksRead == 0);
    REQUIRE(log.str().find("interrupted") != std::string::npos);
}

TEST_CASE("An I/O failure while buffering a stream is reported", "[robustness][stream]")
{
    FailingReadBuffer buffer;
    std::istream stream(&buffer);
    std::atomic<int> progress(0);
    std::ostringstream log;
    LoadedProfile profile;

    profile.blocksRead = fillTreesFromStream(
        progress, stream, profile.beginEndTime, profile.serializedBlocks,
        profile.serializedDescriptors, profile.descriptors, profile.blocks,
        profile.threadedTrees, profile.bookmarks, profile.descriptorsCount,
        profile.version, profile.pid, false, log);

    REQUIRE(profile.blocksRead == 0);
    REQUIRE(log.str().find("Unable to buffer") != std::string::npos);
}

TEST_CASE("A non-seekable stream has a bounded memory buffer", "[robustness][stream]")
{
    LargeNonSeekableBuffer buffer(64U * 1024U * 1024U + 1U);
    std::istream stream(&buffer);
    std::atomic<int> progress(0);
    std::ostringstream log;
    LoadedProfile profile;

    profile.blocksRead = fillTreesFromStream(
        progress, stream, profile.beginEndTime, profile.serializedBlocks,
        profile.serializedDescriptors, profile.descriptors, profile.blocks,
        profile.threadedTrees, profile.bookmarks, profile.descriptorsCount,
        profile.version, profile.pid, false, log);

    REQUIRE(profile.blocksRead == 0);
    REQUIRE(log.str().find("64 MiB buffering limit") != std::string::npos);
}

TEST_CASE("A non-seekable stream validates counts before reserving", "[robustness][stream]")
{
    ProfImage image;
    REQUIRE(makeValidImage(image));
    image.setTotalDescriptors(100000U);

    LoadedProfile profile;
    int progress = 0;
    readNonSeekable(image, profile, progress);

    REQUIRE(profile.blocksRead == 0);
    REQUIRE(profile.descriptors.empty());
    REQUIRE(profile.readerLog.find("available stream data") != std::string::npos);
}

TEST_CASE("A wrong signature", "[robustness]")
{
    ProfImage image;
    REQUIRE(makeValidImage(image));

    image.setSignature(0xDEADBEEFu);

    LoadedProfile profile;
    readImage(image, "robust_signature", profile);

    REQUIRE(profile.blocksRead == 0);
    REQUIRE_FALSE(profile.readerLog.empty());
}

TEST_CASE("A version below the minimum compatible one", "[robustness]")
{
    ProfImage image;
    REQUIRE(makeValidImage(image));

    // MIN_COMPATIBLE_VERSION is 0.1.0 (reader.cpp:79); pick something lower.
    image.setVersion(easy_test::makeVersion(0, 0, 1));

    LoadedProfile profile;
    readImage(image, "robust_old_version", profile);

    REQUIRE(profile.blocksRead == 0);
    REQUIRE_FALSE(profile.readerLog.empty());
    REQUIRE(profile.readerLog.find("Incompatible version") != std::string::npos);
}

TEST_CASE("A truncated header", "[robustness]")
{
    ProfImage image;
    REQUIRE(makeValidImage(image));

    image.truncateTo(ProfHeader::Size / 2);

    LoadedProfile profile;
    readImage(image, "robust_short_header", profile);

    REQUIRE(profile.blocksRead == 0);
}

TEST_CASE("A negative CPU frequency is rejected", "[robustness]")
{
    ProfImage image;
    REQUIRE(makeValidImage(image));

    image.write<int64_t>(ProfHeader::OffsetCpuFrequency, -1);

    LoadedProfile profile;
    readImage(image, "robust_negative_frequency", profile);

    REQUIRE(profile.blocksRead == 0);
    REQUIRE(profile.readerLog.find("frequency") != std::string::npos);
}

TEST_CASE("A zero CPU frequency keeps timestamps in ticks", "[robustness][timestamp]")
{
    ProfImage image;
    REQUIRE(makeValidImage(image));

    image.write<int64_t>(ProfHeader::OffsetCpuFrequency, 0);

    LoadedProfile profile;
    readImage(image, "robust_zero_frequency", profile);

    REQUIRE(profile.readerLog.empty());
    REQUIRE(profile.blocksRead == image.totalBlocks());
    REQUIRE(profile.beginEndTime.beginTime ==
            image.read<uint64_t>(ProfHeader::OffsetBeginTime));
}

TEST_CASE("Timestamp conversion overflow is rejected", "[robustness][timestamp]")
{
    ProfImage image;
    REQUIRE(makeValidImage(image));

    image.write<int64_t>(ProfHeader::OffsetCpuFrequency, 1);
    image.write<uint64_t>(ProfHeader::OffsetBeginTime,
                          std::numeric_limits<uint64_t>::max());
    image.write<uint64_t>(ProfHeader::OffsetEndTime,
                          std::numeric_limits<uint64_t>::max());

    LoadedProfile profile;
    readImage(image, "robust_timestamp_overflow", profile);

    REQUIRE(profile.blocksRead == 0);
    REQUIRE(profile.readerLog.find("Timestamp conversion overflow") != std::string::npos);
}

TEST_CASE("A file truncated right after the header", "[robustness]")
{
    ProfImage image;
    REQUIRE(makeValidImage(image));

    image.truncateTo(ProfHeader::Size);

    LoadedProfile profile;
    readImage(image, "robust_header_only", profile);

    REQUIRE(profile.blocksRead == 0);
}

TEST_CASE("A file truncated in the middle of the block stream", "[robustness]")
{
    ProfImage image;
    REQUIRE(makeValidImage(image));

    const size_t original = image.size();
    REQUIRE(original > ProfHeader::Size + 16);

    // Cut the last quarter; the header and part of the descriptors stay intact.
    image.truncateTo(original - original / 4);

    LoadedProfile profile;
    readImage(image, "robust_truncated_blocks", profile);

    // The requirement is not to crash; the block count must not exceed the claim.
    REQUIRE(profile.blocksRead <= image.totalBlocks());
}

TEST_CASE("More blocks claimed than present", "[robustness]")
{
    ProfImage image;
    REQUIRE(makeValidImage(image));

    image.setTotalBlocks(image.totalBlocks() + 100000u);

    LoadedProfile profile;
    readImage(image, "robust_block_count", profile);

    // The reader must not invent blocks that are not there.
    REQUIRE(profile.blocksRead < image.totalBlocks());
}

TEST_CASE("A block count the stream cannot hold is rejected before anything is reserved", "[robustness]")
{
    // sizeof(BlocksTree) is 64 bytes against a 23-byte minimum block record, so
    // reserve() for a forged count would cost far more than the file itself.
    ProfImage image;
    REQUIRE(makeValidImage(image));

    const size_t remaining = image.size() - ProfHeader::Size;
    image.setTotalBlocks(static_cast<uint32_t>(remaining / 2));

    LoadedProfile profile;
    readImage(image, "robust_block_reserve", profile);

    REQUIRE(profile.blocksRead == 0);
    REQUIRE(profile.readerLog.find("exceeds the available stream data") != std::string::npos);
    REQUIRE(profile.blocks.capacity() == 0);
}

TEST_CASE("A per-thread context-switch count cannot exceed the total", "[robustness][thread]")
{
    ProfImage image;
    REQUIRE(makeValidImage(image));

    image.write<uint32_t>(firstContextSwitchCountOffset(image), image.totalBlocks() + 1U);

    LoadedProfile profile;
    readImage(image, "robust_context_count", profile);

    REQUIRE(profile.blocksRead == 0);
    REQUIRE(profile.readerLog.find("Context-switch count") != std::string::npos);
}

TEST_CASE("A per-thread block count cannot exceed the total", "[robustness][thread]")
{
    ProfImage image;
    REQUIRE(makeValidImage(image));

    image.write<uint32_t>(firstBlockCountOffset(image), image.totalBlocks() + 1U);

    LoadedProfile profile;
    readImage(image, "robust_thread_block_count", profile);

    REQUIRE(profile.blocksRead == 0);
    REQUIRE(profile.readerLog.find("Thread block count") != std::string::npos);
}

TEST_CASE("A current profile with zero blocks is rejected", "[robustness]")
{
    ProfImage image;
    REQUIRE(makeValidImage(image));
    image.setTotalBlocks(0);

    LoadedProfile profile;
    readImage(image, "robust_zero_blocks", profile);

    REQUIRE(profile.blocksRead == 0);
    REQUIRE(profile.readerLog.find("blocks number") != std::string::npos);
}

TEST_CASE("More threads claimed than present", "[robustness]")
{
    ProfImage image;
    REQUIRE(makeValidImage(image));

    image.setThreadsCount(image.threadsCount() + 1000u);

    LoadedProfile profile;
    readImage(image, "robust_thread_count", profile);

    REQUIRE(profile.threadedTrees.size() <= image.threadsCount());
}

TEST_CASE("A current profile with zero threads is rejected", "[robustness]")
{
    ProfImage image;
    REQUIRE(makeValidImage(image));
    image.setThreadsCount(0);

    LoadedProfile profile;
    readImage(image, "robust_zero_threads", profile);

    REQUIRE(profile.blocksRead == 0);
    REQUIRE(profile.readerLog.find("Threads count") != std::string::npos);
}

TEST_CASE("Non-zero current header padding is rejected", "[robustness]")
{
    ProfImage image;
    REQUIRE(makeValidImage(image));
    image.write<uint16_t>(ProfHeader::OffsetPadding, 1U);

    LoadedProfile profile;
    readImage(image, "robust_header_padding", profile);

    REQUIRE(profile.blocksRead == 0);
    REQUIRE(profile.readerLog.find("padding") != std::string::npos);
}

TEST_CASE("Zero block memory size", "[robustness]")
{
    ProfImage image;
    REQUIRE(makeValidImage(image));

    image.setUsedMemorySize(0);

    LoadedProfile profile;
    readImage(image, "robust_zero_blocks_memory", profile);

    REQUIRE(profile.blocksRead == 0);
}

TEST_CASE("Overstated block memory size", "[robustness]")
{
    ProfImage image;
    REQUIRE(makeValidImage(image));

    image.setUsedMemorySize(image.usedMemorySize() * 1000);

    LoadedProfile profile;
    readImage(image, "robust_huge_blocks_memory", profile);

    // Allocating a huge buffer is acceptable; reading garbage past the end is not.
    REQUIRE(profile.blocksRead <= image.totalBlocks());
}

TEST_CASE("Zero descriptor count", "[robustness]")
{
    ProfImage image;
    REQUIRE(makeValidImage(image));

    image.setTotalDescriptors(0);

    LoadedProfile profile;
    readImage(image, "robust_zero_descriptors", profile);

    // Without descriptors the blocks cannot be interpreted.
    REQUIRE(profile.descriptors.empty());
}

TEST_CASE("Descriptor memory size must match the decoded records", "[robustness]")
{
    ProfImage image;
    REQUIRE(makeValidImage(image));

    image.setDescriptorsMemorySize(image.descriptorsMemorySize() + 1);

    LoadedProfile profile;
    readImage(image, "robust_descriptor_size_mismatch", profile);

    REQUIRE(profile.blocksRead == 0);
    REQUIRE(profile.readerLog.find("memory size") != std::string::npos);
}

TEST_CASE("A descriptor shorter than its fixed wire header is rejected", "[robustness][descriptor]")
{
    ProfImage image;
    REQUIRE(makeValidImage(image));

    image.write<uint16_t>(descriptorPrefixOffset(image, 0), 1U);

    LoadedProfile profile;
    readImage(image, "robust_short_descriptor", profile);

    REQUIRE(profile.blocksRead == 0);
    REQUIRE(profile.readerLog.find("Invalid descriptor record") != std::string::npos);
}

TEST_CASE("A zero-sized descriptor prefix is rejected", "[robustness][descriptor]")
{
    ProfImage image;
    REQUIRE(makeValidImage(image));

    image.write<uint16_t>(descriptorPrefixOffset(image, 0), 0U);

    LoadedProfile profile;
    readImage(image, "robust_zero_descriptor", profile);

    INFO(profile.readerLog);
    REQUIRE(profile.blocksRead == 0);
    REQUIRE_FALSE(profile.readerLog.empty());
}

TEST_CASE("A descriptor id must match its table position", "[robustness][descriptor]")
{
    ProfImage image;
    REQUIRE(makeValidImage(image));

    image.write<profiler::block_id_t>(descriptorDataOffset(image, 0), 1U);

    LoadedProfile profile;
    readImage(image, "robust_descriptor_id", profile);

    REQUIRE(profile.blocksRead == 0);
    REQUIRE(profile.readerLog.find("Descriptor id") != std::string::npos);
}

TEST_CASE("Malformed descriptor fields are rejected", "[robustness][descriptor]")
{
    ProfImage image;
    REQUIRE(makeValidImage(image));

    const size_t prefix = descriptorPrefixOffset(image, 0);
    const size_t data = descriptorDataOffset(image, 0);
    const uint16_t recordSize = image.read<uint16_t>(prefix);
    const size_t fixedSize = sizeof(profiler::SerializedBlockDescriptor);
    const size_t typeOffset = sizeof(profiler::block_id_t) + sizeof(int32_t) +
                              sizeof(profiler::color_t);
    const size_t statusOffset = typeOffset + sizeof(profiler::block_type_t);
    const size_t nameLengthOffset = sizeof(profiler::BaseBlockDescriptor);

    SECTION("unknown block type")
    {
        image.write<uint8_t>(data + typeOffset,
                             static_cast<uint8_t>(profiler::BlockType::TypesCount));
    }
    SECTION("unknown block status")
    {
        image.write<uint8_t>(data + statusOffset, 0xffU);
    }
    SECTION("empty descriptor name")
    {
        image.write<uint16_t>(data + nameLengthOffset, 0U);
    }
    SECTION("descriptor name consumes the whole strings area")
    {
        image.write<uint16_t>(data + nameLengthOffset,
                              static_cast<uint16_t>(recordSize - fixedSize));
    }
    SECTION("descriptor name is not terminated")
    {
        const uint16_t nameLength = image.read<uint16_t>(data + nameLengthOffset);
        image.write<char>(data + fixedSize + nameLength - 1U, 'x');
    }
    SECTION("descriptor filename is not terminated")
    {
        image.write<char>(data + recordSize - 1U, 'x');
    }

    LoadedProfile profile;
    readImage(image, "robust_descriptor_field", profile);

    REQUIRE(profile.blocksRead == 0);
    REQUIRE(profile.readerLog.find("Invalid descriptor record") != std::string::npos);
}

TEST_CASE("A thread name must be zero terminated", "[robustness][thread]")
{
    ProfImage image;
    REQUIRE(makeValidImage(image));

    const size_t nameSizeOffset = firstThreadNameSizeOffset(image);
    const uint16_t nameSize = image.read<uint16_t>(nameSizeOffset);
    REQUIRE(nameSize > 0);
    image.write<char>(nameSizeOffset + sizeof(uint16_t) + nameSize - 1U, 'x');

    LoadedProfile profile;
    readImage(image, "robust_thread_name", profile);

    REQUIRE(profile.blocksRead == 0);
    REQUIRE(profile.readerLog.find("Thread name") != std::string::npos);
}

#ifndef _WIN32

TEST_CASE("Malformed context-switch records are rejected", "[robustness][cswitch]")
{
    ProfImage image;
    REQUIRE(makeValidContextSwitchImage(image));

    const size_t prefix = firstContextSwitchPrefixOffset(image);
    const size_t data = prefix + sizeof(uint16_t);

    SECTION("zero record size")
    {
        image.write<uint16_t>(prefix, 0U);
    }
    SECTION("record is shorter than CSwitchEvent and its name terminator")
    {
        image.write<uint16_t>(prefix, static_cast<uint16_t>(sizeof(profiler::CSwitchEvent)));
    }
    SECTION("record exceeds the declared block memory")
    {
        image.write<uint16_t>(prefix, std::numeric_limits<uint16_t>::max());
    }
    SECTION("target process name is not terminated")
    {
        const uint16_t recordSize = image.read<uint16_t>(prefix);
        image.write<char>(data + recordSize - 1U, 'x');
    }

    LoadedProfile profile;
    readImage(image, "robust_cswitch_record", profile);

    REQUIRE(profile.blocksRead == 0);
    REQUIRE_FALSE(profile.readerLog.empty());
}

TEST_CASE("A context switch beginning before capture is clamped", "[robustness][cswitch][timestamp]")
{
    ProfImage image;
    REQUIRE(makeValidContextSwitchImage(image));

    const size_t data = firstContextSwitchPrefixOffset(image) + sizeof(uint16_t);
    image.write<profiler::timestamp_t>(data, 0);

    LoadedProfile profile;
    readImage(image, "robust_cswitch_before_capture", profile);

    REQUIRE(profile.readerLog.empty());
    REQUIRE(profile.blocksRead == image.totalBlocks());
    REQUIRE_FALSE(profile.threadedTrees.empty());
    REQUIRE_FALSE(profile.threadedTrees.begin()->second.sync.empty());
    const profiler::block_index_t switchIndex =
        profile.threadedTrees.begin()->second.sync.front();
    REQUIRE(profile.blocks[switchIndex].cs->begin() == profile.beginEndTime.beginTime);
}

#endif // !_WIN32

TEST_CASE("Malformed block records are rejected", "[robustness][block]")
{
    ProfImage image;
    REQUIRE(makeValidImage(image));

    const size_t prefix = firstBlockPrefixOffset(image);
    const size_t data = prefix + sizeof(uint16_t);

    SECTION("zero record size")
    {
        image.write<uint16_t>(prefix, 0U);
    }
    SECTION("record shorter than BaseBlockData and its name terminator")
    {
        image.write<uint16_t>(prefix, 1U);
    }
    SECTION("record exceeds the declared block memory")
    {
        image.write<uint16_t>(prefix, std::numeric_limits<uint16_t>::max());
    }
    SECTION("descriptor id is outside the descriptor table")
    {
        const size_t idOffset = sizeof(profiler::timestamp_t) * 2U;
        image.write<profiler::block_id_t>(data + idOffset, image.totalDescriptors());
    }
    SECTION("runtime name is not terminated")
    {
        const uint16_t recordSize = image.read<uint16_t>(prefix);
        image.write<char>(data + recordSize - 1U, 'x');
    }

    LoadedProfile profile;
    readImage(image, "robust_block_record", profile);

    REQUIRE(profile.blocksRead == 0);
    REQUIRE_FALSE(profile.readerLog.empty());
}

TEST_CASE("A block beginning before capture is clamped to the capture boundary", "[robustness][block][timestamp]")
{
    ProfImage image;
    REQUIRE(makeValidImage(image));

    const size_t data = firstBlockPrefixOffset(image) + sizeof(uint16_t);
    image.write<profiler::timestamp_t>(data, 0);

    LoadedProfile profile;
    readImage(image, "robust_block_before_capture", profile);

    REQUIRE(profile.readerLog.empty());
    REQUIRE(profile.blocksRead == image.totalBlocks());
    REQUIRE_FALSE(profile.blocks.empty());
    REQUIRE(profile.blocks.front().node->begin() == profile.beginEndTime.beginTime);
}

TEST_CASE("A block ending before capture cannot satisfy the declared block count", "[robustness][block][timestamp]")
{
    ProfImage image;
    REQUIRE(makeValidImage(image));

    const size_t data = firstBlockPrefixOffset(image) + sizeof(uint16_t);
    image.write<profiler::timestamp_t>(data, 0);
    image.write<profiler::timestamp_t>(data + sizeof(profiler::timestamp_t), 0);

    LoadedProfile profile;
    readImage(image, "robust_block_outside_capture", profile);

    REQUIRE(profile.blocksRead == 0);
    REQUIRE(profile.readerLog.find("does not match blocks count") != std::string::npos);
}

TEST_CASE("Malformed arbitrary value fields are rejected", "[robustness][block][value]")
{
    ProfImage image;
    REQUIRE(makeValidValueImage(image));

    const size_t prefix = firstBlockPrefixOffset(image);
    const size_t data = prefix + sizeof(uint16_t);
    const size_t nameStubOffset = sizeof(profiler::BaseBlockData);
    const size_t dataSizeOffset = nameStubOffset + 2U;
    const size_t typeOffset = dataSizeOffset + sizeof(uint16_t);
    const size_t arrayFlagOffset = typeOffset + sizeof(profiler::DataType);

    SECTION("value is shorter than its fixed header")
    {
        image.write<uint16_t>(prefix,
                              static_cast<uint16_t>(sizeof(profiler::ArbitraryValue) - 1U));
    }
    SECTION("value name stub is not empty")
    {
        image.write<char>(data + nameStubOffset, 'x');
    }
    SECTION("value data type is unknown")
    {
        image.write<uint8_t>(data + typeOffset,
                             static_cast<uint8_t>(profiler::DataType::TypesCount));
    }
    SECTION("value array flag is outside the wire range")
    {
        image.write<uint8_t>(data + arrayFlagOffset, 2U);
    }
    SECTION("value data size disagrees with its record size")
    {
        image.write<uint16_t>(data + dataSizeOffset, 1U);
    }
    SECTION("value data size exceeds the format limit")
    {
        image.write<uint16_t>(data + dataSizeOffset,
                              static_cast<uint16_t>(profiler::MAX_BLOCK_DATA_SIZE + 1U));
    }
    SECTION("scalar value has the wrong element width")
    {
        image.write<uint16_t>(prefix,
                              static_cast<uint16_t>(sizeof(profiler::ArbitraryValue) + 2U));
        image.write<uint16_t>(data + dataSizeOffset, 2U);
    }
    SECTION("string value is encoded as a scalar")
    {
        image.write<uint16_t>(prefix,
                              static_cast<uint16_t>(sizeof(profiler::ArbitraryValue) + 1U));
        image.write<uint16_t>(data + dataSizeOffset, 1U);
        image.write<uint8_t>(data + typeOffset,
                             static_cast<uint8_t>(profiler::DataType::String));
        image.write<uint8_t>(data + arrayFlagOffset, 0U);
    }

    LoadedProfile profile;
    readImage(image, "robust_arbitrary_value", profile);

    REQUIRE(profile.blocksRead == 0);
    REQUIRE(profile.readerLog.find("Invalid arbitrary value record") != std::string::npos);
}

TEST_CASE("Everything a loaded profile hands back is safe for a naive consumer",
          "[robustness][consumer][value]")
{
    // Every other case in this file, and the fuzzer, asks one question: does the
    // READER survive its input. That misses a whole class of defect, because the
    // reader can accept a file, report no error, and still hand a consumer
    // something the consumer cannot use safely.
    //
    // Concretely: the GUI used to turn a value into a QString with
    // QString(const char*), which is strlen(), and the format deliberately
    // permits a DataType::String payload with no trailing zero -
    // EASY_STRING(name, text, len) records exactly len bytes. The block buffer
    // is exactly the sum of the payloads, so a record at its end has nothing
    // readable behind it and that strlen() left the allocation.
    //
    // So this test is the consumer. It walks every record the reader produced,
    // does to each one what a consumer does, and checks that every such read
    // lands inside the buffer the reader owns.
    const std::string unterminated(48, 'A');

    LoadedProfile profile;
    easy_test::capture(profile, [&unterminated] {
        { EASY_BLOCK("consumer_host"); }
        {
            const std::string runtimeName = "consumer_runtime_name";
            EASY_BLOCK(runtimeName.c_str());
        }
        EASY_TEXT("consumer_text", "terminated text");
        EASY_VALUE("consumer_scalar", int32_t(7));
        const uint8_t numbers[3] = { 1U, 2U, 3U };
        EASY_ARRAY("consumer_array", numbers, 3);

        // A zero-length array: the reader accepts isArray == 1 with a zero data
        // size, so element 0 of such an array lies outside the record.
        const profiler::BaseBlockDescriptor* descriptor = profiler::registerDescription(
            profiler::ON, "consumer_empty_array_id", "consumer_empty_array", __FILE__, __LINE__,
            profiler::BlockType::Value, profiler::colors::Default, false);
        profiler::storeValue(descriptor, profiler::DataType::Uint8, nullptr, 0U, true,
                             profiler::ValueId());

        // Last on purpose - see the honesty check below.
        EASY_STRING("consumer_unterminated", unterminated.data(), 48);
    });

    INFO("reader log: " << profile.readerLog);
    REQUIRE(profile.readerLog.empty());

    const char* const buffer = profile.serializedBlocks.data();
    const uint64_t bufferSize = profile.serializedBlocks.size();
    REQUIRE(buffer != nullptr);
    REQUIRE(bufferSize != 0);

    // Keep the test honest. The defect only shows at the end of the buffer, so
    // the unterminated string has to be the last record; if anything is ever
    // appended after it this fails instead of passing vacuously.
    const profiler::ArbitraryValue* tail = easy_test::findValue(profile, "consumer_unterminated");
    REQUIRE(tail != nullptr);
    REQUIRE(static_cast<uint64_t>(tail->data() - buffer) + tail->data_size() == bufferSize);

    size_t names = 0, stringValues = 0, arrays = 0, emptyArrays = 0;
    char sink = 0;

    for (size_t i = 0; i < profile.blocks.size(); ++i)
    {
        const profiler::BlocksTree& entry = profile.blocks[i];
        if (entry.node == nullptr)
            continue;

        // The bounds check is mandatory - see blockName() in prof_fixture.h: for
        // a context switch record m_id holds a thread id, not a descriptor id.
        // This profile has no context switches, so every entry that resolves is
        // either a block or a value.
        const profiler::block_id_t id = entry.node->id();
        if (id >= profile.descriptors.size())
            continue;
        const profiler::SerializedBlockDescriptor* descriptor = profile.descriptors[id];
        if (descriptor == nullptr)
            continue;

        if (descriptor->type() != profiler::BlockType::Value)
        {
            // A consumer reads a block's runtime name as a C string.
            const char* name = entry.node->name();
            const uint64_t offset = static_cast<uint64_t>(name - buffer);
            REQUIRE(offset + std::strlen(name) < bufferSize);
            ++names;
            continue;
        }

        const profiler::ArbitraryValue* value = entry.value;
        const uint64_t payload = static_cast<uint64_t>(value->data() - buffer);
        REQUIRE(payload + value->data_size() <= bufferSize);

        if (value->type() == profiler::DataType::String)
        {
            // What stringValueToString() in the GUI does: a read bounded by the
            // recorded length, which must never reach behind the record.
            const profiler::StringValue* text = value->toArray<profiler::DataType::String>();
            REQUIRE(text != nullptr);
            REQUIRE(text->length() <= value->data_size());
            REQUIRE(payload + text->length() <= bufferSize);
            ++stringValues;
            continue;
        }

        // For an array the element count has to be taken from the recorded size
        // BEFORE any element is touched. Reading element 0 first and consulting
        // the count afterwards - which is what the GUI did - walks out of a
        // zero-length array's record.
        const size_t elementSize = valueElementSize(value->type());
        REQUIRE(elementSize != 0);
        if (!value->isArray())
        {
            REQUIRE(value->data_size() == elementSize);
            sink = static_cast<char>(sink ^ value->data()[elementSize - 1]);
            continue;
        }

        ++arrays;
        const size_t count = value->data_size() / elementSize;
        if (count == 0)
        {
            ++emptyArrays;
            continue;
        }

        REQUIRE(payload + count * elementSize <= bufferSize);
        for (size_t byte = 0; byte < count * elementSize; ++byte)
            sink = static_cast<char>(sink ^ value->data()[byte]);
    }

    // Guard against a sweep that silently inspected nothing.
    REQUIRE(names >= 2);
    REQUIRE(stringValues >= 2);
    REQUIRE(arrays >= 2);
    REQUIRE(emptyArrays == 1);
    INFO("checksum of everything read: " << static_cast<int>(sink));
}

TEST_CASE("A valid image reads: control for the corruption harness", "[robustness]")
{
    // Control test: if this one fails, the image capture mechanism itself is
    // broken and every case above is checking the wrong thing.
    ProfImage image;
    REQUIRE(makeValidImage(image));

    LoadedProfile profile;
    readImage(image, "robust_control", profile);

    REQUIRE(profile.readerLog.empty());
    REQUIRE(profile.blocksRead == 3);
    REQUIRE(easy_test::findBlockIndex(profile, "robust_root") >= 0);
}
