/************************************************************************
* file name         : event_trace_win.h
* ----------------- :
* creation time     : 2016/09/04
* author            : Victor Zarubkin
* email             : v.s.zarubkin@gmail.com
* ----------------- :
* description       : The file contains declaration of EasyEventTracer class used for tracing
*                   : Windows system events to get context switches.
* ----------------- :
* change log        : * 2016/09/04 Victor Zarubkin: initial commit.
*                   :
*                   : *
* ----------------- :
* license           : Lightweight profiler library for c++
*                   : Copyright(C) 2016  Sergey Yagovtsev, Victor Zarubkin
*                   : 
*                   : 
*                   : Licensed under the Apache License, Version 2.0 (the "License");
*                   : you may not use this file except in compliance with the License.
*                   : You may obtain a copy of the License at
*                   : 
*                   : http://www.apache.org/licenses/LICENSE-2.0
*                   : 
*                   : Unless required by applicable law or agreed to in writing, software
*                   : distributed under the License is distributed on an "AS IS" BASIS,
*                   : WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
*                   : See the License for the specific language governing permissions and
*                   : limitations under the License.
*                   : 
*                   : 
*                   : GNU General Public License Usage
*                   : Alternatively, this file may be used under the terms of the GNU
*                   : General Public License as published by the Free Software Foundation,
*                   : either version 3 of the License, or (at your option) any later version.
*                   : 
*                   : This program is distributed in the hope that it will be useful,
*                   : but WITHOUT ANY WARRANTY; without even the implied warranty of
*                   : MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.See the
*                   : GNU General Public License for more details.
*                   : 
*                   : You should have received a copy of the GNU General Public License
*                   : along with this program.If not, see <http://www.gnu.org/licenses/>.
************************************************************************/

#ifndef EASY_PROFILER__EVENT_TRACE_WINDOWS__H_
#define EASY_PROFILER__EVENT_TRACE_WINDOWS__H_
#ifdef _WIN32

#define INITGUID  // This is to enable using SystemTraceControlGuid in evntrace.h.
#include <Windows.h>
#include <Strsafe.h>
#include <wmistr.h>
#include <evntrace.h>
#include <evntcons.h>
#include <thread>
#include <atomic>
#include "event_trace_status.h"
#include "spin_lock.h"

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

namespace profiler {

    class EasyEventTracer EASY_FINAL
    {
#ifndef EASY_MAGIC_STATIC_CPP11
        friend class EasyEventTracerInstance;
#endif

#pragma pack(push, 1)
        struct Properties {
            EVENT_TRACE_PROPERTIES base;
            char sessionName[sizeof(KERNEL_LOGGER_NAME)];
        };
#pragma pack(pop)

        ::std::thread       m_processThread;
        Properties             m_properties;
        EVENT_TRACE_LOGFILE         m_trace;
        ::profiler::spin_lock        m_spin;
        ::std::atomic_bool    m_lowPriority;
        TRACEHANDLE         m_sessionHandle = INVALID_PROCESSTRACE_HANDLE;
        TRACEHANDLE          m_openedHandle = INVALID_PROCESSTRACE_HANDLE;
        bool                     m_bEnabled = false;

    public:

        static EasyEventTracer& instance();
        ~EasyEventTracer();

        bool isLowPriority() const;

        ::profiler::EventTracingEnableStatus enable(bool _force = false);
        void disable();
        void setLowPriority(bool _value);
        bool getLowPriority();
        static void setProcessPrivileges();

    private:

        EasyEventTracer();

        inline EVENT_TRACE_PROPERTIES* props()
        {
            return reinterpret_cast<EVENT_TRACE_PROPERTIES*>(&m_properties);
        }

        ::profiler::EventTracingEnableStatus startTrace(bool _force, int _step = 0);

    }; // END of class EasyEventTracer.

} // END of namespace profiler.

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#endif // _WIN32
#endif // EASY_PROFILER__EVENT_TRACE_WINDOWS__H_
