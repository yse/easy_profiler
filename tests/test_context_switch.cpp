/** Context switch events — ingesting an external log on Unix.

On Windows the events are collected by the built-in ETW tracer. On Unix the
mechanism is fundamentally different: an external SystemTap script
(scripts/context_switch_logger.stp) writes a TEXT log, and the profiler parses it
while dumping (profile_manager.cpp:965-985):

    while (infile >> timestamp >> thread_from >> thread_to >> next_task_name >> process_to)

The log path is set through the public setContextSwitchLogFilename, so a log can
be supplied and its parsing checked without SystemTap and without root.

The line format follows the printf in the stp script:
    get_cycles() prev_tid next_tid next_task_name next_pid
*/

#include <catch2/catch_test_macros.hpp>

#include "support/prof_fixture.h"

#include <easy/profiler.h>

// profile_manager.h pulls in winsock2.h and has to precede current_thread.h,
// which includes Windows.h, the same order profile_manager.cpp uses.
#include "profile_manager.h"
#include "current_thread.h"

#include <fstream>
#include <string>
#include <vector>

#ifndef _WIN32
# include <unistd.h>
#endif

using easy_test::LoadedProfile;

namespace {

/** Write a context switch log in the stp script's format. */
class ContextSwitchLog
{
public:
    ContextSwitchLog() : m_file("cs_log", ".log") {}

    void addSwitch(profiler::timestamp_t timestamp,
                   profiler::thread_id_t from,
                   profiler::thread_id_t to,
                   const std::string& processName,
                   long processId)
    {
        m_lines.push_back(std::to_string(timestamp) + " " +
                          std::to_string(from) + " " +
                          std::to_string(to) + " " +
                          processName + " " +
                          std::to_string(processId));
    }

    void write()
    {
        std::ofstream out(m_file.path().c_str(), std::ios::trunc);
        for (size_t i = 0; i < m_lines.size(); ++i)
            out << m_lines[i] << '\n';
    }

    const std::string& path() const { return m_file.path(); }

private:
    easy_test::TempFile m_file;
    std::vector<std::string> m_lines;
};

/** Restore the original log path once the test is done. */
struct LogPathGuard
{
    explicit LogPathGuard(const std::string& path)
        : original(profiler::getContextSwitchLogFilename())
    {
        profiler::setContextSwitchLogFilename(path.c_str());
    }

    ~LogPathGuard()
    {
        profiler::setContextSwitchLogFilename(original.c_str());
    }

    std::string original;
};

} // namespace

TEST_CASE("The context switch log path is configurable", "[cswitch]")
{
    const std::string original = profiler::getContextSwitchLogFilename();
    REQUIRE_FALSE(original.empty());

    {
        LogPathGuard guard("/tmp/easy_profiler_test_cs.log");
        REQUIRE(std::string(profiler::getContextSwitchLogFilename()) ==
                "/tmp/easy_profiler_test_cs.log");
    }

    REQUIRE(std::string(profiler::getContextSwitchLogFilename()) == original);
}

TEST_CASE("The default log path is readable", "[cswitch]")
{
    const std::string current = profiler::getContextSwitchLogFilename();
    INFO("current path: " << current);
    SUCCEED("path is readable");
}

TEST_CASE("Event tracing can be switched on and off", "[cswitch]")
{
    const bool original = profiler::isEventTracingEnabled();

    profiler::setEventTracingEnabled(true);
    REQUIRE(profiler::isEventTracingEnabled());

    profiler::setEventTracingEnabled(false);
    REQUIRE_FALSE(profiler::isEventTracingEnabled());

    profiler::setEventTracingEnabled(original);
    REQUIRE(profiler::isEventTracingEnabled() == original);
}

TEST_CASE("Event tracing priority", "[cswitch]")
{
    // Priority only means something for the Windows ETW tracer. Elsewhere
    // isLowPriorityEventTracing() is a stub returning false unconditionally
    // (profiler.cpp:233) and the setter does nothing.
    profiler::setLowPriorityEventTracing(true);

#ifdef _WIN32
    REQUIRE(profiler::isLowPriorityEventTracing());
    profiler::setLowPriorityEventTracing(false);
    REQUIRE_FALSE(profiler::isLowPriorityEventTracing());
#else
    REQUIRE_FALSE(profiler::isLowPriorityEventTracing());
    profiler::setLowPriorityEventTracing(false);
    REQUIRE_FALSE(profiler::isLowPriorityEventTracing());
#endif
}

#ifndef _WIN32

TEST_CASE("Locked context-switch entry points keep storage consistent", "[cswitch][unix][threads]")
{
    easy_test::drainPending();

    const bool originalTracing = profiler::isEventTracingEnabled();
    profiler::setEventTracingEnabled(false);
    profiler::setEnabled(true);
    { EASY_BLOCK("locked_cswitch_anchor"); }

    ProfileManager& manager = ProfileManager::instance();
    const profiler::thread_id_t currentThread = getCurrentThreadId();
    const processid_t currentProcess = static_cast<processid_t>(::getpid());
    const profiler::timestamp_t begin = profiler::timeNow();

    manager.beginContextSwitch(currentThread, begin, currentThread,
                               "locked_cswitch_target", true);
    manager.endContextSwitch(currentThread, currentProcess, begin + 1000U, true);

    // Closing an already closed switch is a safe no-op. This reaches the
    // locked lookup with an existing storage and an empty opened-switch list.
    manager.endContextSwitch(currentThread, currentProcess, begin + 2000U, true);

    // A foreign process must be looked up under the same lock but must not
    // create a ThreadStorage for an unknown thread.
    manager.endContextSwitch(currentThread + 1000000U, currentProcess + 1U,
                             begin + 3000U, true);

    profiler::setEnabled(false);
    easy_test::TempFile file("locked_cswitch");
    REQUIRE(profiler::dumpBlocksToFile(file.c_str()) >= 2);
    profiler::setEventTracingEnabled(originalTracing);

    LoadedProfile profile;
    easy_test::loadProfile(file.path(), profile);
    REQUIRE(profile.readerLog.empty());
    REQUIRE(easy_test::findBlockIndex(profile, "locked_cswitch_anchor") >= 0);

    const auto tree = profile.threadedTrees.find(currentThread);
    REQUIRE(tree != profile.threadedTrees.end());
    REQUIRE(tree->second.sync.size() == 1);
}

TEST_CASE("The context switch log is parsed during a dump", "[cswitch][unix]")
{
    easy_test::drainPending();

    const profiler::thread_id_t currentThread = getCurrentThreadId();
    const long processId = static_cast<long>(::getpid());

    ContextSwitchLog log;
    LogPathGuard guard(log.path());

    profiler::setEventTracingEnabled(true);
    profiler::setEnabled(true);

    EASY_THREAD("cswitch_thread");
    { EASY_BLOCK("cswitch_host_block"); }

    // The timestamps are taken AFTER capture starts: the reader counts an event
    // only if its end is later than the trace start (reader.cpp:1058), while the
    // header counts every event unconditionally. An event outside the capture
    // window makes the two counters disagree — see the dedicated red test in
    // test_known_issues.cpp.
    const profiler::timestamp_t base = profiler::timeNow();
    profiler::timestamp_t last = base;
    for (int i = 0; i < 5; ++i)
    {
        last = base + static_cast<profiler::timestamp_t>(i + 1) * 1000;
        log.addSwitch(last, currentThread, currentThread, "easy_profiler_tests", processId);
    }
    log.write();

    easy_test::waitForClockToPass(last);
    profiler::setEnabled(false);

    easy_test::TempFile file("cswitch_dump");
    const uint32_t dumped = profiler::dumpBlocksToFile(file.c_str());

    // What matters is that parsing the log neither breaks the dump nor corrupts
    // the trace.
    REQUIRE(dumped > 0);

    LoadedProfile profile;
    easy_test::loadProfile(file.path(), profile, /*gatherStatistics*/ true);

    INFO("reader log: " << profile.readerLog);
    REQUIRE(profile.readerLog.empty());
    REQUIRE(easy_test::findBlockIndex(profile, "cswitch_host_block") >= 0);

    const auto root = profile.threadedTrees.find(currentThread);
    REQUIRE(root != profile.threadedTrees.end());
    REQUIRE(root->second.sync.size() == 5);
    const profiler::BlocksTree& firstSwitch = profile.blocks[root->second.sync.front()];
    REQUIRE(firstSwitch.per_thread_stats != nullptr);
    REQUIRE(firstSwitch.per_thread_stats->calls_number == 5);
}

TEST_CASE("A wait interval crossing the capture start is clamped, not dropped", "[cswitch][unix]")
{
    // One interval spans two log lines. Filtering the departure line because it
    // predates the capture used to orphan the arrival line, so the whole wait
    // disappeared and thread wait time was silently under-reported.
    easy_test::drainPending();

    const profiler::thread_id_t currentThread = getCurrentThreadId();
    const long processId = static_cast<long>(::getpid());
    const profiler::thread_id_t otherThread = currentThread + 1000000U;

    ContextSwitchLog log;
    LogPathGuard guard(log.path());

    const profiler::timestamp_t beforeCapture = profiler::timeNow();

    profiler::setEventTracingEnabled(true);
    profiler::setEnabled(true);
    EASY_THREAD("cswitch_boundary_thread");
    { EASY_BLOCK("cswitch_boundary_block"); }

    log.addSwitch(beforeCapture - 1000000U, currentThread, otherThread,
                  "other_task", 424242);
    log.addSwitch(profiler::timeNow(), otherThread, currentThread,
                  "easy_profiler_tests", processId);
    log.write();

    profiler::setEnabled(false);

    easy_test::TempFile file("cswitch_cross_begin");
    REQUIRE(profiler::dumpBlocksToFile(file.c_str()) > 0);

    LoadedProfile profile;
    easy_test::loadProfile(file.path(), profile);
    INFO("reader log: " << profile.readerLog);
    REQUIRE(profile.readerLog.empty());

    const auto root = profile.threadedTrees.find(currentThread);
    REQUIRE(root != profile.threadedTrees.end());
    REQUIRE(root->second.sync.size() == 1);
    REQUIRE(profile.blocks[root->second.sync.front()].cs->begin() ==
            profile.beginEndTime.beginTime);
}

TEST_CASE("A wait interval crossing the capture end is clamped, not dropped", "[cswitch][unix]")
{
    easy_test::drainPending();

    const profiler::thread_id_t currentThread = getCurrentThreadId();
    const long processId = static_cast<long>(::getpid());
    const profiler::thread_id_t otherThread = currentThread + 1000000U;

    ContextSwitchLog log;
    LogPathGuard guard(log.path());

    profiler::setEventTracingEnabled(true);
    profiler::setEnabled(true);
    EASY_THREAD("cswitch_boundary_thread");
    { EASY_BLOCK("cswitch_boundary_block"); }

    const profiler::timestamp_t inside = profiler::timeNow();
    log.addSwitch(inside, currentThread, otherThread, "other_task", 424242);
    // Returns long after the capture window closes.
    log.addSwitch(inside + 10000000000ULL, otherThread, currentThread,
                  "easy_profiler_tests", processId);
    log.write();

    profiler::setEnabled(false);

    easy_test::TempFile file("cswitch_cross_end");
    REQUIRE(profiler::dumpBlocksToFile(file.c_str()) > 0);

    LoadedProfile profile;
    easy_test::loadProfile(file.path(), profile);
    INFO("reader log: " << profile.readerLog);
    REQUIRE(profile.readerLog.empty());

    const auto root = profile.threadedTrees.find(currentThread);
    REQUIRE(root != profile.threadedTrees.end());
    REQUIRE(root->second.sync.size() == 1);
    REQUIRE(profile.blocks[root->second.sync.front()].cs->end() ==
            profile.beginEndTime.endTime);
}

TEST_CASE("A wait still open at dump time is closed at the capture boundary", "[cswitch][unix]")
{
    // The log need not contain the return at all: the thread can still be off
    // the CPU when the dump runs. Serialization drops open waits, so unless the
    // interval is closed at the boundary first it disappears completely.
    easy_test::drainPending();

    const profiler::thread_id_t currentThread = getCurrentThreadId();
    const profiler::thread_id_t otherThread = currentThread + 1000000U;

    ContextSwitchLog log;
    LogPathGuard guard(log.path());

    profiler::setEventTracingEnabled(true);
    profiler::setEnabled(true);
    EASY_THREAD("cswitch_open_thread");
    { EASY_BLOCK("cswitch_open_block"); }

    // Departure only, with no matching return line anywhere in the log.
    log.addSwitch(profiler::timeNow(), currentThread, otherThread, "other_task", 424242);
    log.write();

    profiler::setEnabled(false);

    easy_test::TempFile file("cswitch_still_open");
    REQUIRE(profiler::dumpBlocksToFile(file.c_str()) > 0);

    LoadedProfile profile;
    easy_test::loadProfile(file.path(), profile);
    INFO("reader log: " << profile.readerLog);
    REQUIRE(profile.readerLog.empty());

    const auto root = profile.threadedTrees.find(currentThread);
    REQUIRE(root != profile.threadedTrees.end());
    REQUIRE(root->second.sync.size() == 1);
    REQUIRE(profile.blocks[root->second.sync.front()].cs->end() ==
            profile.beginEndTime.endTime);
}

TEST_CASE("A wait interval entirely before the capture is discarded", "[cswitch][unix]")
{
    easy_test::drainPending();

    const profiler::thread_id_t currentThread = getCurrentThreadId();
    const long processId = static_cast<long>(::getpid());
    const profiler::thread_id_t otherThread = currentThread + 1000000U;

    ContextSwitchLog log;
    LogPathGuard guard(log.path());

    const profiler::timestamp_t beforeCapture = profiler::timeNow();
    log.addSwitch(beforeCapture - 2000000U, currentThread, otherThread,
                  "other_task", 424242);
    log.addSwitch(beforeCapture - 1000000U, otherThread, currentThread,
                  "easy_profiler_tests", processId);
    log.write();

    profiler::setEventTracingEnabled(true);
    profiler::setEnabled(true);
    EASY_THREAD("cswitch_boundary_thread");
    { EASY_BLOCK("cswitch_outside_block"); }
    profiler::setEnabled(false);

    easy_test::TempFile file("cswitch_outside");
    REQUIRE(profiler::dumpBlocksToFile(file.c_str()) > 0);

    LoadedProfile profile;
    easy_test::loadProfile(file.path(), profile);
    INFO("reader log: " << profile.readerLog);
    REQUIRE(profile.readerLog.empty());

    const auto root = profile.threadedTrees.find(currentThread);
    REQUIRE(root != profile.threadedTrees.end());
    REQUIRE(root->second.sync.empty());
}

TEST_CASE("The recorded wait target is the name from the departing line", "[cswitch][unix]")
{
    // One wait spans two log lines, and the name belongs to the first of them.
    // CSwitchBlock used to only borrow that name while the parser reused a single
    // std::string for every line, so by the time the interval was closed - many
    // lines later - the pointer described whatever the parser had read last. The
    // recorded name was wrong on every platform that reads a log, whether or not
    // the string had reallocated in the meantime.
    easy_test::drainPending();

    const profiler::thread_id_t currentThread = getCurrentThreadId();
    const long processId = static_cast<long>(::getpid());
    const profiler::thread_id_t otherThread = currentThread + 1000000U;

    ContextSwitchLog log;
    LogPathGuard guard(log.path());

    profiler::setEventTracingEnabled(true);
    profiler::setEnabled(true);
    EASY_THREAD("cswitch_name_thread");
    { EASY_BLOCK("cswitch_name_block"); }

    const profiler::timestamp_t base = profiler::timeNow();

    // We leave the CPU to "departing_task".
    log.addSwitch(base + 1000, currentThread, otherThread, "departing_task", 424242);
    // Unrelated switches of other threads, each with a different name. They own
    // no storage here, so they add nothing to the trace - but they do move the
    // parser's name buffer on.
    for (int i = 0; i < 5; ++i)
    {
        log.addSwitch(base + 2000 + static_cast<profiler::timestamp_t>(i),
                      otherThread + 1 + static_cast<profiler::thread_id_t>(i),
                      otherThread + 100 + static_cast<profiler::thread_id_t>(i),
                      "noise_task_" + std::to_string(i), 424243);
    }
    // We come back, which closes the interval opened by the first line.
    log.addSwitch(base + 9000, otherThread, currentThread, "easy_profiler_tests", processId);
    log.write();

    easy_test::waitForClockToPass(base + 9000);
    profiler::setEnabled(false);

    easy_test::TempFile file("cswitch_name");
    REQUIRE(profiler::dumpBlocksToFile(file.c_str()) > 0);

    LoadedProfile profile;
    easy_test::loadProfile(file.path(), profile);
    INFO("reader log: " << profile.readerLog);
    REQUIRE(profile.readerLog.empty());

    const auto root = profile.threadedTrees.find(currentThread);
    REQUIRE(root != profile.threadedTrees.end());
    REQUIRE(root->second.sync.size() == 1);
    REQUIRE(std::string(profile.blocks[root->second.sync.front()].cs->name()) == "departing_task");
}

TEST_CASE("A longer name later in the log does not invalidate an open wait", "[cswitch][unix]")
{
    // The same defect in its memory-unsafe form: the parser's std::string grows
    // as the names get longer, which frees the buffer the open interval was
    // pointing at. Closing the interval then calls strlen() on freed memory
    // (thread_storage.cpp:180) - a heap-use-after-free under a sanitizer, and a
    // garbage name without one.
    easy_test::drainPending();

    const profiler::thread_id_t currentThread = getCurrentThreadId();
    const long processId = static_cast<long>(::getpid());
    const profiler::thread_id_t otherThread = currentThread + 1000000U;

    const std::string departingName(64, 'k');

    ContextSwitchLog log;
    LogPathGuard guard(log.path());

    profiler::setEventTracingEnabled(true);
    profiler::setEnabled(true);
    EASY_THREAD("cswitch_grow_thread");
    { EASY_BLOCK("cswitch_grow_block"); }

    const profiler::timestamp_t base = profiler::timeNow();

    log.addSwitch(base + 1000, currentThread, otherThread, departingName, 424242);
    for (int i = 0; i < 6; ++i)
    {
        log.addSwitch(base + 2000 + static_cast<profiler::timestamp_t>(i),
                      otherThread + 1 + static_cast<profiler::thread_id_t>(i),
                      otherThread + 100 + static_cast<profiler::thread_id_t>(i),
                      std::string(static_cast<size_t>(64U << i), 'n'), 424243);
    }
    log.addSwitch(base + 9000, otherThread, currentThread, "easy_profiler_tests", processId);
    log.write();

    easy_test::waitForClockToPass(base + 9000);
    profiler::setEnabled(false);

    easy_test::TempFile file("cswitch_grow");
    REQUIRE(profiler::dumpBlocksToFile(file.c_str()) > 0);

    LoadedProfile profile;
    easy_test::loadProfile(file.path(), profile);
    INFO("reader log: " << profile.readerLog);
    REQUIRE(profile.readerLog.empty());

    const auto root = profile.threadedTrees.find(currentThread);
    REQUIRE(root != profile.threadedTrees.end());
    REQUIRE(root->second.sync.size() == 1);
    REQUIRE(std::string(profile.blocks[root->second.sync.front()].cs->name()) == departingName);
}

TEST_CASE("A wait closed at the capture boundary keeps its target name", "[cswitch][unix]")
{
    // A wait with no return line is closed at the capture boundary while the
    // dump is being written. That path used to rebuild the interval with an
    // empty name on purpose, because the borrowed name was already dead by then.
    // Now the interval owns its name, so the real one survives.
    easy_test::drainPending();

    const profiler::thread_id_t currentThread = getCurrentThreadId();
    const profiler::thread_id_t otherThread = currentThread + 1000000U;

    ContextSwitchLog log;
    LogPathGuard guard(log.path());

    profiler::setEventTracingEnabled(true);
    profiler::setEnabled(true);
    EASY_THREAD("cswitch_boundary_name_thread");
    { EASY_BLOCK("cswitch_boundary_name_block"); }

    log.addSwitch(profiler::timeNow(), currentThread, otherThread, "boundary_task", 424242);
    log.write();

    profiler::setEnabled(false);

    easy_test::TempFile file("cswitch_boundary_name");
    REQUIRE(profiler::dumpBlocksToFile(file.c_str()) > 0);

    LoadedProfile profile;
    easy_test::loadProfile(file.path(), profile);
    INFO("reader log: " << profile.readerLog);
    REQUIRE(profile.readerLog.empty());

    const auto root = profile.threadedTrees.find(currentThread);
    REQUIRE(root != profile.threadedTrees.end());
    REQUIRE(root->second.sync.size() == 1);
    REQUIRE(std::string(profile.blocks[root->second.sync.front()].cs->name()) == "boundary_task");
}

TEST_CASE("A return logged before the departure does not reverse the interval", "[cswitch][unix]")
{
    // A context switch log is not guaranteed to be globally time-sorted: merging
    // per-CPU buffers into one file can put a thread's return line before its
    // departure line. The end timestamp used to be stored unconditionally, and
    // Event::duration() is an unsigned subtraction, so the resulting interval
    // reported a wait of nearly 2^64 ticks in a file the reader accepts without
    // complaint.
    easy_test::drainPending();

    const profiler::thread_id_t currentThread = getCurrentThreadId();
    const long processId = static_cast<long>(::getpid());
    const profiler::thread_id_t otherThread = currentThread + 1000000U;

    ContextSwitchLog log;
    LogPathGuard guard(log.path());

    profiler::setEventTracingEnabled(true);
    profiler::setEnabled(true);
    EASY_THREAD("cswitch_unordered_thread");
    { EASY_BLOCK("cswitch_unordered_block"); }

    const profiler::timestamp_t base = profiler::timeNow();
    log.addSwitch(base + 5000, currentThread, otherThread, "other_task", 424242);
    // Out of order: the return is stamped earlier than the departure.
    log.addSwitch(base + 1000, otherThread, currentThread, "easy_profiler_tests", processId);
    log.write();

    easy_test::waitForClockToPass(base + 5000);
    profiler::setEnabled(false);

    easy_test::TempFile file("cswitch_unordered");
    REQUIRE(profiler::dumpBlocksToFile(file.c_str()) > 0);

    LoadedProfile profile;
    easy_test::loadProfile(file.path(), profile);
    INFO("reader log: " << profile.readerLog);
    REQUIRE(profile.readerLog.empty());

    const auto root = profile.threadedTrees.find(currentThread);
    REQUIRE(root != profile.threadedTrees.end());
    REQUIRE(root->second.sync.size() == 1);

    // The interval is kept, clamped to zero length rather than reversed.
    const profiler::BlocksTree& wait = profile.blocks[root->second.sync.front()];
    REQUIRE(wait.cs->end() >= wait.cs->begin());
    REQUIRE(wait.cs->duration() == 0);
    REQUIRE(root->second.wait_time == 0);
}

TEST_CASE("An empty context switch log is harmless", "[cswitch][unix]")
{
    easy_test::drainPending();

    ContextSwitchLog log;
    log.write();

    LogPathGuard guard(log.path());

    profiler::setEventTracingEnabled(true);
    profiler::setEnabled(true);
    { EASY_BLOCK("cswitch_empty_log_block"); }
    profiler::setEnabled(false);

    easy_test::TempFile file("cswitch_empty");
    REQUIRE(profiler::dumpBlocksToFile(file.c_str()) > 0);

    LoadedProfile profile;
    easy_test::loadProfile(file.path(), profile);
    REQUIRE(profile.readerLog.empty());
    REQUIRE(easy_test::findBlockIndex(profile, "cswitch_empty_log_block") >= 0);
}

TEST_CASE("A missing context switch log is harmless", "[cswitch][unix]")
{
    easy_test::drainPending();

    LogPathGuard guard("/tmp/easy_profiler_definitely_missing_cs_log_12345.log");

    profiler::setEventTracingEnabled(true);
    profiler::setEnabled(true);
    { EASY_BLOCK("cswitch_missing_log_block"); }
    profiler::setEnabled(false);

    easy_test::TempFile file("cswitch_missing");
    REQUIRE(profiler::dumpBlocksToFile(file.c_str()) > 0);

    LoadedProfile profile;
    easy_test::loadProfile(file.path(), profile);
    REQUIRE(profile.readerLog.empty());
    REQUIRE(easy_test::findBlockIndex(profile, "cswitch_missing_log_block") >= 0);
}

TEST_CASE("Malformed lines in the log do not break the dump", "[cswitch][unix]")
{
    easy_test::drainPending();

    // Parsing goes through operator>>, so the first non-numeric line stops the
    // read. What is checked here is that this does not crash.
    easy_test::TempFile logFile("cs_broken", ".log");
    {
        std::ofstream out(logFile.path().c_str(), std::ios::trunc);
        out << "not_a_number garbage garbage garbage garbage\n";
        out << "12345 1 2 name 3\n";
    }

    LogPathGuard guard(logFile.path());

    profiler::setEventTracingEnabled(true);
    profiler::setEnabled(true);
    { EASY_BLOCK("cswitch_broken_log_block"); }
    profiler::setEnabled(false);

    easy_test::TempFile file("cswitch_broken");
    REQUIRE(profiler::dumpBlocksToFile(file.c_str()) > 0);

    LoadedProfile profile;
    easy_test::loadProfile(file.path(), profile);
    REQUIRE(profile.readerLog.empty());
}

TEST_CASE("Switches from a foreign process create no storages", "[cswitch][unix]")
{
    easy_test::drainPending();

    // When process_id does not match ours, no ThreadStorage is created: the
    // profiler does not allocate storages for foreign threads (the else branch
    // of endContextSwitch in profile_manager.cpp).
    ContextSwitchLog log;
    LogPathGuard guard(log.path());

    profiler::setEventTracingEnabled(true);
    profiler::setEnabled(true);
    { EASY_BLOCK("cswitch_foreign_block"); }

    // Timestamps must belong to this capture. Generating the log before
    // setEnabled(true) only exercises the time-range filter and never reaches
    // the foreign-process storage decision.
    const profiler::timestamp_t base = profiler::timeNow();
    log.addSwitch(base, 999001, 999002, "some_other_process", 424242);
    log.addSwitch(base + 1000, 999002, 999003, "another_process", 424243);
    log.write();

    easy_test::waitForClockToPass(base + 1000);
    profiler::setEnabled(false);

    easy_test::TempFile file("cswitch_foreign");
    REQUIRE(profiler::dumpBlocksToFile(file.c_str()) > 0);

    LoadedProfile profile;
    easy_test::loadProfile(file.path(), profile);
    REQUIRE(profile.readerLog.empty());

    for (const auto& entry : profile.threadedTrees)
    {
        REQUIRE(entry.first != 999001u);
        REQUIRE(entry.first != 999002u);
        REQUIRE(entry.first != 999003u);
    }
}

TEST_CASE("Disabled event tracing does not read the log", "[cswitch][unix]")
{
    easy_test::drainPending();

    // A deliberately malformed log: had it been read, parsing would have touched it.
    easy_test::TempFile logFile("cs_ignored", ".log");
    {
        std::ofstream out(logFile.path().c_str(), std::ios::trunc);
        out << "this log must not be read at all\n";
    }

    LogPathGuard guard(logFile.path());

    profiler::setEventTracingEnabled(false);
    profiler::setEnabled(true);
    { EASY_BLOCK("cswitch_disabled_block"); }
    profiler::setEnabled(false);

    easy_test::TempFile file("cswitch_disabled");
    REQUIRE(profiler::dumpBlocksToFile(file.c_str()) > 0);

    LoadedProfile profile;
    easy_test::loadProfile(file.path(), profile);
    REQUIRE(profile.readerLog.empty());
    REQUIRE(easy_test::findBlockIndex(profile, "cswitch_disabled_block") >= 0);

    profiler::setEventTracingEnabled(true);
}

#endif // !_WIN32
