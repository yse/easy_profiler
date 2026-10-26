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
#include <cstdio>
#include <limits>
#include <sstream>
#include <string>
#include <thread>
#include <type_traits>
#include <utility>
#include <vector>

#ifdef _WIN32
# include <io.h>
#else
# include <unistd.h>
#endif

using easy_test::LoadedProfile;

namespace {

#if EASY_OPTION_LOG_ENABLED != 0
template <class Function>
std::string captureStderr(Function function)
{
    std::fflush(stderr);
    FILE* capture = std::tmpfile();
    REQUIRE(capture != nullptr);

# ifdef _WIN32
    const int stderrDescriptor = ::_fileno(stderr);
    const int savedDescriptor = ::_dup(stderrDescriptor);
    REQUIRE(savedDescriptor >= 0);
    REQUIRE(::_dup2(::_fileno(capture), stderrDescriptor) == 0);
# else
    const int stderrDescriptor = ::fileno(stderr);
    const int savedDescriptor = ::dup(stderrDescriptor);
    REQUIRE(savedDescriptor >= 0);
    REQUIRE(::dup2(::fileno(capture), stderrDescriptor) >= 0);
# endif

    function();
    std::fflush(stderr);

# ifdef _WIN32
    REQUIRE(::_dup2(savedDescriptor, stderrDescriptor) == 0);
    ::_close(savedDescriptor);
# else
    REQUIRE(::dup2(savedDescriptor, stderrDescriptor) >= 0);
    ::close(savedDescriptor);
# endif

    REQUIRE(std::fseek(capture, 0, SEEK_SET) == 0);
    std::string output;
    char buffer[512];
    while (const size_t size = std::fread(buffer, 1, sizeof(buffer), capture))
        output.append(buffer, size);
    std::fclose(capture);
    return output;
}
#endif

template <class T, class = void>
struct HasArrayValuePointer : std::false_type {};

template <class T>
struct HasArrayValuePointer<T, std::void_t<decltype(std::declval<const T&>().value())>>
    : std::true_type {};

template <class T, class = void>
struct HasCString : std::false_type {};

template <class T>
struct HasCString<T, std::void_t<decltype(std::declval<const T&>().c_str())>>
    : std::true_type {};

const profiler::StringValue* findString(const LoadedProfile& profile, const char* name)
{
    const profiler::ArbitraryValue* value = easy_test::findValue(profile, name);
    REQUIRE(value != nullptr);
    REQUIRE(value->type() == profiler::DataType::String);
    const profiler::StringValue* text = value->toArray<profiler::DataType::String>();
    REQUIRE(text != nullptr);
    return text;
}

} // namespace

#if EASY_OPTION_LOG_ENABLED != 0
TEST_CASE("Rejected values explain why they were dropped", "[values][logging]")
{
    const std::string warnings = captureStderr([] {
        const int32_t value = 1;
        profiler::setValue<int32_t>(nullptr, nullptr, profiler::ValueId(), 1U);
        profiler::setValue<int32_t>(
            nullptr, &value, profiler::ValueId(),
            profiler::MAX_BLOCK_DATA_SIZE / sizeof(int32_t) + 1U);
        profiler::setText(nullptr, static_cast<const char*>(nullptr), profiler::ValueId(), 1U);
        profiler::setText(nullptr, "oversized", profiler::ValueId(),
                          profiler::MAX_BLOCK_DATA_SIZE + 1U);
    });

    REQUIRE(warnings.find("EASY_ARRAY ignored a null payload") != std::string::npos);
    REQUIRE(warnings.find("EASY_ARRAY ignored a 2820-byte payload") != std::string::npos);
    REQUIRE(warnings.find("EASY_STRING ignored a null payload") != std::string::npos);
    REQUIRE(warnings.find("EASY_STRING ignored a 2817-byte payload") != std::string::npos);
}
#endif

TEST_CASE("Explicit-size value APIs accept size_t", "[values][api]")
{
    using ArraySetter = void (*)(const profiler::BaseBlockDescriptor*,
                                 const int32_t*, profiler::ValueId, size_t);
    using TextSetter = void (*)(const profiler::BaseBlockDescriptor*,
                                const char*, profiler::ValueId, size_t);

    ArraySetter arraySetter = &profiler::setValue<int32_t>;
    TextSetter textSetter = &profiler::setText;

    REQUIRE(arraySetter != nullptr);
    REQUIRE(textSetter != nullptr);

    LoadedProfile profile;
    easy_test::capture(profile, [arraySetter, textSetter] {
        const profiler::BaseBlockDescriptor* arrayDescriptor = profiler::registerDescription(
            profiler::ON, "explicit_array_api_id", "explicit_array_api", __FILE__, __LINE__,
            profiler::BlockType::Value, profiler::colors::Default, false);
        const profiler::BaseBlockDescriptor* textDescriptor = profiler::registerDescription(
            profiler::ON, "explicit_text_api_id", "explicit_text_api", __FILE__, __LINE__,
            profiler::BlockType::Value, profiler::colors::Default, false);

        const int32_t values[] = {17, 42};
        arraySetter(arrayDescriptor, nullptr, profiler::ValueId(), 1U);
        arraySetter(arrayDescriptor, values, profiler::ValueId(),
                    profiler::MAX_BLOCK_DATA_SIZE / sizeof(int32_t) + 1U);
        arraySetter(arrayDescriptor, values, profiler::ValueId(), 2U);

        const char text[] = "legacy";
        textSetter(textDescriptor, nullptr, profiler::ValueId(), 1U);
        textSetter(textDescriptor, text, profiler::ValueId(),
                   profiler::MAX_BLOCK_DATA_SIZE + 1U);
        textSetter(textDescriptor, text, profiler::ValueId(), sizeof(text));
    });

    REQUIRE(profile.readerLog.empty());
    const profiler::ArbitraryValue* array = easy_test::findValue(profile, "explicit_array_api");
    REQUIRE(array != nullptr);
    REQUIRE(array->toArray<int32_t>()->size() == 2);
    REQUIRE(array->toArray<int32_t>()->at(1) == 42);

    const profiler::ArbitraryValue* text = easy_test::findValue(profile, "explicit_text_api");
    REQUIRE(text != nullptr);
    REQUIRE(std::strcmp(text->data(), "legacy") == 0);
}

TEST_CASE("A value written while profiling is disabled is ignored", "[values][status]")
{
    easy_test::drainPending();
    REQUIRE_FALSE(profiler::isEnabled());

    EASY_VALUE("disabled_value", static_cast<int32_t>(7));

    LoadedProfile profile;
    easy_test::capture(profile, [] {
        EASY_BLOCK("disabled_value_control");
    });

    REQUIRE(profile.readerLog.empty());
    REQUIRE(easy_test::findValue(profile, "disabled_value") == nullptr);
    REQUIRE(easy_test::findBlockIndex(profile, "disabled_value_control") >= 0);
}

TEST_CASE("A value with status OFF is ignored during capture", "[values][status]")
{
    LoadedProfile profile;
    easy_test::capture(profile, [] {
        EASY_BLOCK("off_value_control");
        EASY_VALUE("off_value", static_cast<int32_t>(8), profiler::OFF);
    });

    REQUIRE(profile.readerLog.empty());
    REQUIRE(easy_test::findValue(profile, "off_value") == nullptr);
    REQUIRE(easy_test::findBlockIndex(profile, "off_value_control") >= 0);
}

TEST_CASE("Writing a value implicitly registers a new thread", "[values][threads]")
{
    LoadedProfile profile;
    easy_test::capture(profile, [] {
        std::thread worker([] {
            EASY_VALUE("implicitly_registered_value", static_cast<int32_t>(9));
        });
        worker.join();
    });

    REQUIRE(profile.readerLog.empty());
    const profiler::ArbitraryValue* value =
        easy_test::findValue(profile, "implicitly_registered_value");
    REQUIRE(value != nullptr);
    REQUIRE(value->toValue<int32_t>()->value() == 9);
}

TEST_CASE("A no-children block suppresses ordinary values but permits forced values", "[values][status]")
{
    LoadedProfile profile;
    easy_test::capture(profile, [] {
        EASY_BLOCK("value_no_children", profiler::ON_WITHOUT_CHILDREN);
        EASY_VALUE("suppressed_child_value", static_cast<int32_t>(10));
        EASY_VALUE("forced_child_value", static_cast<int32_t>(11), profiler::FORCE_ON);
    });

    REQUIRE(profile.readerLog.empty());
    REQUIRE(easy_test::findValue(profile, "suppressed_child_value") == nullptr);
    const profiler::ArbitraryValue* forced = easy_test::findValue(profile, "forced_child_value");
    REQUIRE(forced != nullptr);
    REQUIRE(forced->toValue<int32_t>()->value() == 11);
}

TEST_CASE("The low-level value API rejects invalid payload bounds", "[values][api][boundary]")
{
    LoadedProfile profile;
    easy_test::capture(profile, [] {
        const profiler::BaseBlockDescriptor* oversized = profiler::registerDescription(
            profiler::ON, "raw_oversized_value_id", "raw_oversized_value", __FILE__, __LINE__,
            profiler::BlockType::Value, profiler::colors::Default, false);
        const profiler::BaseBlockDescriptor* nullPayload = profiler::registerDescription(
            profiler::ON, "raw_null_value_id", "raw_null_value", __FILE__, __LINE__,
            profiler::BlockType::Value, profiler::colors::Default, false);
        const uint8_t byte = 0x5a;
        profiler::storeValue(
            oversized, profiler::DataType::Uint8, &byte,
            static_cast<uint16_t>(profiler::MAX_BLOCK_DATA_SIZE + 1U), true,
            profiler::ValueId());
        profiler::storeValue(
            nullPayload, profiler::DataType::Uint8, nullptr, 1U, true,
            profiler::ValueId());
        EASY_BLOCK("raw_invalid_value_control");
    });

    REQUIRE(profile.readerLog.empty());
    REQUIRE(easy_test::findValue(profile, "raw_oversized_value") == nullptr);
    REQUIRE(easy_test::findValue(profile, "raw_null_value") == nullptr);
    REQUIRE(easy_test::findBlockIndex(profile, "raw_invalid_value_control") >= 0);
}

TEST_CASE("The low-level value API accepts an empty array", "[values][api][boundary]")
{
    LoadedProfile profile;
    easy_test::capture(profile, [] {
        const profiler::BaseBlockDescriptor* descriptor = profiler::registerDescription(
            profiler::ON, "raw_empty_value_id", "raw_empty_value", __FILE__, __LINE__,
            profiler::BlockType::Value, profiler::colors::Default, false);
        profiler::storeValue(
            descriptor, profiler::DataType::Uint8, nullptr, 0U, true,
            profiler::ValueId());
    });

    INFO(profile.readerLog);
    REQUIRE(profile.readerLog.empty());
    const profiler::ArbitraryValue* value = easy_test::findValue(profile, "raw_empty_value");
    REQUIRE(value != nullptr);
    REQUIRE(value->isArray());
    REQUIRE(value->data_size() == 0);
}

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

        STATIC_REQUIRE_FALSE(HasArrayValuePointer<profiler::Value<profiler::DataType::Int32, true>>::value);

        int32_t copied[5] = {};
        REQUIRE(array->copyTo(copied, 5));
        REQUIRE(copied[0] == 10);
        REQUIRE(copied[4] == 50);
        REQUIRE_FALSE(array->copyTo(copied, 4));
        REQUIRE_FALSE(array->copyTo(nullptr, 5));

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

// EASY_ARRAY keeps a name only when it is a string literal, hence a macro.
#define EASY_TEST_EMPTY_ARRAY(Type, Name) \
    { const Type* nothing = nullptr; EASY_ARRAY(Name, nothing, 0); }

namespace {

template <class T>
void requireEmptyArray(const LoadedProfile& profile, const char* name)
{
    INFO(name);
    const profiler::ArbitraryValue* value = easy_test::findValue(profile, name);
    REQUIRE(value != nullptr);
    REQUIRE(value->isArray());
    REQUIRE(value->data_size() == 0);
    REQUIRE(value->toArray<T>() != nullptr);
    REQUIRE(value->toArray<T>()->size() == 0);
}

} // namespace

TEST_CASE("An empty array of every type has no addressable element", "[values][array][boundary]")
{
    // What the GUI relies on when it derives an element count from
    // data_size(): zero elements, and the array flag intact so that the value
    // is never read as a scalar either.
    LoadedProfile profile;

    easy_test::capture(profile, [] {
        EASY_BLOCK("empty_arrays_host");
        EASY_TEST_EMPTY_ARRAY(bool,     "empty_bool");
        EASY_TEST_EMPTY_ARRAY(char,     "empty_char");
        EASY_TEST_EMPTY_ARRAY(int8_t,   "empty_int8");
        EASY_TEST_EMPTY_ARRAY(uint8_t,  "empty_uint8");
        EASY_TEST_EMPTY_ARRAY(int16_t,  "empty_int16");
        EASY_TEST_EMPTY_ARRAY(uint16_t, "empty_uint16");
        EASY_TEST_EMPTY_ARRAY(int32_t,  "empty_int32");
        EASY_TEST_EMPTY_ARRAY(uint32_t, "empty_uint32");
        EASY_TEST_EMPTY_ARRAY(int64_t,  "empty_int64");
        EASY_TEST_EMPTY_ARRAY(uint64_t, "empty_uint64");
        EASY_TEST_EMPTY_ARRAY(float,    "empty_float");
        EASY_TEST_EMPTY_ARRAY(double,   "empty_double");
        EASY_STRING("empty_string", "", 0);
    });

    INFO(profile.readerLog);
    REQUIRE(profile.readerLog.empty());

    requireEmptyArray<bool>    (profile, "empty_bool");
    requireEmptyArray<char>    (profile, "empty_char");
    requireEmptyArray<int8_t>  (profile, "empty_int8");
    requireEmptyArray<uint8_t> (profile, "empty_uint8");
    requireEmptyArray<int16_t> (profile, "empty_int16");
    requireEmptyArray<uint16_t>(profile, "empty_uint16");
    requireEmptyArray<int32_t> (profile, "empty_int32");
    requireEmptyArray<uint32_t>(profile, "empty_uint32");
    requireEmptyArray<int64_t> (profile, "empty_int64");
    requireEmptyArray<uint64_t>(profile, "empty_uint64");
    requireEmptyArray<float>   (profile, "empty_float");
    requireEmptyArray<double>  (profile, "empty_double");

    const profiler::ArbitraryValue* text = easy_test::findValue(profile, "empty_string");
    REQUIRE(text != nullptr);
    REQUIRE(text->isArray());
    REQUIRE(text->data_size() == 0);
    REQUIRE(text->toArray<profiler::DataType::String>() != nullptr);
    REQUIRE(text->toArray<profiler::DataType::String>()->size() == 0);
}

TEST_CASE("copyTo never writes a partial trailing array element", "[values][array][boundary]")
{
    LoadedProfile profile;
    easy_test::capture(profile, [] {
        EASY_BLOCK("partial_array_host");
        const int32_t values[] = {0x11223344, 0x55667788};
        EASY_ARRAY("partial_array", values, 2);
    });

    const profiler::ArbitraryValue* value = easy_test::findValue(profile, "partial_array");
    REQUIRE(value != nullptr);

    // Exercise copyTo's own public-API boundary independently from the reader.
    // A malformed producer may leave a byte count that is not divisible by the
    // element width; only complete elements belong in the caller's buffer.
    const uint16_t malformedSize = sizeof(int32_t) + 1U;
    char* record = reinterpret_cast<char*>(const_cast<profiler::ArbitraryValue*>(value));
    std::memcpy(record + sizeof(profiler::BaseBlockData) + 2U,
                &malformedSize, sizeof(malformedSize));

    struct Destination
    {
        int32_t value;
        unsigned char canary[sizeof(int32_t) - 1U];
    } destination = {0, {0xa5U, 0xa5U, 0xa5U}};

    const auto* array = value->toArray<int32_t>();
    REQUIRE(array != nullptr);
    REQUIRE(array->size() == 1);
    REQUIRE(array->copyTo(&destination.value, 1));
    REQUIRE(destination.value == 0x11223344);
    REQUIRE(destination.canary[0] == 0xa5U);
    REQUIRE(destination.canary[1] == 0xa5U);
    REQUIRE(destination.canary[2] == 0xa5U);
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

TEST_CASE("A string value reads back exactly, wherever its record sits",
          "[values][text][boundary]")
{
    // EASY_STRING records exactly the bytes it is handed and the trailing zero
    // is documented as a recommendation, not a requirement, so the only correct
    // way to read a String payload back is by its recorded length. The four
    // placements that differ in what lies behind the payload: another record,
    // the end of the buffer, nothing (zero length), and EASY_TEXT, which records
    // its terminator inside the payload.
    // No enclosing block: a block is recorded when it closes, so it would land
    // behind the values and str_tail would no longer end the buffer.
    LoadedProfile profile;
    easy_test::capture(profile, [] {
        EASY_STRING("str_between", "ABCDEFGH", 3);
        EASY_VALUE("str_follower", static_cast<int64_t>(0x4141414141414141LL));
        EASY_STRING("str_empty", "", 0);
        EASY_TEXT("str_terminated", "hello");
        EASY_STRING("str_tail", "XYZW", 4); // last on purpose - see the honesty checks below
    });

    INFO("reader log: " << profile.readerLog);
    REQUIRE(profile.readerLog.empty());

    const char* const buffer = profile.serializedBlocks.data();
    const uint64_t bufferSize = profile.serializedBlocks.size();
    REQUIRE(buffer != nullptr);

    const profiler::StringValue* between = findString(profile, "str_between");
    const profiler::ArbitraryValue* follower = easy_test::findValue(profile, "str_follower");
    REQUIRE(follower != nullptr);
    // The next record starts right behind the three payload bytes, and the
    // payload itself holds no zero: a C-string walk would run into it.
    REQUIRE(reinterpret_cast<const char*>(follower) == between->data() + between->size());
    REQUIRE(std::memchr(between->data(), '\0', between->size()) == nullptr);
    REQUIRE(between->size() == 3);
    REQUIRE(between->length() == 3);
    REQUIRE(std::string(between->data(), between->length()) == "ABC");

    const profiler::StringValue* tail = findString(profile, "str_tail");
    // Last record: the payload ends exactly where the allocation ends.
    REQUIRE(static_cast<uint64_t>(tail->data() - buffer) + tail->size() == bufferSize);
    REQUIRE(tail->size() == 4);
    REQUIRE(tail->length() == 4);
    REQUIRE(std::string(tail->data(), tail->length()) == "XYZW");

    const profiler::StringValue* empty = findString(profile, "str_empty");
    REQUIRE(empty->size() == 0);
    REQUIRE(empty->length() == 0);
    REQUIRE(std::string(empty->data(), empty->length()).empty());

    const profiler::StringValue* terminated = findString(profile, "str_terminated");
    REQUIRE(terminated->size() == 6);
    REQUIRE(terminated->length() == 5);
    REQUIRE(terminated->data()[5] == '\0');
    REQUIRE(std::string(terminated->data(), terminated->length()) == "hello");
}

TEST_CASE("A string value offers no unbounded C-string accessor", "[values][text][api]")
{
    STATIC_REQUIRE_FALSE(HasArrayValuePointer<profiler::StringValue>::value);
    STATIC_REQUIRE_FALSE(HasCString<profiler::StringValue>::value);
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
    easy_test::capture(profile, [=] {
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

TEST_CASE("Value payload limits are enforced before narrowing", "[values][boundary]")
{
    LoadedProfile profile;

    easy_test::capture(profile, [] {
        EASY_BLOCK("value_boundary_host");

        std::vector<uint8_t> exact(profiler::MAX_BLOCK_DATA_SIZE, 0x5a);
        EASY_ARRAY("array_at_limit", exact.data(), exact.size());

        std::vector<uint8_t> oversized(profiler::MAX_BLOCK_DATA_SIZE + 1U, 0x6b);
        EASY_ARRAY("array_over_limit", oversized.data(), oversized.size());

        // This also covers the old uint16_t-wraparound path: the element count
        // is checked as size_t before it can be narrowed for serialization.
        std::vector<uint8_t> huge(65536U, 0x7c);
        EASY_ARRAY("array_uint16_wrap", huge.data(), huge.size());

        const uint8_t* nullArray = nullptr;
        EASY_ARRAY("array_empty", nullArray, 0U);
        EASY_ARRAY("array_null", nullArray, 1U);

        const double doubleValue = 1.0;
        const double* nullDoubleArray = nullptr;
        EASY_ARRAY("array_double_null", nullDoubleArray, 1U);
        EASY_ARRAY("array_double_over_limit", &doubleValue,
                   profiler::MAX_BLOCK_DATA_SIZE / sizeof(double) + 1U);
        EASY_ARRAY("array_count_overflow", &doubleValue,
                   std::numeric_limits<size_t>::max());

        std::string exactText(profiler::MAX_BLOCK_DATA_SIZE, 'x');
        EASY_STRING("text_at_limit", exactText.data(), exactText.size());

        std::string oversizedText(profiler::MAX_BLOCK_DATA_SIZE + 1U, 'y');
        EASY_STRING("text_over_limit", oversizedText.data(), oversizedText.size());
        EASY_TEXT("cstring_over_limit", oversizedText.c_str());

        const char* nullText = nullptr;
        EASY_STRING("string_empty", nullText, 0U);
        EASY_TEXT("cstring_null", nullText);

        const std::string stdText("std-string");
        EASY_TEXT("stdstring_valid", stdText);
        EASY_TEXT("stdstring_over_limit", oversizedText);
    });

    REQUIRE(profile.readerLog.empty());

    const profiler::ArbitraryValue* exactArray = easy_test::findValue(profile, "array_at_limit");
    REQUIRE(exactArray != nullptr);
    REQUIRE(exactArray->data_size() == profiler::MAX_BLOCK_DATA_SIZE);
    REQUIRE(static_cast<unsigned char>(exactArray->data()[0]) == 0x5a);
    REQUIRE(static_cast<unsigned char>(exactArray->data()[profiler::MAX_BLOCK_DATA_SIZE - 1U]) == 0x5a);

    const auto exactArrayValue = exactArray->toArray<uint8_t>();
    REQUIRE(exactArrayValue != nullptr);
    std::vector<uint8_t> exactCopy(profiler::MAX_BLOCK_DATA_SIZE);
    REQUIRE(exactArrayValue->copyTo(exactCopy.data(), exactCopy.size()));
    REQUIRE(exactCopy.front() == 0x5a);
    REQUIRE(exactCopy.back() == 0x5a);
    REQUIRE_FALSE(exactArrayValue->copyTo(nullptr, exactCopy.size()));
    REQUIRE_FALSE(exactArrayValue->copyTo(exactCopy.data(), exactCopy.size() - 1U));

    const profiler::ArbitraryValue* emptyArray = easy_test::findValue(profile, "array_empty");
    REQUIRE(emptyArray != nullptr);
    REQUIRE(emptyArray->data_size() == 0);
    REQUIRE(emptyArray->toArray<uint8_t>()->copyTo(nullptr, 0));

    const profiler::ArbitraryValue* exactText = easy_test::findValue(profile, "text_at_limit");
    REQUIRE(exactText != nullptr);
    REQUIRE(exactText->data_size() == profiler::MAX_BLOCK_DATA_SIZE);
    REQUIRE(exactText->data()[0] == 'x');
    REQUIRE(exactText->data()[profiler::MAX_BLOCK_DATA_SIZE - 1U] == 'x');

    const profiler::ArbitraryValue* emptyString = easy_test::findValue(profile, "string_empty");
    REQUIRE(emptyString != nullptr);
    REQUIRE(emptyString->data_size() == 0);

    REQUIRE(easy_test::findValue(profile, "array_over_limit") == nullptr);
    REQUIRE(easy_test::findValue(profile, "array_uint16_wrap") == nullptr);
    REQUIRE(easy_test::findValue(profile, "array_null") == nullptr);
    REQUIRE(easy_test::findValue(profile, "array_double_null") == nullptr);
    REQUIRE(easy_test::findValue(profile, "array_double_over_limit") == nullptr);
    REQUIRE(easy_test::findValue(profile, "array_count_overflow") == nullptr);
    REQUIRE(easy_test::findValue(profile, "text_over_limit") == nullptr);
    REQUIRE(easy_test::findValue(profile, "cstring_over_limit") == nullptr);
    REQUIRE(easy_test::findValue(profile, "cstring_null") == nullptr);
    const profiler::ArbitraryValue* stdString =
        easy_test::findValue(profile, "stdstring_valid");
    REQUIRE(stdString != nullptr);
    REQUIRE(std::strcmp(stdString->data(), "std-string") == 0);
    REQUIRE(easy_test::findValue(profile, "stdstring_over_limit") == nullptr);
}
