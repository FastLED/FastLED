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
#include <vector>
#include <string>

#pragma comment(lib, "dbghelp.lib")
#pragma comment(lib, "psapi.lib")

namespace crash_handler_win {

// Global flag to track if symbols are initialized
static bool g_symbols_initialized = false;

// Helper function to get module name from address
inline std::string get_module_name(DWORD64 address) {
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
            return std::string(fileName);
        }
    }
    return "unknown";
}

// Helper function to demangle C++ symbols
inline std::string demangle_symbol(const char* symbol_name) {
    if (!symbol_name) return "unknown";
    
    // For now, return as-is. In a full implementation, you might want to use
    // a C++ demangler library or call the Windows undecorate function
    return std::string(symbol_name);
}

inline void print_stacktrace_windows() {
    HANDLE process = GetCurrentProcess();
    
    // Initialize symbol handler if not already done
    if (!g_symbols_initialized) {
        // Set symbol options for better debugging
        SymSetOptions(SYMOPT_LOAD_LINES | 
                     SYMOPT_DEFERRED_LOADS | 
                     SYMOPT_UNDNAME | 
                     SYMOPT_DEBUG);
        
        if (!SymInitialize(process, nullptr, TRUE)) {
            DWORD error = GetLastError();
            printf("SymInitialize failed with error %lu (0x%lx)\n", error, error);
            printf("This may be due to missing debug symbols or insufficient permissions.\n");
            printf("Try running as administrator or ensure debug symbols are available.\n\n");
        } else {
            g_symbols_initialized = true;
            printf("Symbol handler initialized successfully.\n");
        }
    }
    
    // Get stack trace
    void* stack[100];
    WORD numberOfFrames = CaptureStackBackTrace(0, 100, stack, nullptr);
    
    printf("Stack trace (Windows):\n");
    printf("Captured %d frames:\n\n", numberOfFrames);
    
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
        
        // Get module name first
        std::string moduleName = get_module_name(address);
        printf(" [%s]", moduleName.c_str());
        
        // Try to get symbol information
        if (g_symbols_initialized) {
            DWORD64 displacement = 0;
            if (SymFromAddr(process, address, &displacement, pSymbol)) {
                std::string demangled = demangle_symbol(pSymbol->Name);
                printf(" %s+0x%llx", demangled.c_str(), displacement);
                
                // Try to get line number
                DWORD lineDisplacement = 0;
                if (SymGetLineFromAddr64(process, address, &lineDisplacement, &line)) {
                    // Extract just the filename from the full path
                    char* fileName = strrchr(line.FileName, '\\');
                    if (fileName) fileName++;
                    else fileName = line.FileName;
                    printf(" [%s:%lu]", fileName, line.LineNumber);
                }
            } else {
                DWORD error = GetLastError();
                if (error != ERROR_MOD_NOT_FOUND) {
                    printf(" -- symbol lookup failed (error %lu)", error);
                } else {
                    printf(" -- no debug symbols available");
                }
            }
        } else {
            printf(" -- symbol handler not available");
        }
        printf("\n");
    }
    
    printf("\nDebug Information:\n");
    printf("- Symbol handler initialized: %s\n", g_symbols_initialized ? "Yes" : "No");
    printf("- Process ID: %lu\n", GetCurrentProcessId());
    printf("- Thread ID: %lu\n", GetCurrentThreadId());
    
    // Show loaded modules for debugging
    printf("\nLoaded modules:\n");
    HMODULE hModules[1024];
    DWORD cbNeeded;
    if (EnumProcessModules(process, hModules, sizeof(hModules), &cbNeeded)) {
        for (unsigned int i = 0; i < (cbNeeded / sizeof(HMODULE)); i++) {
            char moduleName[MAX_PATH];
            if (GetModuleFileNameA(hModules[i], moduleName, MAX_PATH)) {
                char* fileName = strrchr(moduleName, '\\');
                if (fileName) fileName++;
                else fileName = moduleName;
                printf("  %s\n", fileName);
            }
        }
    }
    
    printf("\n");
}

inline LONG WINAPI windows_exception_handler(EXCEPTION_POINTERS* ExceptionInfo) {
    printf("\n=== WINDOWS EXCEPTION HANDLER ===\n");
    printf("Exception caught: 0x%08lx at address 0x%p\n", 
           ExceptionInfo->ExceptionRecord->ExceptionCode,
           ExceptionInfo->ExceptionRecord->ExceptionAddress);
    
    // Print exception details
    switch (ExceptionInfo->ExceptionRecord->ExceptionCode) {
        case EXCEPTION_ACCESS_VIOLATION:
            printf("Exception type: Access Violation\n");
            printf("Attempted to %s at address 0x%p\n",
                   ExceptionInfo->ExceptionRecord->ExceptionInformation[0] ? "write" : "read",
                   (void*)ExceptionInfo->ExceptionRecord->ExceptionInformation[1]);
            break;
        case EXCEPTION_STACK_OVERFLOW:
            printf("Exception type: Stack Overflow\n");
            break;
        case EXCEPTION_ILLEGAL_INSTRUCTION:
            printf("Exception type: Illegal Instruction\n");
            break;
        case EXCEPTION_PRIV_INSTRUCTION:
            printf("Exception type: Privileged Instruction\n");
            break;
        case EXCEPTION_NONCONTINUABLE_EXCEPTION:
            printf("Exception type: Non-continuable Exception\n");
            break;
        case EXCEPTION_ARRAY_BOUNDS_EXCEEDED:
            printf("Exception type: Array Bounds Exceeded\n");
            break;
        case EXCEPTION_FLT_DENORMAL_OPERAND:
            printf("Exception type: Floating Point Denormal Operand\n");
            break;
        case EXCEPTION_FLT_DIVIDE_BY_ZERO:
            printf("Exception type: Floating Point Divide by Zero\n");
            break;
        case EXCEPTION_FLT_INEXACT_RESULT:
            printf("Exception type: Floating Point Inexact Result\n");
            break;
        case EXCEPTION_FLT_INVALID_OPERATION:
            printf("Exception type: Floating Point Invalid Operation\n");
            break;
        case EXCEPTION_FLT_OVERFLOW:
            printf("Exception type: Floating Point Overflow\n");
            break;
        case EXCEPTION_FLT_STACK_CHECK:
            printf("Exception type: Floating Point Stack Check\n");
            break;
        case EXCEPTION_FLT_UNDERFLOW:
            printf("Exception type: Floating Point Underflow\n");
            break;
        case EXCEPTION_INT_DIVIDE_BY_ZERO:
            printf("Exception type: Integer Divide by Zero\n");
            break;
        case EXCEPTION_INT_OVERFLOW:
            printf("Exception type: Integer Overflow\n");
            break;
        default:
            printf("Exception type: Unknown (0x%08lx)\n", 
                   ExceptionInfo->ExceptionRecord->ExceptionCode);
            break;
    }
    
    print_stacktrace_windows();
    
    printf("=== END EXCEPTION HANDLER ===\n\n");
    
    // Return EXCEPTION_EXECUTE_HANDLER to terminate the process
    return EXCEPTION_EXECUTE_HANDLER;
}

inline void crash_handler(int sig) {
    printf("\n=== SIGNAL HANDLER ===\n");
    fprintf(stderr, "Error: signal %d:\n", sig);
    
    // Print signal details
    switch (sig) {
        case SIGABRT:
            printf("Signal: SIGABRT (Abort)\n");
            break;
        case SIGFPE:
            printf("Signal: SIGFPE (Floating Point Exception)\n");
            break;
        case SIGILL:
            printf("Signal: SIGILL (Illegal Instruction)\n");
            break;
        case SIGINT:
            printf("Signal: SIGINT (Interrupt)\n");
            break;
        case SIGSEGV:
            printf("Signal: SIGSEGV (Segmentation Fault)\n");
            break;
        case SIGTERM:
            printf("Signal: SIGTERM (Termination)\n");
            break;
        default:
            printf("Signal: Unknown (%d)\n", sig);
            break;
    }
    
    print_stacktrace_windows();
    printf("=== END SIGNAL HANDLER ===\n\n");
    exit(1);
}

inline void setup_crash_handler() {
    printf("Setting up Windows crash handler...\n");
    
    // Set up Windows structured exception handling
    SetUnhandledExceptionFilter(windows_exception_handler);
    
    // Also handle standard C signals on Windows
    signal(SIGABRT, crash_handler);
    signal(SIGFPE, crash_handler);
    signal(SIGILL, crash_handler);
    signal(SIGINT, crash_handler);
    signal(SIGSEGV, crash_handler);
    signal(SIGTERM, crash_handler);
    
    printf("Windows crash handler setup complete.\n");
}

inline void print_stacktrace() {
    print_stacktrace_windows();
}

} // namespace crash_handler_win

#endif // _WIN32

#endif // CRASH_HANDLER_WIN_H
