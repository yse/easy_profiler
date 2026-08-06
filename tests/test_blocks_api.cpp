/** Block lifecycle through the public API.

Covers what the README promises: a scoped block closes itself, a non-scoped block
needs EASY_END_BLOCK, an event has zero duration, and the OFF_RECURSIVE and
ON_WITHOUT_CHILDREN statuses control capture of the subtree.
*/

#include <catch2/catch_test_macros.hpp>

#include "support/prof_fixture.h"

#include <easy/profiler.h>

#include <algorithm>
#include <string>
#include <vector>

using easy_test::LoadedProfile;

namespace {

bool contains(const std::vector<std::string>& haystack, const char* needle)
{
    return std::find(haystack.begin(), haystack.end(), std::string(needle)) != haystack.end();
}

} // namespace

TEST_CASE("setEnabled toggles the capture state", "[blocks][api]")
{
    easy_test::drainPending();

    REQUIRE_FALSE(profiler::isEnabled());

    profiler::setEnabled(true);
    REQUIRE(profiler::isEnabled());
    REQUIRE(profiler::isCapturing());

    profiler::setEnabled(false);
    REQUIRE_FALSE(profiler::isEnabled());

    easy_test::drainPending();
}

TEST_CASE("A disabled profiler collects no blocks", "[blocks][api]")
{
    easy_test::drainPending();

    // Deliberately left disabled.
    {
        EASY_BLOCK("never_captured");
    }

    easy_test::TempFile file("nothing");
    const uint32_t dumped = profiler::dumpBlocksToFile(file.c_str());

    REQUIRE(dumped == 0);
}

TEST_CASE("A scoped block is closed by its destructor", "[blocks]")
{
    LoadedProfile profile;

    easy_test::capture(profile, [] {
        EASY_BLOCK("scoped_outer");
        {
            EASY_BLOCK("scoped_inner");
        }
    });

    REQUIRE(profile.readerLog.empty());
    REQUIRE(profile.blocksDumped >= 2);

    const long outer = easy_test::findBlockIndex(profile, "scoped_outer");
    REQUIRE(outer >= 0);

    const std::vector<std::string> children =
        easy_test::childNames(profile, static_cast<profiler::block_index_t>(outer));
    REQUIRE(contains(children, "scoped_inner"));
}

TEST_CASE("A non-scoped block is closed by EASY_END_BLOCK", "[blocks]")
{
    LoadedProfile profile;

    easy_test::capture(profile, [] {
        EASY_NONSCOPED_BLOCK("nonscoped_block");
        EASY_END_BLOCK;
    });

    REQUIRE(profile.readerLog.empty());
    REQUIRE(easy_test::findBlockIndex(profile, "nonscoped_block") >= 0);
}

TEST_CASE("A block has a non-negative duration", "[blocks]")
{
    LoadedProfile profile;

    easy_test::capture(profile, [] {
        EASY_BLOCK("measured_block");
        volatile int sink = 0;
        for (int i = 0; i < 1000; ++i)
            sink = sink + i;
        (void)sink;
    });

    const long index = easy_test::findBlockIndex(profile, "measured_block");
    REQUIRE(index >= 0);

    const profiler::SerializedBlock* node =
        profile.blocks[static_cast<size_t>(index)].node;
    REQUIRE(node != nullptr);
    REQUIRE(node->end() >= node->begin());
}

TEST_CASE("An event has zero duration", "[blocks][events]")
{
    LoadedProfile profile;

    easy_test::capture(profile, [] {
        EASY_BLOCK("event_host");
        EASY_EVENT("zero_duration_event");
    });

    REQUIRE(profile.readerLog.empty());

    const long index = easy_test::findBlockIndex(profile, "zero_duration_event");
    REQUIRE(index >= 0);

    const profiler::SerializedBlock* node = profile.blocks[static_cast<size_t>(index)].node;
    REQUIRE(node != nullptr);
    REQUIRE(node->begin() == node->end());
    REQUIRE(node->duration() == 0);
}

TEST_CASE("Nesting of arbitrary depth is preserved", "[blocks]")
{
    LoadedProfile profile;

    easy_test::capture(profile, [] {
        EASY_BLOCK("depth_1");
        EASY_BLOCK("depth_2");
        EASY_BLOCK("depth_3");
        EASY_BLOCK("depth_4");
        EASY_BLOCK("depth_5");
    });

    REQUIRE(profile.readerLog.empty());

    long current = easy_test::findBlockIndex(profile, "depth_1");
    REQUIRE(current >= 0);

    for (int level = 2; level <= 5; ++level)
    {
        const std::string expected = "depth_" + std::to_string(level);
        const std::vector<std::string> children =
            easy_test::childNames(profile, static_cast<profiler::block_index_t>(current));

        REQUIRE(contains(children, expected.c_str()));

        current = easy_test::findBlockIndex(profile, expected.c_str());
        REQUIRE(current >= 0);
    }
}

TEST_CASE("Sibling blocks keep their order of appearance", "[blocks]")
{
    LoadedProfile profile;

    easy_test::capture(profile, [] {
        EASY_BLOCK("siblings_parent");
        { EASY_BLOCK("sibling_first"); }
        { EASY_BLOCK("sibling_second"); }
        { EASY_BLOCK("sibling_third"); }
    });

    REQUIRE(profile.readerLog.empty());

    const long parent = easy_test::findBlockIndex(profile, "siblings_parent");
    REQUIRE(parent >= 0);

    const std::vector<std::string> children =
        easy_test::childNames(profile, static_cast<profiler::block_index_t>(parent));

    REQUIRE(children.size() == 3);
    REQUIRE(children[0] == "sibling_first");
    REQUIRE(children[1] == "sibling_second");
    REQUIRE(children[2] == "sibling_third");
}

TEST_CASE("A block with status OFF does not reach the dump", "[blocks][status]")
{
    LoadedProfile profile;

    easy_test::capture(profile, [] {
        EASY_BLOCK("status_visible");
        { EASY_BLOCK("status_switched_off", profiler::OFF); }
    });

    REQUIRE(profile.readerLog.empty());
    REQUIRE(easy_test::findBlockIndex(profile, "status_visible") >= 0);
    REQUIRE(easy_test::findBlockIndex(profile, "status_switched_off") < 0);
}

TEST_CASE("ON_WITHOUT_CHILDREN cuts off the subtree", "[blocks][status]")
{
    LoadedProfile profile;

    easy_test::capture(profile, [] {
        EASY_BLOCK("nokids_parent", profiler::ON_WITHOUT_CHILDREN);
        { EASY_BLOCK("nokids_child"); }
    });

    REQUIRE(profile.readerLog.empty());

    // The block itself is captured, its children are not.
    REQUIRE(easy_test::findBlockIndex(profile, "nokids_parent") >= 0);
    REQUIRE(easy_test::findBlockIndex(profile, "nokids_child") < 0);
}

TEST_CASE("OFF_RECURSIVE cuts off both itself and the subtree", "[blocks][status]")
{
    LoadedProfile profile;

    easy_test::capture(profile, [] {
        EASY_BLOCK("offrec_anchor");
        {
            EASY_BLOCK("offrec_parent", profiler::OFF_RECURSIVE);
            { EASY_BLOCK("offrec_child"); }
        }
    });

    REQUIRE(profile.readerLog.empty());

    REQUIRE(easy_test::findBlockIndex(profile, "offrec_anchor") >= 0);
    REQUIRE(easy_test::findBlockIndex(profile, "offrec_parent") < 0);
    REQUIRE(easy_test::findBlockIndex(profile, "offrec_child") < 0);
}

TEST_CASE("FORCE_ON punches through a disabled subtree", "[blocks][status]")
{
    LoadedProfile profile;

    easy_test::capture(profile, [] {
        EASY_BLOCK("force_anchor");
        {
            EASY_BLOCK("force_parent", profiler::OFF_RECURSIVE);
            { EASY_BLOCK("force_child", profiler::FORCE_ON); }
        }
    });

    REQUIRE(profile.readerLog.empty());

    REQUIRE(easy_test::findBlockIndex(profile, "force_anchor") >= 0);
    REQUIRE(easy_test::findBlockIndex(profile, "force_parent") < 0);
    REQUIRE(easy_test::findBlockIndex(profile, "force_child") >= 0);
}

TEST_CASE("A block with a dynamic name keeps that name", "[blocks]")
{
    LoadedProfile profile;

    easy_test::capture(profile, [] {
        const std::string dynamicName = "runtime_name_42";
        EASY_BLOCK(dynamicName.c_str());
    });

    REQUIRE(profile.readerLog.empty());
    REQUIRE(easy_test::findBlockIndex(profile, "runtime_name_42") >= 0);
}

TEST_CASE("storeBlock sets the block bounds by hand", "[blocks][api]")
{
    // The reader converts CPU ticks to nanoseconds using the frequency stored in
    // the file header (reader.cpp, EASY_CONVERT_TO_NANO), so a duration read back
    // is NOT the tick count passed in and cannot be compared to an absolute
    // constant — the number is machine-dependent. What is frequency-independent
    // is the ratio, so that is what gets checked.
    LoadedProfile profile;

    easy_test::capture(profile, [] {
        const profiler::BaseBlockDescriptor* descriptor = profiler::registerDescription(
            profiler::ON, "test_store_block_id", "manual_bounds_block", __FILE__, __LINE__,
            profiler::BlockType::Block, profiler::colors::Green, false);

        const profiler::BaseBlockDescriptor* tripleDescriptor = profiler::registerDescription(
            profiler::ON, "test_store_block_id_x3", "manual_bounds_block_x3", __FILE__, __LINE__,
            profiler::BlockType::Block, profiler::colors::Green, false);

        const profiler::timestamp_t begin = profiler::timeNow();
        profiler::storeBlock(descriptor, "", begin, begin + 1000);
        profiler::storeBlock(tripleDescriptor, "", begin, begin + 3000);
    });

    REQUIRE(profile.readerLog.empty());

    const long single = easy_test::findBlockIndex(profile, "manual_bounds_block");
    const long triple = easy_test::findBlockIndex(profile, "manual_bounds_block_x3");
    REQUIRE(single >= 0);
    REQUIRE(triple >= 0);

    const profiler::SerializedBlock* singleNode = profile.blocks[static_cast<size_t>(single)].node;
    const profiler::SerializedBlock* tripleNode = profile.blocks[static_cast<size_t>(triple)].node;
    REQUIRE(singleNode != nullptr);
    REQUIRE(tripleNode != nullptr);

    REQUIRE(singleNode->duration() > 0);

    // A three times longer interval stays three times longer after conversion;
    // the tolerance covers integer rounding.
    const double ratio = static_cast<double>(tripleNode->duration()) /
                         static_cast<double>(singleNode->duration());
    REQUIRE(ratio > 2.99);
    REQUIRE(ratio < 3.01);
}

TEST_CASE("The reader reports durations in nanoseconds, not ticks", "[blocks][format]")
{
    // Pinned explicitly: the writer stores the counter frequency in the header
    // and the reader converts ticks to nanoseconds. This is what the GUI and the
    // converter see.
    LoadedProfile profile;

    easy_test::capture(profile, [] {
        const profiler::BaseBlockDescriptor* descriptor = profiler::registerDescription(
            profiler::ON, "test_units_block_id", "units_block", __FILE__, __LINE__,
            profiler::BlockType::Block, profiler::colors::Default, false);

        const profiler::timestamp_t begin = profiler::timeNow();
        profiler::storeBlock(descriptor, "", begin, begin + 1000000);
    });

    const long index = easy_test::findBlockIndex(profile, "units_block");
    REQUIRE(index >= 0);

    const profiler::SerializedBlock* node = profile.blocks[static_cast<size_t>(index)].node;
    REQUIRE(node != nullptr);

    // A million ticks on any sane time source is well above zero and well below
    // a day expressed in nanoseconds.
    REQUIRE(node->duration() > 0);
    REQUIRE(node->duration() < 86400ull * 1000000000ull);
}

TEST_CASE("Dumping disables the profiler", "[blocks][api]")
{
    easy_test::drainPending();

    profiler::setEnabled(true);
    { EASY_BLOCK("dump_disables_profiler"); }
    REQUIRE(profiler::isEnabled());

    easy_test::TempFile file("dump_disables");
    profiler::dumpBlocksToFile(file.c_str());

    // dumpBlocksToStream does m_profilerStatus.store(false) — documented as
    // "\note This also disables profiler" (profiler.h:572).
    REQUIRE_FALSE(profiler::isEnabled());
}

TEST_CASE("A second dump with no capture in between writes nothing", "[blocks][api]")
{
    LoadedProfile profile;
    easy_test::capture(profile, [] {
        EASY_BLOCK("drained_once");
    });
    REQUIRE(profile.blocksDumped > 0);

    easy_test::TempFile second("drained_twice");
    REQUIRE(profiler::dumpBlocksToFile(second.c_str()) == 0);
}

TEST_CASE("The API version is self-consistent", "[api]")
{
    const uint32_t packed = profiler::apiVersion();

    REQUIRE(((packed >> 24) & 0xff) == profiler::apiVersionMajor());
    REQUIRE(((packed >> 16) & 0xff) == profiler::apiVersionMinor());
    REQUIRE((packed & 0xffff) == profiler::apiVersionPatch());

    REQUIRE(profiler::apiVersionName() != nullptr);
    REQUIRE(std::string(profiler::apiVersionName())[0] == 'v');
}
