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
    // Prevent recursion if handler crashes
    static volatile sig_atomic_t already_dumping = 0;
    if (already_dumping) {
        // Recursive crash - bail out immediately
        signal(sig, SIG_DFL);
        raise(sig);
        return;
    }
    already_dumping = 1;

    fprintf(stderr, "\n=== INTERNAL CRASH HANDLER (SIGNAL %d) ===\n", sig);

    // Internal stack trace dump
    print_stacktrace_execinfo();

    fprintf(stderr, "=== END INTERNAL HANDLER ===\n\n");
    fflush(stdout);
    fflush(stderr);

    // CHAINING: Uninstall our handler and re-raise signal
    // This allows external debuggers to catch the signal
    fprintf(stderr, "Uninstalling crash handler and re-raising signal %d for external debugger...\n", sig);
    fflush(stderr);

    // Restore default handler
    signal(sig, SIG_DFL);

    // Re-raise the signal - will now go to default handler or external debugger
    raise(sig);

    // If we get here, signal didn't terminate us - exit manually
    exit(1);
}

inline void setup_crash_handler() {
    // Check if crash handler should be disabled (for debugger attachment)
    const char* disable_handler = getenv("FASTLED_DISABLE_CRASH_HANDLER");
    if (disable_handler && (strcmp(disable_handler, "1") == 0 || strcmp(disable_handler, "true") == 0)) {
        printf("Crash handler disabled (FASTLED_DISABLE_CRASH_HANDLER set)\n");
        printf("This allows external debuggers to attach for deadlock detection.\n");
        return;
    }

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
