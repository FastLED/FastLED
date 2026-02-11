#ifndef CRASH_HANDLER_LIBUNWIND_H
#define CRASH_HANDLER_LIBUNWIND_H

#ifdef USE_LIBUNWIND

#define UNW_LOCAL_ONLY
#include <libunwind.h>
#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace crash_handler_libunwind {

inline void print_stacktrace_libunwind() {
    // Use libunwind for better stack traces
    unw_cursor_t cursor;
    unw_context_t context;

    unw_getcontext(&context);
    unw_init_local(&cursor, &context);

    int depth = 0;
    printf("Stack trace (libunwind):\n");
    while (unw_step(&cursor) > 0) {
        unw_word_t offset, pc;
        char sym[256];

        unw_get_reg(&cursor, UNW_REG_IP, &pc);
        if (pc == 0) {
            break;
        }

        printf("#%-2d 0x%lx:", depth++, pc);

        if (unw_get_proc_name(&cursor, sym, sizeof(sym), &offset) == 0) {
            printf(" (%s+0x%lx)\n", sym, offset);
        } else {
            printf(" -- symbol not found\n");
        }
    }
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
    print_stacktrace_libunwind();

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

    // libunwind is ready to use
}

inline void print_stacktrace() {
    print_stacktrace_libunwind();
}

} // namespace crash_handler_libunwind

#endif // USE_LIBUNWIND

#endif // CRASH_HANDLER_LIBUNWIND_H
