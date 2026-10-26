/** Interposes pthread_create so a test can make std::thread fail on demand.

This is a shared library of its own, linked into the test executable, rather
than a definition inside the executable: with easy_profiler built shared on
macOS, the library's call binds to libsystem through the two-level namespace
and never sees a symbol in the executable. dyld's __interpose section rewrites
that binding in every loaded image, static or shared. On Linux the executable
and its direct dependencies precede libc in the lookup scope, so the
definition here is found first.
*/

#include "thread_failure_shim.h"

#include <atomic>
#include <dlfcn.h>
#include <errno.h>
#include <pthread.h>

namespace {

std::atomic<int> g_threadsBeforeFailure(-1);
std::atomic<int> g_observed(0);
std::atomic<int> g_refused(0);

using CreateFn = int (*)(pthread_t*, const pthread_attr_t*, void* (*)(void*), void*);

int createThread(pthread_t* thread, const pthread_attr_t* attr, void* (*start)(void*), void* arg)
{
#ifdef __APPLE__
    // dyld does not apply an image's own interposing to that image, so this
    // binds to the function one step down the interposing chain.
    static const CreateFn real = &::pthread_create;
#else
    static const CreateFn real = reinterpret_cast<CreateFn>(dlsym(RTLD_NEXT, "pthread_create"));
#endif

    const int remaining = g_threadsBeforeFailure.load();
    if (remaining < 0)
        return real(thread, attr, start, arg);

    g_observed.fetch_add(1);
    if (remaining == 0)
    {
        g_refused.fetch_add(1);
        return EAGAIN;
    }

    g_threadsBeforeFailure.store(remaining - 1);
    return real(thread, attr, start, arg);
}

} // namespace

#ifdef __APPLE__

__attribute__((used)) static const struct
{
    const void* replacement;
    const void* original;
} interposer __attribute__((section("__DATA,__interpose"))) = {
    reinterpret_cast<const void*>(&createThread),
    reinterpret_cast<const void*>(&::pthread_create)
};

#else

extern "C" int pthread_create(pthread_t* thread, const pthread_attr_t* attr,
                              void* (*start)(void*), void* arg)
{
    return createThread(thread, attr, start, arg);
}

#endif

extern "C" void easy_test_refuse_threads_after(int allowed)
{
    g_observed.store(0);
    g_refused.store(0);
    g_threadsBeforeFailure.store(allowed);
}

extern "C" int easy_test_observed_thread_creations()
{
    return g_observed.load();
}

extern "C" int easy_test_refused_thread_creations()
{
    return g_refused.load();
}
