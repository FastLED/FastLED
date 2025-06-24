#ifndef CRASH_HANDLER_WIN_H
#define CRASH_HANDLER_WIN_H

#ifdef _WIN32

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <dbghelp.h>
#include <psapi.h>
#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace crash_handler_win {

inline void print_stacktrace_windows() {
    HANDLE process = GetCurrentProcess();
    // HANDLE thread = GetCurrentThread(); // Unused, commented out
    
    // Initialize symbol handler
    if (!SymInitialize(process, nullptr, TRUE)) {
        printf("SymInitialize failed with error %lu\n", GetLastError());
        return;
    }
    
    // Set symbol options for better debugging
    SymSetOptions(SYMOPT_LOAD_LINES | SYMOPT_DEFERRED_LOADS);
    
    // Get stack trace
    void* stack[100];
    WORD numberOfFrames = CaptureStackBackTrace(0, 100, stack, nullptr);
    
    printf("Stack trace (Windows):\n");
    
    // Buffer for symbol information
    char symbolBuffer[sizeof(SYMBOL_INFO) + MAX_SYM_NAME * sizeof(TCHAR)];
    PSYMBOL_INFO pSymbol = (PSYMBOL_INFO)symbolBuffer;
    pSymbol->SizeOfStruct = sizeof(SYMBOL_INFO);
    pSymbol->MaxNameLen = MAX_SYM_NAME;
    
    // Line information
    IMAGEHLP_LINE64 line;
    line.SizeOfStruct = sizeof(IMAGEHLP_LINE64);
    
    for (WORD i = 0; i < numberOfFrames; i++) {
        DWORD64 address = (DWORD64)(stack[i]);
        
        printf("#%-2d 0x%016llx", i, address);
        
        // Get symbol name
        DWORD64 displacement = 0;
        if (SymFromAddr(process, address, &displacement, pSymbol)) {
            printf(" %s+0x%llx", pSymbol->Name, displacement);
            
            // Try to get line number
            DWORD lineDisplacement = 0;
            if (SymGetLineFromAddr64(process, address, &lineDisplacement, &line)) {
                printf(" [%s:%lu]", line.FileName, line.LineNumber);
            }
        } else {
            // If no symbol, try to get module name
            HMODULE hModule;
            if (GetModuleHandleEx(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | 
                                 GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                                 (LPCTSTR)address, &hModule)) {
                char moduleName[MAX_PATH];
                if (GetModuleFileNameA(hModule, moduleName, MAX_PATH)) {
                    // Extract just the filename
                    char* fileName = strrchr(moduleName, '\\');
                    if (fileName) fileName++;
                    else fileName = moduleName;
                    printf(" [%s]", fileName);
                }
            } else {
                printf(" -- symbol not found");
            }
        }
        printf("\n");
    }
    
    SymCleanup(process);
}

inline LONG WINAPI windows_exception_handler(EXCEPTION_POINTERS* ExceptionInfo) {
    printf("Exception caught: 0x%08lx at address 0x%p\n", 
           ExceptionInfo->ExceptionRecord->ExceptionCode,
           ExceptionInfo->ExceptionRecord->ExceptionAddress);
    
    print_stacktrace_windows();
    
    // Return EXCEPTION_EXECUTE_HANDLER to terminate the process
    return EXCEPTION_EXECUTE_HANDLER;
}

inline void crash_handler(int sig) {
    fprintf(stderr, "Error: signal %d:\n", sig);
    print_stacktrace_windows();
    exit(1);
}

inline void setup_crash_handler() {
    // Set up Windows structured exception handling
    SetUnhandledExceptionFilter(windows_exception_handler);
    
    // Also handle standard C signals on Windows
    signal(SIGABRT, crash_handler);
    signal(SIGFPE, crash_handler);
    signal(SIGILL, crash_handler);
    signal(SIGINT, crash_handler);
    signal(SIGSEGV, crash_handler);
    signal(SIGTERM, crash_handler);
}

inline void print_stacktrace() {
    print_stacktrace_windows();
}

} // namespace crash_handler_win

#endif // _WIN32

#endif // CRASH_HANDLER_WIN_H
