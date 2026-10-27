// Asserts the compiled library agrees with the VERSION file, and that the wire
// format version is where it belongs.
//
// The expected product version is read from VERSION at run time rather than
// taken from a compile definition: a definition would come from the same CMake
// variable the library is built with, so the comparison would hold even if both
// had drifted away from the file. The format version is written out here as
// literals, so bumping the product version cannot move it and moving it
// requires editing this file on purpose.

#include <catch2/catch_test_macros.hpp>

#include <easy/profiler.h>

#include <cstdio>
#include <fstream>
#include <sstream>
#include <string>

#ifndef EASY_TEST_SOURCE_DIR
#  error EASY_TEST_SOURCE_DIR must be defined
#endif

namespace {

struct Version
{
    unsigned major = 0;
    unsigned minor = 0;
    unsigned patch = 0;
    std::string text;
};

Version readVersionFile()
{
    const std::string path = std::string(EASY_TEST_SOURCE_DIR) + "/../VERSION";
    std::ifstream file(path.c_str());
    REQUIRE(file.is_open());

    std::string line;
    REQUIRE(std::getline(file, line));

    while (!line.empty() && (line.back() == '\r' || line.back() == '\n' || line.back() == ' '))
    {
        line.pop_back();
    }

    Version version;
    version.text = line;

    char dot1 = 0;
    char dot2 = 0;
    std::istringstream stream(line);
    stream >> version.major >> dot1 >> version.minor >> dot2 >> version.patch;
    REQUIRE_FALSE(stream.fail());
    REQUIRE(dot1 == '.');
    REQUIRE(dot2 == '.');

    return version;
}

} // namespace

TEST_CASE("the library reports the version in the VERSION file", "[version]")
{
    const Version expected = readVersionFile();

    CHECK(static_cast<unsigned>(profiler::apiVersionMajor()) == expected.major);
    CHECK(static_cast<unsigned>(profiler::apiVersionMinor()) == expected.minor);
    CHECK(static_cast<unsigned>(profiler::apiVersionPatch()) == expected.patch);

    REQUIRE(profiler::apiVersionName() != nullptr);
    CHECK(std::string(profiler::apiVersionName()) == "v" + expected.text);

    const uint32_t packed = (expected.major << 24) | (expected.minor << 16) | expected.patch;
    CHECK(profiler::apiVersion() == packed);
}

TEST_CASE("the wire format version is pinned to 2.1.0", "[version]")
{
    CHECK(static_cast<unsigned>(profiler::formatVersionMajor()) == 2u);
    CHECK(static_cast<unsigned>(profiler::formatVersionMinor()) == 1u);
    CHECK(static_cast<unsigned>(profiler::formatVersionPatch()) == 0u);
    CHECK(profiler::formatVersion() == ((2u << 24) | (1u << 16) | 0u));
}
