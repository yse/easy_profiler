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
#ifdef __ARM_ARCH
#include <sys/time.h>
#endif//__ARM_ARCH
#endif

static inline profiler::timestamp_t getCurrentTime()
{
#ifdef _WIN32
    //see https://msdn.microsoft.com/library/windows/desktop/dn553408(v=vs.85).aspx
    LARGE_INTEGER elapsedMicroseconds;
    if (!QueryPerformanceCounter(&elapsedMicroseconds))
        return 0;
    return (profiler::timestamp_t)elapsedMicroseconds.QuadPart;
#else// not _WIN32

#if (defined(__GNUC__) || defined(__ICC))

	// part of code from google/benchmark library (Licensed under the Apache License, Version 2.0)
	// see https://github.com/google/benchmark/blob/master/src/cycleclock.h#L111
    #if defined(__i386__)
	  int64_t ret;
	  __asm__ volatile("rdtsc" : "=A"(ret));
	  return ret;
	#elif defined(__x86_64__) || defined(__amd64__)
	  uint64_t low, high;
	  __asm__ volatile("rdtsc" : "=a"(low), "=d"(high));
	  return (high << 32) | low;
	#elif defined(__powerpc__) || defined(__ppc__)
	  // This returns a time-base, which is not always precisely a cycle-count.
	  int64_t tbl, tbu0, tbu1;
	  asm("mftbu %0" : "=r"(tbu0));
	  asm("mftb  %0" : "=r"(tbl));
	  asm("mftbu %0" : "=r"(tbu1));
	  tbl &= -static_cast<int64>(tbu0 == tbu1);
	  // high 32 bits in tbu1; low 32 bits in tbl  (tbu0 is garbage)
	  return (tbu1 << 32) | tbl;
	#elif defined(__sparc__)
	  int64_t tick;
	  asm(".byte 0x83, 0x41, 0x00, 0x00");
	  asm("mov   %%g1, %0" : "=r"(tick));
	  return tick;
	#elif defined(__ia64__)
	  int64_t itc;
	  asm("mov %0 = ar.itc" : "=r"(itc));
	  return itc;
	#elif defined(COMPILER_MSVC) && defined(_M_IX86)
	  // Older MSVC compilers (like 7.x) don't seem to support the
	  // __rdtsc intrinsic properly, so I prefer to use _asm instead
	  // when I know it will work.  Otherwise, I'll use __rdtsc and hope
	  // the code is being compiled with a non-ancient compiler.
	  _asm rdtsc
	#elif defined(COMPILER_MSVC)
	  return __rdtsc();
	#elif defined(__aarch64__)
	  // System timer of ARMv8 runs at a different frequency than the CPU's.
	  // The frequency is fixed, typically in the range 1-50MHz.  It can be
	  // read at CNTFRQ special register.  We assume the OS has set up
	  // the virtual timer properly.
	  int64_t virtual_timer_value;
	  asm volatile("mrs %0, cntvct_el0" : "=r"(virtual_timer_value));
	  return virtual_timer_value;
	#elif defined(__ARM_ARCH)
	#if (__ARM_ARCH >= 6)  // V6 is the earliest arch that has a standard cyclecount
	  uint32_t pmccntr;
	  uint32_t pmuseren;
	  uint32_t pmcntenset;
	  // Read the user mode perf monitor counter access permissions.
	  asm volatile("mrc p15, 0, %0, c9, c14, 0" : "=r"(pmuseren));
	  if (pmuseren & 1) {  // Allows reading perfmon counters for user mode code.
		asm volatile("mrc p15, 0, %0, c9, c12, 1" : "=r"(pmcntenset));
		if (pmcntenset & 0x80000000ul) {  // Is it counting?
		  asm volatile("mrc p15, 0, %0, c9, c13, 0" : "=r"(pmccntr));
		  // The counter is set up to count every 64th cycle
		  return static_cast<int64_t>(pmccntr) * 64;  // Should optimize to << 6
		}
	  }
	#endif
	  struct timeval tv;
	  gettimeofday(&tv, nullptr);
	  return static_cast<int64_t>(tv.tv_sec) * 1000000 + tv.tv_usec;
	#elif defined(__mips__)
	  // mips apparently only allows rdtsc for superusers, so we fall
	  // back to gettimeofday.  It's possible clock_gettime would be better.
	  struct timeval tv;
	  gettimeofday(&tv, nullptr);
	  return static_cast<int64_t>(tv.tv_sec) * 1000000 + tv.tv_usec;
	#else
	  #warning You need to define fast getCurrentTime() for your OS and CPU
	  return std::chrono::time_point_cast<std::chrono::nanoseconds>(std::chrono::system_clock::now()).time_since_epoch().count();
      #define USE_STD_CHRONO
	#endif

#else // not _WIN32, __GNUC__, __ICC
	#warning You need to define fast getCurrentTime() for your OS and CPU
    return std::chrono::time_point_cast<std::chrono::nanoseconds>(std::chrono::system_clock::now()).time_since_epoch().count();
    #define USE_STD_CHRONO
#endif

#endif
}


#endif // EASY_______CURRENT_TIME_H_____
