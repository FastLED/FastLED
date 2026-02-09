# Signal Handler Chaining for Dual Crash/Hang Detection

## Overview

FastLED uses **signal handler chaining** to provide both:
1. **Internal crash dumps** - Immediate stack traces when tests crash
2. **External debugger access** - lldb/gdb attachment for hung tests

This is achieved through a pass-through handler design where the internal handler dumps first, then uninstalls itself and re-raises the signal for external processing.

## How It Works

### Normal Crash Flow

```
Test crashes → Signal raised (SIGSEGV, SIGABRT, etc.)
              ↓
         Internal handler catches signal
              ↓
         Print internal stack trace
              ↓
         Uninstall handler (signal(sig, SIG_DFL))
              ↓
         Re-raise signal (raise(sig))
              ↓
         Signal goes to default handler or external debugger
              ↓
         Process terminates
```

### Hung Test Flow

```
Test hangs → External watchdog detects (no output for 10s)
           ↓
      Attach lldb/gdb to process
           ↓
      Debugger dumps all thread stacks
           ↓
      Kill process
```

### Actual Crash + External Debugger

```
Test crashes → Internal handler dumps stack
             ↓
        Handler uninstalls itself
             ↓
        Handler re-raises signal
             ↓
        External debugger (if attached) catches signal
             ↓
        Debugger dumps additional context
             ↓
        Process terminates
```

## Implementation Details

### Windows Exception Handler

```cpp
inline LONG WINAPI windows_exception_handler(EXCEPTION_POINTERS* ExceptionInfo) {
    // Prevent recursion
    static volatile LONG already_dumping = 0;
    if (InterlockedExchange(&already_dumping, 1) != 0) {
        return EXCEPTION_CONTINUE_SEARCH;  // Recursive - bail out
    }

    printf("\n=== INTERNAL EXCEPTION HANDLER ===\n");
    print_stacktrace_windows();  // Internal dump
    printf("=== END INTERNAL HANDLER ===\n\n");
    fflush(stdout);

    // CHAINING: Remove our handler
    SetUnhandledExceptionFilter(NULL);

    // Return EXCEPTION_CONTINUE_SEARCH to pass to next handler
    return EXCEPTION_CONTINUE_SEARCH;
}
```

**Key Points:**
- Uses `InterlockedExchange` for thread-safe recursion prevention
- Returns `EXCEPTION_CONTINUE_SEARCH` to pass exception to next handler
- Removes filter via `SetUnhandledExceptionFilter(NULL)`

### Unix Signal Handler (Linux/Mac)

```cpp
inline void crash_handler(int sig) {
    // Prevent recursion
    static volatile sig_atomic_t already_dumping = 0;
    if (already_dumping) {
        signal(sig, SIG_DFL);
        raise(sig);
        return;
    }
    already_dumping = 1;

    fprintf(stderr, "\n=== INTERNAL CRASH HANDLER ===\n");
    print_stacktrace();  // Internal dump
    fprintf(stderr, "=== END INTERNAL HANDLER ===\n\n");
    fflush(stderr);

    // CHAINING: Restore default handler
    signal(sig, SIG_DFL);

    // Re-raise signal for external debugger
    raise(sig);

    // Fallback exit if raise doesn't terminate
    exit(1);
}
```

**Key Points:**
- Uses `sig_atomic_t` for signal-safe recursion check
- Restores default handler via `signal(sig, SIG_DFL)`
- Re-raises signal via `raise(sig)` for external handlers

## Recursion Prevention

All handlers include recursion guards to prevent infinite loops if the handler itself crashes:

```cpp
static volatile sig_atomic_t already_dumping = 0;
if (already_dumping) {
    signal(sig, SIG_DFL);
    raise(sig);
    return;
}
already_dumping = 1;
```

This ensures:
- First crash → Full internal dump
- Handler crashes → Immediate bail-out to default handler
- No stack overflow from recursive handler calls

## Benefits

### 1. Best of Both Worlds
- ✅ **Fast crashes** get immediate internal dumps (< 1s)
- ✅ **Hung tests** get external lldb/gdb dumps (> 10s)
- ✅ No configuration needed - works automatically

### 2. Backwards Compatible
- Existing crash dumps continue to work
- No test changes required
- No environment variables to set

### 3. Debugger-Friendly
- External debuggers can still attach
- Signal chaining is standard practice
- Works with lldb, gdb, WinDbg, Visual Studio debugger

### 4. Dual Diagnosis
- Internal handler provides immediate context
- External debugger provides deeper analysis
- Both stack traces available for comparison

## Comparison with Alternatives

| Approach | Internal Dumps | External Dumps | Config Required | Test Changes |
|----------|----------------|----------------|-----------------|--------------|
| **Signal Chaining** ⭐ | ✅ Yes | ✅ Yes | ❌ No | ❌ No |
| Disable Handler | ❌ No | ✅ Yes | ✅ Yes (env var) | ❌ No |
| Time-Based Uninstall | ⚠️ <3s only | ✅ Yes | ✅ Yes (timeout) | ❌ No |
| Watchdog Thread | ✅ Yes | ✅ Yes | ❌ No | ✅ Yes (heartbeat) |

## Example Output

### Internal Crash (SIGSEGV)

```
=== INTERNAL CRASH HANDLER (SIGNAL 11) ===
Stack trace (libunwind):
  #0  0x00007f8b9c123456 in MyClass::crash_me() at test.cpp:42
  #1  0x00007f8b9c123789 in test_case_1() at test.cpp:67
  #2  0x00007f8b9c124000 in main at main.cpp:12
=== END INTERNAL HANDLER ===

Uninstalling crash handler and re-raising signal 11 for external debugger...
Segmentation fault (core dumped)
```

### Hung Test Detection

```
Running test: my_test (timeout: 10.0s)
[doctest] TEST CASE:  test_infinite_loop
================================================================================
TEST HUNG: my_test
Exceeded timeout of 10.0s
================================================================================

📍 Attaching lldb to hung process (PID 12345)...
   Note: Crash handlers use signal chaining (internal dump → external debugger)

THREAD STACK TRACES:
--------------------------------------------------------------------------------
* thread #1, name = 'my_test', stop reason = signal SIGSTOP
  * frame #0: 0x00007ff6b9a41234 test.dll`infinite_loop(...) at test.cpp:12
    frame #1: 0x00007ff6b9a41456 test.dll`test_case() at test.cpp:23
--------------------------------------------------------------------------------

🔪 Killed hung process (PID 12345)
```

## Optional: Disable Chaining

If you want ONLY external dumps (no internal dumps), set:

```bash
export FASTLED_DISABLE_CRASH_HANDLER=1
```

This completely disables internal handlers, allowing pure external debugger control.

## Platform-Specific Notes

### Windows
- Structured Exception Handling (SEH) via `SetUnhandledExceptionFilter`
- Returns `EXCEPTION_CONTINUE_SEARCH` to chain
- Works with WinDbg, Visual Studio debugger, lldb

### Linux/Mac
- POSIX signals via `signal()` / `sigaction()`
- Re-raises via `raise(sig)` to chain
- Works with gdb, lldb

### Signal Coverage

All platforms handle:
- `SIGABRT` - Abort (assertion failures)
- `SIGFPE` - Floating point exceptions
- `SIGILL` - Illegal instructions
- `SIGINT` - Interrupts (Ctrl+C)
- `SIGSEGV` - Segmentation faults
- `SIGTERM` - Termination requests

## Testing the Chaining

### Test 1: Crash Detection

```cpp
FL_TEST_CASE("test_crash") {
    int* ptr = nullptr;
    *ptr = 42;  // SIGSEGV - should see internal dump then terminate
}
```

**Expected**:
```
=== INTERNAL CRASH HANDLER (SIGNAL 11) ===
Stack trace: ...
=== END INTERNAL HANDLER ===
Uninstalling crash handler and re-raising signal 11...
Segmentation fault
```

### Test 2: Hang Detection

```cpp
FL_TEST_CASE("test_hang") {
    volatile bool keep_running = true;
    while (keep_running) {
        // Infinite loop - should trigger external lldb after 10s
    }
}
```

**Expected**:
```
TEST HUNG: test_hang
Exceeded timeout of 10.0s
📍 Attaching lldb to hung process (PID ...)
THREAD STACK TRACES: ...
🔪 Killed hung process
```

## Summary

Signal handler chaining provides the ideal solution for test diagnostics:

1. **Crashes** → Internal handler dumps immediately → Re-raise for external debugger
2. **Hangs** → External watchdog detects → Attach debugger → Dump stacks
3. **No configuration** → Works out of the box → Backwards compatible
4. **Recursion-safe** → Guards prevent infinite loops → Clean failover

This gives agents complete visibility into both crashes and hangs without requiring any test modifications or environment configuration!
