/** Regression tests for defects fixed while reviving easy_profiler.

Each test keeps the original failure mode and rationale so the bug cannot return
silently. This is part of the normal green suite.
*/

#include <catch2/catch_test_macros.hpp>

#include "support/prof_image.h"
#include "support/prof_fixture.h"

#include <easy/profiler.h>

#include "current_thread.h"

#include <chrono>
#include <ctime>
#include <fstream>
#include <limits>
#include <string>

#ifndef _WIN32
# include <unistd.h>
#endif

using easy_test::LoadedProfile;
using easy_test::ProfImage;

namespace {

double millisecondsOf(std::chrono::steady_clock::time_point from,
                      std::chrono::steady_clock::time_point to)
{
    return std::chrono::duration<double, std::milli>(to - from).count();
}

} // namespace

//////////////////////////////////////////////////////////////////////////
// Defect: out-of-bounds heap write while parsing an untrusted .prof
//////////////////////////////////////////////////////////////////////////

TEST_CASE("The reader rejects an understated descriptors_memory_size", "[regression][security]")
{
    // DEFECT: reader.cpp:955-958 — the bounds check is commented out.
    //
    //     941:  serialized_descriptors.set(descriptors_memory_size); // new char[N], N from file
    //     955:  //if (i + sz > descriptors_memory_size) {
    //     956:  //    printf("FILE CORRUPTED\n");
    //     957:  //    return 0;
    //     958:  //}
    //     960:  char* data = serialized_descriptors[i];  // m_data + i, unchecked
    //     961:  read(inStream, data, sz);                // sz from file
    //
    // The buffer size, the offset and the record length all come from the file
    // and nothing is validated. Changing a single header field is enough to write
    // past the end of the heap buffer with data from the file (CWE-787). Under
    // ASan this reproduces as a heap-buffer-overflow WRITE.
    //
    // AFTER THE FIX: the reader returns 0 and writes the reason to the log. The
    // identical check is alive in three parallel paths — reader.cpp:1524,
    // reader.cpp:1037 and reader.cpp:1108 — so its being commented out here looks
    // like an oversight. The fix is to uncomment four lines.
    //
    // NOTE: this test may take the process down. It lives in a separate binary,
    // and catch_discover_tests registers every TEST_CASE as its own CTest test,
    // so a crash does not take neighbouring cases with it.
    ProfImage image;
    REQUIRE(image.captureFrom([] {
        EASY_BLOCK("overflow_probe_root");
        { EASY_BLOCK("overflow_probe_child"); }
    }));

    const uint64_t original = image.descriptorsMemorySize();
    REQUIRE(original > 64);

    // The only edit: understate the declared descriptor buffer size. The
    // descriptor data stays in place and is longer than the declared buffer.
    image.setDescriptorsMemorySize(64);

    easy_test::TempFile file("known_issue_overflow");
    image.writeTo(file.path());

    LoadedProfile profile;
    easy_test::loadProfile(file.path(), profile);

    REQUIRE(profile.blocksRead == 0);
    REQUIRE_FALSE(profile.readerLog.empty());
}

//////////////////////////////////////////////////////////////////////////
// Defect: the format version check has no upper bound
//////////////////////////////////////////////////////////////////////////

TEST_CASE("The reader rejects a newer format line", "[regression][format]")
{
    // DEFECT: reader.cpp:117-120 —
    //
    //     static bool isCompatibleVersion(uint32_t _version)
    //     {
    //         return _version >= MIN_COMPATIBLE_VERSION;
    //     }
    //
    // Only the lower bound is checked. A file written by a future version passes
    // and is then parsed against today's layout — silently mangled instead of
    // producing a clear error.
    //
    // AFTER THE FIX: a newer major.minor format line is rejected. Patch
    // releases in the current line remain compatible because they do not alter
    // the v2.1 wire layout.
    //
    // This cannot be fixed retroactively: already distributed reader builds
    // cannot be re-taught. The sooner a patch ships, the larger the share of
    // users who update before files of the new format appear.
    ProfImage image;
    REQUIRE(image.captureFrom([] {
        EASY_BLOCK("future_version_probe");
    }));

    image.setVersion(easy_test::makeVersion(99, 0, 0));

    easy_test::TempFile file("known_issue_future_version");
    image.writeTo(file.path());

    LoadedProfile profile;
    easy_test::loadProfile(file.path(), profile);

    REQUIRE(profile.blocksRead == 0);
    REQUIRE_FALSE(profile.readerLog.empty());
}

//////////////////////////////////////////////////////////////////////////
// Defect: counter calibration is a 100M-iteration busy loop
//////////////////////////////////////////////////////////////////////////

TEST_CASE("A dump does not recalibrate", "[regression][performance]")
{
    // DEFECT: profile_manager.cpp:1061 — the dump path calls
    // calculate_cpu_frequency() again, and that is a busy loop of 100,000,000
    // iterations (profile_manager.cpp:270-292):
    //
    //     volatile uint64_t i;
    //     for (i = 0; i < 100000000; i++); /* must be CPU intensive */
    //
    // Every dumpBlocksToFile call pays hundreds of milliseconds and a fully
    // loaded core for this. Measured on arm64: dumping 200,000 blocks takes
    // 261 ms, of which ~220 ms is calibration.
    //
    // AFTER THE FIX: the frequency is computed once and cheaply
    // (mach_timebase_info, clock_gettime, QueryPerformanceFrequency) and the dump
    // simply reads it.
    //
    // The loop has a fixed ITERATION COUNT rather than a duration, so its cost is
    // inversely proportional to CPU frequency: on a 500 MHz core it is already
    // about 1.6 s.
    easy_test::drainPending();

    profiler::setEnabled(true);
    for (int i = 0; i < 100; ++i)
    {
        EASY_BLOCK("calibration_probe");
    }
    profiler::setEnabled(false);

    easy_test::TempFile file("known_issue_dump_cost");

    const std::clock_t cpuStarted = std::clock();
    const auto started = std::chrono::steady_clock::now();
    const uint32_t dumped = profiler::dumpBlocksToFile(file.c_str());
    const auto finished = std::chrono::steady_clock::now();
    const std::clock_t cpuFinished = std::clock();

    REQUIRE(dumped == 100);

    const double elapsed = millisecondsOf(started, finished);
    const double cpuElapsed = 1000.0 * static_cast<double>(cpuFinished - cpuStarted) /
                              static_cast<double>(CLOCKS_PER_SEC);
    INFO("dumping 100 blocks took " << elapsed << " ms");
    INFO("CPU time while dumping 100 blocks was " << cpuElapsed << " ms");

    // dumpBlocksToStream deliberately waits for in-flight writers for 20 ms.
    // Wall time therefore cannot identify calibration; CPU time can, because
    // the old 100M-iteration loop kept a core busy throughout the dump.
    REQUIRE(cpuElapsed < 10.0);
}

//////////////////////////////////////////////////////////////////////////
// Defect: context switch events outside the capture window corrupt the file
//////////////////////////////////////////////////////////////////////////

#ifndef _WIN32

TEST_CASE("Context switch events outside the capture window do not corrupt the file",
          "[regression][cswitch]")
{
    // DEFECT: the header's block counter and the reader's counter count
    // different things.
    //
    // On dump, blocks_number receives ALL context switch events:
    //     uint32_t num = thread.blocks.closedList.markedSize() + thread.sync.closedList.size();
    // (profile_manager.cpp, the tally before the header is written).
    //
    // The reader, however, counts an event only if it falls inside the trace
    // window (reader.cpp:1058):
    //     if (*t_end > begin_time) { ... blocks_counter++; }
    //
    // So events that happened BEFORE capture started land in the header counter
    // but not in the reader's, and the file is rejected with
    // "Read blocks count: N does not match blocks count stored in header: M. File corrupted".
    //
    // This is not a contrived case: the SystemTap script writes its log
    // continuously from the moment it starts, so by the time capture begins the
    // log ALREADY contains events. That is exactly the normal usage described in
    // the README.
    //
    // AFTER THE FIX: either the header counts only the events that will end up in
    // the file, or the reader stops treating the mismatch as an error. A related
    // task is to clip the log to the capture window on read.
    easy_test::drainPending();

    const profiler::thread_id_t currentThread = getCurrentThreadId();
    const long processId = static_cast<long>(::getpid());

    // Timestamps deliberately BEFORE capture is enabled.
    const profiler::timestamp_t beforeCapture = profiler::timeNow();

    easy_test::TempFile logFile("known_issue_cs", ".log");
    {
        std::ofstream out(logFile.path().c_str(), std::ios::trunc);
        for (int i = 0; i < 5; ++i)
        {
            out << (beforeCapture - static_cast<profiler::timestamp_t>(5 - i) * 1000) << ' '
                << currentThread << ' ' << currentThread << ' '
                << "easy_profiler_tests" << ' ' << processId << '\n';
        }
        out << (std::numeric_limits<profiler::timestamp_t>::max() - 1000) << ' '
            << currentThread << ' ' << currentThread << ' '
            << "easy_profiler_tests_future" << ' ' << processId << '\n';
    }

    const std::string originalLog = profiler::getContextSwitchLogFilename();
    profiler::setContextSwitchLogFilename(logFile.path().c_str());

    profiler::setEventTracingEnabled(true);
    profiler::setEnabled(true);
    EASY_THREAD("known_issue_cs_thread");
    { EASY_BLOCK("known_issue_cs_block"); }
    profiler::setEnabled(false);

    easy_test::TempFile file("known_issue_cs_dump");
    const uint32_t dumped = profiler::dumpBlocksToFile(file.c_str());

    profiler::setContextSwitchLogFilename(originalLog.c_str());

    REQUIRE(dumped > 0);

    LoadedProfile profile;
    easy_test::loadProfile(file.path(), profile);

    INFO("reader log: " << profile.readerLog);
    REQUIRE(profile.readerLog.empty());
}

#endif // !_WIN32

TEST_CASE("The first block costs nothing while the profiler is disabled", "[regression][performance]")
{
    // DEFECT: profile_manager.cpp:351 — the ProfileManager constructor body calls
    // calculate_cpu_frequency() in a stock build on Linux/macOS/Android (the
    // #if !defined(EASY_CHRONO_CLOCK) && !defined(_WIN32) branch).
    //
    // The constructor runs on the first instance() call, that is on the very
    // first EASY_BLOCK — regardless of whether the profiler is enabled. Measured
    // on arm64: 220 ms with the profiler DISABLED.
    //
    // This directly contradicts the README's "Disabled profiler will not affect
    // your application execution in any way" and makes the library unsuitable for
    // living permanently in a production build.
    //
    // AFTER THE FIX: the first touch of the API costs microseconds, and
    // calibration (if needed at all) happens lazily when capture is actually
    // enabled.
    //
    // ON RUNNING THIS: the cost is paid once per process, so the test only means
    // anything in a fresh process. Under ctest that holds — every TEST_CASE runs
    // as its own invocation of the binary. Running the whole binary by hand may
    // turn this test falsely green if an earlier test already paid for the
    // initialization.
    //
    // No profiler::* call precedes the measurement: ANY call, including an
    // innocent-looking isEnabled(), creates the ProfileManager singleton and pays
    // for calibration, after which there is nothing left to measure.
    const auto started = std::chrono::steady_clock::now();
    {
        EASY_BLOCK("first_touch_probe");
    }
    const auto finished = std::chrono::steady_clock::now();

    const double elapsed = millisecondsOf(started, finished);
    INFO("the first EASY_BLOCK with the profiler disabled took " << elapsed << " ms");

    // The profiler was off the whole time — the block was not even stored.
    REQUIRE_FALSE(profiler::isEnabled());

    REQUIRE(elapsed < 5.0);
}
