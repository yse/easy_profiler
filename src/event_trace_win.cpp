

#ifdef _WIN32
#include <memory.h>
#include <chrono>
#include <fstream>
#include "event_trace_win.h"
#include "profiler/profiler.h"
#include "profile_manager.h"

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

extern decltype(LARGE_INTEGER::QuadPart) CPU_FREQUENCY;
//extern ProfileManager& MANAGER;
#define MANAGER ProfileManager::instance()

namespace profiler {

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

    void WINAPI processTraceEvent(PEVENT_RECORD _traceEvent)
    {
        //static ::std::ofstream outputFile("csw.csv", ::std::fstream::app);

        static const decltype(_traceEvent->EventHeader.EventDescriptor.Opcode) SWITCH_CONTEXT_OPCODE = 36;
        if (_traceEvent->EventHeader.EventDescriptor.Opcode != SWITCH_CONTEXT_OPCODE)
            return;

        if (sizeof(CSwitch) != _traceEvent->UserDataLength)
            return;

        auto _contextSwitchEvent = reinterpret_cast<CSwitch*>(_traceEvent->UserData);

        auto timestampValue = _traceEvent->EventHeader.TimeStamp.QuadPart;

        //LARGE_INTEGER currtime;
        //if (QueryPerformanceCounter(&currtime))
        //    timestampValue = currtime.QuadPart;

        //timestampValue *= 1000000000LL;
        //timestampValue /= CPU_FREQUENCY;
        const auto time = (::profiler::timestamp_t)timestampValue;

        static const ::profiler::StaticBlockDescriptor desc("OS.ContextSwitch", __FILE__, __LINE__, ::profiler::BLOCK_TYPE_CONTEXT_SWITCH, ::profiler::colors::White);

        //if (_contextSwitchEvent->OldThreadId != 0)
        MANAGER._cswitchBeginBlock(time, desc.id(), _contextSwitchEvent->OldThreadId);

        //if (_contextSwitchEvent->NewThreadId != 0)
        MANAGER._cswitchEndBlock(_contextSwitchEvent->NewThreadId, time);

        //static const auto firstTime = time;
        //outputFile << _contextSwitchEvent->OldThreadId << "\t" << _contextSwitchEvent->NewThreadId << "\t" << (((time - firstTime) * 1000000000LL) / CPU_FREQUENCY) << ::std::endl;
        //outputFile << _contextSwitchEvent->OldThreadId << "\t" << _contextSwitchEvent->NewThreadId << "\t" << time << ::std::endl;
    }

    //////////////////////////////////////////////////////////////////////////

    EasyEventTracer& EasyEventTracer::instance()
    {
        static EasyEventTracer tracer;
        return tracer;
    }

    EasyEventTracer::EasyEventTracer()
    {
    }

    EasyEventTracer::~EasyEventTracer()
    {
        disable();
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

        if (m_bEnabled)
            return EVENT_TRACING_LAUNCHED_SUCCESSFULLY;

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
        m_stubThread = ::std::move(::std::thread([this]()
        {
//             EASY_THREAD("ProcessTrace");
//             EASY_BLOCK("ProcessTrace", ::profiler::colors::Red);
            ProcessTrace(&m_openedHandle, 1, 0, 0);
        }));

        m_bEnabled = true;

        return EVENT_TRACING_LAUNCHED_SUCCESSFULLY;
    }

    void EasyEventTracer::disable()
    {
        if (!m_bEnabled)
            return;

        auto controlTraceResult = ControlTrace(m_openedHandle, KERNEL_LOGGER_NAME, props(), EVENT_TRACE_CONTROL_STOP);

        // ERROR_CTX_CLOSE_PENDING(7007L): The call was successful. The ProcessTrace function will stop after it has processed all real-time events in its buffers (it will not receive any new events).
        // ERROR_BUSY(170L): Prior to Windows Vista, you cannot close the trace until the ProcessTrace function completes.
        // ERROR_INVALID_HANDLE(6L): One of the following is true: TraceHandle is NULL. TraceHandle is INVALID_HANDLE_VALUE.
        auto closeTraceStatus = CloseTrace(m_openedHandle);

        // Wait for ProcessThread to finish
        if (m_stubThread.joinable())
            m_stubThread.join();

        m_bEnabled = false;
    }

} // END of namespace profiler.

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#endif // _WIN32
