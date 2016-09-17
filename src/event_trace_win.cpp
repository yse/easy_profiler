

#ifdef _WIN32
#include <memory.h>
#include <chrono>
#include <unordered_map>
#include "profiler/profiler.h"
#include "profile_manager.h"

#include "event_trace_win.h"
#include <Psapi.h>

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

//extern ProfileManager& MANAGER;
#define MANAGER ProfileManager::instance()

namespace profiler {

    //////////////////////////////////////////////////////////////////////////

    struct ProcessInfo final {
        std::string    name;
        uint32_t     id = 0;
        int8_t    valid = 0;
    };

    //////////////////////////////////////////////////////////////////////////

    // CSwitch class
    // See https://msdn.microsoft.com/en-us/library/windows/desktop/aa964744(v=vs.85).aspx
    // EventType = 36
    struct CSwitch final
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

    typedef ::std::unordered_map<decltype(CSwitch::NewThreadId), ProcessInfo*, ::profiler::do_not_calc_hash> thread_process_info_map;
    typedef ::std::unordered_map<uint32_t, ProcessInfo, ::profiler::do_not_calc_hash> process_info_map;

    // Using static is safe because processTraceEvent() is called from one thread
    static process_info_map PROCESS_INFO_TABLE;
    static thread_process_info_map THREAD_PROCESS_INFO_TABLE = ([](){ thread_process_info_map initial; initial[0U] = nullptr; return ::std::move(initial); })();

    //////////////////////////////////////////////////////////////////////////

    void WINAPI processTraceEvent(PEVENT_RECORD _traceEvent)
    {
        static const decltype(_traceEvent->EventHeader.EventDescriptor.Opcode) SWITCH_CONTEXT_OPCODE = 36;
        if (_traceEvent->EventHeader.EventDescriptor.Opcode != SWITCH_CONTEXT_OPCODE)
            return;

        if (sizeof(CSwitch) != _traceEvent->UserDataLength)
            return;

        EASY_FUNCTION(::profiler::colors::White, ::profiler::DISABLED);

        auto _contextSwitchEvent = reinterpret_cast<CSwitch*>(_traceEvent->UserData);
        const auto time = static_cast<::profiler::timestamp_t>(_traceEvent->EventHeader.TimeStamp.QuadPart);

        const char* process_name = "";
        auto it = THREAD_PROCESS_INFO_TABLE.find(_contextSwitchEvent->NewThreadId);
        if (it == THREAD_PROCESS_INFO_TABLE.end())
        {
            auto hThread = OpenThread(THREAD_QUERY_LIMITED_INFORMATION, FALSE, _contextSwitchEvent->NewThreadId);
            if (hThread != nullptr)
            {
                auto pid = GetProcessIdOfThread(hThread);
                auto pinfo = &PROCESS_INFO_TABLE[pid];

                if (pinfo->valid == 0)
                {
                    if (pinfo->name.empty())
                    {
                        static char numbuf[128] = {};
                        sprintf(numbuf, "%u", pid);
                        pinfo->name = numbuf;
                        pinfo->id = pid;
                    }

                    // According to documentation, using GetModuleBaseName() requires
                    // PROCESS_QUERY_INFORMATION | PROCESS_VM_READ access rights.
                    // But it works fine with PROCESS_QUERY_LIMITED_INFORMATION instead of PROCESS_QUERY_INFORMATION.
                    // 
                    // See https://msdn.microsoft.com/en-us/library/windows/desktop/ms683196(v=vs.85).aspx
                    //auto hProc = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, pid);
                    //if (hProc == nullptr)
                    auto hProc = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION | PROCESS_VM_READ, FALSE, pid);
                    if (hProc != nullptr)
                    {
                        static TCHAR buf[MAX_PATH] = {}; // Using static is safe because processTraceEvent() is called from one thread
                        auto success = GetModuleBaseName(hProc, 0, buf, MAX_PATH);

                        if (success)
                        {
                            auto len = strlen(buf);
                            pinfo->name.reserve(pinfo->name.size() + 2 + len);
                            pinfo->name.append(" ", 1);
                            pinfo->name.append(buf, len);
                            pinfo->valid = 1;
                        }

                        CloseHandle(hProc);
                    }
                    else
                    {
                        //auto err = GetLastError();
                        //printf("OpenProcess(%u) fail: GetLastError() == %u\n", pid, err);
                        pinfo->valid = -1;
                    }
                }

                process_name = pinfo->name.c_str();
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
                process_name = pinfo->name.c_str();
        }

        MANAGER.beginContextSwitch(_contextSwitchEvent->OldThreadId, time, _contextSwitchEvent->NewThreadId, process_name);
        MANAGER.endContextSwitch(_contextSwitchEvent->NewThreadId, time);
    }

    //////////////////////////////////////////////////////////////////////////

    EasyEventTracer& EasyEventTracer::instance()
    {
        static EasyEventTracer tracer;
        return tracer;
    }

    EasyEventTracer::EasyEventTracer()
    {
        m_lowPriority = ATOMIC_VAR_INIT(EASY_LOW_PRIORITY_EVENT_TRACING);
    }

    EasyEventTracer::~EasyEventTracer()
    {
        disable();
    }

    void EasyEventTracer::setLowPriority(bool _value)
    {
        m_lowPriority.store(_value, ::std::memory_order_release);
    }

    bool EasyEventTracer::setDebugPrivilege()
    {
        bool success = false;

        HANDLE hToken = nullptr;
        if (OpenProcessToken(GetCurrentProcess(), TOKEN_ADJUST_PRIVILEGES | TOKEN_QUERY, &hToken))
        {
            LUID privilegyId;
            if (LookupPrivilegeValue(NULL, SE_DEBUG_NAME, &privilegyId))
            {
                TOKEN_PRIVILEGES tokenPrivilege;
                tokenPrivilege.PrivilegeCount = 1;
                tokenPrivilege.Privileges[0].Luid = privilegyId;
                tokenPrivilege.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;
                success = AdjustTokenPrivileges(hToken, FALSE, &tokenPrivilege, sizeof(TOKEN_PRIVILEGES), NULL, NULL) != FALSE;
            }

            CloseHandle(hToken);
        }

        return success;
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

                    if ((_step == 0 && 32 < (int)ShellExecute(NULL, NULL, "logman", "stop \"" KERNEL_LOGGER_NAME "\" -ets", NULL, SW_HIDE)) || (_step > 0 && _step < 6))
                    {
                        // Command executed successfully. Wait for a few time until tracing session finish.
                        ::std::this_thread::sleep_for(::std::chrono::milliseconds(500));
                        return startTrace(true, ++_step);
                    }
                }

                return EVENT_TRACING_WAS_LAUNCHED_BY_SOMEBODY_ELSE;
            }

            case ERROR_ACCESS_DENIED:
                return EVENT_TRACING_NOT_ENOUGH_ACCESS_RIGHTS;

            case ERROR_BAD_LENGTH:
                return EVENT_TRACING_BAD_PROPERTIES_SIZE;
        }

        return EVENT_TRACING_MISTERIOUS_ERROR;
    }

    ::profiler::EventTracingEnableStatus EasyEventTracer::enable(bool _force)
    {
        static const decltype(m_properties.base.Wnode.ClientContext) RAW_TIMESTAMP_TIME_TYPE = 1;

        profiler::guard_lock<profiler::spin_lock> lock(m_spin);
        if (m_bEnabled)
            return EVENT_TRACING_LAUNCHED_SUCCESSFULLY;

        // Trying to set debug privilege for current process
        // to be able to get other process information (process name)
        if (!m_bPrivilegeSet)
            m_bPrivilegeSet = setDebugPrivilege();

        // Clear properties
        memset(&m_properties, 0, sizeof(m_properties));
        m_properties.base.Wnode.BufferSize = sizeof(m_properties);
        m_properties.base.Wnode.Flags = WNODE_FLAG_TRACED_GUID;
        m_properties.base.Wnode.ClientContext = RAW_TIMESTAMP_TIME_TYPE;
        m_properties.base.Wnode.Guid = SystemTraceControlGuid;
        m_properties.base.LoggerNameOffset = sizeof(m_properties.base);
        m_properties.base.EnableFlags = EVENT_TRACE_FLAG_CSWITCH;
        m_properties.base.LogFileMode = EVENT_TRACE_REAL_TIME_MODE;

        auto res = startTrace(_force);
        if (res != EVENT_TRACING_LAUNCHED_SUCCESSFULLY)
            return res;

        memset(&m_trace, 0, sizeof(m_trace));
        m_trace.LoggerName = KERNEL_LOGGER_NAME;
        m_trace.ProcessTraceMode = PROCESS_TRACE_MODE_REAL_TIME | PROCESS_TRACE_MODE_EVENT_RECORD | PROCESS_TRACE_MODE_RAW_TIMESTAMP;
        m_trace.EventRecordCallback = ::profiler::processTraceEvent;

        m_openedHandle = OpenTrace(&m_trace);
        if (m_openedHandle == INVALID_PROCESSTRACE_HANDLE)
            return EVENT_TRACING_OPEN_TRACE_ERROR;

        // Have to launch a thread to process events because according to MSDN documentation:
        // 
        // The ProcessTrace function blocks the thread until it delivers all events, the BufferCallback function returns FALSE,
        // or you call CloseTrace. If the consumer is consuming events in real time, the ProcessTrace function returns after
        // the controller stops the trace session. (Note that there may be a several-second delay before the function returns.)
        // 
        // https://msdn.microsoft.com/en-us/library/windows/desktop/aa364093(v=vs.85).aspx
        m_processThread = ::std::move(::std::thread([this]()
        {
            EASY_THREAD("EasyProfiler.ETW");
            ProcessTrace(&m_openedHandle, 1, 0, 0);
        }));

        // Set low priority for event tracing thread
        if (m_lowPriority.load(::std::memory_order_acquire))
            SetThreadPriority(m_processThread.native_handle(), THREAD_PRIORITY_LOWEST);

        m_bEnabled = true;

        return EVENT_TRACING_LAUNCHED_SUCCESSFULLY;
    }

    void EasyEventTracer::disable()
    {
        profiler::guard_lock<profiler::spin_lock> lock(m_spin);
        if (!m_bEnabled)
            return;

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
    }

} // END of namespace profiler.

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#endif // _WIN32
