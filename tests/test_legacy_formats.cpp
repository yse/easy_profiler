/** Reading older versions of the .prof format.

Backward read compatibility is a stated project requirement: the reader must
parse files starting from v0.1.0 (MIN_COMPATIBLE_VERSION, reader.cpp:79). Three
separate header-parsing functions exist for that - readHeader_v1, readHeader_v2
and readHeader_v2_1 - and before these tests the first two were not covered at all.

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
    REQUIRE(image.version() == profiler::formatVersion());
    REQUIRE(image.version() == easy_test::makeVersion(2, 1, 0));

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

TEST_CASE("Patch releases in the current format line remain compatible", "[legacy][format]")
{
    ProfImage image;
    REQUIRE(captureReference(image));

    // The patch component is a product version, not a format discriminator.
    // The v2.1 header/body layout remains identical across patch releases.
    image.setVersion(easy_test::makeVersion(2, 1, 0xffff));

    easy_test::TempFile file("legacy_v21_future_patch");
    image.writeTo(file.path());

    LoadedProfile profile;
    easy_test::loadProfile(file.path(), profile);

    REQUIRE(profile.readerLog.empty());
    REQUIRE(profile.blocksRead == 4);
}

TEST_CASE("A profile saved by the 2.1 writer still reads", "[legacy][format]")
{
    // writeTreesToStream() in 2.1 and earlier declared descriptors_memory_size
    // as the payload sum plus the two-byte record prefixes. Profiles saved from
    // an older GUI carry that spelling and must stay readable.
    ProfImage image;
    REQUIRE(captureReference(image));

    const uint64_t payloadSum = image.descriptorsMemorySize();
    const uint64_t legacy =
        payloadSum + static_cast<uint64_t>(image.totalDescriptors()) * sizeof(uint16_t);
    REQUIRE(legacy > payloadSum);
    image.setDescriptorsMemorySize(legacy);

    easy_test::TempFile file("legacy_writer_descriptor_memory");
    image.writeTo(file.path());

    LoadedProfile profile;
    easy_test::loadProfile(file.path(), profile);

    INFO("reader log: " << profile.readerLog);
    REQUIRE(profile.readerLog.empty());
    REQUIRE(profile.blocksRead == 4);
    REQUIRE(easy_test::findDescriptor(profile, profiler::BlockType::Block, "legacy_root") != nullptr);
}

TEST_CASE("Repeated saves by a 2.x GUI keep the profile readable", "[legacy][format]")
{
    // The 2.x reader allocated the declared descriptor buffer, and the 2.x
    // writer then declared that buffer size plus the record prefixes again. The
    // surplus therefore grew by descriptors_count * 2 on every round trip, so a
    // trace saved three times is not off by two bytes but by six per descriptor.
    ProfImage image;
    REQUIRE(captureReference(image));

    const uint64_t payloadSum = image.descriptorsMemorySize();
    const uint64_t prefixBytes =
        static_cast<uint64_t>(image.totalDescriptors()) * sizeof(uint16_t);
    REQUIRE(prefixBytes > 0);

    for (uint64_t saves = 1; saves <= 4; ++saves)
    {
        image.setDescriptorsMemorySize(payloadSum + prefixBytes * saves);

        easy_test::TempFile file("legacy_repeated_save");
        image.writeTo(file.path());

        LoadedProfile profile;
        easy_test::loadProfile(file.path(), profile);

        INFO("re-saves: " << saves << ", reader log: " << profile.readerLog);
        REQUIRE(profile.readerLog.empty());
        REQUIRE(profile.blocksRead == 4);
    }
}

TEST_CASE("Repeated 2.x saves of a profile with many descriptors and one block stay readable",
          "[legacy][format]")
{
    // The four-block reference above has enough record prefixes and thread
    // header for several saves' worth of surplus to hide in the stream size.
    // With one block and 16 extra descriptors the second save already declares
    // more descriptor bytes than the whole file is short of, so this is the
    // shape that catches a stream-size check taking descriptors_memory_size
    // for physical bytes.
    ProfImage image;
    REQUIRE(image.captureFrom([] {
        for (int i = 0; i < 16; ++i)
        {
            const std::string id = "legacy_unused_" + std::to_string(i);
            profiler::registerDescription(profiler::ON, id.c_str(), id.c_str(), __FILE__, __LINE__,
                                          profiler::BlockType::Block, profiler::colors::Default, true);
        }
        EASY_BLOCK("legacy_only_block");
    }));
    REQUIRE(image.totalBlocks() == 1);
    REQUIRE(image.totalDescriptors() >= 17);

    const uint64_t payloadSum = image.descriptorsMemorySize();
    const uint64_t prefixBytes =
        static_cast<uint64_t>(image.totalDescriptors()) * sizeof(uint16_t);

    for (uint64_t saves = 1; saves <= 6; ++saves)
    {
        image.setDescriptorsMemorySize(payloadSum + prefixBytes * saves);

        easy_test::TempFile file("legacy_repeated_save_one_block");
        image.writeTo(file.path());

        LoadedProfile profile;
        easy_test::loadProfile(file.path(), profile);

        INFO("re-saves: " << saves << ", reader log: " << profile.readerLog);
        REQUIRE(profile.readerLog.empty());
        REQUIRE(profile.blocksRead == 1);
        REQUIRE(profile.descriptorsCount == image.totalDescriptors());
        REQUIRE(easy_test::findBlockIndex(profile, "legacy_only_block") >= 0);
    }
}

TEST_CASE("A descriptor memory size that matches neither spelling is rejected", "[legacy][format][robustness]")
{
    ProfImage image;
    REQUIRE(captureReference(image));
    image.setDescriptorsMemorySize(image.descriptorsMemorySize() + 1);

    easy_test::TempFile file("legacy_writer_descriptor_memory_bad");
    image.writeTo(file.path());

    LoadedProfile profile;
    easy_test::loadProfile(file.path(), profile);

    REQUIRE(profile.blocksRead == 0);
    REQUIRE(profile.readerLog.find("memory size") != std::string::npos);
}

TEST_CASE("A descriptor surplus is accepted however many re-saves it represents",
          "[legacy][format][robustness]")
{
    // Each old-GUI save added descriptors_count * 2 to the declared size, so a multiple of
    // that is the shape a re-saved file carries and any multiple has to read. Capping the
    // count would only reject valid files: the surplus bytes are allocated but never
    // addressed, and the allocation itself is clamped to the stream size.
    ProfImage image;
    REQUIRE(captureReference(image));

    const uint64_t payloadSum = image.descriptorsMemorySize();
    const uint64_t prefixBytes =
        static_cast<uint64_t>(image.totalDescriptors()) * sizeof(uint16_t);
    REQUIRE(prefixBytes > 0);

    const struct { uint64_t surplus; bool accepted; } cases[] = {
        { prefixBytes, true },
        { prefixBytes * 1024, true },
        { prefixBytes * 1025, true },
        { prefixBytes * (static_cast<uint64_t>(1) << 35), true },
        { prefixBytes * 6 + 1, false },
    };

    for (const auto& probe : cases)
    {
        image.setDescriptorsMemorySize(payloadSum + probe.surplus);

        easy_test::TempFile file("legacy_surplus_bound");
        image.writeTo(file.path());

        LoadedProfile profile;
        easy_test::loadProfile(file.path(), profile);

        INFO("surplus: " << probe.surplus << ", reader log: " << profile.readerLog);
        REQUIRE(profile.readerLog.empty() == probe.accepted);
        REQUIRE((profile.blocksRead == 4) == probe.accepted);
    }
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
