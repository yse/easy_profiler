/** Byte-for-byte compatibility fixtures for every supported header layout. */

#include <catch2/catch_test_macros.hpp>

#include "support/prof_fixture.h"
#include "support/prof_image.h"

#include <easy/profiler.h>
#include <easy/writer.h>

#include <cctype>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

namespace {

int hexDigit(char value)
{
    if (value >= '0' && value <= '9')
        return value - '0';
    if (value >= 'a' && value <= 'f')
        return value - 'a' + 10;
    if (value >= 'A' && value <= 'F')
        return value - 'A' + 10;
    return -1;
}

std::vector<char> loadHexFixture(const char* name)
{
    const std::string path = std::string(EASY_TEST_SOURCE_DIR) + "/golden/" + name;
    std::ifstream input(path.c_str());
    REQUIRE(input.good());

    std::vector<char> bytes;
    int high = -1;
    char value = 0;
    while (input.get(value))
    {
        if (std::isspace(static_cast<unsigned char>(value)))
            continue;
        const int digit = hexDigit(value);
        REQUIRE(digit >= 0);
        if (high < 0)
            high = digit;
        else
        {
            bytes.push_back(static_cast<char>((high << 4) | digit));
            high = -1;
        }
    }
    REQUIRE(high < 0);
    return bytes;
}

void checkGolden(const char* filename, uint32_t expectedVersion)
{
    const std::vector<char> bytes = loadHexFixture(filename);
    easy_test::TempFile file("golden_profile");
    easy_test::writeBytes(bytes, file.path());

    easy_test::LoadedProfile profile;
    easy_test::loadProfile(file.path(), profile);

    INFO("fixture: " << filename << ", reader log: " << profile.readerLog);
    REQUIRE(profile.readerLog.empty());
    REQUIRE(profile.version == expectedVersion);
    REQUIRE(profile.blocksRead == 1);
    REQUIRE(profile.threadedTrees.size() == 1);
    REQUIRE(profile.threadedTrees.begin()->second.thread_name == "main");
    REQUIRE(easy_test::findDescriptor(profile, profiler::BlockType::Block, "root") != nullptr);
    REQUIRE(easy_test::findBlockIndex(profile, "root") >= 0);
}

} // namespace

TEST_CASE("Golden profiles preserve every supported wire layout", "[golden][format][legacy]")
{
    checkGolden("v1_3.prof.hex", easy_test::makeVersion(1, 3, 0));
    checkGolden("v2_0.prof.hex", easy_test::makeVersion(2, 0, 0));
    checkGolden("v2_1.prof.hex", profiler::formatVersion());
}

TEST_CASE("A profile written by the real 2.1 writer still opens", "[golden][format][legacy]")
{
    // Unlike the fixtures above, this one was produced by building easy_profiler
    // v2.1.0 and running its writeTreesToStream() - the path behind the GUI's
    // "Save As". That writer declared descriptors_memory_size as the descriptor
    // payload sum PLUS the two-byte record prefixes, so a reader that demands
    // the payload sum exactly rejects every trace a 2.x GUI ever saved.
    //
    // Fixtures synthesized from a current dump cannot catch that: they carry
    // whatever the current writer emits. This one is a real artifact.
    const std::vector<char> bytes = loadHexFixture("v2_1_writer.prof.hex");

    easy_test::ProfImage image;
    easy_test::TempFile file("golden_v21_writer");
    easy_test::writeBytes(bytes, file.path());
    REQUIRE(image.loadFrom(file.path()));

    // Pin the property that makes this fixture worth keeping. If someone
    // regenerates it with a current build, this fails instead of silently
    // turning the test into a duplicate of the v2_1 case above.
    size_t offset = easy_test::ProfHeader::Size;
    uint64_t payloadSum = 0;
    for (uint32_t i = 0; i < image.totalDescriptors(); ++i)
    {
        const uint16_t size = image.read<uint16_t>(offset);
        payloadSum += size;
        offset += sizeof(uint16_t) + size;
    }
    REQUIRE(image.descriptorsMemorySize() ==
            payloadSum + static_cast<uint64_t>(image.totalDescriptors()) * sizeof(uint16_t));
    REQUIRE(image.descriptorsMemorySize() != payloadSum);

    easy_test::LoadedProfile profile;
    easy_test::loadProfile(file.path(), profile);

    INFO("reader log: " << profile.readerLog);
    REQUIRE(profile.readerLog.empty());
    REQUIRE(profile.version == easy_test::makeVersion(2, 1, 0));
    REQUIRE(profile.blocksRead == 1);
    REQUIRE(easy_test::findDescriptor(profile, profiler::BlockType::Block, "root") != nullptr);
    REQUIRE(easy_test::findBlockIndex(profile, "root") >= 0);
}

TEST_CASE("A profile re-saved six times by the 2.1 writer still opens", "[golden][format][legacy]")
{
    // The 2.1 reader allocated the declared descriptor size and the 2.1 writer
    // then declared that allocation plus the prefixes again, so every
    // load/save cycle through a 2.x GUI compounds the surplus. This file - 17
    // descriptors, one block, six cycles - declares 12 * descriptors_count
    // bytes the descriptor section does not hold, which with so small a block
    // section is more than the record prefixes and thread header could cover.
    const std::vector<char> bytes = loadHexFixture("v2_1_writer_resaved.prof.hex");

    easy_test::ProfImage image;
    easy_test::TempFile file("golden_v21_writer_resaved");
    easy_test::writeBytes(bytes, file.path());
    REQUIRE(image.loadFrom(file.path()));

    size_t offset = easy_test::ProfHeader::Size;
    uint64_t payloadSum = 0;
    for (uint32_t i = 0; i < image.totalDescriptors(); ++i)
    {
        const uint16_t size = image.read<uint16_t>(offset);
        payloadSum += size;
        offset += sizeof(uint16_t) + size;
    }
    REQUIRE(image.totalDescriptors() == 17);
    REQUIRE(image.totalBlocks() == 1);
    REQUIRE(image.descriptorsMemorySize() ==
            payloadSum + 6 * static_cast<uint64_t>(image.totalDescriptors()) * sizeof(uint16_t));

    easy_test::LoadedProfile profile;
    easy_test::loadProfile(file.path(), profile);

    INFO("reader log: " << profile.readerLog);
    REQUIRE(profile.readerLog.empty());
    REQUIRE(profile.version == easy_test::makeVersion(2, 1, 0));
    REQUIRE(profile.blocksRead == 1);
    REQUIRE(profile.descriptorsCount == 17);
    REQUIRE(easy_test::findBlockIndex(profile, "the_only_block") >= 0);

    // The current writer re-saves what was read, declares the payload sum and
    // reads its own output back.
    easy_test::TempFile resaved("golden_v21_writer_resaved_again");
    std::stringstream log;
    const profiler::blocks_t& blocks = profile.blocks;
    const profiler::block_index_t written = writeTreesToFile(
        resaved.c_str(), profile.serializedDescriptors, profile.descriptors, profile.descriptorsCount,
        profile.threadedTrees, profile.bookmarks,
        [&blocks](profiler::block_index_t index) -> const profiler::BlocksTree& { return blocks[index]; },
        profile.beginEndTime.beginTime, profile.beginEndTime.endTime, profile.pid, log);
    INFO("writer log: " << log.str());
    REQUIRE(written == 1);

    easy_test::ProfImage resavedImage;
    REQUIRE(resavedImage.loadFrom(resaved.path()));
    REQUIRE(resavedImage.descriptorsMemorySize() == payloadSum);

    easy_test::LoadedProfile reloaded;
    easy_test::loadProfile(resaved.path(), reloaded);
    INFO("reader log: " << reloaded.readerLog);
    REQUIRE(reloaded.readerLog.empty());
    REQUIRE(reloaded.blocksRead == 1);
    REQUIRE(easy_test::findBlockIndex(reloaded, "the_only_block") >= 0);
}
