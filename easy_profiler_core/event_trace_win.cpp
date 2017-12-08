/************************************************************************
* file name         : event_trace_win.cpp
* ----------------- :
* creation time     : 2016/09/04
* author            : Victor Zarubkin
* email             : v.s.zarubkin@gmail.com
* ----------------- :
* description       : The file contains implementation of EasyEventTracer class used for tracing
*                   : Windows system events to get context switches.
* ----------------- :
* change log        : * 2016/09/04 Victor Zarubkin: initial commit.
*                   :
*                   : * 2016/09/13 Victor Zarubkin: get process id and process name
*                   :        of the owner of thread with id == CSwitch::NewThreadId.
*                   : 
*                   : * 2016/09/17 Victor Zarubkin: added log messages printing.
* ----------------- :
* license           : Lightweight profiler library for c++
*                   : Copyright(C) 2016-2017  Sergey Yagovtsev, Victor Zarubkin
*                   :
*                   : Licensed under either of
*                   :     * MIT license (LICENSE.MIT or http://opensource.org/licenses/MIT)
*                   :     * Apache License, Version 2.0, (LICENSE.APACHE or http://www.apache.org/licenses/LICENSE-2.0)
*                   : at your option.
*                   :
*                   : The MIT License
*                   :
*                   : Permission is hereby granted, free of charge, to any person obtaining a copy
*                   : of this software and associated documentation files (the "Software"), to deal
*                   : in the Software without restriction, including without limitation the rights
*                   : to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies
*                   : of the Software, and to permit persons to whom the Software is furnished
*                   : to do so, subject to the following conditions:
*                   :
*                   : The above copyright notice and this permission notice shall be included in all
*                   : copies or substantial portions of the Software.
*                   :
*                   : THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED,
*                   : INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR
*                   : PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE
*                   : LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
*                   : TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE
*                   : USE OR OTHER DEALINGS IN THE SOFTWARE.
*                   :
*                   : The Apache License, Version 2.0 (the "License")
*                   :
*                   : You may not use this file except in compliance with the License.
*                   : You may obtain a copy of the License at
*                   :
*                   : http://www.apache.org/licenses/LICENSE-2.0
*                   :
*                   : Unless required by applicable law or agreed to in writing, software
*                   : distributed under the License is distributed on an "AS IS" BASIS,
*                   : WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
*                   : See the License for the specific language governing permissions and
*                   : limitations under the License.
************************************************************************/

#ifdef _WIN32
#include <memory.h>
#include <chrono>
#if UNICODE
#   include <codecvt>
#   include <sstream>
#endif
#include <unordered_map>
#include <easy/profiler.h>
#include "profile_manager.h"
#include "current_time.h"

#include "event_trace_win.h"
#include <Psapi.h>

#ifdef __MINGW32__
#include <processthreadsapi.h>
#endif

//#include <Shellapi.h>

#if EASY_OPTION_LOG_ENABLED != 0
# include <iostream>

# ifndef EASY_ERRORLOG
#  define EASY_ERRORLOG ::std::cerr
# endif

# ifndef EASY_LOG
#  define EASY_LOG ::std::cerr
# endif

# ifndef EASY_ERROR
#  define EASY_ERROR(LOG_MSG) EASY_ERRORLOG << "EasyProfiler ERROR: " << LOG_MSG
# endif

# ifndef EASY_WARNING
#  define EASY_WARNING(LOG_MSG) EASY_ERRORLOG << "EasyProfiler WARNING: " << LOG_MSG
# endif

# ifndef EASY_LOGMSG
#  define EASY_LOGMSG(LOG_MSG) EASY_LOG << "EasyProfiler INFO: " << LOG_MSG
# endif

# ifndef EASY_LOG_ONLY
#  define EASY_LOG_ONLY(CODE) CODE
# endif

#else

# ifndef EASY_ERROR
#  define EASY_ERROR(LOG_MSG) 
# endif

# ifndef EASY_WARNING
#  define EASY_WARNING(LOG_MSG) 
# endif

# ifndef EASY_LOGMSG
#  define EASY_LOGMSG(LOG_MSG) 
# endif

# ifndef EASY_LOG_ONLY
#  define EASY_LOG_ONLY(CODE) 
# endif

#endif

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

//extern ProfileManager& MANAGER;
#define MANAGER ProfileManager::instance()

extern const ::profiler::color_t EASY_COLOR_INTERNAL_EVENT;

#ifdef __MINGW32__
::std::atomic<uint64_t> TRACING_END_TIME = ATOMIC_VAR_INIT(~0ULL);
char KERNEL_LOGGER[] = KERNEL_LOGGER_NAME;
#else
::std::atomic_uint64_t TRACING_END_TIME = ATOMIC_VAR_INIT(~0ULL);
#endif

namespace profiler {

    const decltype(EVENT_DESCRIPTOR::Opcode) SWITCH_CONTEXT_OPCODE = 36;
    const int RAW_TIMESTAMP_TIME_TYPE = 1;

    namespace {

        //////////////////////////////////////////////////////////////////////////

        class ProcessInfo {
        public:
            bool initialized() const;
            void init(processid_t process_id);
            void set_name(PTCHAR process_name);

            const char* info_cstr() const
            {
                return name.c_str();
            }

        private:
            std::string name;
        };

        bool ProcessInfo::initialized() const
        {
            return !name.empty();
        }

        void ProcessInfo::init(processid_t process_id)
        {
            if (initialized())
                return;

            std::stringstream ss;
            ss << process_id;
            name = ss.str();
        }

        void ProcessInfo::set_name(PTCHAR process_name)
        {
            name += " ";
#if UNICODE
            using ConvertType = std::codecvt_utf8<wchar_t>;
            std::wstring_convert<ConvertType, wchar_t> converter;
            name += converter.to_bytes(process_name);
#else
            name.append(process_name);
#endif
        }

    }  // END of namespace unnamed.

    //////////////////////////////////////////////////////////////////////////

    // CSwitch class
    // See https://msdn.microsoft.com/en-us/library/windows/desktop/aa964744(v=vs.85).aspx
    // EventType = 36
    struct CSwitch
    {
        uint32_t                 NewThreadId;
        uint32_t                 OldThreadId;
        int8_t             NewThreadPriority;
        int8_t             OldThreadPriority;
        uint8_t               PreviousCState;
        int8_t                     SpareByte;
        int8_t           OldThreadWaitReason;
        int8_t             OldThreadWaitMode;
        int8_t                OldThreadState;
        int8_t   OldThreadWaitIdealProcessor;
        uint32_t           NewThreadWaitTime;
        uint32_t                    Reserved;
    };

    //////////////////////////////////////////////////////////////////////////

    struct do_not_calc_hash {
        template <class T> inline size_t operator()(T _value) const {
            return static_cast<size_t>(_value);
        }
    };

    typedef ::std::unordered_map<decltype(CSwitch::NewThreadId), ProcessInfo*, do_not_calc_hash> thread_process_info_map;
    typedef ::std::unordered_map<processid_t, ProcessInfo, do_not_calc_hash> process_info_map;

    // Using static is safe because processTraceEvent() is called from one thread
    process_info_map PROCESS_INFO_TABLE;
    thread_process_info_map THREAD_PROCESS_INFO_TABLE = ([](){ thread_process_info_map initial; initial[0U] = nullptr; return ::std::move(initial); })();

    //////////////////////////////////////////////////////////////////////////

    void WINAPI processTraceEvent(PEVENT_RECORD _traceEvent)
    {
        if (_traceEvent->EventHeader.EventDescriptor.Opcode != SWITCH_CONTEXT_OPCODE)
            return;

        if (sizeof(CSwitch) != _traceEvent->UserDataLength)
            return;

        EASY_FUNCTION(EASY_COLOR_INTERNAL_EVENT, ::profiler::OFF);

        auto _contextSwitchEvent = reinterpret_cast<CSwitch*>(_traceEvent->UserData);
        const auto time = static_cast<::profiler::timestamp_t>(_traceEvent->EventHeader.TimeStamp.QuadPart);
        if (time > TRACING_END_TIME.load(::std::memory_order_acquire))
            return;

        DWORD pid = 0;
        const char* process_name = "";

        // Trying to get target process name and id
        auto it = THREAD_PROCESS_INFO_TABLE.find(_contextSwitchEvent->NewThreadId);
        if (it == THREAD_PROCESS_INFO_TABLE.end())
        {
            auto hThread = OpenThread(THREAD_QUERY_LIMITED_INFORMATION, FALSE, _contextSwitchEvent->NewThreadId);
            if (hThread != nullptr)
            {
                pid = GetProcessIdOfThread(hThread);
                auto pinfo = &PROCESS_INFO_TABLE[pid];

                if (!pinfo->initialized())
                {
                    pinfo->init(pid);

                    /*
                    According to documentation, using GetModuleBaseName() requires
                    PROCESS_QUERY_INFORMATION | PROCESS_VM_READ access rights.
                    But it works fine with PROCESS_QUERY_LIMITED_INFORMATION instead of PROCESS_QUERY_INFORMATION.
                    
                    See https://msdn.microsoft.com/en-us/library/windows/desktop/ms683196(v=vs.85).aspx
                    */

                    //auto hProc = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, pid);
                    //if (hProc == nullptr)
                    auto hProc = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION | PROCESS_VM_READ, FALSE, pid);
                    if (hProc != nullptr)
                    {
                        static TCHAR buf[MAX_PATH] = {}; // Using static is safe because processTraceEvent() is called from one thread
                        auto len = GetModuleBaseName(hProc, 0, buf, MAX_PATH);

                        if (len != 0)
                            pinfo->set_name(buf);

                        CloseHandle(hProc);
                    }
                    else
                    {
                        //auto err = GetLastError();
                        //printf("OpenProcess(%u) fail: GetLastError() == %u\n", pid, err);

                        if (pid == 4)
                            pinfo->set_name(TEXT("System"));
                    }
                }

                process_name = pinfo->info_cstr();
                THREAD_PROCESS_INFO_TABLE[_contextSwitchEvent->NewThreadId] = pinfo;

                CloseHandle(hThread);
            }
            else
            {
                //printf("Can not OpenThread(%u);\n", _contextSwitchEvent->NewThreadId);
                THREAD_PROCESS_INFO_TABLE[_contextSwitchEvent->NewThreadId] = nullptr;
            }
        }
        else
        {
            auto pinfo = it->second;
            if (pinfo != nullptr)
                process_name = pinfo->info_cstr();
            else if (it->first == 0)
                process_name = "System Idle";
            else if (it->first == 4)
                process_name = "System";
        }

        MANAGER.beginContextSwitch(_contextSwitchEvent->OldThreadId, time, _contextSwitchEvent->NewThreadId, process_name);
        MANAGER.endContextSwitch(_contextSwitchEvent->NewThreadId, pid, time);
    }

    //////////////////////////////////////////////////////////////////////////

    EasyEventTracer::Properties::Properties()
    {
#if UNICODE
        std::wcstombs(sessionName, KERNEL_LOGGER_NAME, sizeof(sessionName));
#else
        strncpy(sessionName, KERNEL_LOGGER_NAME, sizeof(sessionName));
#endif
    }

    //////////////////////////////////////////////////////////////////////////

#ifndef EASY_MAGIC_STATIC_CPP11
    class EasyEventTracerInstance {
        friend EasyEventTracer;
        EasyEventTracer instance;
    } EASY_EVENT_TRACER;
#endif

    EasyEventTracer& EasyEventTracer::instance()
    {
#ifndef EASY_MAGIC_STATIC_CPP11
        return EASY_EVENT_TRACER.instance;
#else
        static EasyEventTracer tracer;
        return tracer;
#endif
    }

    EasyEventTracer::EasyEventTracer()
    {
        m_lowPriority = ATOMIC_VAR_INIT(EASY_OPTION_LOW_PRIORITY_EVENT_TRACING);
    }

    EasyEventTracer::~EasyEventTracer()
    {
        disable();
    }

    bool EasyEventTracer::isLowPriority() const
    {
        return m_lowPriority.load(::std::memory_order_acquire);
    }

    void EasyEventTracer::setLowPriority(bool _value)
    {
        m_lowPriority.store(_value, ::std::memory_order_release);
    }

    bool setPrivilege(HANDLE hToken, PTCHAR _privelegeName)
    {
        bool success = false;

        if (hToken)
        {
            LUID privilegyId;
            if (LookupPrivilegeValue(NULL, _privelegeName, &privilegyId))
            {
                TOKEN_PRIVILEGES tokenPrivilege;
                tokenPrivilege.PrivilegeCount = 1;
                tokenPrivilege.Privileges[0].Luid = privilegyId;
                tokenPrivilege.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;
                success = AdjustTokenPrivileges(hToken, FALSE, &tokenPrivilege, sizeof(TOKEN_PRIVILEGES), NULL, NULL) != FALSE;
            }
        }

        EASY_LOG_ONLY(
            if (!success)
                EASY_WARNING("Failed to set " << _privelegeName << " privelege for the application.\n");
        )

        return success;
    }

    void EasyEventTracer::setProcessPrivileges()
    {
        static bool alreadySet = false;
        if (alreadySet)
            return;

        alreadySet = true;

        HANDLE hToken = nullptr;
        if (OpenProcessToken(GetCurrentProcess(), TOKEN_ADJUST_PRIVILEGES | TOKEN_QUERY, &hToken))
        {
#if EASY_OPTION_LOG_ENABLED != 0
            const bool success = setPrivilege(hToken, SE_DEBUG_NAME);
            if (!success)
                EASY_WARNING("Some context switch events could not get process name.\n");
#else
            setPrivilege(hToken, SE_DEBUG_NAME);
#endif
            
            CloseHandle(hToken);
        }
        EASY_LOG_ONLY(
            else {
                EASY_WARNING("Failed to open process to adjust priveleges.\n");
            }
        )
    }

    ::profiler::EventTracingEnableStatus EasyEventTracer::startTrace(bool _force, int _step)
    {
        auto startTraceResult = StartTrace(&m_sessionHandle, KERNEL_LOGGER_NAME, props());
        switch (startTraceResult)
        {
            case ERROR_SUCCESS:
                return EVENT_TRACING_LAUNCHED_SUCCESSFULLY;

            case ERROR_ALREADY_EXISTS:
            {
                if (_force)
                {
                    // Try to stop another event tracing session to force launch self session.

                    if (_step == 0)
                    {
                        /*
                        According to https://msdn.microsoft.com/en-us/library/windows/desktop/aa363696(v=vs.85).aspx
                        SessionHandle is ignored (and could be NULL) if SessionName is not NULL,
                        and you only need to set the Wnode.BufferSize, Wnode.Guid, LoggerNameOffset, and LogFileNameOffset
                        in EVENT_TRACE_PROPERTIES structure if ControlCode is EVENT_TRACE_CONTROL_STOP.
                        All data is already set for m_properties to the moment. Simply copy m_properties and use the copy.

                        This method supposed to be faster than launching console window and executing shell command,
                        but if that would not work, return to using shell command "logman stop".
                        */

                        // static is safe because we are guarded by spin-lock m_spin
                        static Properties p;
                        p.base = m_properties.base; // Use copy of m_properties to make sure m_properties will not be changed

                        // Stop another session
                        ControlTrace((TRACEHANDLE)NULL, KERNEL_LOGGER_NAME, reinterpret_cast<EVENT_TRACE_PROPERTIES*>(&p), EVENT_TRACE_CONTROL_STOP);

                        // Console window variant:
                        //if (32 >= (int)ShellExecute(NULL, NULL, "logman", "stop \"" KERNEL_LOGGER_NAME "\" -ets", NULL, SW_HIDE))
                        //    return EVENT_TRACING_WAS_LAUNCHED_BY_SOMEBODY_ELSE;
                    }

                    if (_step < 4)
                    {
                        // Command executed successfully. Wait for a few time until tracing session finish.
                        ::std::this_thread::sleep_for(::std::chrono::milliseconds(500));
                        return startTrace(true, ++_step);
                    }
                }

                EASY_ERROR("Event tracing not launched: ERROR_ALREADY_EXISTS. To stop another session execute cmd: logman stop \"" << KERNEL_LOGGER_NAME << "\" -ets\n");
                return EVENT_TRACING_WAS_LAUNCHED_BY_SOMEBODY_ELSE;
            }

            case ERROR_ACCESS_DENIED:
                EASY_ERROR("Event tracing not launched: ERROR_ACCESS_DENIED. Try to launch your application as Administrator.\n");
                return EVENT_TRACING_NOT_ENOUGH_ACCESS_RIGHTS;

            case ERROR_BAD_LENGTH:
                EASY_ERROR("Event tracing not launched: ERROR_BAD_LENGTH. It seems that your KERNEL_LOGGER_NAME differs from \"" << m_properties.sessionName << "\". Try to re-compile easy_profiler or contact EasyProfiler developers.\n");
                return EVENT_TRACING_BAD_PROPERTIES_SIZE;
        }

        EASY_ERROR("Event tracing not launched: StartTrace() returned " << startTraceResult << ::std::endl);

        return EVENT_TRACING_MISTERIOUS_ERROR;
    }

    ::profiler::EventTracingEnableStatus EasyEventTracer::enable(bool _force)
    {
        ::profiler::guard_lock<::profiler::spin_lock> lock(m_spin);
        if (m_bEnabled)
            return EVENT_TRACING_LAUNCHED_SUCCESSFULLY;

        /*
        Trying to set debug privilege for current process
        to be able to get other process information (process name).
        */
        EasyEventTracer::setProcessPrivileges();

        // Clear properties
        memset(&m_properties, 0, sizeof(m_properties));
        m_properties.base.Wnode.BufferSize = sizeof(m_properties);
        m_properties.base.Wnode.Flags = WNODE_FLAG_TRACED_GUID;
        m_properties.base.Wnode.ClientContext = RAW_TIMESTAMP_TIME_TYPE;
        m_properties.base.Wnode.Guid = SystemTraceControlGuid;
        m_properties.base.LoggerNameOffset = sizeof(m_properties.base);
        m_properties.base.EnableFlags = EVENT_TRACE_FLAG_CSWITCH;
        m_properties.base.LogFileMode = EVENT_TRACE_REAL_TIME_MODE;

        // Start event tracing
        auto res = startTrace(_force);
        if (res != EVENT_TRACING_LAUNCHED_SUCCESSFULLY)
            return res;

        memset(&m_trace, 0, sizeof(m_trace));
#ifdef __MINGW32__
        m_trace.LoggerName = KERNEL_LOGGER;
#else
        m_trace.LoggerName = KERNEL_LOGGER_NAME;
#endif
        m_trace.ProcessTraceMode = PROCESS_TRACE_MODE_REAL_TIME | PROCESS_TRACE_MODE_EVENT_RECORD | PROCESS_TRACE_MODE_RAW_TIMESTAMP;
        m_trace.EventRecordCallback = ::profiler::processTraceEvent;

        m_openedHandle = OpenTrace(&m_trace);
        if (m_openedHandle == INVALID_PROCESSTRACE_HANDLE)
        {
            EASY_ERROR("Event tracing not launched: OpenTrace() returned invalid handle.\n");
            return EVENT_TRACING_OPEN_TRACE_ERROR;
        }

        /*
        Have to launch a thread to process events because according to MSDN documentation:
        
        The ProcessTrace function blocks the thread until it delivers all events, the BufferCallback function returns FALSE,
        or you call CloseTrace. If the consumer is consuming events in real time, the ProcessTrace function returns after
        the controller stops the trace session. (Note that there may be a several-second delay before the function returns.)
        
        https://msdn.microsoft.com/en-us/library/windows/desktop/aa364093(v=vs.85).aspx
        */
        m_processThread = ::std::thread([this](bool _lowPriority)
        {
            if (_lowPriority) // Set low priority for event tracing thread
                SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_LOWEST);
            EASY_THREAD_SCOPE("EasyProfiler.ETW");
            ProcessTrace(&m_openedHandle, 1, 0, 0);

        }, m_lowPriority.load(::std::memory_order_acquire));

        m_bEnabled = true;

        EASY_LOGMSG("Event tracing launched\n");
        return EVENT_TRACING_LAUNCHED_SUCCESSFULLY;
    }

    void EasyEventTracer::disable()
    {
        ::profiler::guard_lock<::profiler::spin_lock> lock(m_spin);
        if (!m_bEnabled)
            return;

        EASY_LOGMSG("Event tracing is stopping...\n");

        TRACING_END_TIME.store(getCurrentTime(), ::std::memory_order_release);

        ControlTrace(m_openedHandle, KERNEL_LOGGER_NAME, props(), EVENT_TRACE_CONTROL_STOP);
        CloseTrace(m_openedHandle);

        // Wait for ProcessTrace to finish to make sure no processTraceEvent() will be called later.
        if (m_processThread.joinable())
            m_processThread.join();

        m_bEnabled = false;

        // processTraceEvent() is not called anymore. Clean static maps is safe.
        PROCESS_INFO_TABLE.clear();
        THREAD_PROCESS_INFO_TABLE.clear();
        THREAD_PROCESS_INFO_TABLE[0U] = nullptr;

        TRACING_END_TIME.store(~0ULL, ::std::memory_order_release);

        EASY_LOGMSG("Event tracing stopped\n");
    }

} // END of namespace profiler.

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#endif // _WIN32
