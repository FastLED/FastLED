# Debugging C++ Crashes and Hangs

## Built-in Crash Handler (Recommended First Step)

**FastLED has built-in crash handlers that automatically provide excellent stack traces!**

When unit tests crash, the crash handler automatically captures:
- Full function names with line numbers
- Source file paths
- Complete call stack (19+ frames)
- Module names and memory addresses

**For most debugging needs, the automatic stack traces are sufficient.** See the crash output for detailed information.

## Signal Handler Chaining (Crash + Hang Detection)

FastLED uses **signal handler chaining** to provide both crash dumps AND debugger access:

### How It Works

1. **Test crashes** → Internal handler dumps stack trace → Uninstalls itself → Re-raises signal → External debugger can catch it
2. **Test hangs** (>10s timeout) → External watchdog attaches lldb/gdb → Dumps all thread stacks → Kills process

### Benefits for Agents

✅ **Automatic crash dumps** - Internal handler provides immediate stack traces (< 1s)
✅ **Automatic hang detection** - Tests timeout after 10 seconds, stack traces dumped automatically
✅ **Zero configuration** - Works out of the box, no environment variables needed
✅ **Debugger-friendly** - External debuggers can still attach if needed

### Example Output

**Crash (SIGSEGV):**
```
=== INTERNAL CRASH HANDLER (SIGNAL 11) ===
Stack trace:
  #0 test_crash() at test.cpp:42
=== END INTERNAL HANDLER ===
Segmentation fault
```

**Hang (timeout after 10s):**
```
TEST HUNG: test_name
Exceeded timeout of 10.0s
📍 Attaching lldb to hung process (PID 12345)...
THREAD STACK TRACES:
  * frame #0: infinite_loop() at test.cpp:67
🔪 Killed hung process
```

**For detailed technical information, see:**
- `docs/signal-handler-chaining.md` - Signal chaining implementation
- `docs/deadlock-detection.md` - Hang detection system

## Interactive Debugging with LLDB

**When you need to inspect program state before a crash or use breakpoints:**

1. **Recompile with debug flags**: Build the program with `-g3` (full debug info) and `-O0` (no optimization)
   ```bash
   uv run test.py <test_name> --cpp --build-mode debug
   ```

2. **Run under LLDB**: Execute the program using LLDB
   ```bash
   # Interactive debugging
   uv run clang-tool-chain-lldb <program>

   # With arguments
   uv run clang-tool-chain-lldb <program> -- <args>

   # For unit tests (requires manual setup)
   uv run clang-tool-chain-lldb .build/meson-debug/tests/runner.exe -- <test_name>.dll
   ```

3. **Reproduce the crash**: Run the program until the crash occurs
   ```
   (lldb) run
   ```

4. **Inspect crash state**: When stopped at a breakpoint or crash, examine:
   - Stack trace: `bt` (backtrace) or `bt all` (all threads)
   - Current frame variables: `frame variable`
   - Specific variable values: `print <variable_name>` or `p <var>`
   - Register state: `register read`
   - Memory contents: `memory read <address>`
   - Set breakpoints: `breakpoint set --name <function>` or `b <file>:<line>`

5. **Report findings**: Document the exact crash location, variable states, and any suspicious values

This systematic approach often reveals null pointers, uninitialized variables, buffer overflows, or memory corruption.

**LLDB commands:** `run`, `bt`, `bt all`, `frame select <n>`, `print <var>`, `frame variable`, `register read`

**Note:** LLDB is included in the `clang-tool-chain` package. For detailed LLDB usage, see `docs/agents/lldb-debugging.md`.
