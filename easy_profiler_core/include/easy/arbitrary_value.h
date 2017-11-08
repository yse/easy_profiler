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

#include <easy/arbitrary_value_aux.h>
#include <string.h>

#ifdef USING_EASY_PROFILER

# define EASY_VIN(member) ::profiler::ValueId(member)

# define EASY_VALUE(name, value, ...)\
    EASY_LOCAL_STATIC_PTR(const ::profiler::BaseBlockDescriptor*, EASY_UNIQUE_DESC(__LINE__), ::profiler::registerDescription(\
        ::profiler::extract_enable_flag(__VA_ARGS__), EASY_UNIQUE_LINE_ID, EASY_COMPILETIME_NAME(name),\
            __FILE__, __LINE__, ::profiler::BLOCK_TYPE_VALUE, ::profiler::extract_color(__VA_ARGS__), false));\
    ::profiler::setValue(EASY_UNIQUE_DESC(__LINE__), value, ::profiler::extract_value_id(value , ## __VA_ARGS__));

# define EASY_TEXT(name, text, ...)\
    EASY_LOCAL_STATIC_PTR(const ::profiler::BaseBlockDescriptor*, EASY_UNIQUE_DESC(__LINE__), ::profiler::registerDescription(\
        ::profiler::extract_enable_flag(__VA_ARGS__), EASY_UNIQUE_LINE_ID, EASY_COMPILETIME_NAME(name),\
            __FILE__, __LINE__, ::profiler::BLOCK_TYPE_VALUE, ::profiler::extract_color(__VA_ARGS__), false));\
    ::profiler::setValue(EASY_UNIQUE_DESC(__LINE__), text, ::profiler::extract_value_id(text , ## __VA_ARGS__));

#else

# define EASY_VIN(member) 
# define EASY_VALUE(...) 
# define EASY_TEXT(...) 

#endif

namespace profiler
{

    enum class DataType : uint8_t
    {
        Bool = 0,
        Char,
        Int8,
        Uint8,
        Int16,
        Uint16,
        Int32,
        Uint32,
        Int64,
        Uint64,
        Float,
        Double,
        String,

        TypesCount
    };

    namespace {
        template <DataType dataType> struct StdType;
        template <class T>           struct StdToDataType;

        template <> struct StdType<DataType::Bool  > { using value_type = bool    ; };
        template <> struct StdType<DataType::Char  > { using value_type = char    ; };
        template <> struct StdType<DataType::Int8  > { using value_type = int8_t  ; };
        template <> struct StdType<DataType::Uint8 > { using value_type = uint8_t ; };
        template <> struct StdType<DataType::Int16 > { using value_type = int16_t ; };
        template <> struct StdType<DataType::Uint16> { using value_type = uint16_t; };
        template <> struct StdType<DataType::Int32 > { using value_type = int32_t ; };
        template <> struct StdType<DataType::Uint32> { using value_type = uint32_t; };
        template <> struct StdType<DataType::Int64 > { using value_type = int64_t ; };
        template <> struct StdType<DataType::Uint64> { using value_type = uint64_t; };
        template <> struct StdType<DataType::Float > { using value_type = float   ; };
        template <> struct StdType<DataType::Double> { using value_type = double  ; };
        template <> struct StdType<DataType::String> { using value_type = char    ; };

        template <> struct StdToDataType<bool       > { EASY_STATIC_CONSTEXPR auto data_type = DataType::Bool  ; };
        template <> struct StdToDataType<char       > { EASY_STATIC_CONSTEXPR auto data_type = DataType::Char  ; };
        template <> struct StdToDataType<int8_t     > { EASY_STATIC_CONSTEXPR auto data_type = DataType::Int8  ; };
        template <> struct StdToDataType<uint8_t    > { EASY_STATIC_CONSTEXPR auto data_type = DataType::Uint8 ; };
        template <> struct StdToDataType<int16_t    > { EASY_STATIC_CONSTEXPR auto data_type = DataType::Int16 ; };
        template <> struct StdToDataType<uint16_t   > { EASY_STATIC_CONSTEXPR auto data_type = DataType::Uint16; };
        template <> struct StdToDataType<int32_t    > { EASY_STATIC_CONSTEXPR auto data_type = DataType::Int32 ; };
        template <> struct StdToDataType<uint32_t   > { EASY_STATIC_CONSTEXPR auto data_type = DataType::Uint32; };
        template <> struct StdToDataType<int64_t    > { EASY_STATIC_CONSTEXPR auto data_type = DataType::Int64 ; };
        template <> struct StdToDataType<uint64_t   > { EASY_STATIC_CONSTEXPR auto data_type = DataType::Uint64; };
        template <> struct StdToDataType<float      > { EASY_STATIC_CONSTEXPR auto data_type = DataType::Float ; };
        template <> struct StdToDataType<double     > { EASY_STATIC_CONSTEXPR auto data_type = DataType::Double; };
        template <> struct StdToDataType<const char*> { EASY_STATIC_CONSTEXPR auto data_type = DataType::String; };
    }

    template <DataType dataType, bool isArray>
    struct Value;

    template <bool isArray>
    struct Value<DataType::TypesCount, isArray>;



#pragma pack(push, 1)
    class PROFILER_API ArbitraryValue : protected BaseBlockData
    {
        friend ::ThreadStorage;

    protected:

        uint16_t m_size;
        DataType m_type;
        bool  m_isArray;

        ArbitraryValue(vin_t _vin, block_id_t _id, uint16_t _size, DataType _type, bool _isArray)
            : BaseBlockData(0, static_cast<timestamp_t>(_vin), _id)
            , m_size(_size)
            , m_type(_type)
            , m_isArray(_isArray)
        {
        }

    public:

        const char* data() const {
            return reinterpret_cast<const char*>(this) + sizeof(ArbitraryValue);
        }

        vin_t value_id() const {
            return static_cast<vin_t>(m_end);
        }

        template <DataType dataType>
        const Value<dataType, false>* convertToValue() const {
            return m_type == dataType ? static_cast<const Value<dataType, false>*>(this) : nullptr;
        }

        template <class T>
        const Value<StdToDataType<T>::data_type, false>* convertToValue() const {
            return convertToValue<StdToDataType<T>::data_type>();
        }

        template <DataType dataType>
        const Value<dataType, true>* convertToArray() const {
            return m_isArray && m_type == dataType ? static_cast<const Value<dataType, true>*>(this) : nullptr;
        }

        template <class T>
        const Value<StdToDataType<T>::data_type, true>* convertToArray() const {
            return convertToArray<StdToDataType<T>::data_type>();
        }
    };
#pragma pack(pop)



    template <DataType dataType> struct Value<dataType, false> : public ArbitraryValue {
        using value_type = typename StdType<dataType>::value_type;
        value_type value() const { return *reinterpret_cast<const value_type*>(data()); }
    };


    template <DataType dataType> struct Value<dataType, true> : public ArbitraryValue {
        using value_type = typename StdType<dataType>::value_type;
        const value_type* value() const { return reinterpret_cast<const value_type*>(data()); }
        uint16_t size() const { return m_size / sizeof(value_type); }
        value_type operator [] (int i) const { return value()[i]; }
    };


    template <> struct Value<DataType::String, true> : public ArbitraryValue {
        using value_type = char;
        const char* value() const { return data(); }
        uint16_t size() const { return m_size; }
        char operator [] (int i) const { return data()[i]; }
        const char* c_str() const { return data(); }
    };


    template <DataType dataType>
    using SingleValue = Value<dataType, false>;


    template <DataType dataType>
    using ArrayValue = Value<dataType, true>;


    using StringValue = Value<DataType::String, true>;



#ifdef USING_EASY_PROFILER
    extern "C" {
        PROFILER_API void storeValue(const BaseBlockDescriptor* _desc, DataType _type, const void* _data, size_t _size, bool _isArray, ValueId _vin);
    }
#else
    inline void storeValue(const BaseBlockDescriptor*, DataType, const void*, size_t, bool, ValueId) {}
#endif



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


    inline void setText(const BaseBlockDescriptor* _desc, const std::string& _text, ValueId _vin)
    {
        storeValue(_desc, DataType::String, _text.c_str(), _text.size() + 1, true, _vin);
    }


    template <size_t N>
    inline void setText(const BaseBlockDescriptor* _desc, const char (&_text)[N], ValueId _vin)
    {
        storeValue(_desc, DataType::String, &_text[0], N, true, _vin);
    }

} // end of namespace profiler.

#endif // EASY_PROFILER_ARBITRARY_VALUE_H
