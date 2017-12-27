/**
Lightweight profiler library for c++
Copyright(C) 2016-2017  Sergey Yagovtsev, Victor Zarubkin

Licensed under either of
    * MIT license (LICENSE.MIT or http://opensource.org/licenses/MIT)
    * Apache License, Version 2.0, (LICENSE.APACHE or http://www.apache.org/licenses/LICENSE-2.0)
at your option.

The MIT License
    Permission is hereby granted, free of charge, to any person obtaining a copy
    of this software and associated documentation files (the "Software"), to deal
    in the Software without restriction, including without limitation the rights 
    to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies 
    of the Software, and to permit persons to whom the Software is furnished 
    to do so, subject to the following conditions:

    The above copyright notice and this permission notice shall be included in all 
    copies or substantial portions of the Software.

    THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, 
    INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR 
    PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE 
    LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, 
    TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE 
    USE OR OTHER DEALINGS IN THE SOFTWARE.


The Apache License, Version 2.0 (the "License");
    You may not use this file except in compliance with the License.
    You may obtain a copy of the License at

    http://www.apache.org/licenses/LICENSE-2.0

    Unless required by applicable law or agreed to in writing, software
    distributed under the License is distributed on an "AS IS" BASIS,
    WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
    See the License for the specific language governing permissions and
    limitations under the License.

**/

#ifndef EASY_PROFILER_ARBITRARY_VALUE_H
#define EASY_PROFILER_ARBITRARY_VALUE_H

#include <easy/details/arbitrary_value_public_types.h>

#if defined(__clang__)
# pragma clang diagnostic push
# pragma clang diagnostic ignored "-Wgnu-zero-variadic-macro-arguments"
#endif

#ifdef USING_EASY_PROFILER

/** Macro used to create a unique Value Identification Number.

Use this if you want to change the same value from different places in your code.
Otherwise do not mind.

\code
struct A {
  int someCount;
};

void foo(const A& a) {
  EASY_VALUE("foo count", a.someCount); // Value ID is automatically set to (uint64_t)&a.someCount

  // This notation is completely the same as EASY_VALUE("foo count", a.someCount, EASY_VIN(a.someCount));
}

void bar(const A& b) {
  EASY_VALUE("bar count", b.someCount); // Same ID as for "foo count" if &b == &a (and different ID otherwise)
}

void baz(const A& c) {
  EASY_VALUE("baz count", c.someCount, EASY_VIN(EASY_FUNC_NAME)); // Different ID from "foo count" and "bar count"
  EASY_VALUE("qux count", 100500, EASY_VIN(EASY_FUNC_NAME)); // Same ID as for "baz count"
}
\endcode

\ingroup profiler
*/
# define EASY_VIN(member) ::profiler::ValueId(member)

/** Macro used to identify global value which would be recognized by it's name in GUI.

\code
struct A {
  int someCount;
};

struct B {
  int someOtherCount;
};

void foo(const A& a) {
  EASY_VALUE("Count", a.someCount, EASY_GLOBAL_VIN);
}

void bar(const B& b) {
  EASY_VALUE("Count", b.someOtherCount, EASY_GLOBAL_VIN); // Same ID as for foo::"Count" despite of &b != &a
}
\endcode

\ingroup profiler
*/
# define EASY_GLOBAL_VIN ::profiler::ValueId()

/** Macro used to identify unique value.

\code
struct A {
  int someCount;
};

void foo(const A& a) {
  // All these values would have different IDs despite of names, pointers and values are the same
  EASY_VALUE("foo count", a.someCount, EASY_UNIQUE_VIN);
  EASY_VALUE("foo count", a.someCount, EASY_UNIQUE_VIN);
  EASY_VALUE("foo count", a.someCount, EASY_UNIQUE_VIN);
}
\endcode

\ingroup profiler
*/
# define EASY_UNIQUE_VIN ::profiler::ValueId(EASY_UNIQUE_DESC(__LINE__))

/** Macro used to store single arbitrary value.

\note Also stores a time-stamp of it's occurrence like an Event.

\note To store an array, please, use EASY_ARRAY macro.

\note Currently arbitrary values support only compile-time names.

\sa EASY_ARRAY, EASY_TEXT, EASY_STRING

\ingroup profiler
*/
# define EASY_VALUE(name, value, ...)\
    EASY_LOCAL_STATIC_PTR(const ::profiler::BaseBlockDescriptor*, EASY_UNIQUE_DESC(__LINE__), ::profiler::registerDescription(\
        ::profiler::extract_enable_flag(__VA_ARGS__), EASY_UNIQUE_LINE_ID, EASY_COMPILETIME_NAME(name),\
            __FILE__, __LINE__, ::profiler::BlockType::Value, ::profiler::extract_color(__VA_ARGS__), false));\
    ::profiler::setValue(EASY_UNIQUE_DESC(__LINE__), value, ::profiler::extract_value_id(value, ## __VA_ARGS__));

/** Macro used to store an array of arbitrary values.

\note Also stores a time-stamp of it's occurrence like an Event.

\note Currently arbitrary values support only compile-time names.

\sa EASY_VALUE, EASY_TEXT, EASY_STRING

\ingroup profiler
*/
# define EASY_ARRAY(name, value, size, ...)\
    EASY_LOCAL_STATIC_PTR(const ::profiler::BaseBlockDescriptor*, EASY_UNIQUE_DESC(__LINE__), ::profiler::registerDescription(\
        ::profiler::extract_enable_flag(__VA_ARGS__), EASY_UNIQUE_LINE_ID, EASY_COMPILETIME_NAME(name),\
            __FILE__, __LINE__, ::profiler::BlockType::Value, ::profiler::extract_color(__VA_ARGS__), false));\
    ::profiler::setValue(EASY_UNIQUE_DESC(__LINE__), value, ::profiler::extract_value_id(value, ## __VA_ARGS__), size);

/** Macro used to store custom text.

Could be C-string or std::string.

\note Also stores a time-stamp of it's occurrence like an Event.

\note Currently arbitrary values support only compile-time names.

\sa EASY_VALUE, EASY_ARRAY, EASY_STRING

\ingroup profiler
*/
# define EASY_TEXT(name, text, ...)\
    EASY_LOCAL_STATIC_PTR(const ::profiler::BaseBlockDescriptor*, EASY_UNIQUE_DESC(__LINE__), ::profiler::registerDescription(\
        ::profiler::extract_enable_flag(__VA_ARGS__), EASY_UNIQUE_LINE_ID, EASY_COMPILETIME_NAME(name),\
            __FILE__, __LINE__, ::profiler::BlockType::Value, ::profiler::extract_color(__VA_ARGS__), false));\
    ::profiler::setText(EASY_UNIQUE_DESC(__LINE__), text, ::profiler::extract_value_id(text , ## __VA_ARGS__));

/** Macro used to store custom text of specified length.

Same as EASY_TEXT, but with explicitly specified length.
Use this for C-strings of known length (compile-time or run-time).

\note Recommendation (not a requirement): Take into account a zero-terminator '\0' symbol (e.g. strlen("BlaBla") + 1).

\note Also stores a time-stamp of it's occurrence like an Event.

\note Currently arbitrary values support only compile-time names.

\sa EASY_VALUE, EASY_ARRAY, EASY_TEXT

\ingroup profiler
*/
# define EASY_STRING(name, text, size, ...)\
    EASY_LOCAL_STATIC_PTR(const ::profiler::BaseBlockDescriptor*, EASY_UNIQUE_DESC(__LINE__), ::profiler::registerDescription(\
        ::profiler::extract_enable_flag(__VA_ARGS__), EASY_UNIQUE_LINE_ID, EASY_COMPILETIME_NAME(name),\
            __FILE__, __LINE__, ::profiler::BlockType::Value, ::profiler::extract_color(__VA_ARGS__), false));\
    ::profiler::setText(EASY_UNIQUE_DESC(__LINE__), text, ::profiler::extract_value_id(text, ## __VA_ARGS__), size);

namespace profiler
{

    EASY_CONSTEXPR uint16_t MaxArbitraryValuesArraySize = 65535;

    extern "C" PROFILER_API void storeValue(const BaseBlockDescriptor* _desc, DataType _type, const void* _data,
                                            size_t _size, bool _isArray, ValueId _vin);

    template <class T>
    inline void setValue(const BaseBlockDescriptor* _desc, T _value, ValueId _vin)
    {
        static_assert(!::std::is_pointer<T>::value,
                      "You should not pass pointers into EASY_VALUE. Pass a reference instead.");

        using Type = typename ::std::remove_reference<typename ::std::remove_cv<T>::type>::type;

        static_assert(StdToDataType<Type>::data_type != DataType::TypesCount,
                      "You should use standard builtin scalar types as profiler::Value type!");

        storeValue(_desc, StdToDataType<Type>::data_type, &_value, sizeof(Type), false, _vin);
    }

    template <class T>
    inline void setValue(const BaseBlockDescriptor* _desc, const T* _valueArray, ValueId _vin, uint16_t _arraySize)
    {
        static_assert(StdToDataType<T>::data_type != DataType::TypesCount,
                      "You should use standard builtin scalar types as profiler::Value type!");

        storeValue(_desc, StdToDataType<T>::data_type, _valueArray, sizeof(T) * _arraySize, true, _vin);
    }

    template <class T, size_t N>
    inline void setValue(const BaseBlockDescriptor* _desc, const T (&_value)[N], ValueId _vin)
    {
        static_assert(StdToDataType<T>::data_type != DataType::TypesCount,
                      "You should use standard builtin scalar types as profiler::Value type!");

        static_assert(N <= MaxArbitraryValuesArraySize, "Maximum arbitrary values array size is 65535.");

        storeValue(_desc, StdToDataType<T>::data_type, _value, sizeof(_value), true, _vin);
    }

    inline void setText(const BaseBlockDescriptor* _desc, const char* _text, ValueId _vin, uint16_t _textLength)
    {
        storeValue(_desc, DataType::String, _text, _textLength, true, _vin);
    }

    inline void setText(const BaseBlockDescriptor* _desc, const char* _text, ValueId _vin)
    {
        storeValue(_desc, DataType::String, _text, strlen(_text) + 1, true, _vin);
    }

    inline void setText(const BaseBlockDescriptor* _desc, const ::std::string& _text, ValueId _vin)
    {
        storeValue(_desc, DataType::String, _text.c_str(), _text.size() + 1, true, _vin);
    }

    template <size_t N>
    inline void setText(const BaseBlockDescriptor* _desc, const char (&_text)[N], ValueId _vin)
    {
        static_assert(N <= MaxArbitraryValuesArraySize, "Maximum arbitrary values array size is 65535.");
        storeValue(_desc, DataType::String, &_text[0], N, true, _vin);
    }

} // end of namespace profiler.

#else

# define EASY_GLOBAL_VIN
# define EASY_UNIQUE_VIN
# define EASY_VIN(member)
# define EASY_VALUE(...)
# define EASY_ARRAY(...)
# define EASY_TEXT(...)
# define EASY_STRING(...)

namespace profiler
{

    inline void storeValue(const BaseBlockDescriptor*, DataType, const void*, size_t, bool, ValueId) {}

    template <class T>
    inline void setValue(const BaseBlockDescriptor*, T, ValueId) {}

    template <class T>
    inline void setValue(const BaseBlockDescriptor*, const T*, ValueId, uint16_t) {}

    template <class T, size_t N>
    inline void setValue(const BaseBlockDescriptor*, const T (&)[N], ValueId) {}

    inline void setText(const BaseBlockDescriptor*, const char*, ValueId, uint16_t) {}

    inline void setText(const BaseBlockDescriptor*, const char*, ValueId) {}

    inline void setText(const BaseBlockDescriptor*, const ::std::string&, ValueId) {}

    template <size_t N>
    inline void setText(const BaseBlockDescriptor*, const char (&)[N], ValueId) {}

} // end of namespace profiler.

#endif // USING_EASY_PROFILER

#if defined(__clang__)
# pragma clang diagnostic pop
#endif

#endif // EASY_PROFILER_ARBITRARY_VALUE_H
