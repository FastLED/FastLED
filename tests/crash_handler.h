#ifndef CRASH_HANDLER_H
#define CRASH_HANDLER_H

// allow-include-after-namespace
// Determine which crash handler implementation to use
#ifdef _WIN32
    #include "crash_handler_win.h"
    namespace impl = crash_handler_win;
#elif defined(USE_LIBUNWIND)
    #include "crash_handler_libunwind.h"
    namespace impl = crash_handler_libunwind;
#elif __has_include(<execinfo.h>)
    #define USE_EXECINFO 1
    #include "crash_handler_execinfo.h" 
    namespace impl = crash_handler_execinfo;
#else
    #include "crash_handler_noop.h"
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
