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
    for (int i = 0; i < 5; ++i)
    {
        log.addSwitch(base + static_cast<profiler::timestamp_t>(i + 1) * 1000,
                      currentThread, currentThread, "easy_profiler_tests", processId);
    }
    log.write();

    profiler::setEnabled(false);

    easy_test::TempFile file("cswitch_dump");
    const uint32_t dumped = profiler::dumpBlocksToFile(file.c_str());

    // What matters is that parsing the log neither breaks the dump nor corrupts
    // the trace.
    REQUIRE(dumped > 0);

    LoadedProfile profile;
    easy_test::loadProfile(file.path(), profile);

    INFO("reader log: " << profile.readerLog);
    REQUIRE(profile.readerLog.empty());
    REQUIRE(easy_test::findBlockIndex(profile, "cswitch_host_block") >= 0);
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
    const profiler::timestamp_t base = profiler::timeNow();
    log.addSwitch(base, 999001, 999002, "some_other_process", 424242);
    log.addSwitch(base + 1000, 999002, 999003, "another_process", 424243);
    log.write();

    LogPathGuard guard(log.path());

    profiler::setEventTracingEnabled(true);
    profiler::setEnabled(true);
    { EASY_BLOCK("cswitch_foreign_block"); }
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
