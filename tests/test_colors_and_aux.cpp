/** profiler_aux.h and profiler_colors.h - macro argument parsing.

EASY_BLOCK("name", ...) accepts a color and an enable flag in any order and in
any combination, including neither. Parsing happens at compile time through the
extract_color/extract_enable_flag overloads. These functions decide the color and
status a descriptor is registered with, so a mistake here silently changes
instrumentation behaviour across the whole application.
*/

#include <catch2/catch_test_macros.hpp>

#include <easy/details/profiler_aux.h>
#include <easy/details/profiler_colors.h>

TEST_CASE("color_t is a four-byte ARGB value", "[colors]")
{
    REQUIRE(sizeof(profiler::color_t) == 4);

    REQUIRE((profiler::colors::Amber100 & 0xff000000u) == 0xff000000u);
    REQUIRE((profiler::colors::Red & 0xff000000u) == 0xff000000u);
}

TEST_CASE("The default color is Amber100", "[colors]")
{
    // The README promises Amber100 for a block with no explicit color.
    REQUIRE(profiler::colors::Default == 0xffffecb3u);
    REQUIRE(profiler::colors::Amber100 == 0xffffecb3u);
    REQUIRE(profiler::colors::Wheat == profiler::colors::Amber100);
}

TEST_CASE("Color aliases point at palette shades", "[colors]")
{
    REQUIRE(profiler::colors::Red == profiler::colors::Red500);
    REQUIRE(profiler::colors::DarkMagenta == profiler::colors::PurpleA700);
}

TEST_CASE("extract_color parses the macro arguments", "[aux]")
{
    using profiler::extract_color;

    SECTION("no arguments: the default color")
    {
        REQUIRE(extract_color() == profiler::colors::Default);
    }

    SECTION("a single color argument")
    {
        REQUIRE(extract_color(profiler::colors::Red) == profiler::colors::Red);
        REQUIRE(extract_color(profiler::color_t(0x12345678u)) == 0x12345678u);
    }

    SECTION("a single non-color argument: the default color")
    {
        REQUIRE(extract_color(profiler::ON) == profiler::colors::Default);
        REQUIRE(extract_color(profiler::OFF) == profiler::colors::Default);
    }

    SECTION("color first, flag second")
    {
        REQUIRE(extract_color(profiler::colors::Blue, profiler::OFF) == profiler::colors::Blue);
    }

    SECTION("flag first, color second")
    {
        REQUIRE(extract_color(profiler::OFF, profiler::colors::Blue) == profiler::colors::Blue);
    }

    SECTION("two arguments, neither a color")
    {
        REQUIRE(extract_color(profiler::ON, profiler::FORCE_ON) == profiler::colors::Default);
    }
}

TEST_CASE("extract_enable_flag parses the macro arguments", "[aux]")
{
    using profiler::extract_enable_flag;

    SECTION("no arguments: the block is enabled")
    {
        REQUIRE(extract_enable_flag() == profiler::ON);
    }

    SECTION("a single flag argument")
    {
        REQUIRE(extract_enable_flag(profiler::OFF) == profiler::OFF);
        REQUIRE(extract_enable_flag(profiler::FORCE_ON) == profiler::FORCE_ON);
        REQUIRE(extract_enable_flag(profiler::ON_WITHOUT_CHILDREN) == profiler::ON_WITHOUT_CHILDREN);
    }

    SECTION("a single color argument: the block is enabled")
    {
        REQUIRE(extract_enable_flag(profiler::colors::Red) == profiler::ON);
    }

    SECTION("flag first, color second")
    {
        REQUIRE(extract_enable_flag(profiler::OFF, profiler::colors::Red) == profiler::OFF);
    }

    SECTION("color first, flag second")
    {
        REQUIRE(extract_enable_flag(profiler::colors::Red, profiler::OFF) == profiler::OFF);
    }
}

TEST_CASE("Argument parsing happens at compile time", "[aux]")
{
    // Were the parsing not constexpr, instrumentation would pay for it at runtime.
    static_assert(profiler::extract_color() == profiler::colors::Default,
                  "extract_color must be evaluated at compile time");
    static_assert(profiler::extract_enable_flag() == profiler::ON,
                  "extract_enable_flag must be evaluated at compile time");

    SUCCEED("static assertions passed at compile time");
}

TEST_CASE("EasyBlockStatus values compose as bits", "[aux]")
{
    // The statuses are designed as a bit combination and the core relies on it.
    REQUIRE(profiler::OFF == 0);
    REQUIRE(profiler::ON == 1);
    REQUIRE(profiler::FORCE_ON == (profiler::ON | 2));
    REQUIRE(profiler::OFF_RECURSIVE == 4);
    REQUIRE(profiler::ON_WITHOUT_CHILDREN == (profiler::ON | profiler::OFF_RECURSIVE));
    REQUIRE(profiler::FORCE_ON_WITHOUT_CHILDREN == (profiler::FORCE_ON | profiler::OFF_RECURSIVE));

    // "Block enabled" is a test of the low bit.
    REQUIRE((profiler::ON & profiler::ON) != 0);
    REQUIRE((profiler::FORCE_ON & profiler::ON) != 0);
    REQUIRE((profiler::ON_WITHOUT_CHILDREN & profiler::ON) != 0);
    REQUIRE((profiler::OFF & profiler::ON) == 0);
    REQUIRE((profiler::OFF_RECURSIVE & profiler::ON) == 0);

    // "Children disabled" is a test of the OFF_RECURSIVE bit.
    REQUIRE((profiler::ON_WITHOUT_CHILDREN & profiler::OFF_RECURSIVE) != 0);
    REQUIRE((profiler::ON & profiler::OFF_RECURSIVE) == 0);
}
