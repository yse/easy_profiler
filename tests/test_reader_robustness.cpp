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

#include <easy/profiler.h>

#include <fstream>
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

void readImage(const ProfImage& image, const char* stem, LoadedProfile& out)
{
    easy_test::TempFile file(stem);
    image.writeTo(file.path());
    easy_test::loadProfile(file.path(), out);
}

} // namespace

TEST_CASE("A missing file", "[robustness]")
{
    LoadedProfile profile;
    easy_test::loadProfile("definitely_missing_file_12345.prof", profile);

    REQUIRE(profile.blocksRead == 0);
    REQUIRE_FALSE(profile.readerLog.empty());
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

TEST_CASE("More threads claimed than present", "[robustness]")
{
    ProfImage image;
    REQUIRE(makeValidImage(image));

    image.setThreadsCount(image.threadsCount() + 1000u);

    LoadedProfile profile;
    readImage(image, "robust_thread_count", profile);

    REQUIRE(profile.threadedTrees.size() <= image.threadsCount());
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

TEST_CASE("A valid image reads — control for the corruption harness", "[robustness]")
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
