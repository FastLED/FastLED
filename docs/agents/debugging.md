# Debugging C++ Crashes

**When a mysterious, non-trivial C++ crash appears:**

1. **Recompile with debug flags**: Build the program with `-g3` (full debug info) and `-O0` (no optimization)
2. **Run under GDB**: Execute the program using `gdb <program>` or `gdb --args <program> <args>`
3. **Reproduce the crash**: Run the program until the crash occurs (`run` command in GDB)
4. **Inspect crash state**: When the crash happens, examine:
   - Stack trace: `bt` (backtrace) or `bt full` (with local variables)
   - Current frame variables: `info locals`
   - Specific variable values: `print <variable_name>`
   - Register state: `info registers`
   - Memory contents: `x/<format> <address>`
5. **Report findings**: Document the exact crash location, variable states, and any suspicious values

This systematic approach often reveals null pointers, uninitialized variables, buffer overflows, or memory corruption.

**GDB commands:** `run`, `bt`, `bt full`, `frame <n>`, `print <var>`, `info locals`, `info args`
