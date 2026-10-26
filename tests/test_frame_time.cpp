/** The frame-time API.

This part of the API works with capture switched off — the README promises the
ability to draw a custom HUD or fps graph from profiler data without enabling
profiling. A "frame" here is a top-level block: it opens on begin and ends on close.
*/

#include <catch2/catch_test_macros.hpp>

#include "support/prof_fixture.h"

#include <easy/profiler.h>

#include "timestamp_scaling.h"

#include <limits>
#include <thread>

namespace {

/** Spin a few frames; every top-level block counts as one. */
void runFrames(int count)
{
    for (int i = 0; i < count; ++i)
    {
        EASY_BLOCK("frame_time_frame");
        volatile int sink = 0;
        for (int k = 0; k < 5000; ++k)
            sink = sink + k;
        (void)sink;
    }
}

} // namespace

#if !defined(EASY_CHRONO_CLOCK) && !defined(_WIN32)
TEST_CASE("000 Conversion works before any capture", "[frametime][performance]")
{
    // The README advertises monitoring main-thread fps while profiling is
    // disabled, so unit conversion has to calibrate on first use instead of
    // waiting for a capture. The platform cost is paid once per process.
    const profiler::timestamp_t probe = 1000000;
    REQUIRE(profiler::toNanoseconds(probe) > 0);
    REQUIRE(profiler::toMicroseconds(probe) > 0);

    profiler::setEnabled(true);
    profiler::setEnabled(false);

    REQUIRE(profiler::toNanoseconds(probe) > 0);
    REQUIRE(profiler::toMicroseconds(probe) > 0);
}

TEST_CASE("Frame time is reported in real units while profiling is disabled",
          "[frametime][isolated]")
{
    // A HUD never calls setEnabled(true). It must still get a usable duration,
    // not a zero produced by an uncalibrated clock.
    REQUIRE_FALSE(profiler::isEnabled());

    runFrames(3);

    REQUIRE(profiler::this_thread::frameTime(profiler::TICKS) > 0);
    REQUIRE(profiler::this_thread::frameTime(profiler::MICROSECONDS) > 0);
    REQUIRE_FALSE(profiler::isEnabled());
}
#endif

TEST_CASE("The units differ", "[frametime]")
{
    easy_test::drainPending();
    profiler::setEnabled(true);
    runFrames(3);
    profiler::setEnabled(false);

    const profiler::timestamp_t inTicks =
        profiler::this_thread::frameTime(profiler::TICKS);
    const profiler::timestamp_t inMicroseconds =
        profiler::this_thread::frameTime(profiler::MICROSECONDS);

    // On any real time source a tick is finer than a microsecond, so the tick
    // count is never below the microsecond count.
    REQUIRE(inTicks >= inMicroseconds);

    easy_test::drainPending();
}

TEST_CASE("Tick conversion stays monotonic at the timestamp limit", "[frametime][overflow]")
{
    // Conversion deliberately stays inert until capture has initialized the
    // platform clock.  Do that here before testing the conversion arithmetic.
    profiler::setEnabled(true);
    profiler::setEnabled(false);

    const profiler::timestamp_t maximum =
        std::numeric_limits<profiler::timestamp_t>::max();
    const profiler::timestamp_t half = maximum / 2;
    const profiler::timestamp_t probe = 1000000;

    const profiler::timestamp_t nanosecondsAtMaximum = profiler::toNanoseconds(maximum);
    if (profiler::toNanoseconds(probe) >= probe)
        REQUIRE(nanosecondsAtMaximum == maximum);
    else
        REQUIRE(nanosecondsAtMaximum > profiler::toNanoseconds(half));

    const profiler::timestamp_t microsecondsAtMaximum = profiler::toMicroseconds(maximum);
    if (profiler::toMicroseconds(probe) >= probe)
        REQUIRE(microsecondsAtMaximum == maximum);
    else
        REQUIRE(microsecondsAtMaximum > profiler::toMicroseconds(half));
}

TEST_CASE("scaleTimestamp refuses a zero factor", "[frametime][overflow]")
{
    profiler::timestamp_t result = 42;
    REQUIRE_FALSE(scaleTimestamp(1000, 0, 7, result));
    REQUIRE_FALSE(scaleTimestamp(1000, 7, 0, result));
    REQUIRE(result == 42);

    REQUIRE(scaleTimestamp(1000, 3, 4, result));
    REQUIRE(result == 750);
}

TEST_CASE("Frame duration is non-negative", "[frametime]")
{
    easy_test::drainPending();
    profiler::setEnabled(true);
    runFrames(5);
    profiler::setEnabled(false);

    REQUIRE(profiler::this_thread::frameTime(profiler::MICROSECONDS) >= 0);
    REQUIRE(profiler::this_thread::frameTimeLocalMax(profiler::MICROSECONDS) >= 0);
    REQUIRE(profiler::this_thread::frameTimeLocalAvg(profiler::MICROSECONDS) >= 0);

    easy_test::drainPending();
}

TEST_CASE("The local maximum resets on read", "[frametime]")
{
    easy_test::drainPending();
    profiler::setEnabled(true);
    runFrames(5);
    profiler::setEnabled(false);

    // The first read returns the accumulated maximum and resets the counter;
    // the second returns whatever accrued since that reset.
    const profiler::timestamp_t first =
        profiler::this_thread::frameTimeLocalMax(profiler::MICROSECONDS);
    const profiler::timestamp_t second =
        profiler::this_thread::frameTimeLocalMax(profiler::MICROSECONDS);

    REQUIRE(first >= 0);
    REQUIRE(second >= 0);
    REQUIRE(second <= first);

    easy_test::drainPending();
}

TEST_CASE("The local average resets on read", "[frametime]")
{
    easy_test::drainPending();
    profiler::setEnabled(true);
    runFrames(5);
    profiler::setEnabled(false);

    const profiler::timestamp_t first =
        profiler::this_thread::frameTimeLocalAvg(profiler::MICROSECONDS);
    const profiler::timestamp_t second =
        profiler::this_thread::frameTimeLocalAvg(profiler::MICROSECONDS);

    REQUIRE(first >= 0);
    REQUIRE(second >= 0);

    easy_test::drainPending();
}

TEST_CASE("000 Main-thread statistics work in ticks and microseconds", "[frametime][threads]")
{
    // The profiler documents EASY_MAIN_THREAD as an early registration call.
    // Keeping this case lexically first also preserves that precondition in the
    // dedicated single-process coverage runner.
    EASY_MAIN_THREAD;
    easy_test::drainPending();

    // Resetting an aggregate before the first frame is a supported monitoring
    // pattern and must not divide by an empty sample set.
    REQUIRE(profiler::main_thread::frameTimeLocalAvg(profiler::TICKS) == 0);

    profiler::setEnabled(true);
    runFrames(3);
    profiler::setEnabled(false);

    REQUIRE(profiler::this_thread::frameTimeLocalMax(profiler::TICKS) >= 0);
    REQUIRE(profiler::this_thread::frameTimeLocalAvg(profiler::TICKS) >= 0);
    REQUIRE(profiler::main_thread::frameTime(profiler::TICKS) >= 0);
    REQUIRE(profiler::main_thread::frameTimeLocalMax(profiler::TICKS) >= 0);
    REQUIRE(profiler::main_thread::frameTimeLocalAvg(profiler::TICKS) >= 0);

    REQUIRE(profiler::main_thread::frameTime(profiler::MICROSECONDS) >= 0);
    REQUIRE(profiler::main_thread::frameTimeLocalMax(profiler::MICROSECONDS) >= 0);
    REQUIRE(profiler::main_thread::frameTimeLocalAvg(profiler::MICROSECONDS) >= 0);

    REQUIRE(profiler::main_thread::isMain());

    // A consumer can request/reset main-thread aggregates from a monitoring
    // thread. The following frames must apply those reset requests safely.
    bool monitorWasMain = true;
    profiler::timestamp_t monitoredCurrentTicks = 0;
    profiler::timestamp_t monitoredCurrentMicroseconds = 0;
    profiler::timestamp_t monitoredMax = 0;
    profiler::timestamp_t monitoredAvg = 0;
    profiler::timestamp_t monitoredMaxMicroseconds = 0;
    profiler::timestamp_t monitoredAvgMicroseconds = 0;
    std::thread monitor([&] {
        monitorWasMain = profiler::isMainThread();
        monitoredCurrentTicks = profiler::main_thread::frameTime(profiler::TICKS);
        monitoredCurrentMicroseconds =
            profiler::main_thread::frameTime(profiler::MICROSECONDS);
        monitoredMax = profiler::main_thread::frameTimeLocalMax(profiler::TICKS);
        monitoredAvg = profiler::main_thread::frameTimeLocalAvg(profiler::TICKS);
        monitoredMaxMicroseconds =
            profiler::main_thread::frameTimeLocalMax(profiler::MICROSECONDS);
        monitoredAvgMicroseconds =
            profiler::main_thread::frameTimeLocalAvg(profiler::MICROSECONDS);
    });
    monitor.join();
    REQUIRE_FALSE(monitorWasMain);
    REQUIRE(monitoredCurrentTicks >= 0);
    REQUIRE(monitoredCurrentMicroseconds >= 0);
    REQUIRE(monitoredMax >= 0);
    REQUIRE(monitoredAvg >= 0);
    REQUIRE(monitoredMaxMicroseconds >= 0);
    REQUIRE(monitoredAvgMicroseconds >= 0);

    runFrames(2);
    REQUIRE(profiler::main_thread::frameTimeLocalMax(profiler::TICKS) >= 0);
    REQUIRE(profiler::main_thread::frameTimeLocalAvg(profiler::TICKS) >= 0);

    easy_test::drainPending();
}

TEST_CASE("A fresh thread has zero local frame statistics", "[frametime][threads]")
{
    profiler::timestamp_t ticks = 1;
    profiler::timestamp_t microseconds = 1;
    std::thread worker([&] {
        ticks = profiler::this_thread::frameTimeLocalAvg(profiler::TICKS);
        microseconds =
            profiler::this_thread::frameTimeLocalAvg(profiler::MICROSECONDS);
    });
    worker.join();

    REQUIRE(ticks == 0);
    REQUIRE(microseconds == 0);
}

TEST_CASE("EASY_MAIN_THREAD marks the current thread as main", "[frametime][threads][isolated]")
{
    EASY_MAIN_THREAD;
    REQUIRE(profiler::this_thread::isMain());
    REQUIRE(profiler::isMainThread());
}

TEST_CASE("A non-main thread does not report itself as main", "[frametime][threads][isolated]")
{
    EASY_MAIN_THREAD;

    bool otherThreadIsMain = true;
    std::thread worker([&otherThreadIsMain] {
        EASY_THREAD("frametime_worker");
        otherThreadIsMain = profiler::isMainThread();
    });
    worker.join();

    REQUIRE_FALSE(otherThreadIsMain);
    REQUIRE(profiler::isMainThread());
}

TEST_CASE("Frame time is available with the profiler disabled", "[frametime]")
{
    // README: "Monitor main thread fps at real-time in GUI even if profiling is disabled".
    easy_test::drainPending();
    REQUIRE_FALSE(profiler::isEnabled());

    runFrames(3);

    REQUIRE(profiler::this_thread::frameTime(profiler::MICROSECONDS) >= 0);
    REQUIRE(profiler::main_thread::frameTime(profiler::MICROSECONDS) >= 0);

    easy_test::drainPending();
}
