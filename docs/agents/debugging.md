# Debugging C++ Crashes

## Built-in Crash Handler (Recommended First Step)

**FastLED has built-in crash handlers that automatically provide excellent stack traces!**

When unit tests crash, the crash handler automatically captures:
- Full function names with line numbers
- Source file paths
- Complete call stack (19+ frames)
- Module names and memory addresses

**For most debugging needs, the automatic stack traces are sufficient.** See the crash output for detailed information.

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
