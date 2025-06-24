#ifndef CRASH_HANDLER_LIBUNWIND_H
#define CRASH_HANDLER_LIBUNWIND_H

#ifdef USE_LIBUNWIND

#include <libunwind.h>
#include <csignal>
#include <cstdio>
#include <cstdlib>

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
    fprintf(stderr, "Error: signal %d:\n", sig);
    print_stacktrace_libunwind();
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
    
    // libunwind is ready to use
}

inline void print_stacktrace() {
    print_stacktrace_libunwind();
}

} // namespace crash_handler_libunwind

#endif // USE_LIBUNWIND

#endif // CRASH_HANDLER_LIBUNWIND_H
