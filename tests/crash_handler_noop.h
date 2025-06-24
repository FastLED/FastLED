#ifndef CRASH_HANDLER_NOOP_H
#define CRASH_HANDLER_NOOP_H

#include <csignal>
#include <cstdio>
#include <cstdlib>

namespace crash_handler_noop {

inline void print_stacktrace_noop() {
    printf("Stack trace (no-op): Stack trace functionality not available\n");
    printf("  Compile with one of the following to enable stack traces:\n");
    printf("  - Windows: Automatically enabled on Windows builds\n");
    printf("  - libunwind: Define USE_LIBUNWIND and link with -lunwind\n");
    printf("  - execinfo: Available on most Unix-like systems with glibc\n");
}

inline void crash_handler(int sig) {
    fprintf(stderr, "Error: signal %d:\n", sig);
    print_stacktrace_noop();
    exit(1);
}

inline void setup_crash_handler() {
    // Set up basic signal handlers (no stack trace capability)
    signal(SIGABRT, crash_handler);
    signal(SIGFPE, crash_handler);
    signal(SIGILL, crash_handler);
    signal(SIGINT, crash_handler);
    signal(SIGSEGV, crash_handler);
    signal(SIGTERM, crash_handler);
}

inline void print_stacktrace() {
    print_stacktrace_noop();
}

} // namespace crash_handler_noop

#endif // CRASH_HANDLER_NOOP_H
