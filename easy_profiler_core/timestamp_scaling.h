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

#ifndef EASY_PROFILER_TIMESTAMP_SCALING_H
#define EASY_PROFILER_TIMESTAMP_SCALING_H

#include <easy/details/profiler_public_types.h>

#include <cstdint>
#include <limits>

//! Computes ticks * numerator / denominator without the intermediate product
//! overflowing.
//! \returns false, leaving result untouched, when either factor is zero or the
//! quotient itself does not fit timestamp_t; the caller decides whether that
//! saturates or fails.
inline bool scaleTimestamp(profiler::timestamp_t ticks, uint64_t numerator, uint64_t denominator,
                           profiler::timestamp_t& result)
{
    if (numerator == 0 || denominator == 0)
        return false;

    const profiler::timestamp_t maximum = std::numeric_limits<profiler::timestamp_t>::max();
    const profiler::timestamp_t whole = ticks / denominator;
    const profiler::timestamp_t remainder = ticks % denominator;
    if (whole > maximum / numerator)
        return false;

    const profiler::timestamp_t scaled = whole * numerator;
    profiler::timestamp_t fractional = 0;
    if (remainder <= maximum / numerator)
    {
        fractional = remainder * numerator / denominator;
    }
    else
    {
        // This term is always below numerator, so the floating-point fallback
        // cannot overflow its integer destination.
        fractional = static_cast<profiler::timestamp_t>(
            static_cast<long double>(remainder) *
            static_cast<long double>(numerator) /
            static_cast<long double>(denominator));
    }

    if (fractional > maximum - scaled)
        return false;

    result = scaled + fractional;
    return true;
}

#endif // EASY_PROFILER_TIMESTAMP_SCALING_H
