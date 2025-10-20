#ifndef CRASH_HANDLER_H
#define CRASH_HANDLER_H

// Phase 1: Include the appropriate implementation at the top (before namespace declarations)
#ifdef _WIN32
    #include "crash_handler_win.h"
#elif defined(USE_LIBUNWIND)
    #include "crash_handler_libunwind.h"
#elif __has_include(<execinfo.h>)
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
#elif __has_include(<execinfo.h>)
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

#endif // CRASH_HANDLER_H
