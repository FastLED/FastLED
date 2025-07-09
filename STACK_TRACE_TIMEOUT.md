# Stack Trace Dumping on Test Timeout

## Overview

The FastLED test suite now includes automatic stack trace dumping when tests timeout. This feature helps developers debug hanging or stuck tests by providing detailed stack trace information before the process is killed.

## How It Works

When a test process exceeds its timeout limit, the system will:

1. **Detect the timeout** - The `RunningProcess` class monitors process execution time
2. **Dump stack trace** - Use GDB to attach to the running process and capture stack information
3. **Display debug info** - Show process details, command, and PID
4. **Kill the process** - Terminate the hanging process to prevent indefinite hanging
5. **Raise exception** - Throw a `TimeoutError` with details about the timeout

## Usage

### Default Behavior (Stack Trace Enabled)

By default, stack trace dumping is **enabled** for all test runs:

```bash
# Stack trace dumping is enabled by default
bash test
bash test --cpp
bash test --quick
```

### Disable Stack Trace Dumping

To disable stack trace dumping, use the `--no-stack-trace` flag:

```bash
# Disable stack trace dumping
bash test --no-stack-trace
bash test --cpp --no-stack-trace
bash test --quick --no-stack-trace
```

### Command Line Options

- `--no-stack-trace` - Disable stack trace dumping on timeout
- Stack trace dumping is enabled by default when this flag is not used

## Example Output

When a test times out with stack trace dumping enabled, you'll see output like this:

```
Process timeout after 300 seconds, dumping stack trace...
Command: uv run ci/cpp_test_run.py
Process ID: 12345

================================================================================
STACK TRACE DUMP (GDB Output)
================================================================================
Could not attach to process.  If your uid matches the uid of the target
process, check the setting of /proc/sys/kernel/yama/ptrace_scope, or try
again as the root user.  For more details, see /etc/sysctl.d/10-ptrace.conf
/tmp/tmpxp_fsd7z:2: Error in sourced command file:
ptrace: Inappropriate ioctl for device.
================================================================================
Killing timed out process: uv run ci/cpp_test_run.py
```

## Technical Details

### Implementation

The stack trace functionality is implemented in:

- **`ci/ci/running_process.py`** - `RunningProcess` class with `_dump_stack_trace()` method
- **`test.py`** - Main test runner with `--no-stack-trace` argument handling

### GDB Integration

The stack trace dumping uses GDB to:

1. **Attach to process** - Connect to the running process by PID
2. **Execute commands** - Run GDB commands to gather stack information:
   - `bt full` - Full backtrace with local variables
   - `info registers` - CPU register state
   - `x/16i $pc` - Disassembly around program counter
   - `thread apply all bt full` - Stack traces for all threads
3. **Detach and quit** - Cleanly disconnect from the process

### Timeout Configuration

- **Default timeout**: 300 seconds (5 minutes) per process
- **GDB timeout**: 30 seconds for stack trace collection
- **Configurable**: Timeout can be adjusted per `RunningProcess` instance

## Troubleshooting

### GDB Attachment Failures

In some environments (like containers), GDB may fail to attach due to security restrictions:

```
Could not attach to process. If your uid matches the uid of the target
process, check the setting of /proc/sys/kernel/yama/ptrace_scope
```

**Solutions:**
- Run tests as root user (if possible)
- Adjust ptrace settings: `echo 0 > /proc/sys/kernel/yama/ptrace_scope`
- Use `--no-stack-trace` to disable the feature

### Performance Impact

- **Minimal overhead** - Stack trace dumping only occurs on timeout
- **30-second GDB timeout** - Prevents indefinite hanging during stack trace collection
- **Automatic cleanup** - GDB scripts are automatically removed after use

## Integration with Existing Tests

The stack trace functionality integrates seamlessly with all existing test types:

- **C++ tests** - `uv run ci/cpp_test_run.py`
- **Python tests** - `uv run pytest`
- **Compilation tests** - `uv run ci/ci-compile-native.py`
- **Platform compilation** - `uv run ci/ci-compile.py`
- **Code quality checks** - Various linting and analysis tools

## Best Practices

1. **Keep enabled by default** - Stack traces provide valuable debugging information
2. **Use in CI/CD** - Helps identify hanging tests in automated environments
3. **Disable when needed** - Use `--no-stack-trace` in environments where GDB attachment fails
4. **Monitor timeouts** - Regular timeouts may indicate performance issues or bugs

## Related Features

- **Crash handling** - See `tests/crash_handler.h` for crash-time stack traces
- **GDB integration** - Used in `ci/cpp_test_run.py` for crash analysis
- **Process management** - `RunningProcess` class provides comprehensive subprocess control
