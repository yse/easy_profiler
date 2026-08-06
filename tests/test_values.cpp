/** EASY_VALUE / EASY_ARRAY / EASY_TEXT / EASY_STRING.

Arbitrary values are the format's second kind of payload alongside blocks. They
share the common record prefix with blocks: ArbitraryValue carries an artificial
m_nameStub byte (serialized_block.h:174) imitating an empty block name so that
the reader walks blocks and values through a single code path. The tests below
check that every data type survives a round-trip intact and that the type-safe
accessors never hand back a foreign type.
*/

#include <catch2/catch_test_macros.hpp>

#include "support/prof_fixture.h"

#include <easy/arbitrary_value.h>
#include <easy/profiler.h>

#include <cstring>
#include <string>
#include <vector>

using easy_test::LoadedProfile;

TEST_CASE("Scalar values survive a round-trip", "[values]")
{
    LoadedProfile profile;

    easy_test::capture(profile, [] {
        EASY_BLOCK("values_host");

        const bool     asBool   = true;
        const char     asChar   = 'Z';
        const int8_t   asInt8   = -42;
        const uint8_t  asUint8  = 200;
        const int16_t  asInt16  = -12345;
        const uint16_t asUint16 = 54321;
        const int32_t  asInt32  = -1234567;
        const uint32_t asUint32 = 4000000000u;
        const int64_t  asInt64  = -1234567890123ll;
        const uint64_t asUint64 = 12345678901234567890ull;
        const float    asFloat  = 3.5f;
        const double   asDouble = 2.718281828;

        EASY_VALUE("v_bool", asBool);
        EASY_VALUE("v_char", asChar);
        EASY_VALUE("v_int8", asInt8);
        EASY_VALUE("v_uint8", asUint8);
        EASY_VALUE("v_int16", asInt16);
        EASY_VALUE("v_uint16", asUint16);
        EASY_VALUE("v_int32", asInt32);
        EASY_VALUE("v_uint32", asUint32);
        EASY_VALUE("v_int64", asInt64);
        EASY_VALUE("v_uint64", asUint64);
        EASY_VALUE("v_float", asFloat);
        EASY_VALUE("v_double", asDouble);
    });

    REQUIRE(profile.readerLog.empty());

    SECTION("bool")
    {
        const profiler::ArbitraryValue* value = easy_test::findValue(profile, "v_bool");
        REQUIRE(value != nullptr);
        REQUIRE(value->type() == profiler::DataType::Bool);
        REQUIRE_FALSE(value->isArray());
        REQUIRE(value->toValue<bool>() != nullptr);
        REQUIRE(value->toValue<bool>()->value() == true);
    }

    SECTION("char")
    {
        const profiler::ArbitraryValue* value = easy_test::findValue(profile, "v_char");
        REQUIRE(value != nullptr);
        REQUIRE(value->type() == profiler::DataType::Char);
        REQUIRE(value->toValue<char>()->value() == 'Z');
    }

    SECTION("signed integers")
    {
        REQUIRE(easy_test::findValue(profile, "v_int8")->toValue<int8_t>()->value() == -42);
        REQUIRE(easy_test::findValue(profile, "v_int16")->toValue<int16_t>()->value() == -12345);
        REQUIRE(easy_test::findValue(profile, "v_int32")->toValue<int32_t>()->value() == -1234567);
        REQUIRE(easy_test::findValue(profile, "v_int64")->toValue<int64_t>()->value() == -1234567890123ll);
    }

    SECTION("unsigned integers")
    {
        REQUIRE(easy_test::findValue(profile, "v_uint8")->toValue<uint8_t>()->value() == 200);
        REQUIRE(easy_test::findValue(profile, "v_uint16")->toValue<uint16_t>()->value() == 54321);
        REQUIRE(easy_test::findValue(profile, "v_uint32")->toValue<uint32_t>()->value() == 4000000000u);
        REQUIRE(easy_test::findValue(profile, "v_uint64")->toValue<uint64_t>()->value() == 12345678901234567890ull);
    }

    SECTION("floating point")
    {
        REQUIRE(easy_test::findValue(profile, "v_float")->toValue<float>()->value() == 3.5f);
        REQUIRE(easy_test::findValue(profile, "v_double")->toValue<double>()->value() == 2.718281828);
    }
}

TEST_CASE("The data type is recorded in the value", "[values]")
{
    LoadedProfile profile;

    easy_test::capture(profile, [] {
        EASY_BLOCK("value_types_host");
        EASY_VALUE("t_int32", static_cast<int32_t>(1));
        EASY_VALUE("t_double", 1.0);
        EASY_VALUE("t_bool", false);
    });

    REQUIRE(easy_test::findValue(profile, "t_int32")->type() == profiler::DataType::Int32);
    REQUIRE(easy_test::findValue(profile, "t_double")->type() == profiler::DataType::Double);
    REQUIRE(easy_test::findValue(profile, "t_bool")->type() == profiler::DataType::Bool);
}

TEST_CASE("Type-safe accessors reject a foreign type", "[values]")
{
    LoadedProfile profile;

    easy_test::capture(profile, [] {
        EASY_BLOCK("typed_access_host");
        EASY_VALUE("typed_int", static_cast<int32_t>(7));
    });

    const profiler::ArbitraryValue* value = easy_test::findValue(profile, "typed_int");
    REQUIRE(value != nullptr);

    REQUIRE(value->toValue<int32_t>() != nullptr);

    // A mismatched type yields nullptr rather than garbage.
    REQUIRE(value->toValue<double>() == nullptr);
    REQUIRE(value->toValue<uint64_t>() == nullptr);
    REQUIRE(value->toValue<bool>() == nullptr);

    REQUIRE(value->toArray<int32_t>() == nullptr);
}

TEST_CASE("EASY_ARRAY preserves content and size", "[values][array]")
{
    LoadedProfile profile;

    easy_test::capture(profile, [] {
        EASY_BLOCK("array_host");

        const int32_t integers[] = {10, 20, 30, 40, 50};
        EASY_ARRAY("arr_int32", integers, 5);

        const double doubles[] = {1.5, 2.5, 3.5};
        EASY_ARRAY("arr_double", doubles, 3);
    });

    REQUIRE(profile.readerLog.empty());

    SECTION("integer array")
    {
        const profiler::ArbitraryValue* value = easy_test::findValue(profile, "arr_int32");
        REQUIRE(value != nullptr);
        REQUIRE(value->isArray());

        const profiler::Value<profiler::DataType::Int32, true>* array = value->toArray<int32_t>();
        REQUIRE(array != nullptr);
        REQUIRE(array->size() == 5);

        REQUIRE(array->value()[0] == 10);
        REQUIRE(array->value()[4] == 50);
        REQUIRE(array->at(2) == 30);
        REQUIRE((*array)[3] == 40);
    }

    SECTION("floating point array")
    {
        const profiler::Value<profiler::DataType::Double, true>* array =
            easy_test::findValue(profile, "arr_double")->toArray<double>();

        REQUIRE(array != nullptr);
        REQUIRE(array->size() == 3);
        REQUIRE(array->at(0) == 1.5);
        REQUIRE(array->at(2) == 3.5);
    }
}

TEST_CASE("A one-element array is still an array", "[values][array]")
{
    LoadedProfile profile;

    easy_test::capture(profile, [] {
        EASY_BLOCK("single_element_host");
        const int32_t single[] = {99};
        EASY_ARRAY("arr_single", single, 1);
    });

    const profiler::ArbitraryValue* value = easy_test::findValue(profile, "arr_single");
    REQUIRE(value != nullptr);
    REQUIRE(value->isArray());
    REQUIRE(value->toArray<int32_t>()->size() == 1);
    REQUIRE(value->toArray<int32_t>()->at(0) == 99);
}

TEST_CASE("toValue and toArray check the array flag asymmetrically", "[values]")
{
    // Pins actual behaviour, not desired behaviour.
    //
    // toArray() checks BOTH the type and the array flag (serialized_block.h:233):
    //     return m_isArray && m_type == dataType ? ... : nullptr;
    // while toValue() checks the type ONLY (serialized_block.h:221):
    //     return m_type == dataType ? ... : nullptr;
    //
    // So an array reads successfully as a scalar and yields its first element,
    // while a scalar cannot be read as an array. Should a refactoring remove the
    // asymmetry, this test will say so.
    LoadedProfile profile;

    easy_test::capture(profile, [] {
        EASY_BLOCK("asymmetry_host");
        const int32_t values[] = {11, 22, 33};
        EASY_ARRAY("asym_array", values, 3);
        EASY_VALUE("asym_scalar", static_cast<int32_t>(44));
    });

    const profiler::ArbitraryValue* array = easy_test::findValue(profile, "asym_array");
    const profiler::ArbitraryValue* scalar = easy_test::findValue(profile, "asym_scalar");
    REQUIRE(array != nullptr);
    REQUIRE(scalar != nullptr);

    REQUIRE(array->toArray<int32_t>() != nullptr);
    REQUIRE(array->toValue<int32_t>() != nullptr);
    REQUIRE(array->toValue<int32_t>()->value() == 11);

    REQUIRE(scalar->toValue<int32_t>() != nullptr);
    REQUIRE(scalar->toArray<int32_t>() == nullptr);

    // A type mismatch is rejected in both directions.
    REQUIRE(array->toArray<double>() == nullptr);
    REQUIRE(array->toValue<double>() == nullptr);
    REQUIRE(scalar->toValue<double>() == nullptr);
}

TEST_CASE("EASY_TEXT preserves the string", "[values][text]")
{
    LoadedProfile profile;

    easy_test::capture(profile, [] {
        EASY_BLOCK("text_host");
        EASY_TEXT("txt_message", "hello easy_profiler");
    });

    REQUIRE(profile.readerLog.empty());

    const profiler::ArbitraryValue* value = easy_test::findValue(profile, "txt_message");
    REQUIRE(value != nullptr);
    REQUIRE(value->type() == profiler::DataType::String);
    REQUIRE(std::strcmp(value->data(), "hello easy_profiler") == 0);
}

TEST_CASE("Empty text is valid", "[values][text]")
{
    LoadedProfile profile;

    easy_test::capture(profile, [] {
        EASY_BLOCK("empty_text_host");
        EASY_TEXT("txt_empty", "");
    });

    const profiler::ArbitraryValue* value = easy_test::findValue(profile, "txt_empty");
    REQUIRE(value != nullptr);
    REQUIRE(value->type() == profiler::DataType::String);
    REQUIRE(std::strlen(value->data()) == 0);
}

TEST_CASE("Values belong to the block they were written inside", "[values]")
{
    LoadedProfile profile;

    easy_test::capture(profile, [] {
        EASY_BLOCK("value_owner");
        EASY_VALUE("owned_value", static_cast<int32_t>(123));
    });

    REQUIRE(profile.readerLog.empty());
    REQUIRE(easy_test::findValue(profile, "owned_value") != nullptr);
    REQUIRE(easy_test::findBlockIndex(profile, "value_owner") >= 0);
}

TEST_CASE("EASY_VIN gives a value series a stable id", "[values][vin]")
{
    LoadedProfile profile;

    easy_test::capture(profile, [] {
        EASY_BLOCK("vin_host");
        // The address of a local changes between calls, which is why the README
        // recommends EASY_VIN for a stable series id.
        for (int i = 0; i < 3; ++i)
        {
            uint32_t n = static_cast<uint32_t>(i);
            EASY_VALUE("vin_series", n, EASY_VIN("vin_series"));
        }
    });

    REQUIRE(profile.readerLog.empty());

    profiler::vin_t identifier = 0;
    int seen = 0;

    for (size_t i = 0; i < profile.blocks.size(); ++i)
    {
        const profiler::BlocksTree& entry = profile.blocks[i];
        if (entry.node == nullptr)
            continue;

        const profiler::block_id_t id = entry.node->id();
        if (id >= profile.descriptors.size())
            continue;

        const profiler::SerializedBlockDescriptor* descriptor = profile.descriptors[id];
        if (descriptor == nullptr || descriptor->type() != profiler::BlockType::Value)
            continue;
        if (std::strcmp(descriptor->name(), "vin_series") != 0)
            continue;

        if (seen == 0)
            identifier = entry.value->value_id();
        else
            REQUIRE(entry.value->value_id() == identifier);

        ++seen;
    }

    REQUIRE(seen == 3);
}

TEST_CASE("Several values in one block do not get mixed up", "[values]")
{
    LoadedProfile profile;

    easy_test::capture(profile, [] {
        EASY_BLOCK("multi_value_host");
        EASY_VALUE("multi_a", static_cast<int32_t>(1));
        EASY_VALUE("multi_b", static_cast<int32_t>(2));
        EASY_VALUE("multi_c", 3.0);
        EASY_TEXT("multi_d", "four");
    });

    REQUIRE(easy_test::findValue(profile, "multi_a")->toValue<int32_t>()->value() == 1);
    REQUIRE(easy_test::findValue(profile, "multi_b")->toValue<int32_t>()->value() == 2);
    REQUIRE(easy_test::findValue(profile, "multi_c")->toValue<double>()->value() == 3.0);
    REQUIRE(std::strcmp(easy_test::findValue(profile, "multi_d")->data(), "four") == 0);
}

TEST_CASE("The value data size matches the type", "[values]")
{
    LoadedProfile profile;

    easy_test::capture(profile, [] {
        EASY_BLOCK("size_host");
        EASY_VALUE("size_int32", static_cast<int32_t>(1));
        EASY_VALUE("size_int64", static_cast<int64_t>(1));

        const int32_t five[] = {1, 2, 3, 4, 5};
        EASY_ARRAY("size_array", five, 5);
    });

    REQUIRE(easy_test::findValue(profile, "size_int32")->data_size() == sizeof(int32_t));
    REQUIRE(easy_test::findValue(profile, "size_int64")->data_size() == sizeof(int64_t));
    REQUIRE(easy_test::findValue(profile, "size_array")->data_size() == 5 * sizeof(int32_t));
}

TEST_CASE("A large array within the allowed size", "[values][array]")
{
    LoadedProfile profile;

    // MAX_BLOCK_DATA_SIZE is 2816 bytes; 512 * 4 == 2048 stays well below it.
    const uint16_t count = 512;
    easy_test::capture(profile, [count] {
        EASY_BLOCK("big_array_host");
        std::vector<int32_t> data(count);
        for (uint16_t i = 0; i < count; ++i)
            data[i] = static_cast<int32_t>(i);
        EASY_ARRAY("arr_big", data.data(), count);
    });

    REQUIRE(profile.readerLog.empty());

    const profiler::Value<profiler::DataType::Int32, true>* array =
        easy_test::findValue(profile, "arr_big")->toArray<int32_t>();

    REQUIRE(array != nullptr);
    REQUIRE(array->size() == count);
    REQUIRE(array->at(0) == 0);
    REQUIRE(array->at(count - 1) == count - 1);
}
