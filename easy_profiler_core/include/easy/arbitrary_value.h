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

# define EASY_VIN(member) ::profiler::ValueId(member)

# define EASY_VALUE(name, value, ...)\
    EASY_LOCAL_STATIC_PTR(const ::profiler::BaseBlockDescriptor*, EASY_UNIQUE_DESC(__LINE__), ::profiler::registerDescription(\
        ::profiler::extract_enable_flag(__VA_ARGS__), EASY_UNIQUE_LINE_ID, EASY_COMPILETIME_NAME(name),\
            __FILE__, __LINE__, ::profiler::BlockType::Value, ::profiler::extract_color(__VA_ARGS__), false));\
    ::profiler::setValue(EASY_UNIQUE_DESC(__LINE__), value, ::profiler::extract_value_id(value , ## __VA_ARGS__));

# define EASY_TEXT(name, text, ...)\
    EASY_LOCAL_STATIC_PTR(const ::profiler::BaseBlockDescriptor*, EASY_UNIQUE_DESC(__LINE__), ::profiler::registerDescription(\
        ::profiler::extract_enable_flag(__VA_ARGS__), EASY_UNIQUE_LINE_ID, EASY_COMPILETIME_NAME(name),\
            __FILE__, __LINE__, ::profiler::BlockType::Value, ::profiler::extract_color(__VA_ARGS__), false));\
    ::profiler::setValue(EASY_UNIQUE_DESC(__LINE__), text, ::profiler::extract_value_id(text , ## __VA_ARGS__));

namespace profiler
{

    extern "C" PROFILER_API void storeValue(const BaseBlockDescriptor* _desc, DataType _type, const void* _data,
                                            size_t _size, bool _isArray, ValueId _vin);

    template <class T>
    inline void setValue(const BaseBlockDescriptor* _desc, T _value, ValueId _vin)
    {
        storeValue(_desc, StdToDataType<T>::data_type, &_value, sizeof(T), false, _vin);
    }

    template <class T, size_t N>
    inline void setValue(const BaseBlockDescriptor* _desc, const T (&_value)[N], ValueId _vin)
    {
        storeValue(_desc, StdToDataType<T>::data_type, &_value[0], sizeof(_value), true, _vin);
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
        storeValue(_desc, DataType::String, &_text[0], N, true, _vin);
    }

} // end of namespace profiler.

#else

# define EASY_VIN(member)
# define EASY_VALUE(...)
# define EASY_TEXT(...)

namespace profiler
{

    inline void storeValue(const BaseBlockDescriptor*, DataType, const void*, size_t, bool, ValueId) {}

    template <class T>
    inline void setValue(const BaseBlockDescriptor*, T, ValueId) {}

    template <class T, size_t N>
    inline void setValue(const BaseBlockDescriptor*, const T (&)[N], ValueId) {}

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
