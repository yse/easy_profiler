/** End-to-end round-trip: write a .prof and read it back.

Replaces the former hand-rolled tests/profiler_roundtrip.cpp, carrying its
coverage over to Catch2 and extending it with header checks.
*/

#include <catch2/catch_test_macros.hpp>

#include "support/prof_image.h"
#include "support/prof_fixture.h"

#include <easy/arbitrary_value.h>
#include <easy/profiler.h>

#include <cstring>
#include <string>

using easy_test::LoadedProfile;

namespace {

void mixedWorkload()
{
    EASY_FUNCTION(profiler::colors::Green);
    EASY_BLOCK("rt_outer", profiler::colors::Blue);
    EASY_EVENT("rt_marker", profiler::colors::Red);
    EASY_VALUE("rt_answer", 42);

    const int coordinates[] = {1, 2, 3};
    EASY_ARRAY("rt_coords", coordinates, 3);
    EASY_TEXT("rt_message", "hello easy_profiler");

    {
        EASY_BLOCK("rt_inner", profiler::colors::Cyan);
    }
}

void functionNamedBlock()
{
    EASY_FUNCTION();
}

} // namespace

TEST_CASE("A mixed workload survives a round-trip", "[roundtrip][format]")
{
    LoadedProfile profile;

    easy_test::capture(profile, [] {
        EASY_MAIN_THREAD;
        mixedWorkload();
    });

    REQUIRE(profile.readerLog.empty());
    REQUIRE(profile.blocksDumped > 0);
    REQUIRE(profile.blocksRead == profile.blocksDumped);
    REQUIRE_FALSE(profile.threadedTrees.empty());
}

TEST_CASE("The header of a loaded profile is populated", "[roundtrip][format]")
{
    LoadedProfile profile;

    easy_test::capture(profile, [] {
        EASY_BLOCK("header_block");
    });

    REQUIRE(profile.readerLog.empty());
    REQUIRE(profile.version != 0);
    REQUIRE(profile.pid != 0);
    REQUIRE(profile.descriptorsCount > 0);
    REQUIRE_FALSE(profile.descriptors.empty());
    REQUIRE(profile.beginEndTime.endTime >= profile.beginEndTime.beginTime);
}

TEST_CASE("The version in the file matches the wire-format version", "[roundtrip][format]")
{
    LoadedProfile profile;

    easy_test::capture(profile, [] {
        EASY_BLOCK("version_block");
    });

    REQUIRE(profile.version == profiler::formatVersion());
    REQUIRE(profile.version == easy_test::makeVersion(2, 1, 0));
    REQUIRE(profiler::apiVersion() == easy_test::makeVersion(3, 0, 0));
}

TEST_CASE("Block and event descriptors make it through", "[roundtrip][format]")
{
    LoadedProfile profile;

    easy_test::capture(profile, [] {
        mixedWorkload();
    });

    REQUIRE(easy_test::findDescriptor(profile, profiler::BlockType::Block, "rt_outer") != nullptr);
    REQUIRE(easy_test::findDescriptor(profile, profiler::BlockType::Block, "rt_inner") != nullptr);
    REQUIRE(easy_test::findDescriptor(profile, profiler::BlockType::Event, "rt_marker") != nullptr);
}

TEST_CASE("Values make it through with their types", "[roundtrip][values]")
{
    LoadedProfile profile;

    easy_test::capture(profile, [] {
        mixedWorkload();
    });

    const profiler::ArbitraryValue* answer = easy_test::findValue(profile, "rt_answer");
    REQUIRE(answer != nullptr);
    REQUIRE(answer->toValue<int>() != nullptr);
    REQUIRE(answer->toValue<int>()->value() == 42);

    const profiler::ArbitraryValue* coordinates = easy_test::findValue(profile, "rt_coords");
    REQUIRE(coordinates != nullptr);
    REQUIRE(coordinates->toArray<int>() != nullptr);
    REQUIRE(coordinates->toArray<int>()->size() == 3);
    REQUIRE(coordinates->toArray<int>()->at(0) == 1);
    REQUIRE(coordinates->toArray<int>()->at(2) == 3);

    const profiler::ArbitraryValue* message = easy_test::findValue(profile, "rt_message");
    REQUIRE(message != nullptr);
    REQUIRE(message->type() == profiler::DataType::String);
    REQUIRE(std::strcmp(message->data(), "hello easy_profiler") == 0);
}

TEST_CASE("EASY_FUNCTION picks up the function name", "[roundtrip]")
{
    LoadedProfile profile;

    easy_test::capture(profile, [] {
        mixedWorkload();
    });

    const long index = easy_test::findBlockIndex(profile, "mixedWorkload");
    REQUIRE(index >= 0);
    const profiler::SerializedBlock* node = profile.blocks[static_cast<size_t>(index)].node;
    REQUIRE(node != nullptr);
    const profiler::SerializedBlockDescriptor* descriptor =
        easy_test::findDescriptor(profile, profiler::BlockType::Block, "mixedWorkload");

    // Where the name lands is decided by the compiler, not by the library.
    // NameSwitch (profiler_aux.h:206) stores a name whose decltype is a
    // reference in the descriptor, as for a string literal, and any other name
    // in every block record. EASY_FUNC_NAME is __func__
    // (easy_compiler_support.h:210), or __FUNCTION__ on MSVC (:85). Measured:
    // clang 17 and 18 give const char[N] for __func__, GCC 13 gives
    // const char(&)[N]; MSVC's __FUNCTION__ is a string literal. So an
    // EASY_FUNCTION block costs 21 + strlen(function name) bytes under clang and
    // the same 21 bytes as EASY_BLOCK("literal") under GCC and MSVC.
    const uint64_t count = 100;
    easy_test::ProfImage image;
    REQUIRE(image.captureFrom([=] {
        for (uint64_t i = 0; i < count; ++i)
            functionNamedBlock();
    }));

#if defined(__clang__)
    REQUIRE(descriptor == nullptr);
    REQUIRE(std::strcmp(node->name(), "mixedWorkload") == 0);
    REQUIRE(image.usedMemorySize() == count * (21 + std::strlen("functionNamedBlock")));
#else
    REQUIRE(descriptor != nullptr);
    REQUIRE(*node->name() == '\0');
    REQUIRE(image.usedMemorySize() == count * 21);
#endif
}

TEST_CASE("A literal name is stored in the descriptor, not in the block", "[roundtrip][format]")
{
    // The flip side of the previous test, and the basis of the format's economy:
    // a compile-time name is not duplicated into every block record.
    LoadedProfile profile;

    easy_test::capture(profile, [] {
        EASY_BLOCK("literal_named_block");
    });

    REQUIRE(easy_test::findDescriptor(profile, profiler::BlockType::Block,
                                      "literal_named_block") != nullptr);

    const long index = easy_test::findBlockIndex(profile, "literal_named_block");
    REQUIRE(index >= 0);

    // The record's runtime name is empty; the name comes from the descriptor.
    const profiler::SerializedBlock* node = profile.blocks[static_cast<size_t>(index)].node;
    REQUIRE(node != nullptr);
    REQUIRE(node->name() != nullptr);
    REQUIRE(*node->name() == '\0');
}

TEST_CASE("The thread name is preserved", "[roundtrip][threads]")
{
    LoadedProfile profile;

    easy_test::capture(profile, [] {
        EASY_THREAD("roundtrip_named_thread");
        EASY_BLOCK("thread_named_block");
    });

    REQUIRE(profile.readerLog.empty());
    REQUIRE(profile.threadedTrees.size() >= 1);

    bool foundNamed = false;
    for (const auto& entry : profile.threadedTrees)
    {
        if (entry.second.thread_name == "roundtrip_named_thread")
            foundNamed = true;
    }

    // A thread is named once per process: if an earlier test already named this
    // thread, the second name does not stick. Hence the loose assertion — what
    // matters is that a name exists at all.
    const profiler::BlocksTreeRoot& first = profile.threadedTrees.begin()->second;
    REQUIRE((foundNamed || first.got_name()));
}

TEST_CASE("A second round-trip does not accumulate blocks", "[roundtrip]")
{
    LoadedProfile firstProfile;
    easy_test::capture(firstProfile, [] {
        EASY_BLOCK("rt_first_batch");
        EASY_BLOCK("rt_first_batch_nested");
    });

    LoadedProfile secondProfile;
    easy_test::capture(secondProfile, [] {
        EASY_BLOCK("rt_second_batch");
    });

    REQUIRE(firstProfile.readerLog.empty());
    REQUIRE(secondProfile.readerLog.empty());

    REQUIRE(easy_test::findBlockIndex(secondProfile, "rt_first_batch") < 0);
    REQUIRE(easy_test::findBlockIndex(secondProfile, "rt_second_batch") >= 0);
}

TEST_CASE("An empty capture returns 0 yet still creates the file", "[roundtrip]")
{
    // Actual behaviour, pinned: dumpBlocksToFile opens the ofstream BEFORE it
    // turns out there is nothing to write (in profile_manager.cpp the
    // "std::ofstream outputFile(...)" precedes the dumpBlocksToStream call), so
    // a file is left on disk while 0 is returned.
    //
    // Callers must not treat the file's existence as success — the returned
    // block count is the signal.
    easy_test::drainPending();

    easy_test::TempFile file("rt_empty");

    profiler::setEnabled(true);
    profiler::setEnabled(false);

    const uint32_t dumped = profiler::dumpBlocksToFile(file.c_str());

    REQUIRE(dumped == 0);
    REQUIRE(file.exists());

    // The file is not even empty: the header and the whole descriptor table are
    // already written by the time the absence of blocks is discovered. The
    // descriptor table is process-wide and holds everything registered since
    // process start, so such an "empty" dump grows over the application's life.
    REQUIRE(file.sizeBytes() >= easy_test::ProfHeader::Size);

    easy_test::ProfImage image;
    REQUIRE(image.loadFrom(file.path()));
    REQUIRE(image.signature() == 0x45617379u);
    REQUIRE(image.totalBlocks() == 0);
    REQUIRE(image.usedMemorySize() == 0);
}

TEST_CASE("A large trace is read back in full", "[roundtrip]")
{
    LoadedProfile profile;

    const int count = 5000;
    easy_test::capture(profile, [=] {
        EASY_BLOCK("bulk_root");
        for (int i = 0; i < count; ++i)
        {
            EASY_BLOCK("bulk_child");
        }
    });

    REQUIRE(profile.readerLog.empty());
    REQUIRE(profile.blocksDumped == static_cast<uint32_t>(count + 1));
    REQUIRE(profile.blocksRead == profile.blocksDumped);

    const long root = easy_test::findBlockIndex(profile, "bulk_root");
    REQUIRE(root >= 0);
    REQUIRE(profile.blocks[static_cast<size_t>(root)].children.size() == static_cast<size_t>(count));
}

TEST_CASE("The number of blocks read matches the header", "[roundtrip][format]")
{
    easy_test::TempFile file("rt_counts");

    easy_test::drainPending();
    profiler::setEnabled(true);
    {
        EASY_BLOCK("count_root");
        { EASY_BLOCK("count_a"); }
        { EASY_BLOCK("count_b"); }
    }
    profiler::setEnabled(false);

    const uint32_t dumped = profiler::dumpBlocksToFile(file.c_str());
    REQUIRE(dumped == 3);

    easy_test::ProfImage image;
    REQUIRE(image.loadFrom(file.path()));
    REQUIRE(image.totalBlocks() == dumped);

    LoadedProfile profile;
    easy_test::loadProfile(file.path(), profile);
    REQUIRE(profile.readerLog.empty());
    REQUIRE(profile.blocksRead == dumped);
}
