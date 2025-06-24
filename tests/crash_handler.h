#ifndef CRASH_HANDLER_H
#define CRASH_HANDLER_H

#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <unistd.h>


#ifdef USE_LIBUNWIND
#include <libunwind.h>
#elif __has_include(<execinfo.h>)
#define USE_EXECINFO 1
#include <execinfo.h>
#else
// Disable libunwind as a fallback.
#undef USE_LIBUNWIND
#endif

void crash_handler(int sig);
void print_stacktrace();
void setup_crash_handler();

inline void print_stacktrace() {
#ifdef USE_LIBUNWIND
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
#elif USE_EXECINFO
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
#endif
}

inline void crash_handler(int sig) {
    fprintf(stderr, "Error: signal %d:\n", sig);
    print_stacktrace();
    exit(1);
}

inline void setup_crash_handler() {

#ifdef USE_EXECINFO
    // Install execinfo.h backtrace
    backtrace_init();
#elif USE_LIBUNWIND
    // Install libunwind backtrace
    unw_backtrace_init();
#endif
}

#endif // CRASH_HANDLER_H
