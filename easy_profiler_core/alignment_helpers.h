/************************************************
 * creation time : 2017/08/20
 * author        : Blake Martin
 * email         : rationalcoder@gmail.com
 ************************************************/

/**
Lightweight profiler library for c++
Copyright(C) 2016-2019  Sergey Yagovtsev, Victor Zarubkin

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

#ifndef EASY_PROFILER_ALIGNMENT_HELPERS_H
#define EASY_PROFILER_ALIGNMENT_HELPERS_H

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <easy/details/easy_compiler_support.h>

//! Checks if a pointer is aligned.
//! \param ptr The pointer to check.
//! \param alignment The alignement (must be a power of 2)
//! \returns true if the memory is aligned.
//!
template <uint32_t ALIGNMENT>
EASY_FORCE_INLINE bool is_aligned(const void* ptr)
{
    static_assert(ALIGNMENT != 0 && (ALIGNMENT & (ALIGNMENT - 1)) == 0,
                  "Alignment must be a power of two.");
    return ((uintptr_t)ptr & (ALIGNMENT-1)) == 0;
}

EASY_FORCE_INLINE void unaligned_zero16(void* ptr)
{
#ifndef EASY_ENABLE_STRICT_ALIGNMENT
    *(uint16_t*)ptr = 0;
#else
    std::memset(ptr, 0, sizeof(uint16_t));
#endif
}

EASY_FORCE_INLINE void unaligned_zero32(void* ptr)
{
#ifndef EASY_ENABLE_STRICT_ALIGNMENT
    *(uint32_t*)ptr = 0;
#else
    std::memset(ptr, 0, sizeof(uint32_t));
#endif
}

EASY_FORCE_INLINE void unaligned_zero64(void* ptr)
{
#ifndef EASY_ENABLE_STRICT_ALIGNMENT
    *(uint64_t*)ptr = 0;
#else
    std::memset(ptr, 0, sizeof(uint64_t));
#endif
}

template <typename T>
EASY_FORCE_INLINE void unaligned_store16(void* ptr, T val)
{
    static_assert(sizeof(T) == 2, "16 bit type required.");

#ifndef EASY_ENABLE_STRICT_ALIGNMENT
    *(T*)ptr = val;
#else
    std::memcpy(ptr, &val, sizeof(T));
#endif
}

template <typename T>
EASY_FORCE_INLINE void unaligned_store32(void* ptr, T val)
{
    static_assert(sizeof(T) == 4, "32 bit type required.");

#ifndef EASY_ENABLE_STRICT_ALIGNMENT
    *(T*)ptr = val;
#else
    std::memcpy(ptr, &val, sizeof(T));
#endif
}

template <typename T>
EASY_FORCE_INLINE void unaligned_store64(void* ptr, T val)
{
    static_assert(sizeof(T) == 8, "64 bit type required.");

#ifndef EASY_ENABLE_STRICT_ALIGNMENT
    *(T*)ptr = val;
#else
    std::memcpy(ptr, &val, sizeof(T));
#endif
}

template <typename T>
EASY_FORCE_INLINE T unaligned_load16(const void* ptr)
{
    static_assert(sizeof(T) == 2, "16 bit type required.");

#ifndef EASY_ENABLE_STRICT_ALIGNMENT
    return *(const T*)ptr;
#else
    T value;
    std::memcpy(&value, ptr, sizeof(T));
    return value;
#endif
}

template <typename T>
EASY_FORCE_INLINE T unaligned_load16(const void* ptr, T* val)
{
    static_assert(sizeof(T) == 2, "16 bit type required.");

#ifndef EASY_ENABLE_STRICT_ALIGNMENT
    *val = *(const T*)ptr;
#else
    std::memcpy(val, ptr, sizeof(T));
#endif

    return *val;
}

template <typename T>
EASY_FORCE_INLINE T unaligned_load32(const void* ptr)
{
    static_assert(sizeof(T) == 4, "32 bit type required.");

#ifndef EASY_ENABLE_STRICT_ALIGNMENT
    return *(const T*)ptr;
#else
    T value;
    std::memcpy(&value, ptr, sizeof(T));
    return value;
#endif
}

template <typename T>
EASY_FORCE_INLINE T unaligned_load32(const void* ptr, T* val)
{
    static_assert(sizeof(T) == 4, "32 bit type required.");

#ifndef EASY_ENABLE_STRICT_ALIGNMENT
    *val = *(const T*)ptr;
#else
    std::memcpy(val, ptr, sizeof(T));
#endif

    return *val;
}

template <typename T>
EASY_FORCE_INLINE T unaligned_load64(const void* ptr)
{
    static_assert(sizeof(T) == 8, "64 bit type required.");

#ifndef EASY_ENABLE_STRICT_ALIGNMENT
    return *(const T*)ptr;
#else
    T value;
    std::memcpy(&value, ptr, sizeof(T));
    return value;
#endif
}

template <typename T>
EASY_FORCE_INLINE T unaligned_load64(const void* ptr, T* val)
{
    static_assert(sizeof(T) == 8, "64 bit type required.");

#ifndef EASY_ENABLE_STRICT_ALIGNMENT
    *val = *(const T*)ptr;
#else
    std::memcpy(val, ptr, sizeof(T));
#endif
    return *val;
}

#endif //EASY_PROFILER_ALIGNMENT_HELPERS_H
