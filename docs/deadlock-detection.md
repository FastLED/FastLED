# Deadlock Detection System for Hung Tests

## Overview

The deadlock detection system automatically detects, diagnoses, and reports hung tests by:
1. Monitoring test execution with configurable timeout (default: 10 seconds)
2. Detecting when a test exceeds the timeout
3. Attaching lldb/gdb to dump full thread stack traces with symbol resolution
4. Killing the hung process and reporting failure with diagnostic output

## Architecture

### Components

1. **Hung Test** (`tests/fl/test_hung.cpp`)
   - Demonstration test that intentionally hangs
   - Uses volatile variables to prevent compiler optimization

2. **Deadlock Detector** (`ci/util/deadlock_detector.py`)
   - Finds available debugger (lldb preferred, gdb fallback)
   - Attaches to hung process by PID
   - Executes debugger commands to dump all thread stacks
   - Returns formatted backtrace with symbol resolution

3. **Test Wrapper** (`tests/test_wrapper.py`)
   - Runs tests with timeout monitoring
   - Sets `FASTLED_DISABLE_CRASH_HANDLER=1` environment variable
   - Detects hangs and calls deadlock detector
   - Kills hung process and reports failure

4. **Crash Handler Bypass** (all `tests/crash_handler_*.h` files)
   - Checks `FASTLED_DISABLE_CRASH_HANDLER` environment variable
   - If set to "1" or "true", skips signal handler installation
   - Allows external debuggers to attach without interference

5. **Meson Integration** (`tests/meson.build`)
   - Added `timeout: 10` to all test registrations
   - Optional test wrapper mode via `-Dtest_wrapper=true`

## Signal Handler Chaining (Automatic Solution)

**Previous Problem**: Internal crash handlers would block external debugger attachment.

**Current Solution**: FastLED uses **signal handler chaining** to provide BOTH internal dumps AND external debugger access:

1. **Internal handler dumps first** - Prints stack trace when test crashes
2. **Handler uninstalls itself** - Removes signal handler (signal(sig, SIG_DFL))
3. **Handler re-raises signal** - Re-raises for external handlers (raise(sig))
4. **External debugger can catch** - Debuggers like lldb/gdb can now process the signal

**Result**: No configuration needed! Internal dumps work for crashes, external debuggers work for hangs.

### Optional: Manual Debugger Control

If you want ONLY external debugger control (no internal dumps), set:
```bash
export FASTLED_DISABLE_CRASH_HANDLER=1
```

This completely disables internal handlers. However, this is **rarely needed** since signal chaining allows both to coexist.

**Applied to all crash handler implementations:**
- `crash_handler_win.h` (Windows SEH)
- `crash_handler_libunwind.h` (Linux/Mac with libunwind)
- `crash_handler_execinfo.h` (Linux/Mac with execinfo)
- `crash_handler_noop.h` (no-op, unchanged)

## Usage

### Direct Testing (Meson default)

```bash
# Run tests normally (Meson handles timeout, no stack dumps)
bash test hung

# Test times out after 10 seconds, Meson reports TIMEOUT
```

### With Deadlock Detection (Test Wrapper)

```bash
# Enable test wrapper mode for stack dumps
meson setup -Dtest_wrapper=true .build/meson-quick

# Run test - wrapper will dump stacks on timeout
bash test hung
```

### Manual Deadlock Detection

```python
from ci.util.deadlock_detector import handle_hung_test

# Detect hung process and dump stacks
handle_hung_test(pid=12345, test_name="my_test", timeout_seconds=10.0)
```

## How It Works

### Normal Test Flow
1. Meson runs `runner.exe test.dll`
2. Crash handler installs signal handlers
3. Test runs normally
4. If test crashes, crash handler catches it and prints stack trace
5. Test exits with error code

### Hung Test Flow (Automatic with Signal Chaining)
1. Wrapper starts `runner.exe test.dll`
2. Crash handler installs with signal chaining enabled (automatic)
3. Test runs and hangs in infinite loop
4. After 10 seconds, wrapper detects no completion
5. Wrapper calls `handle_hung_test(pid, ...)`:
   - Finds lldb or gdb
   - Creates debugger script (backtrace commands)
   - Attaches to process: `lldb --batch --attach-pid <pid>`
   - Executes `thread backtrace all`
   - Captures output with full stack traces
   - **Note**: Signal chaining allows debugger to attach without interference
6. Wrapper kills hung process
7. Wrapper reports failure with stack traces

**Key difference**: No need to disable crash handlers! Signal chaining allows both internal dumps (for crashes) and external debugger access (for hangs).

### Debugger Commands

**LLDB script** (`tmpXXXX.lldb`):
```lldb
settings set target.process.stop-on-exec false
settings set target.process.stop-on-sharedlibrary-events false
settings set target.x86-disassembly-flavor intel
thread backtrace all
thread list
quit
```

**GDB script** (`tmpXXXX.gdb`):
```gdb
set pagination off
set confirm off
set print pretty on
thread apply all backtrace full
info threads
quit
```

## Environment Variables

- `FASTLED_DISABLE_CRASH_HANDLER=1` - (Optional) Disables internal crash handler completely
- **Not required** for deadlock detection - signal chaining handles this automatically
- Only needed if you want pure external debugger control with no internal dumps

## Configuration

### Meson Options (`meson.options`)

```meson
option('test_wrapper', type: 'boolean', value: false,
       description: 'Enable test wrapper with deadlock detection')
```

### Test Timeout (`tests/meson.build`)

```meson
test_timeout = 10  # seconds per test

test(test_name, runner_exe,
  args: [test_dll.full_path()],
  timeout: test_timeout,
  ...
)
```

## Testing the System

### Create a Hung Test

```cpp
// tests/fl/test_hung.cpp
#include "test.h"
#include <cstdio>

__attribute__((noinline))
static void infinite_loop(volatile bool* keep_running) {
    volatile int counter = 0;
    while (*keep_running) {
        counter++;
        if (counter % 100000000 == 0) {
            printf("Still spinning...\n");
            fflush(stdout);
        }
    }
}

FL_TEST_CASE("test_intentional_hang") {
    printf("Entering infinite loop...\n");
    fflush(stdout);

    volatile bool keep_running = true;
    infinite_loop(&keep_running);

    FL_CHECK(false);  // Never reached
}
```

### Run with Deadlock Detection

```bash
# Compile
bash test hung

# Run with wrapper (will timeout and dump stacks)
# Note: FASTLED_DISABLE_CRASH_HANDLER not needed due to signal chaining
uv run python tests/test_wrapper.py \
  .build/meson-quick/tests/runner.exe \
  .build/meson-quick/tests/fl_test_hung.dll \
  5  # 5 second timeout
```

## Output Example

```
Running test: fl_test_hung (timeout: 5.0s)
[doctest] doctest version is "2.4.11"
TEST CASE:  test_intentional_hang
Entering infinite loop...
Still spinning...
================================================================================
TEST HUNG: fl_test_hung
Exceeded timeout of 5.0s
================================================================================

📍 Attaching lldb to hung process (PID 12345)...
   Note: Crash handlers use signal chaining (internal dump → external debugger)
Running: lldb --batch --source /tmp/tmpXXX.lldb --attach-pid 12345

THREAD STACK TRACES:
--------------------------------------------------------------------------------
* thread #1, name = 'test_hung', stop reason = signal SIGSTOP
  * frame #0: 0x00007ff6b9a41234 test_hung.dll`infinite_loop(...) at test_hung.cpp:12
    frame #1: 0x00007ff6b9a41456 test_hung.dll`____C_A_T_C_H____T_E_S_T____0() at test_hung.cpp:23
    frame #2: 0x00007ff6b9a50000 runner.exe`main + 123
    ...
--------------------------------------------------------------------------------
================================================================================

🔪 Killed hung process (PID 12345)
Test was killed due to timeout (5.0s)
```

## Benefits

1. **Immediate Visibility**: Agents see exactly where tests are stuck
2. **Full Thread Context**: All threads with backtraces
3. **Symbol Resolution**: Function names, file names, line numbers
4. **No Manual Intervention**: Automatic detection and diagnosis
5. **CI/CD Friendly**: Works in automated environments

## Limitations

1. **Debugger Required**: Needs lldb or gdb installed
2. **Platform-Specific**: Stack trace format varies (Windows vs Linux/Mac)
3. **Symbol Quality**: Depends on debug information in binaries
4. **Overhead**: Debugger attachment adds ~2-3 seconds
5. **Windows lldb**: May have compatibility issues, gdb fallback available

## Future Improvements

- [ ] Parse and format stack traces for better readability
- [ ] Add thread state information (running, waiting, blocked)
- [ ] Capture register state for deeper analysis
- [ ] Generate minidumps/core dumps for post-mortem analysis
- [ ] Add deadlock detection (detect mutex/lock cycles)
- [ ] Support remote debugging for CI environments
