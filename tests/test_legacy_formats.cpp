/** Reading older versions of the .prof format.

Backward read compatibility is a stated project requirement: the reader must
parse files starting from v0.1.0 (MIN_COMPATIBLE_VERSION, reader.cpp:79). Three
separate header-parsing functions exist for that — readHeader_v1, readHeader_v2
and readHeader_v2_1 — and before these tests the first two were not covered at all.

Legacy files are assembled from the body of a current dump with a substituted
header: the body did not change over the version range [1.3.0, 2.1.0), so such a
file is indistinguishable from a genuine file of that era.
*/

#include <catch2/catch_test_macros.hpp>

#include "support/prof_image.h"
#include "support/prof_fixture.h"

#include <easy/profiler.h>

#include <string>
#include <vector>

using easy_test::LoadedProfile;
using easy_test::ProfImage;

namespace {

/** Take the reference dump the legacy files are assembled from. */
bool captureReference(ProfImage& image)
{
    return image.captureFrom([] {
        EASY_BLOCK("legacy_root");
        {
            EASY_BLOCK("legacy_branch");
            { EASY_BLOCK("legacy_leaf"); }
        }
        { EASY_BLOCK("legacy_sibling"); }
    });
}

void readLegacy(const std::vector<char>& bytes, const char* stem, LoadedProfile& out)
{
    easy_test::TempFile file(stem);
    easy_test::writeBytes(bytes, file.path());
    easy_test::loadProfile(file.path(), out);
}

} // namespace

TEST_CASE("Control: the reference dump reads as v2.1", "[legacy][format]")
{
    ProfImage image;
    REQUIRE(captureReference(image));

    // A current version means readHeader_v2_1 handled it.
    REQUIRE(image.version() >= easy_test::makeVersion(2, 1, 0));

    LoadedProfile profile;
    readLegacy(std::vector<char>(), "legacy_control_unused", profile);
    REQUIRE(profile.blocksRead == 0);

    easy_test::TempFile file("legacy_control");
    image.writeTo(file.path());

    LoadedProfile actual;
    easy_test::loadProfile(file.path(), actual);
    REQUIRE(actual.readerLog.empty());
    REQUIRE(actual.blocksRead == 4);
}

TEST_CASE("A version 2.0 file reads", "[legacy][format]")
{
    ProfImage image;
    REQUIRE(captureReference(image));

    const std::vector<char> legacy = easy_test::toLegacyV2(image);
    // The v2.0 header is shorter: no threads count, bookmarks or padding.
    REQUIRE(legacy.size() == image.size() - 8);

    LoadedProfile profile;
    readLegacy(legacy, "legacy_v20", profile);

    REQUIRE(profile.readerLog.empty());
    REQUIRE(profile.version == easy_test::makeVersion(2, 0, 0));
    REQUIRE(profile.blocksRead == 4);
}

TEST_CASE("Topology is preserved when reading version 2.0", "[legacy][format][tree]")
{
    ProfImage image;
    REQUIRE(captureReference(image));

    LoadedProfile profile;
    readLegacy(easy_test::toLegacyV2(image), "legacy_v20_tree", profile);

    REQUIRE(profile.readerLog.empty());

    const long root = easy_test::findBlockIndex(profile, "legacy_root");
    REQUIRE(root >= 0);

    const std::vector<std::string> children =
        easy_test::childNames(profile, static_cast<profiler::block_index_t>(root));
    REQUIRE(children.size() == 2);
    REQUIRE(children[0] == "legacy_branch");
    REQUIRE(children[1] == "legacy_sibling");
}

TEST_CASE("A version 1.3 file reads", "[legacy][format]")
{
    ProfImage image;
    REQUIRE(captureReference(image));

    // 1.3.0 is the first version with a 64-bit thread_id, so the body is compatible.
    const std::vector<char> legacy =
        easy_test::toLegacyV1(image, easy_test::makeVersion(1, 3, 0));

    LoadedProfile profile;
    readLegacy(legacy, "legacy_v13", profile);

    REQUIRE(profile.readerLog.empty());
    REQUIRE(profile.version == easy_test::makeVersion(1, 3, 0));
    REQUIRE(profile.blocksRead == 4);
}

TEST_CASE("Topology is preserved when reading version 1.3", "[legacy][format][tree]")
{
    ProfImage image;
    REQUIRE(captureReference(image));

    LoadedProfile profile;
    readLegacy(easy_test::toLegacyV1(image, easy_test::makeVersion(1, 3, 0)),
               "legacy_v13_tree", profile);

    REQUIRE(profile.readerLog.empty());

    const long branch = easy_test::findBlockIndex(profile, "legacy_branch");
    REQUIRE(branch >= 0);

    const std::vector<std::string> leaves =
        easy_test::childNames(profile, static_cast<profiler::block_index_t>(branch));
    REQUIRE(leaves.size() == 1);
    REQUIRE(leaves[0] == "legacy_leaf");
}

TEST_CASE("A version 1.9 file reads through the v1 layout", "[legacy][format]")
{
    // Anything below 2.0.0 is parsed by readHeader_v1.
    ProfImage image;
    REQUIRE(captureReference(image));

    LoadedProfile profile;
    readLegacy(easy_test::toLegacyV1(image, easy_test::makeVersion(1, 9, 9)),
               "legacy_v199", profile);

    REQUIRE(profile.readerLog.empty());
    REQUIRE(profile.blocksRead == 4);
}

TEST_CASE("Descriptors are read from legacy files", "[legacy][format]")
{
    ProfImage image;
    REQUIRE(captureReference(image));

    LoadedProfile fromV2;
    readLegacy(easy_test::toLegacyV2(image), "legacy_desc_v20", fromV2);
    REQUIRE(fromV2.readerLog.empty());
    REQUIRE(easy_test::findDescriptor(fromV2, profiler::BlockType::Block, "legacy_root") != nullptr);

    LoadedProfile fromV1;
    readLegacy(easy_test::toLegacyV1(image, easy_test::makeVersion(1, 3, 0)),
               "legacy_desc_v13", fromV1);
    REQUIRE(fromV1.readerLog.empty());
    REQUIRE(easy_test::findDescriptor(fromV1, profiler::BlockType::Block, "legacy_root") != nullptr);
}

TEST_CASE("A legacy file with zero blocks is rejected", "[legacy][format][robustness]")
{
    // readHeader_v1 checks blocks_count == 0 separately and reports it.
    ProfImage image;
    REQUIRE(captureReference(image));

    ProfImage broken;
    {
        easy_test::TempFile file("legacy_zero_blocks_src");
        image.writeTo(file.path());
        REQUIRE(broken.loadFrom(file.path()));
    }
    broken.setTotalBlocks(0);

    LoadedProfile profile;
    readLegacy(easy_test::toLegacyV1(broken, easy_test::makeVersion(1, 3, 0)),
               "legacy_zero_blocks", profile);

    REQUIRE(profile.blocksRead == 0);
    REQUIRE(profile.readerLog.find("Profiled blocks number == 0") != std::string::npos);
}

TEST_CASE("A legacy file with zero block memory is rejected", "[legacy][format][robustness]")
{
    ProfImage image;
    REQUIRE(captureReference(image));

    ProfImage broken;
    {
        easy_test::TempFile file("legacy_zero_mem_src");
        image.writeTo(file.path());
        REQUIRE(broken.loadFrom(file.path()));
    }
    broken.setUsedMemorySize(0);

    LoadedProfile profile;
    readLegacy(easy_test::toLegacyV1(broken, easy_test::makeVersion(1, 3, 0)),
               "legacy_zero_mem", profile);

    REQUIRE(profile.blocksRead == 0);
    REQUIRE(profile.readerLog.find("Wrong memory size == 0") != std::string::npos);
}

TEST_CASE("A legacy file with zero descriptors is rejected", "[legacy][format][robustness]")
{
    ProfImage image;
    REQUIRE(captureReference(image));

    ProfImage broken;
    {
        easy_test::TempFile file("legacy_zero_desc_src");
        image.writeTo(file.path());
        REQUIRE(broken.loadFrom(file.path()));
    }
    broken.setTotalDescriptors(0);

    LoadedProfile profile;
    readLegacy(easy_test::toLegacyV1(broken, easy_test::makeVersion(1, 3, 0)),
               "legacy_zero_desc", profile);

    REQUIRE(profile.blocksRead == 0);
    REQUIRE(profile.readerLog.find("Blocks description number == 0") != std::string::npos);
}

TEST_CASE("Version 2.0 with zero descriptor memory is rejected", "[legacy][format][robustness]")
{
    ProfImage image;
    REQUIRE(captureReference(image));

    ProfImage broken;
    {
        easy_test::TempFile file("legacy_v20_zero_desc_mem_src");
        image.writeTo(file.path());
        REQUIRE(broken.loadFrom(file.path()));
    }
    broken.setDescriptorsMemorySize(0);

    LoadedProfile profile;
    readLegacy(easy_test::toLegacyV2(broken), "legacy_v20_zero_desc_mem", profile);

    REQUIRE(profile.blocksRead == 0);
    REQUIRE_FALSE(profile.readerLog.empty());
}
