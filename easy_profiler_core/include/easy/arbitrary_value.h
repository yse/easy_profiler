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

#include <easy/profiler.h>
#include <string.h>

#ifdef USING_EASY_PROFILER

# define EASY_VIN(member) ::profiler::ValueId(member) 

# define EASY_VALUE(name, value, ...)\
    EASY_LOCAL_STATIC_PTR(const ::profiler::BaseBlockDescriptor*, EASY_UNIQUE_DESC(__LINE__), ::profiler::registerDescription(\
        ::profiler::extract_enable_flag(__VA_ARGS__), EASY_UNIQUE_LINE_ID, EASY_COMPILETIME_NAME(name),\
            __FILE__, __LINE__, ::profiler::BLOCK_TYPE_VALUE, ::profiler::extract_color(__VA_ARGS__), false));\
    ::profiler::setValue(EASY_UNIQUE_DESC(__LINE__), value);

# define EASY_TEXT(name, text, ...)\
    EASY_LOCAL_STATIC_PTR(const ::profiler::BaseBlockDescriptor*, EASY_UNIQUE_DESC(__LINE__), ::profiler::registerDescription(\
        ::profiler::extract_enable_flag(__VA_ARGS__), EASY_UNIQUE_LINE_ID, EASY_COMPILETIME_NAME(name),\
            __FILE__, __LINE__, ::profiler::BLOCK_TYPE_VALUE, ::profiler::extract_color(__VA_ARGS__), false));\
    ::profiler::setValue(EASY_UNIQUE_DESC(__LINE__), text);

#else

# define EASY_VIN(member) 
# define EASY_VALUE(...) 
# define EASY_TEXT(...) 

#endif

namespace profiler
{

    class ValueId EASY_FINAL {
        size_t m_id;
    public:
        inline explicit ValueId() : m_id(0) {}
        inline ValueId(const ValueId&) = default;
        template <class T> ValueId(const T& _member) : m_id(reinterpret_cast<size_t>(&_member)) {}
        inline size_t id() const { return m_id; }
    };

    inline ValueId extract_value_id() {
        return ValueId();
    }

    template <class T, class ... TArgs>
    inline ValueId extract_value_id(T, ::profiler::ValueId _vin, TArgs...) {
        return _vin;
    }

    template <class ... TArgs>
    inline ValueId extract_value_id(::profiler::ValueId _vin, TArgs...) {
        return _vin;
    }

    template <class ... TArgs>
    inline ValueId extract_value_id(TArgs...) {
        static_assert(sizeof...(TArgs) < 2, "No EasyBlockStatus in arguments list for EASY_BLOCK(name, ...)!");
        return ValueId();
    }

    enum DataType : uint8_t
    {
        DATA_BOOL = 0,
        DATA_CHAR,
        DATA_INT8,
        DATA_UINT8,
        DATA_INT16,
        DATA_UINT16,
        DATA_INT32,
        DATA_UINT32,
        DATA_INT64,
        DATA_UINT64,
        DATA_FLOAT,
        DATA_DOUBLE,
        DATA_STR,

        DATA_TYPES_NUMBER
    };

    template <DataType dataType> struct StdType;
    template <> struct StdType<DATA_BOOL  > { using value_type = bool    ; };
    template <> struct StdType<DATA_CHAR  > { using value_type = char    ; };
    template <> struct StdType<DATA_INT8  > { using value_type = int8_t  ; };
    template <> struct StdType<DATA_UINT8 > { using value_type = uint8_t ; };
    template <> struct StdType<DATA_INT16 > { using value_type = int16_t ; };
    template <> struct StdType<DATA_UINT16> { using value_type = uint16_t; };
    template <> struct StdType<DATA_INT32 > { using value_type = int32_t ; };
    template <> struct StdType<DATA_UINT32> { using value_type = uint32_t; };
    template <> struct StdType<DATA_INT64 > { using value_type = int64_t ; };
    template <> struct StdType<DATA_UINT64> { using value_type = uint64_t; };
    template <> struct StdType<DATA_FLOAT > { using value_type = float   ; };
    template <> struct StdType<DATA_DOUBLE> { using value_type = double  ; };
    template <> struct StdType<DATA_STR   > { using value_type = char    ; };

    template <class T> struct StdToDataType;
    template <> struct StdToDataType<bool       > { static const DataType data_type = DATA_BOOL  ; };
    template <> struct StdToDataType<char       > { static const DataType data_type = DATA_CHAR  ; };
    template <> struct StdToDataType<int8_t     > { static const DataType data_type = DATA_INT8  ; };
    template <> struct StdToDataType<uint8_t    > { static const DataType data_type = DATA_UINT8 ; };
    template <> struct StdToDataType<int16_t    > { static const DataType data_type = DATA_INT16 ; };
    template <> struct StdToDataType<uint16_t   > { static const DataType data_type = DATA_UINT16; };
    template <> struct StdToDataType<int32_t    > { static const DataType data_type = DATA_INT32 ; };
    template <> struct StdToDataType<uint32_t   > { static const DataType data_type = DATA_UINT32; };
    template <> struct StdToDataType<int64_t    > { static const DataType data_type = DATA_INT64 ; };
    template <> struct StdToDataType<uint64_t   > { static const DataType data_type = DATA_UINT64; };
    template <> struct StdToDataType<float      > { static const DataType data_type = DATA_FLOAT ; };
    template <> struct StdToDataType<double     > { static const DataType data_type = DATA_DOUBLE; };
    template <> struct StdToDataType<const char*> { static const DataType data_type = DATA_STR   ; };

    template <DataType dataType, bool isArray> struct Value;
    template <bool isArray> struct Value<DATA_TYPES_NUMBER, isArray>;

#pragma pack(push, 1)
    class PROFILER_API ArbitraryValue : protected BaseBlockData
    {
        friend ::ThreadStorage;

    protected:

        void setSize(uint16_t _size) { m_end16[3] = _size; }
        void setType(DataType _type) { m_end8[2] = static_cast<uint8_t>(_type); }
        void setArray(bool _isArray) { m_end8[3] = static_cast<uint8_t>(_isArray); }

        uint16_t size() const { return m_end16[0]; }
        DataType type() const { return static_cast<DataType>(m_end8[2]); }
        bool isArray() const { return m_end8[3] != 0; }

        ArbitraryValue(block_id_t _id, uint16_t _size, DataType _type, bool _isArray) : BaseBlockData(0, 0, _id)
        {
            setSize(_size);
            setType(_type);
            setArray(_isArray);
        }

    public:

        const char* data() const {
            return reinterpret_cast<const char*>(this) + sizeof(ArbitraryValue);
        }

        template <DataType dataType>
        const Value<dataType, false>* convertToValue() const {
            return type() == dataType ? static_cast<const Value<dataType, false>*>(this) : nullptr;
        }

        template <class T>
        const Value<StdToDataType<T>::data_type, false>* convertToValue() const {
            return convertToValue<StdToDataType<T>::data_type>();
        }

        template <DataType dataType>
        const Value<dataType, true>* convertToArray() const {
            return isArray() && type() == dataType ? static_cast<const Value<dataType, true>*>(this) : nullptr;
        }

        template <class T>
        const Value<StdToDataType<T>::data_type, true>* convertToArray() const {
            return convertToArray<StdToDataType<T>::data_type>();
        }

    private:

        char* data() {
            return reinterpret_cast<char*>(this) + sizeof(ArbitraryValue);
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
        uint16_t size() const { return ArbitraryValue::size() / sizeof(value_type); }
        value_type operator [] (int i) const { return value()[i]; }
    };

    template <> struct Value<DATA_STR, true> : public ArbitraryValue {
        using value_type = char;
        const char* value() const { return data(); }
        uint16_t size() const { return ArbitraryValue::size(); }
        char operator [] (int i) const { return data()[i]; }
        const char* c_str() const { return data(); }
    };

    template <DataType dataType>
    using SingleValue = Value<dataType, false>;

    template <DataType dataType>
    using ArrayValue = Value<dataType, true>;

    using StringValue = Value<DATA_STR, true>;

#ifdef USING_EASY_PROFILER
    extern "C" {
        PROFILER_API void storeValue(const BaseBlockDescriptor* _desc, DataType _type, const void* _data, size_t _size, bool _isArray);
    }
#else
    inline void storeValue(const BaseBlockDescriptor*, DataType, const void*, size_t, bool) {}
#endif

    template <class T>
    inline void setValue(const BaseBlockDescriptor* _desc, T _value)
    {
        storeValue(_desc, StdToDataType<T>::data_type, &_value, sizeof(T), false);
    }

    template <class T, size_t N>
    inline void setValue(const BaseBlockDescriptor* _desc, const T (&_value)[N])
    {
        storeValue(_desc, StdToDataType<T>::data_type, &_value[0], sizeof(_value), true);
    }

    inline void setText(const BaseBlockDescriptor* _desc, const char* _text)
    {
        storeValue(_desc, DATA_STR, _text, strlen(_text) + 1, true);
    }

    inline void setText(const BaseBlockDescriptor* _desc, const std::string& _text)
    {
        storeValue(_desc, DATA_STR, _text.c_str(), _text.size() + 1, true);
    }

    template <size_t N>
    inline void setText(const BaseBlockDescriptor* _desc, const char (&_text)[N])
    {
        storeValue(_desc, DATA_STR, &_text[0], N, true);
    }

} // END of namespace profiler.

#endif // EASY_PROFILER_ARBITRARY_VALUE_H
