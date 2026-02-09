# LLDB Debugging Guide

## Overview

The `clang-tool-chain[sccache]` package includes LLDB debugger support for interactive debugging and crash analysis. However, **the existing crash handler already provides excellent stack traces** that are often sufficient for debugging.

## Available Tools

The clang-tool-chain package provides:

- `clang-tool-chain-lldb` - LLDB debugger wrapper
- `clang-tool-chain-lldb-check-python` - Check Python bindings status
- Raw LLDB executable at: `~/.clang-tool-chain/lldb/win/x86_64/bin/lldb.exe`

## Usage

### Basic Usage

```bash
# Interactive debugging
uv run clang-tool-chain-lldb executable.exe

# Automated crash analysis (--print mode)
uv run clang-tool-chain-lldb --print executable.exe

# Interactive LLDB session (no executable)
uv run clang-tool-chain-lldb
```

### Help

```bash
uv run clang-tool-chain-lldb --help
```

## Current Stack Trace Support

**IMPORTANT:** FastLED already has built-in crash handlers that provide excellent stack traces with:

- Function names with full signature
- Source file paths and line numbers
- Instruction addresses and offsets
- Module names (exe/dll)
- Complete call stack (19+ frames)
- Loaded module list

### Example Stack Trace Output

```
#7  0x00007ffadf7d1d2c [fl_test.dll] function_c() + 84
    (Line 9 of "../../tests/fl\test.cpp" starts at address 0x180001d2c)
#8  0x00007ffadf7d1cc7 [fl_test.dll] function_b() + 15
    (Line 13 of "../../tests/fl\test.cpp" starts at address 0x180001cc2)
#9  0x00007ffadf7d1ca7 [fl_test.dll] function_a() + 15
    (Line 17 of "../../tests/fl\test.cpp" starts at address 0x180001ca2)
```

## LLDB Integration Challenges

### Crash Handler Interference

The existing crash handlers catch signals before LLDB can intercept them, which means:

- LLDB's `--print` mode cannot capture crashes automatically
- The process exits before LLDB can generate a backtrace
- The built-in crash handler provides stack traces instead

### Potential Solutions (Not Yet Implemented)

1. **Disable crash handler via environment variable** (requires implementation)
2. **Configure LLDB exception handling before crash handler** (limited success)
3. **Use LLDB interactively with breakpoints** (manual process)

## When to Use LLDB

LLDB is most useful for:

- **Interactive debugging**: Setting breakpoints, stepping through code, inspecting variables
- **Pre-crash inspection**: Examining program state before a crash occurs
- **Complex debugging scenarios**: Thread analysis, watchpoints, conditional breakpoints

## When NOT to Use LLDB

The built-in crash handler is sufficient for:

- **Post-mortem analysis**: Stack traces from crashes
- **Unit test failures**: Automated test runs with stack traces
- **CI/CD debugging**: Non-interactive environments

## Test Framework Integration

The FastLED test framework (`uv run test.py`) automatically:

- Compiles tests with debug symbols (in debug mode)
- Captures crash stack traces via built-in crash handler
- Reports crashes with full file/line information

To compile tests in debug mode:

```bash
uv run test.py <test_name> --cpp --build-mode debug
```

## LLDB Command Reference

Common LLDB commands for debugging (migrated from GDB):

| Task | GDB Command | LLDB Command |
|------|-------------|--------------|
| Run program | `gdb <program>` | `uv run clang-tool-chain-lldb <program>` |
| Backtrace | `bt` | `bt` |
| Full backtrace | `bt full` | `bt all` |
| Select frame | `frame <n>` | `frame select <n>` |
| Print variable | `print <var>` | `print <var>` or `p <var>` |
| Show locals | `info locals` | `frame variable` |
| Show args | `info args` | `frame variable` |
| Registers | `info registers` | `register read` |
| Memory dump | `x/<format> <addr>` | `memory read <addr>` |
| Set breakpoint | `break <location>` | `breakpoint set -n <function>` or `b <file>:<line>` |
| Continue | `continue` or `c` | `continue` or `c` |
| Step over | `next` or `n` | `next` or `n` |
| Step into | `step` or `s` | `step` or `s` |
| Step out | `finish` | `finish` |

## References

- LLDB Official Docs: https://lldb.llvm.org/
- clang-tool-chain PyPI: https://pypi.org/project/clang-tool-chain/
- LLDB Python API: https://lldb.llvm.org/python_reference/lldb-module.html

## Conclusion

**The built-in crash handler provides stack traces that are sufficient for most debugging needs.** LLDB is available if you need interactive debugging, but the automatic crash analysis mode is currently blocked by the existing crash handler.

For typical unit test debugging, rely on the existing stack traces. For complex interactive debugging, use LLDB directly with breakpoints.
