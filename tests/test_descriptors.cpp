/** Block descriptor registration.

Descriptor deduplication is what makes the format compact: everything constant
for a call site (name, file, line, color, type, status) is stored once, and every
block instance carries only a 4-byte id. The rationale is recorded in the code
itself (profiler_public_types.h:84): "There are much more blocks than descriptors".

The key property: registering the same autogenUniqueId again must return the
existing descriptor rather than create a new one.

Note: the descriptor table is process-wide and never cleared, so each test uses
its own unique ids.
*/

#include <catch2/catch_test_macros.hpp>

#include "support/prof_fixture.h"

#include <easy/profiler.h>

#include <string>

namespace {

const profiler::BaseBlockDescriptor* registerBlock(const char* uniqueId,
                                                   const char* name,
                                                   profiler::color_t color = profiler::colors::Default,
                                                   profiler::EasyBlockStatus status = profiler::ON,
                                                   profiler::BlockType type = profiler::BlockType::Block,
                                                   int line = 100)
{
    return profiler::registerDescription(status, uniqueId, name, __FILE__, line, type, color, false);
}

} // namespace

TEST_CASE("registerDescription returns a valid descriptor", "[descriptors]")
{
    const profiler::BaseBlockDescriptor* descriptor =
        registerBlock("test_desc_basic", "basic_block", profiler::colors::Red, profiler::ON,
                      profiler::BlockType::Block, 4242);

    REQUIRE(descriptor != nullptr);
    REQUIRE(descriptor->line() == 4242);
    REQUIRE(descriptor->color() == profiler::colors::Red);
    REQUIRE(descriptor->type() == profiler::BlockType::Block);
    REQUIRE(descriptor->status() == profiler::ON);
}

TEST_CASE("Re-registering the same id does not create a second descriptor", "[descriptors]")
{
    const profiler::BaseBlockDescriptor* first =
        registerBlock("test_desc_dedup", "dedup_block", profiler::colors::Blue);

    const profiler::BaseBlockDescriptor* second =
        registerBlock("test_desc_dedup", "dedup_block", profiler::colors::Blue);

    REQUIRE(first != nullptr);
    REQUIRE(second != nullptr);

    // This is deduplication: same id, same descriptor, same block_id_t.
    REQUIRE(first == second);
    REQUIRE(first->id() == second->id());
}

TEST_CASE("Different ids yield different descriptors", "[descriptors]")
{
    const profiler::BaseBlockDescriptor* first = registerBlock("test_desc_a", "block_a");
    const profiler::BaseBlockDescriptor* second = registerBlock("test_desc_b", "block_b");

    REQUIRE(first != second);
    REQUIRE(first->id() != second->id());
}

TEST_CASE("Descriptor ids are handed out sequentially", "[descriptors]")
{
    const profiler::BaseBlockDescriptor* first = registerBlock("test_desc_seq_1", "seq_1");
    const profiler::BaseBlockDescriptor* second = registerBlock("test_desc_seq_2", "seq_2");
    const profiler::BaseBlockDescriptor* third = registerBlock("test_desc_seq_3", "seq_3");

    REQUIRE(second->id() == first->id() + 1);
    REQUIRE(third->id() == second->id() + 1);
}

TEST_CASE("The block type is preserved", "[descriptors]")
{
    const profiler::BaseBlockDescriptor* block =
        registerBlock("test_desc_type_block", "as_block", profiler::colors::Default,
                      profiler::ON, profiler::BlockType::Block);

    const profiler::BaseBlockDescriptor* event =
        registerBlock("test_desc_type_event", "as_event", profiler::colors::Default,
                      profiler::ON, profiler::BlockType::Event);

    const profiler::BaseBlockDescriptor* value =
        registerBlock("test_desc_type_value", "as_value", profiler::colors::Default,
                      profiler::ON, profiler::BlockType::Value);

    REQUIRE(block->type() == profiler::BlockType::Block);
    REQUIRE(event->type() == profiler::BlockType::Event);
    REQUIRE(value->type() == profiler::BlockType::Value);
}

TEST_CASE("The status is preserved on registration", "[descriptors]")
{
    REQUIRE(registerBlock("test_desc_st_on", "st_on", profiler::colors::Default,
                          profiler::ON)->status() == profiler::ON);

    REQUIRE(registerBlock("test_desc_st_off", "st_off", profiler::colors::Default,
                          profiler::OFF)->status() == profiler::OFF);

    REQUIRE(registerBlock("test_desc_st_force", "st_force", profiler::colors::Default,
                          profiler::FORCE_ON)->status() == profiler::FORCE_ON);

    REQUIRE(registerBlock("test_desc_st_nokids", "st_nokids", profiler::colors::Default,
                          profiler::ON_WITHOUT_CHILDREN)->status() == profiler::ON_WITHOUT_CHILDREN);
}

TEST_CASE("An arbitrary ARGB color survives registration intact", "[descriptors]")
{
    const profiler::color_t custom = 0xfff080aau; // the color from the README example
    const profiler::BaseBlockDescriptor* descriptor =
        registerBlock("test_desc_custom_color", "custom", custom);

    REQUIRE(descriptor->color() == custom);
}

TEST_CASE("Descriptor name and file reach the dump", "[descriptors][format]")
{
    easy_test::LoadedProfile profile;

    easy_test::capture(profile, [] {
        EASY_BLOCK("descriptor_named_block", profiler::colors::Magenta);
    });

    REQUIRE(profile.readerLog.empty());
    REQUIRE(profile.descriptorsCount > 0);

    const profiler::SerializedBlockDescriptor* descriptor =
        easy_test::findDescriptor(profile, profiler::BlockType::Block, "descriptor_named_block");

    REQUIRE(descriptor != nullptr);
    REQUIRE(descriptor->color() == profiler::colors::Magenta);
    REQUIRE(descriptor->line() > 0);

    // The file path sits right behind the name and must be non-empty.
    REQUIRE(descriptor->file() != nullptr);
    REQUIRE(std::string(descriptor->file()).find("test_descriptors.cpp") != std::string::npos);
}

TEST_CASE("Descriptors of different types stay distinguishable in the dump", "[descriptors][format]")
{
    easy_test::LoadedProfile profile;

    easy_test::capture(profile, [] {
        EASY_BLOCK("mixed_block_kind");
        EASY_EVENT("mixed_event_kind");
    });

    REQUIRE(profile.readerLog.empty());

    REQUIRE(easy_test::findDescriptor(profile, profiler::BlockType::Block, "mixed_block_kind") != nullptr);
    REQUIRE(easy_test::findDescriptor(profile, profiler::BlockType::Event, "mixed_event_kind") != nullptr);

    REQUIRE(easy_test::findDescriptor(profile, profiler::BlockType::Event, "mixed_block_kind") == nullptr);
    REQUIRE(easy_test::findDescriptor(profile, profiler::BlockType::Block, "mixed_event_kind") == nullptr);
}
