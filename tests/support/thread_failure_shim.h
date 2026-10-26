/** Control interface of the pthread_create interposer in thread_failure_shim.cpp. */

#ifndef EASY_PROFILER_TESTS_THREAD_FAILURE_SHIM_H
#define EASY_PROFILER_TESTS_THREAD_FAILURE_SHIM_H

extern "C" {

/// Let the next `allowed` thread creations through and refuse the ones after
/// with EAGAIN. A negative value passes every call through. Resets the counters.
void easy_test_refuse_threads_after(int allowed);

/// Calls that reached the interposer since the last easy_test_refuse_threads_after().
int easy_test_observed_thread_creations();

/// Calls refused with EAGAIN since the last easy_test_refuse_threads_after().
int easy_test_refused_thread_creations();

}

#endif // EASY_PROFILER_TESTS_THREAD_FAILURE_SHIM_H
