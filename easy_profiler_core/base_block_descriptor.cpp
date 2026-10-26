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

#include <easy/details/profiler_public_types.h>

#if defined(_MSC_VER)
# include <intrin.h>
#endif

namespace profiler
{

    BaseBlockDescriptor::BaseBlockDescriptor(block_id_t _id, EasyBlockStatus _status, int _line,
                                             block_type_t _block_type, color_t _color) EASY_NOEXCEPT
        : m_id(_id)
        , m_line(_line)
        , m_color(_color)
        , m_type(_block_type)
        , m_status(_status)
    {

    }

    EasyBlockStatus BaseBlockDescriptor::status() const EASY_NOEXCEPT
    {
#if defined(_MSC_VER)
        return static_cast<EasyBlockStatus>(_InterlockedCompareExchange8(
            reinterpret_cast<volatile char*>(const_cast<EasyBlockStatus*>(&m_status)), 0, 0));
#elif defined(__clang__) || defined(__GNUC__)
        return static_cast<EasyBlockStatus>(__atomic_load_n(
            reinterpret_cast<const uint8_t*>(&m_status), __ATOMIC_RELAXED));
#else
# error "Atomic block status access is not implemented for this compiler"
#endif
    }

    void BaseBlockDescriptor::setStatusAtomic(EasyBlockStatus _status) EASY_NOEXCEPT
    {
#if defined(_MSC_VER)
        _InterlockedExchange8(reinterpret_cast<volatile char*>(&m_status), static_cast<char>(_status));
#elif defined(__clang__) || defined(__GNUC__)
        __atomic_store_n(reinterpret_cast<uint8_t*>(&m_status), static_cast<uint8_t>(_status), __ATOMIC_RELAXED);
#else
# error "Atomic block status access is not implemented for this compiler"
#endif
    }

} // end of namespace profiler.
