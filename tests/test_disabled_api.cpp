/** The fully disabled profiler path.

This translation unit is built with -DDISABLE_EASY_PROFILER, so
USING_EASY_PROFILER stays undefined, every macro expands to nothing and the
profiler::* functions become inline stubs (profiler.h:378-418, 768-805).

What is checked here is the README's central promise: "Disabled profiler will not
affect your application execution in any way". Replaces the former
tests/profiler_disabled_api.cpp.
*/

#include <catch2/catch_test_macros.hpp>

#include <easy/arbitrary_value.h>
#include <easy/profiler.h>

#include <cstdio>
#include <fstream>
#include <sstream>
#include <string>

namespace {

std::string tempName(const char* stem)
{
    static int counter = 0;
    std::ostringstream stream;
    stream << stem << '_' << counter++ << ".prof";
    return stream.str();
}

bool fileExists(const std::string& path)
{
    std::ifstream file(path.c_str(), std::ios::binary);
    return file.good();
}

/** A workload using every macro: must compile and do nothing. */
void instrumentedWork()
{
    EASY_FUNCTION(profiler::colors::Red);
    EASY_BLOCK("disabled_block", profiler::colors::Blue);
    EASY_NONSCOPED_BLOCK("disabled_nonscoped");
    EASY_END_BLOCK;
    EASY_EVENT("disabled_event");

    EASY_VALUE("disabled_value", 7);

    const int numbers[] = {1, 2, 3};
    EASY_ARRAY("disabled_array", numbers, 3);
    EASY_TEXT("disabled_text", "text");

    EASY_THREAD("disabled_thread");
    EASY_MAIN_THREAD;
}

} // namespace

TEST_CASE("USING_EASY_PROFILER is not defined", "[disabled]")
{
#ifdef USING_EASY_PROFILER
    FAIL("USING_EASY_PROFILER is defined, so this target was built without "
         "DISABLE_EASY_PROFILER and these tests exercise the wrong path");
#else
    SUCCEED("the profiler is disabled at compile time");
#endif
}

TEST_CASE("Every macro compiles and is a no-op", "[disabled]")
{
    EASY_PROFILER_ENABLE;
    instrumentedWork();
    EASY_PROFILER_DISABLE;

    SUCCEED("instrumented code built and ran without a profiler");
}

TEST_CASE("The profiler cannot be enabled", "[disabled]")
{
    EASY_PROFILER_ENABLE;
    REQUIRE_FALSE(profiler::isEnabled());

    profiler::setEnabled(true);
    REQUIRE_FALSE(profiler::isEnabled());
    REQUIRE_FALSE(profiler::isCapturing());

    profiler::startCapture();
    REQUIRE_FALSE(profiler::isEnabled());
}

TEST_CASE("A dump returns nothing and creates no file", "[disabled]")
{
    const std::string path = tempName("disabled_dump");

    EASY_PROFILER_ENABLE;
    instrumentedWork();

    const uint32_t dumped = profiler::dumpBlocksToFile(path.c_str());

    REQUIRE(dumped == 0);
    REQUIRE_FALSE(fileExists(path));

    std::remove(path.c_str());
}

TEST_CASE("The time stubs return zero", "[disabled]")
{
    REQUIRE(profiler::timeNow() == 0);
    REQUIRE(profiler::toNanoseconds(1000) == 0);
    REQUIRE(profiler::toMicroseconds(1000) == 0);
    REQUIRE(profiler::currentTime() == 0);
}

TEST_CASE("The frame-time stubs return zero", "[disabled]")
{
    REQUIRE(profiler::this_thread::frameTime(profiler::MICROSECONDS) == 0);
    REQUIRE(profiler::this_thread::frameTimeLocalMax(profiler::MICROSECONDS) == 0);
    REQUIRE(profiler::this_thread::frameTimeLocalAvg(profiler::MICROSECONDS) == 0);

    REQUIRE(profiler::main_thread::frameTime(profiler::MICROSECONDS) == 0);
    REQUIRE(profiler::main_thread::frameTimeLocalMax(profiler::MICROSECONDS) == 0);
    REQUIRE(profiler::main_thread::frameTimeLocalAvg(profiler::MICROSECONDS) == 0);
}

TEST_CASE("The thread registration stub returns an empty string", "[disabled]")
{
    const char* name = profiler::registerThread("ignored_name");
    REQUIRE(name != nullptr);
    REQUIRE(std::string(name).empty());

    REQUIRE_FALSE(profiler::isMainThread());
    REQUIRE_FALSE(profiler::this_thread::isMain());

    // main_thread::isMain is a constant and stays true even here.
    REQUIRE(profiler::main_thread::isMain());
}

TEST_CASE("The version is marked as disabled", "[disabled]")
{
    REQUIRE(profiler::apiVersionMajor() == 0);
    REQUIRE(profiler::apiVersionMinor() == 0);
    REQUIRE(profiler::apiVersionPatch() == 0);
    REQUIRE(profiler::apiVersion() == 0);
    REQUIRE(std::string(profiler::apiVersionName()) == "v0.0.0_disabled");
}

TEST_CASE("The network stubs start no threads", "[disabled]")
{
    profiler::startListen(45300);
    REQUIRE_FALSE(profiler::isListening());

    profiler::stopListen();
    REQUIRE_FALSE(profiler::isListening());
}

TEST_CASE("The event tracing stubs stay off", "[disabled]")
{
    profiler::setEventTracingEnabled(true);
    REQUIRE_FALSE(profiler::isEventTracingEnabled());

    profiler::setLowPriorityEventTracing(false);
    REQUIRE_FALSE(profiler::isLowPriorityEventTracing());
}

TEST_CASE("registerDescription returns a non-null stub", "[disabled]")
{
    // The stub hands back 0xbad rather than nullptr: the macros never dereference
    // it, but a null check in user code must not fire spuriously.
    const profiler::BaseBlockDescriptor* descriptor = profiler::registerDescription(
        profiler::ON, "disabled_desc_id", "disabled_desc", __FILE__, __LINE__,
        profiler::BlockType::Block, profiler::colors::Default, false);

    REQUIRE(descriptor != nullptr);
}

TEST_CASE("Calling instrumented code repeatedly is harmless", "[disabled]")
{
    for (int i = 0; i < 1000; ++i)
        instrumentedWork();

    REQUIRE_FALSE(profiler::isEnabled());
    SUCCEED("a thousand passes over disabled instrumentation completed");
}
