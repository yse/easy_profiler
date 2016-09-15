


#ifndef EASY_PROFILER__EVENT_TRACE_WINDOWS__H_
#define EASY_PROFILER__EVENT_TRACE_WINDOWS__H_
#ifdef _WIN32

#define INITGUID  // This is to enable using SystemTraceControlGuid in evntrace.h.
#include <Windows.h>
//#include <Strsafe.h>
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

    class EasyEventTracer final
    {

#pragma pack(push, 1)
        struct Properties final {
            EVENT_TRACE_PROPERTIES base;
            char sessionName[sizeof(KERNEL_LOGGER_NAME)];
        };
#pragma pack(pop)

        ::std::thread       m_processThread;
        Properties             m_properties;
        EVENT_TRACE_LOGFILE         m_trace;
        profiler::spin_lock          m_spin;
        ::std::atomic_bool    m_lowPriority;
        TRACEHANDLE         m_sessionHandle = INVALID_PROCESSTRACE_HANDLE;
        TRACEHANDLE          m_openedHandle = INVALID_PROCESSTRACE_HANDLE;
        bool                     m_bEnabled = false;

    public:

        static EasyEventTracer& instance();
        ~EasyEventTracer();

        ::profiler::EventTracingEnableStatus enable(bool _force = false);
        void disable();
        void setLowPriority(bool _value);

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
