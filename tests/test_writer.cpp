/** Re-saving a loaded tree - writer.cpp.

This path is not involved in an ordinary dump from an application: dumpBlocksToFile
writes straight out of the chunks. writeTreesToFile works on an ALREADY LOADED
tree and exists to save a snapshot of a selected region from the GUI, as well as
to slice by time.

Before these tests writer.cpp was linked into no test at all and had zero
coverage, despite writing the same format the reader reads.
*/

#include <catch2/catch_test_macros.hpp>

#include "support/prof_image.h"
#include "support/prof_fixture.h"

#include <easy/arbitrary_value.h>
#include <easy/profiler.h>
#include <easy/reader.h>
#include <easy/writer.h>

#include "current_thread.h"

#include <fstream>
#include <sstream>
#include <string>
#include <cstring>
#include <thread>
#include <vector>

#ifndef _WIN32
# include <unistd.h>
#endif

using easy_test::LoadedProfile;

namespace {

/** Re-save a loaded profile into a file; returns the number of blocks written. */
profiler::block_index_t resave(const LoadedProfile& source,
                               const std::string& path,
                               std::string& log,
                               profiler::timestamp_t beginTime,
                               profiler::timestamp_t endTime)
{
    std::stringstream logStream;

    const profiler::blocks_t& blocks = source.blocks;
    profiler::block_getter_fn getter =
        [&blocks](profiler::block_index_t index) -> const profiler::BlocksTree&
        {
            return blocks[index];
        };

    const profiler::block_index_t written = writeTreesToFile(
        path.c_str(), source.serializedDescriptors, source.descriptors,
        static_cast<profiler::block_id_t>(source.descriptorsCount), source.threadedTrees,
        source.bookmarks, getter, beginTime, endTime, source.pid, logStream);

    log = logStream.str();
    return written;
}

void captureSample(LoadedProfile& out)
{
    easy_test::capture(out, [] {
        EASY_BLOCK("writer_root");
        {
            EASY_BLOCK("writer_branch");
            { EASY_BLOCK("writer_leaf_a"); }
            { EASY_BLOCK("writer_leaf_b"); }
        }
        { EASY_BLOCK("writer_sibling"); }
    });
}

#ifndef _WIN32
void captureContextSwitchOnly(LoadedProfile& out)
{
    easy_test::drainPending();
    profiler::registerDescription(profiler::ON, "writer_cswitch_descriptor_id",
                                  "writer_cswitch_descriptor", __FILE__, __LINE__,
                                  profiler::BlockType::Block,
                                  profiler::colors::Default, false);

    easy_test::TempFile switchLog("writer_cswitch", ".log");
    const std::string originalPath = profiler::getContextSwitchLogFilename();
    const bool originalTracing = profiler::isEventTracingEnabled();

    profiler::setContextSwitchLogFilename(switchLog.c_str());
    profiler::setEventTracingEnabled(true);
    profiler::setEnabled(true);
    EASY_THREAD("writer_cswitch_thread");

    const profiler::timestamp_t base = profiler::timeNow();
    const profiler::thread_id_t thread = getCurrentThreadId();
    const long process = static_cast<long>(::getpid());
    profiler::timestamp_t last = base;
    {
        std::ofstream output(switchLog.c_str(), std::ios::trunc);
        for (int i = 0; i < 3; ++i)
        {
            last = base + static_cast<profiler::timestamp_t>(i + 1) * 1000;
            output << last << ' ' << thread << ' ' << thread << " writer_cswitch " << process << '\n';
        }
    }

    easy_test::waitForClockToPass(last);
    profiler::setEnabled(false);
    easy_test::TempFile profileFile("writer_cswitch_source");
    out.blocksDumped = profiler::dumpBlocksToFile(profileFile.c_str());

    profiler::setEventTracingEnabled(originalTracing);
    profiler::setContextSwitchLogFilename(originalPath.c_str());

    if (out.blocksDumped != 0)
        easy_test::loadProfile(profileFile.path(), out);
}
#endif

} // namespace

TEST_CASE("Re-saving preserves every block", "[writer][format]")
{
    LoadedProfile source;
    captureSample(source);
    REQUIRE(source.readerLog.empty());
    REQUIRE(source.blocksRead == 5);

    easy_test::TempFile file("writer_resave");
    std::string log;
    const profiler::block_index_t written =
        resave(source, file.path(), log, source.beginEndTime.beginTime, source.beginEndTime.endTime);

    REQUIRE(log.empty());
    REQUIRE(written == source.blocksRead);
    REQUIRE(file.exists());
}

TEST_CASE("A re-saved file reads back", "[writer][format]")
{
    LoadedProfile source;
    captureSample(source);
    REQUIRE(source.readerLog.empty());

    easy_test::TempFile file("writer_roundtrip");
    std::string log;
    REQUIRE(resave(source, file.path(), log,
                   source.beginEndTime.beginTime, source.beginEndTime.endTime) == source.blocksRead);
    REQUIRE(log.empty());

    LoadedProfile reloaded;
    easy_test::loadProfile(file.path(), reloaded);

    REQUIRE(reloaded.readerLog.empty());
    REQUIRE(reloaded.blocksRead == source.blocksRead);
    REQUIRE(reloaded.pid == source.pid);
}

TEST_CASE("A re-save declares only descriptor payload bytes", "[writer][format]")
{
    LoadedProfile source;
    captureSample(source);
    REQUIRE(source.readerLog.empty());

    easy_test::TempFile file("writer_descriptor_memory");
    std::string log;
    REQUIRE(resave(source, file.path(), log,
                   source.beginEndTime.beginTime, source.beginEndTime.endTime) == source.blocksRead);
    REQUIRE(log.empty());

    easy_test::ProfImage image;
    REQUIRE(image.loadFrom(file.path()));
    REQUIRE(image.descriptorsMemorySize() == source.serializedDescriptors.size());
}

TEST_CASE("Re-saving honours an existing cancellation before writing", "[writer][cancel]")
{
    LoadedProfile source;
    captureSample(source);

    const profiler::blocks_t& blocks = source.blocks;
    profiler::block_getter_fn getter =
        [&blocks](profiler::block_index_t index) -> const profiler::BlocksTree&
        {
            return blocks[index];
        };

    std::atomic<int> progress(-1);
    std::ostringstream output;
    std::ostringstream log;
    REQUIRE(writeTreesToStream(
                progress, output, source.serializedDescriptors, source.descriptors,
                static_cast<profiler::block_id_t>(source.descriptorsCount), source.threadedTrees,
                source.bookmarks, getter, source.beginEndTime.beginTime,
                source.beginEndTime.endTime, source.pid, log) == 0);
    REQUIRE(log.str().find("interrupted") != std::string::npos);
    REQUIRE(output.str().empty());
}

TEST_CASE("File re-saving honours an existing cancellation before opening the file", "[writer][cancel]")
{
    LoadedProfile source;
    captureSample(source);

    const profiler::blocks_t& blocks = source.blocks;
    profiler::block_getter_fn getter =
        [&blocks](profiler::block_index_t index) -> const profiler::BlocksTree&
        {
            return blocks[index];
        };

    std::atomic<int> progress(-1);
    easy_test::TempFile output("writer_cancelled_file");
    std::ostringstream log;
    REQUIRE(writeTreesToFile(
                progress, output.c_str(), source.serializedDescriptors, source.descriptors,
                static_cast<profiler::block_id_t>(source.descriptorsCount), source.threadedTrees,
                source.bookmarks, getter, source.beginEndTime.beginTime,
                source.beginEndTime.endTime, source.pid, log) == 0);
    REQUIRE(log.str().find("interrupted") != std::string::npos);
    REQUIRE_FALSE(output.exists());
}

TEST_CASE("File re-saving reports an output path that cannot be opened", "[writer][robustness]")
{
    LoadedProfile source;
    captureSample(source);

    const std::string missingDirectory = easy_test::uniqueName("writer_missing_directory", "");
    const std::string outputPath = missingDirectory + "/trace.prof";
    std::string log;
    REQUIRE(resave(source, outputPath, log,
                   source.beginEndTime.beginTime, source.beginEndTime.endTime) == 0);
    REQUIRE(log.find("Can not open file") != std::string::npos);
}

TEST_CASE("A successful re-save reports complete progress", "[writer][progress]")
{
    LoadedProfile source;
    captureSample(source);

    const profiler::blocks_t& blocks = source.blocks;
    profiler::block_getter_fn getter =
        [&blocks](profiler::block_index_t index) -> const profiler::BlocksTree&
        {
            return blocks[index];
        };

    std::atomic<int> progress(0);
    std::ostringstream output;
    std::ostringstream log;
    REQUIRE(writeTreesToStream(
                progress, output, source.serializedDescriptors, source.descriptors,
                static_cast<profiler::block_id_t>(source.descriptorsCount), source.threadedTrees,
                source.bookmarks, getter, source.beginEndTime.beginTime,
                source.beginEndTime.endTime, source.pid, log) == source.blocksRead);
    REQUIRE(log.str().empty());
    REQUIRE(progress.load() == 100);
}

TEST_CASE("Re-saving rejects a descriptor count beyond the table", "[writer][robustness]")
{
    LoadedProfile source;
    captureSample(source);

    const profiler::blocks_t& blocks = source.blocks;
    profiler::block_getter_fn getter =
        [&blocks](profiler::block_index_t index) -> const profiler::BlocksTree&
        {
            return blocks[index];
        };

    std::ostringstream output;
    std::ostringstream log;
    const profiler::block_id_t invalidCount =
        static_cast<profiler::block_id_t>(source.descriptors.size() + 1U);
    REQUIRE(writeTreesToStream(
                output, source.serializedDescriptors, source.descriptors, invalidCount,
                source.threadedTrees, source.bookmarks, getter,
                source.beginEndTime.beginTime, source.beginEndTime.endTime,
                source.pid, log) == 0);
    REQUIRE(log.str().find("Invalid descriptor table") != std::string::npos);
}

TEST_CASE("Re-saving an empty tree reports nothing to save", "[writer][robustness]")
{
    LoadedProfile source;
    captureSample(source);

    const profiler::blocks_t& blocks = source.blocks;
    profiler::block_getter_fn getter =
        [&blocks](profiler::block_index_t index) -> const profiler::BlocksTree&
        {
            return blocks[index];
        };

    profiler::thread_blocks_tree_t emptyTrees;
    std::ostringstream output;
    std::ostringstream log;
    REQUIRE(writeTreesToStream(
                output, source.serializedDescriptors, source.descriptors,
                static_cast<profiler::block_id_t>(source.descriptorsCount), emptyTrees,
                source.bookmarks, getter, source.beginEndTime.beginTime,
                source.beginEndTime.endTime, source.pid, log) == 0);
    REQUIRE(output.str().empty());
    REQUIRE(log.str().find("Nothing to save") != std::string::npos);
}

TEST_CASE("Re-saving without descriptor bytes reports nothing to save", "[writer][robustness]")
{
    LoadedProfile source;
    captureSample(source);

    const profiler::blocks_t& blocks = source.blocks;
    profiler::block_getter_fn getter =
        [&blocks](profiler::block_index_t index) -> const profiler::BlocksTree&
        {
            return blocks[index];
        };

    profiler::SerializedData emptyDescriptors;
    std::ostringstream output;
    std::ostringstream log;
    REQUIRE(writeTreesToStream(
                output, emptyDescriptors, source.descriptors,
                static_cast<profiler::block_id_t>(source.descriptorsCount),
                source.threadedTrees, source.bookmarks, getter,
                source.beginEndTime.beginTime, source.beginEndTime.endTime,
                source.pid, log) == 0);
    REQUIRE(output.str().empty());
    REQUIRE(log.str().find("Nothing to save") != std::string::npos);
}

TEST_CASE("Re-saving with a zero descriptor count reports nothing to save", "[writer][robustness]")
{
    LoadedProfile source;
    captureSample(source);

    const profiler::blocks_t& blocks = source.blocks;
    profiler::block_getter_fn getter =
        [&blocks](profiler::block_index_t index) -> const profiler::BlocksTree&
        {
            return blocks[index];
        };

    std::ostringstream output;
    std::ostringstream log;
    REQUIRE(writeTreesToStream(
                output, source.serializedDescriptors, source.descriptors, 0,
                source.threadedTrees, source.bookmarks, getter,
                source.beginEndTime.beginTime, source.beginEndTime.endTime,
                source.pid, log) == 0);
    REQUIRE(output.str().empty());
    REQUIRE(log.str().find("Nothing to save") != std::string::npos);
}

TEST_CASE("Topology survives a re-save", "[writer][format][tree]")
{
    LoadedProfile source;
    captureSample(source);

    easy_test::TempFile file("writer_topology");
    std::string log;
    REQUIRE(resave(source, file.path(), log,
                   source.beginEndTime.beginTime, source.beginEndTime.endTime) > 0);

    LoadedProfile reloaded;
    easy_test::loadProfile(file.path(), reloaded);
    REQUIRE(reloaded.readerLog.empty());

    const long root = easy_test::findBlockIndex(reloaded, "writer_root");
    REQUIRE(root >= 0);

    const std::vector<std::string> children =
        easy_test::childNames(reloaded, static_cast<profiler::block_index_t>(root));
    REQUIRE(children.size() == 2);
    REQUIRE(children[0] == "writer_branch");
    REQUIRE(children[1] == "writer_sibling");

    const long branch = easy_test::findBlockIndex(reloaded, "writer_branch");
    REQUIRE(branch >= 0);
    const std::vector<std::string> leaves =
        easy_test::childNames(reloaded, static_cast<profiler::block_index_t>(branch));
    REQUIRE(leaves.size() == 2);
    REQUIRE(leaves[0] == "writer_leaf_a");
    REQUIRE(leaves[1] == "writer_leaf_b");
}

TEST_CASE("Re-saving to a stream matches re-saving to a file", "[writer][format]")
{
    LoadedProfile source;
    captureSample(source);

    const profiler::blocks_t& blocks = source.blocks;
    profiler::block_getter_fn getter =
        [&blocks](profiler::block_index_t index) -> const profiler::BlocksTree&
        {
            return blocks[index];
        };

    std::stringstream log;
    std::ostringstream stream;
    const profiler::block_index_t written = writeTreesToStream(
        stream, source.serializedDescriptors, source.descriptors,
        static_cast<profiler::block_id_t>(source.descriptorsCount), source.threadedTrees,
        source.bookmarks, getter, source.beginEndTime.beginTime, source.beginEndTime.endTime,
        source.pid, log);

    REQUIRE(log.str().empty());
    REQUIRE(written == source.blocksRead);

    const std::string produced = stream.str();
    REQUIRE_FALSE(produced.empty());

    uint32_t signature = 0;
    std::memcpy(&signature, produced.data(), sizeof(signature));
    REQUIRE(signature == 0x45617379u);
}

TEST_CASE("Time slicing drops blocks outside the range", "[writer][format]")
{
    // This is the mechanism behind "save a snapshot of the selected region" in the GUI.
    LoadedProfile source;

    easy_test::capture(source, [] {
        for (int i = 0; i < 20; ++i)
        {
            EASY_BLOCK("sliced_block");
        }
    });

    REQUIRE(source.readerLog.empty());
    REQUIRE(source.blocksRead == 20);

    // The boundary comes from real block timestamps rather than half of the trace
    // interval: beginEndTime also covers the time before the first block, so
    // "half the interval" would not guarantee any cut.
    REQUIRE(source.threadedTrees.size() == 1);
    const profiler::BlocksTreeRoot& root = source.threadedTrees.begin()->second;
    REQUIRE(root.children.size() == 20);

    const profiler::timestamp_t begin = source.beginEndTime.beginTime;
    const profiler::timestamp_t cut = source.blocks[root.children[4]].node->end();

    easy_test::TempFile file("writer_sliced");
    std::string log;
    const profiler::block_index_t written = resave(source, file.path(), log, begin, cut);

    REQUIRE(log.empty());
    REQUIRE(written > 0);
    REQUIRE(written < source.blocksRead);

    LoadedProfile reloaded;
    easy_test::loadProfile(file.path(), reloaded);
    REQUIRE(reloaded.readerLog.empty());
    REQUIRE(reloaded.blocksRead == written);
}

TEST_CASE("A time range outside the trace writes nothing", "[writer][format][range]")
{
    LoadedProfile source;
    captureSample(source);
    REQUIRE(source.readerLog.empty());

    easy_test::TempFile file("writer_empty_range");
    std::string log;
    const profiler::timestamp_t afterTrace = source.beginEndTime.endTime + 1000;

    REQUIRE(resave(source, file.path(), log, afterTrace, afterTrace + 1000) == 0);
    REQUIRE(log.find("Nothing to save") != std::string::npos);
}

TEST_CASE("A range covering the whole trace keeps everything", "[writer][format]")
{
    LoadedProfile source;
    captureSample(source);

    easy_test::TempFile file("writer_full_range");
    std::string log;

    const profiler::block_index_t written = resave(
        source, file.path(), log,
        source.beginEndTime.beginTime > 1000 ? source.beginEndTime.beginTime - 1000 : 0,
        source.beginEndTime.endTime + 1000);

    REQUIRE(log.empty());
    REQUIRE(written == source.blocksRead);

    LoadedProfile reloaded;
    easy_test::loadProfile(file.path(), reloaded);
    REQUIRE(reloaded.readerLog.empty());
    REQUIRE(reloaded.blocksRead == source.blocksRead);
}

TEST_CASE("A sliced re-save drops bookmarks before the selected range", "[writer][format][bookmarks]")
{
    LoadedProfile source;
    captureSample(source);
    REQUIRE(source.readerLog.empty());

    const profiler::timestamp_t begin = source.beginEndTime.beginTime;
    const profiler::timestamp_t span = source.beginEndTime.endTime - begin;
    REQUIRE(span >= 4);

    for (int i = 1; i <= 3; ++i)
    {
        profiler::Bookmark bookmark;
        bookmark.pos = begin + span * static_cast<profiler::timestamp_t>(i) / 4;
        bookmark.color = profiler::colors::Orange;
        bookmark.text = std::string("writer bookmark ") + static_cast<char>('0' + i);
        source.bookmarks.push_back(bookmark);
    }

    easy_test::TempFile file("writer_bookmark_slice");
    std::string log;
    REQUIRE(resave(source, file.path(), log,
                   source.bookmarks[1].pos, source.beginEndTime.endTime) > 0);
    REQUIRE(log.empty());

    LoadedProfile reloaded;
    easy_test::loadProfile(file.path(), reloaded);
    REQUIRE(reloaded.readerLog.empty());
    REQUIRE(reloaded.bookmarks.size() == 2);
    REQUIRE(reloaded.bookmarks[0].text == "writer bookmark 2");
    REQUIRE(reloaded.bookmarks[1].text == "writer bookmark 3");
}

TEST_CASE("A full-range re-save preserves every bookmark", "[writer][format][bookmarks]")
{
    LoadedProfile source;
    captureSample(source);
    REQUIRE(source.readerLog.empty());

    profiler::Bookmark first;
    first.pos = source.beginEndTime.beginTime;
    first.color = profiler::colors::Green;
    first.text = "writer first bookmark";
    source.bookmarks.push_back(first);

    profiler::Bookmark last;
    last.pos = source.beginEndTime.endTime;
    last.color = profiler::colors::Red;
    last.text = "writer last bookmark";
    source.bookmarks.push_back(last);

    easy_test::TempFile file("writer_all_bookmarks");
    std::string log;
    REQUIRE(resave(source, file.path(), log,
                   source.beginEndTime.beginTime,
                   source.beginEndTime.endTime) == source.blocksRead);
    REQUIRE(log.empty());

    LoadedProfile reloaded;
    easy_test::loadProfile(file.path(), reloaded);
    REQUIRE(reloaded.readerLog.empty());
    REQUIRE(reloaded.bookmarks.size() == 2);
    REQUIRE(reloaded.bookmarks.front().text == first.text);
    REQUIRE(reloaded.bookmarks.back().text == last.text);
}

TEST_CASE("Values survive a re-save", "[writer][format][values]")
{
    LoadedProfile source;

    easy_test::capture(source, [] {
        EASY_BLOCK("writer_value_host");
        EASY_VALUE("writer_value_int", static_cast<int32_t>(4242));
        const int32_t numbers[] = {7, 8, 9};
        EASY_ARRAY("writer_value_array", numbers, 3);
        EASY_TEXT("writer_value_text", "resaved text");
    });

    REQUIRE(source.readerLog.empty());

    easy_test::TempFile file("writer_values");
    std::string log;
    REQUIRE(resave(source, file.path(), log,
                   source.beginEndTime.beginTime, source.beginEndTime.endTime) > 0);
    REQUIRE(log.empty());

    LoadedProfile reloaded;
    easy_test::loadProfile(file.path(), reloaded);
    REQUIRE(reloaded.readerLog.empty());

    const profiler::ArbitraryValue* asInt = easy_test::findValue(reloaded, "writer_value_int");
    REQUIRE(asInt != nullptr);
    REQUIRE(asInt->toValue<int32_t>()->value() == 4242);

    const profiler::ArbitraryValue* asArray = easy_test::findValue(reloaded, "writer_value_array");
    REQUIRE(asArray != nullptr);
    REQUIRE(asArray->toArray<int32_t>()->size() == 3);
    REQUIRE(asArray->toArray<int32_t>()->at(2) == 9);

    const profiler::ArbitraryValue* asText = easy_test::findValue(reloaded, "writer_value_text");
    REQUIRE(asText != nullptr);
    REQUIRE(std::strcmp(asText->data(), "resaved text") == 0);
}

TEST_CASE("A multithreaded trace survives a re-save", "[writer][format][threads]")
{
    LoadedProfile source;

    easy_test::capture(source, [] {
        std::vector<std::thread> workers;
        for (int t = 0; t < 3; ++t)
        {
            workers.emplace_back([] {
                EASY_THREAD_SCOPE("writer_worker");
                EASY_BLOCK("writer_thread_block");
            });
        }
        for (std::thread& worker : workers)
            worker.join();
    });

    REQUIRE(source.readerLog.empty());
    const size_t sourceThreads = source.threadedTrees.size();

    easy_test::TempFile file("writer_threads");
    std::string log;
    REQUIRE(resave(source, file.path(), log,
                   source.beginEndTime.beginTime, source.beginEndTime.endTime) > 0);
    REQUIRE(log.empty());

    LoadedProfile reloaded;
    easy_test::loadProfile(file.path(), reloaded);

    REQUIRE(reloaded.readerLog.empty());
    REQUIRE(reloaded.threadedTrees.size() == sourceThreads);
    REQUIRE(easy_test::countBlocksNamed(reloaded, "writer_thread_block") == 3);
}

#ifndef _WIN32
TEST_CASE("A context-switch-only trace survives a re-save", "[writer][format][cswitch][unix]")
{
    LoadedProfile source;
    captureContextSwitchOnly(source);
    INFO("reader log: " << source.readerLog);
    REQUIRE(source.readerLog.empty());
    REQUIRE(source.blocksRead == 3);
    REQUIRE(source.threadedTrees.size() == 1);
    REQUIRE(source.threadedTrees.begin()->second.children.empty());
    REQUIRE(source.threadedTrees.begin()->second.sync.size() == 3);

    easy_test::TempFile file("writer_cswitch_resave");
    std::string log;
    REQUIRE(resave(source, file.path(), log,
                   source.beginEndTime.beginTime, source.beginEndTime.endTime) == 3);
    REQUIRE(log.empty());

    LoadedProfile reloaded;
    easy_test::loadProfile(file.path(), reloaded);
    REQUIRE(reloaded.readerLog.empty());
    REQUIRE(reloaded.blocksRead == 3);
    REQUIRE(reloaded.threadedTrees.size() == 1);
    REQUIRE(reloaded.threadedTrees.begin()->second.sync.size() == 3);
}
#endif

TEST_CASE("Re-saving is idempotent", "[writer][format]")
{
    // A second pass through the writer must produce a file equivalent to the
    // first: the writer reads back what it wrote.
    LoadedProfile source;
    captureSample(source);

    easy_test::TempFile first("writer_pass_one");
    std::string log;
    REQUIRE(resave(source, first.path(), log,
                   source.beginEndTime.beginTime, source.beginEndTime.endTime) > 0);

    LoadedProfile afterFirst;
    easy_test::loadProfile(first.path(), afterFirst);
    REQUIRE(afterFirst.readerLog.empty());

    easy_test::TempFile second("writer_pass_two");
    REQUIRE(resave(afterFirst, second.path(), log,
                   afterFirst.beginEndTime.beginTime, afterFirst.beginEndTime.endTime)
            == afterFirst.blocksRead);

    LoadedProfile afterSecond;
    easy_test::loadProfile(second.path(), afterSecond);

    REQUIRE(afterSecond.readerLog.empty());
    REQUIRE(afterSecond.blocksRead == afterFirst.blocksRead);
    REQUIRE(first.sizeBytes() == second.sizeBytes());
}
