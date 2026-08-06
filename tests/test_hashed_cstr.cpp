/** hashed_cstr.h — the key type of the descriptor table.

ProfileManager maps "unique call-site id -> block_id_t" in an unordered_map keyed
by profiler::string_with_hash (profile_manager.h:88). This is what implements
descriptor deduplication: a block's static information (name, file, line, color)
is stored once, and every block instance carries only a 4-byte id.

Platform note: the cstring/hashed_cstr branch is disabled by `#if 0 == 1`
(hashed_cstr.h:155), so hashed_stdstring is what lives on every platform and
string_with_hash is an alias for it. These tests exercise what actually compiles.
*/

#include <catch2/catch_test_macros.hpp>

#include "hashed_cstr.h"

#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

TEST_CASE("string_with_hash is hashed_stdstring", "[hash]")
{
    // Should this stop holding, the tests below exercise the wrong type and the
    // descriptor table changes behaviour.
    const bool sameType = std::is_same<profiler::string_with_hash, profiler::hashed_stdstring>::value;
    REQUIRE(sameType);
}

TEST_CASE("Construction and content access", "[hash]")
{
    SECTION("from const char*")
    {
        profiler::hashed_stdstring value("EASY_BLOCK@main.cpp,42");
        REQUIRE(std::string(value.c_str()) == "EASY_BLOCK@main.cpp,42");
        REQUIRE(value.size() == 22);
    }

    SECTION("from std::string")
    {
        std::string source("descriptor-key");
        profiler::hashed_stdstring value(source);
        REQUIRE(std::string(value.c_str()) == source);
        REQUIRE(value.size() == source.size());
    }

    SECTION("empty string")
    {
        profiler::hashed_stdstring value("");
        REQUIRE(value.size() == 0);
        REQUIRE(std::string(value.c_str()).empty());
    }
}

TEST_CASE("The hash is deterministic and address-independent", "[hash]")
{
    const char first[] = "one-and-the-same";
    const char second[] = "one-and-the-same";
    REQUIRE(static_cast<const void*>(first) != static_cast<const void*>(second));

    profiler::hashed_stdstring a(first);
    profiler::hashed_stdstring b(second);

    REQUIRE(a.hcode() == b.hcode());
    REQUIRE(a == b);
    REQUIRE_FALSE(a != b);
}

TEST_CASE("Different content yields different objects", "[hash]")
{
    profiler::hashed_stdstring a("alpha");
    profiler::hashed_stdstring b("beta");

    REQUIRE(a != b);
    REQUIRE_FALSE(a == b);
}

TEST_CASE("Equality considers both hash and content", "[hash]")
{
    // The explicit-hash constructor lets a foreign hash be planted; operator==
    // compares hash and string, so the forgery does not pass.
    profiler::hashed_stdstring genuine("payload");
    profiler::hashed_stdstring forged("payload", genuine.hcode() ^ 0x1);

    REQUIRE(std::string(genuine.c_str()) == std::string(forged.c_str()));
    REQUIRE(genuine != forged);
}

TEST_CASE("std::hash returns the precomputed value", "[hash]")
{
    profiler::hashed_stdstring value("cached");
    const std::hash<profiler::hashed_stdstring> hasher;

    REQUIRE(hasher(value) == value.hcode());
    REQUIRE(hasher(value) == hasher(value));
}

TEST_CASE("Works as an unordered_map key", "[hash]")
{
    // Exactly the usage in ProfileManager::m_descriptorsMap.
    std::unordered_map<profiler::string_with_hash, uint32_t> table;

    table[profiler::string_with_hash("block-a")] = 1;
    table[profiler::string_with_hash("block-b")] = 2;
    table[profiler::string_with_hash("block-c")] = 3;

    REQUIRE(table.size() == 3);

    REQUIRE(table.find(profiler::string_with_hash("block-a")) != table.end());
    REQUIRE(table[profiler::string_with_hash("block-a")] == 1);
    REQUIRE(table[profiler::string_with_hash("block-c")] == 3);

    REQUIRE(table.find(profiler::string_with_hash("block-missing")) == table.end());

    // Re-inserting the same key updates rather than appends.
    table[profiler::string_with_hash("block-a")] = 100;
    REQUIRE(table.size() == 3);
    REQUIRE(table[profiler::string_with_hash("block-a")] == 100);
}

TEST_CASE("Copy and move preserve the hash", "[hash]")
{
    profiler::hashed_stdstring original("movable");
    const size_t originalHash = original.hcode();

    profiler::hashed_stdstring copied(original);
    REQUIRE(copied.hcode() == originalHash);
    REQUIRE(copied == original);

    profiler::hashed_stdstring moved(std::move(original));
    REQUIRE(moved.hcode() == originalHash);
    REQUIRE(std::string(moved.c_str()) == "movable");
}

TEST_CASE("Bulk insertion loses no keys", "[hash]")
{
    // Collision stress: a real application's descriptor table holds hundreds to
    // thousands of entries.
    std::unordered_map<profiler::string_with_hash, size_t> table;
    std::vector<std::string> keys;

    const size_t count = 2000;
    keys.reserve(count);
    for (size_t i = 0; i < count; ++i)
        keys.push_back("EASY_BLOCK@some/path/file.cpp," + std::to_string(i));

    for (size_t i = 0; i < count; ++i)
        table[profiler::string_with_hash(keys[i])] = i;

    REQUIRE(table.size() == count);

    for (size_t i = 0; i < count; ++i)
    {
        auto found = table.find(profiler::string_with_hash(keys[i]));
        REQUIRE(found != table.end());
        REQUIRE(found->second == i);
    }
}

TEST_CASE("Strings containing an embedded NUL", "[hash]")
{
    // hashed_stdstring is built from std::string, so interior nulls survive.
    std::string withNul("abc");
    withNul.push_back('\0');
    withNul += "def";

    profiler::hashed_stdstring value(withNul);
    REQUIRE(value.size() == 7);

    profiler::hashed_stdstring same(withNul);
    REQUIRE(value == same);
}
