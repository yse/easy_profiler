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

#ifndef EASY_PROFILER_CHUNK_ALLOCATOR_H
#define EASY_PROFILER_CHUNK_ALLOCATOR_H

#include <easy/details/easy_compiler_support.h>
#include <cstring>
#include <cstddef>
#include <stdint.h>
#include "outstream.h"

//////////////////////////////////////////////////////////////////////////

#ifndef EASY_ENABLE_ALIGNMENT 
# define EASY_ENABLE_ALIGNMENT 0
#endif

#ifndef EASY_ALIGNMENT_SIZE
# define EASY_ALIGNMENT_SIZE alignof(std::max_align_t)
#endif

#if EASY_ENABLE_ALIGNMENT == 0
# define EASY_ALIGNED(TYPE, VAR, A) TYPE VAR
# define EASY_MALLOC(MEMSIZE, A) malloc(MEMSIZE)
# define EASY_FREE(MEMPTR) free(MEMPTR)
#else
// MSVC and GNUC aligned versions of malloc are defined in malloc.h
# include <malloc.h>
# if defined(_MSC_VER)
#  define EASY_ALIGNED(TYPE, VAR, A) __declspec(align(A)) TYPE VAR
#  define EASY_MALLOC(MEMSIZE, A) _aligned_malloc(MEMSIZE, A)
#  define EASY_FREE(MEMPTR) _aligned_free(MEMPTR)
# elif defined(__GNUC__)
#  define EASY_ALIGNED(TYPE, VAR, A) TYPE VAR __attribute__((aligned(A)))
#  define EASY_MALLOC(MEMSIZE, A) memalign(A, MEMSIZE)
#  define EASY_FREE(MEMPTR) free(MEMPTR)
# else
#  define EASY_ALIGNED(TYPE, VAR, A) TYPE VAR
#  define EASY_MALLOC(MEMSIZE, A) malloc(MEMSIZE)
#  define EASY_FREE(MEMPTR) free(MEMPTR)
# endif
#endif

//////////////////////////////////////////////////////////////////////////

//! Checks if a pointer is aligned.
//! \param ptr The pointer to check.
//! \param alignment The alignement (must be a power of 2)
//! \returns true if the memory is aligned.
//!
template <uint32_t ALIGNMENT>
EASY_FORCE_INLINE bool is_aligned(void* ptr)
{
    static_assert(ALIGNMENT % 2 == 0, "Alignment must be a power of two.");
    return ((uintptr_t)ptr & (ALIGNMENT-1)) == 0;
}

EASY_FORCE_INLINE void unaligned_zero16(void* ptr)
{
#ifndef EASY_ENABLE_STRICT_ALIGNMENT
    *(uint16_t*)ptr = 0;
#else
    ((char*)ptr)[0] = 0;
    ((char*)ptr)[1] = 0;
#endif
}

EASY_FORCE_INLINE void unaligned_zero32(void* ptr)
{
#ifndef EASY_ENABLE_STRICT_ALIGNMENT
    *(uint32_t*)ptr = 0;
#else
    ((char*)ptr)[0] = 0;
    ((char*)ptr)[1] = 0;
    ((char*)ptr)[2] = 0;
    ((char*)ptr)[3] = 0;
#endif
}

EASY_FORCE_INLINE void unaligned_zero64(void* ptr)
{
#ifndef EASY_ENABLE_STRICT_ALIGNMENT
    *(uint64_t*)ptr = 0;
#else
    // Assume unaligned is more common.
    if (!is_aligned<alignof(uint64_t)>(ptr)) {
        ((char*)ptr)[0] = 0;
        ((char*)ptr)[1] = 0;
        ((char*)ptr)[2] = 0;
        ((char*)ptr)[3] = 0;
        ((char*)ptr)[4] = 0;
        ((char*)ptr)[5] = 0;
        ((char*)ptr)[6] = 0;
        ((char*)ptr)[7] = 0;
    }
    else {
        *(uint64_t*)ptr = 0;
    }
#endif
}

template <typename T>
EASY_FORCE_INLINE void unaligned_store16(void* ptr, T val)
{
    static_assert(sizeof(T) == 2, "16 bit type required.");
#ifndef EASY_ENABLE_STRICT_ALIGNMENT
    *(T*)ptr = val;
#else
    const char* const temp = (char*)&val;
    ((char*)ptr)[0] = temp[0];
    ((char*)ptr)[1] = temp[1];
#endif
}

template <typename T>
EASY_FORCE_INLINE void unaligned_store32(void* ptr, T val)
{
    static_assert(sizeof(T) == 4, "32 bit type required.");
#ifndef EASY_ENABLE_STRICT_ALIGNMENT
    *(T*)ptr = val;
#else
    const char* const temp = (char*)&val;
    ((char*)ptr)[0] = temp[0];
    ((char*)ptr)[1] = temp[1];
    ((char*)ptr)[2] = temp[2];
    ((char*)ptr)[3] = temp[3];
#endif
}

template <typename T>
EASY_FORCE_INLINE void unaligned_store64(void* ptr, T val)
{
    static_assert(sizeof(T) == 8, "64 bit type required.");
#ifndef EASY_ENABLE_STRICT_ALIGNMENT
    *(T*)ptr = val;
#else
    const char* const temp = (char*)&val;
    // Assume unaligned is more common.
    if (!is_aligned<alignof(T)>(ptr)) {
        ((char*)ptr)[0] = temp[0];
        ((char*)ptr)[1] = temp[1];
        ((char*)ptr)[2] = temp[2];
        ((char*)ptr)[3] = temp[3];
        ((char*)ptr)[4] = temp[4];
        ((char*)ptr)[5] = temp[5];
        ((char*)ptr)[6] = temp[6];
        ((char*)ptr)[7] = temp[7];
    }
    else {
        *(T*)ptr = val;
    }
#endif
}

template <typename T>
EASY_FORCE_INLINE T unaligned_load16(const void* ptr)
{
    static_assert(sizeof(T) == 2, "16 bit type required.");
#ifndef EASY_ENABLE_STRICT_ALIGNMENT
    return *(T*)ptr;
#else
    T value;
    ((char*)&value)[0] = ((char*)ptr)[0];
    ((char*)&value)[1] = ((char*)ptr)[1];
    return value;
#endif
}

template <typename T>
EASY_FORCE_INLINE T unaligned_load16(const void* ptr, T* val)
{
    static_assert(sizeof(T) == 2, "16 bit type required.");
#ifndef EASY_ENABLE_STRICT_ALIGNMENT
    *val = *(T*)ptr;
    return *val;
#else
    ((char*)val)[0] = ((char*)ptr)[0];
    ((char*)val)[1] = ((char*)ptr)[1];
    return *val;
#endif
}

template <typename T>
EASY_FORCE_INLINE T unaligned_load32(const void* ptr)
{
    static_assert(sizeof(T) == 4, "32 bit type required.");
#ifndef EASY_ENABLE_STRICT_ALIGNMENT
    return *(T*)ptr;
#else
    T value;
    ((char*)&value)[0] = ((char*)ptr)[0];
    ((char*)&value)[1] = ((char*)ptr)[1];
    ((char*)&value)[2] = ((char*)ptr)[2];
    ((char*)&value)[3] = ((char*)ptr)[3];
    return value;
#endif
}

template <typename T>
EASY_FORCE_INLINE T unaligned_load32(const void* ptr, T* val)
{
    static_assert(sizeof(T) == 4, "32 bit type required.");
#ifndef EASY_ENABLE_STRICT_ALIGNMENT
    *val = *(T*)ptr;
#else
    ((char*)&val)[0] = ((char*)ptr)[0];
    ((char*)&val)[1] = ((char*)ptr)[1];
    ((char*)&val)[2] = ((char*)ptr)[2];
    ((char*)&val)[3] = ((char*)ptr)[3];
    return *val;
#endif
}

template <typename T>
EASY_FORCE_INLINE T unaligned_load64(const void* ptr)
{
    static_assert(sizeof(T) == 8, "64 bit type required.");
#ifndef EASY_ENABLE_STRICT_ALIGNMENT
    return *(T*)ptr;
#else
    if (!is_aligned<alignof(T)>(ptr)) {
        T value;
        ((char*)&value)[0] = ((char*)ptr)[0];
        ((char*)&value)[1] = ((char*)ptr)[1];
        ((char*)&value)[2] = ((char*)ptr)[2];
        ((char*)&value)[3] = ((char*)ptr)[3];
        ((char*)&value)[4] = ((char*)ptr)[4];
        ((char*)&value)[5] = ((char*)ptr)[5];
        ((char*)&value)[6] = ((char*)ptr)[6];
        ((char*)&value)[7] = ((char*)ptr)[7];
        return value;
    }
    else {
        return *(T*)ptr;
    }
#endif
}

template <typename T>
EASY_FORCE_INLINE T unaligned_load64(const void* ptr, T* val)
{
    static_assert(sizeof(T) == 8, "64 bit type required.");
#ifndef EASY_ENABLE_STRICT_ALIGNMENT
    *val = *(T*)ptr;
#else
    if (!is_aligned<alignof(T)>(ptr)) {
        ((char*)&val)[0] = ((char*)ptr)[0];
        ((char*)&val)[1] = ((char*)ptr)[1];
        ((char*)&val)[2] = ((char*)ptr)[2];
        ((char*)&val)[3] = ((char*)ptr)[3];
        ((char*)&val)[4] = ((char*)ptr)[4];
        ((char*)&val)[5] = ((char*)ptr)[5];
        ((char*)&val)[6] = ((char*)ptr)[6];
        ((char*)&val)[7] = ((char*)ptr)[7];
        return *val;
    }
    else {
        *val = *(T*)ptr;
        return *val;
    }
#endif
}

//////////////////////////////////////////////////////////////////////////

template <uint16_t N>
class chunk_allocator
{
    struct chunk { EASY_ALIGNED(char, data[N], EASY_ALIGNMENT_SIZE); chunk* prev = nullptr; };

    struct chunk_list
    {
        chunk* last;

        chunk_list() : last(nullptr)
        {
            static_assert(sizeof(char) == 1, "easy_profiler logic error: sizeof(char) != 1 for this platform! Please, contact easy_profiler authors to resolve your problem.");
            emplace_back();
        }

        ~chunk_list()
        {
            do free_last(); while (last != nullptr);
        }

        void clear_all_except_last()
        {
            while (last->prev != nullptr)
                free_last();
            zero_last_chunk_size();
        }

        void emplace_back()
        {
            auto prev = last;
            last = ::new (EASY_MALLOC(sizeof(chunk), EASY_ALIGNMENT_SIZE)) chunk();
            last->prev = prev;
            zero_last_chunk_size();
        }

        /** Invert current chunks list to enable to iterate over chunks list in direct order.

        This method is used by serialize().
        */
        void invert()
        {
            chunk* next = nullptr;

            while (last->prev != nullptr) {
                auto p = last->prev;
                last->prev = next;
                next = last;
                last = p;
            }

            last->prev = next;
        }

    private:

        chunk_list(const chunk_list&) = delete;
        chunk_list(chunk_list&&) = delete;

        void free_last()
        {
            auto p = last;
            last = last->prev;
            EASY_FREE(p);
        }

        void zero_last_chunk_size()
        {
            // Although there is no need for unaligned access stuff b/c a new chunk will
            // usually be at least 8 byte aligned (and we only need 2 byte alignment),
            // this is the only way I have been able to get rid of the GCC strict-aliasing warning
            // without using std::memset. It's an extra line, but is just as fast as *(uint16_t*)last->data = 0;
            char* const data = last->data;
            *(uint16_t*)data = (uint16_t)0;
        }
    };

    // Used in serialize(): workaround for no constexpr support in MSVC 2013.
    static const int_fast32_t MAX_CHUNK_OFFSET = N - sizeof(uint16_t);
    static const uint16_t N_MINUS_ONE = N - 1;

    chunk_list      m_chunks; ///< List of chunks.
    uint32_t          m_size; ///< Number of elements stored(# of times allocate() has been called.)
    uint16_t   m_chunkOffset; ///< Number of bytes used in the current chunk.

public:

    chunk_allocator() : m_size(0), m_chunkOffset(0)
    {
    }

    /** Allocate n bytes.

    Automatically checks if there is enough preserved memory to store additional n bytes
    and allocates additional buffer if needed.
    */
    void* allocate(uint16_t n)
    {
        ++m_size;

        if (!need_expand(n))
        {
            // Temp to avoid extra load due to this* aliasing.
            uint16_t chunkOffset = m_chunkOffset;
            char* data = m_chunks.last->data + chunkOffset;
            chunkOffset += n + sizeof(uint16_t);
            m_chunkOffset = chunkOffset;

            unaligned_store16(data, n);
            data += sizeof(uint16_t);

            // If there is enough space for at least another payload size,
            // set it to zero.
            if (chunkOffset < N_MINUS_ONE)
                unaligned_zero16(data + n);

            return data;
        }

        m_chunkOffset = n + sizeof(uint16_t);
        m_chunks.emplace_back();

        char* data = m_chunks.last->data;
        unaligned_store16(data, n);
        data += sizeof(uint16_t);
        
        // We assume here that it takes more than one element to fill a chunk.
        unaligned_zero16(data + n);

        return data;
    }

    /** Check if current storage is not enough to store additional n bytes.
    */
    bool need_expand(uint16_t n) const
    {
        return (m_chunkOffset + n + sizeof(uint16_t)) > N;
    }

    uint32_t size() const
    {
        return m_size;
    }

    bool empty() const
    {
        return m_size == 0;
    }

    void clear()
    {
        m_size = 0;
        m_chunkOffset = 0;
        m_chunks.clear_all_except_last(); // There is always at least one chunk
    }

    /** Serialize data to stream.

    \warning Data will be cleared after serialization.
    */
    void serialize(profiler::OStream& _outputStream)
    {
        // Chunks are stored in reversed order (stack).
        // To be able to iterate them in direct order we have to invert the chunks list.
        m_chunks.invert();

        // Each chunk is an array of N bytes that can hold between
        // 1(if the list isn't empty) and however many elements can fit in a chunk,
        // where an element consists of a payload size + a payload as follows:
        // elementStart[0..1]: size as a uint16_t
        // elementStart[2..size-1]: payload.
        
        // The maximum chunk offset is N-sizeof(uint16_t) b/c, if we hit that (or go past),
        // there is either no space left, 1 byte left, or 2 bytes left, all of which are
        // too small to cary more than a zero-sized element.

        chunk* current = m_chunks.last;
        do {
            const char* data = current->data;
            int_fast32_t chunkOffset = 0; // signed int so overflow is not checked.
            uint16_t payloadSize = unaligned_load16<uint16_t>(data);
            while (chunkOffset < MAX_CHUNK_OFFSET && payloadSize != 0) {
                const uint16_t chunkSize = sizeof(uint16_t) + payloadSize;
                _outputStream.write(data, chunkSize);
                data += chunkSize;
                chunkOffset += chunkSize;
                unaligned_load16(data, &payloadSize);
            }

            current = current->prev;
        } while (current != nullptr);

        clear();
    }

private:

    chunk_allocator(const chunk_allocator&) = delete;
    chunk_allocator(chunk_allocator&&) = delete;

}; // END of class chunk_allocator.

//////////////////////////////////////////////////////////////////////////

#endif // EASY_PROFILER_CHUNK_ALLOCATOR_H
