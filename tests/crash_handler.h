#ifndef CRASH_HANDLER_H
#define CRASH_HANDLER_H

#include "fl/has_include.h"  // IWYU pragma: keep

// Phase 1: Include the appropriate implementation at the top (before namespace declarations)
#ifdef _WIN32
    #include "crash_handler_win.h"
#elif defined(USE_LIBUNWIND)
    #include "crash_handler_libunwind.h"
#elif FL_HAS_INCLUDE(<execinfo.h>)
    #define USE_EXECINFO 1
    #include "crash_handler_execinfo.h"
#else
    #include "crash_handler_noop.h"
#endif

// Phase 2: Create namespace alias
#ifdef _WIN32
    namespace impl = crash_handler_win;
#elif defined(USE_LIBUNWIND)
    namespace impl = crash_handler_libunwind;
#elif FL_HAS_INCLUDE(<execinfo.h>)
    namespace impl = crash_handler_execinfo;
#else
    namespace impl = crash_handler_noop;
#endif

// Unified interface - these functions will dispatch to the appropriate implementation
inline void crash_handler(int sig) {
    impl::crash_handler(sig);
}

inline void print_stacktrace() {
    impl::print_stacktrace();
}

inline void setup_crash_handler() {
    impl::setup_crash_handler();
}

// Walk the stack of a specific suspended thread (Windows only).
// On other platforms, use print_stacktrace() from within the target thread.
#ifdef _WIN32
inline void print_stacktrace_for_thread(void* thread_handle) {
    impl::print_stacktrace_for_thread((HANDLE)thread_handle);
}
#endif

#endif // CRASH_HANDLER_H
