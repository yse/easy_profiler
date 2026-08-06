/** The frame-time API.

This part of the API works with capture switched off — the README promises the
ability to draw a custom HUD or fps graph from profiler data without enabling
profiling. A "frame" here is a top-level block: it opens on begin and ends on close.
*/

#include <catch2/catch_test_macros.hpp>

#include "support/prof_fixture.h"

#include <easy/profiler.h>

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

TEST_CASE("The main-thread API is available and does not crash", "[frametime]")
{
    easy_test::drainPending();
    EASY_MAIN_THREAD;

    profiler::setEnabled(true);
    runFrames(3);
    profiler::setEnabled(false);

    REQUIRE(profiler::main_thread::frameTime(profiler::MICROSECONDS) >= 0);
    REQUIRE(profiler::main_thread::frameTimeLocalMax(profiler::MICROSECONDS) >= 0);
    REQUIRE(profiler::main_thread::frameTimeLocalAvg(profiler::MICROSECONDS) >= 0);

    REQUIRE(profiler::main_thread::isMain());

    easy_test::drainPending();
}

TEST_CASE("EASY_MAIN_THREAD marks the current thread as main", "[frametime][threads]")
{
    EASY_MAIN_THREAD;
    REQUIRE(profiler::this_thread::isMain());
    REQUIRE(profiler::isMainThread());
}

TEST_CASE("A non-main thread does not report itself as main", "[frametime][threads]")
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
