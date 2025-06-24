#ifndef CRASH_HANDLER_EXECINFO_H
#define CRASH_HANDLER_EXECINFO_H

#ifdef USE_EXECINFO

#include <execinfo.h>
#include <csignal>
#include <cstdio>
#include <cstdlib>

namespace crash_handler_execinfo {

inline void print_stacktrace_execinfo() {
    // Fallback to execinfo.h backtrace
    void *buffer[100];
    int nptrs;
    char **strings;

    printf("Stack trace (backtrace):\n");
    nptrs = backtrace(buffer, 100);
    strings = backtrace_symbols(buffer, nptrs);
    
    if (strings == nullptr) {
        printf("backtrace_symbols failed\n");
        return;
    }

    for (int j = 0; j < nptrs; j++) {
        printf("#%-2d %s\n", j, strings[j]);
    }

    free(strings);
}

inline void crash_handler(int sig) {
    fprintf(stderr, "Error: signal %d:\n", sig);
    print_stacktrace_execinfo();
    exit(1);
}

inline void setup_crash_handler() {
    // Set up POSIX signal handlers
    signal(SIGABRT, crash_handler);
    signal(SIGFPE, crash_handler);
    signal(SIGILL, crash_handler);
    signal(SIGINT, crash_handler);
    signal(SIGSEGV, crash_handler);
    signal(SIGTERM, crash_handler);
    
    // execinfo.h backtrace is ready to use
}

inline void print_stacktrace() {
    print_stacktrace_execinfo();
}

} // namespace crash_handler_execinfo

#endif // USE_EXECINFO

#endif // CRASH_HANDLER_EXECINFO_H
