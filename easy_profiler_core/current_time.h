/**
Lightweight profiler library for c++
Copyright(C) 2016  Sergey Yagovtsev, Victor Zarubkin


Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.


GNU General Public License Usage
Alternatively, this file may be used under the terms of the GNU
General Public License as published by the Free Software Foundation,
either version 3 of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.If not, see <http://www.gnu.org/licenses/>.
**/

#ifndef EASY_______CURRENT_TIME_H_____
#define EASY_______CURRENT_TIME_H_____

#include "easy/profiler.h"

#ifdef _WIN32
#include <Windows.h>
#else
#include <chrono>
#include <time.h>
#endif

static inline profiler::timestamp_t getCurrentTime()
{
#ifdef _WIN32
    //see https://msdn.microsoft.com/library/windows/desktop/dn553408(v=vs.85).aspx
    LARGE_INTEGER elapsedMicroseconds;
    if (!QueryPerformanceCounter(&elapsedMicroseconds))
        return 0;
    return (profiler::timestamp_t)elapsedMicroseconds.QuadPart;
#else

#if (defined(__GNUC__) || defined(__ICC))

    #if defined(__i386__)
        unsigned long long t;
        __asm__ __volatile__("rdtsc" : "=A"(t));
        return t;
    #elif defined(__x86_64__)
        unsigned int hi, lo;
        __asm__ __volatile__("rdtsc" : "=a" (lo), "=d" (hi));
        return ((uint64_t)hi << 32) | lo;
    #endif

#else
    return std::chrono::time_point_cast<std::chrono::nanoseconds>(std::chrono::system_clock::now()).time_since_epoch().count();
#define USE_STD_CHRONO
#endif

#endif
}


#endif // EASY_______CURRENT_TIME_H_____
