#ifndef CRASH_HANDLER_H
#define CRASH_HANDLER_H

#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <execinfo.h>
#include <unistd.h>
#include <libunwind.h>

void print_stacktrace() {
    unw_cursor_t cursor;
    unw_context_t context;

    unw_getcontext(&context);
    unw_init_local(&cursor, &context);

    int depth = 0;
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

void crash_handler(int sig) {
    fprintf(stderr, "Error: signal %d:\n", sig);
    print_stacktrace();
    exit(1);
}

void setup_crash_handler() {
    signal(SIGSEGV, crash_handler);
    signal(SIGABRT, crash_handler);
}

#endif // CRASH_HANDLER_H
