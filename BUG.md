# Bug Report: `bash test` Process Management Issue

## Issue Description
The command `bash test` fails with exit code 120 during the subprocess execution phase, specifically affecting `uv run python -m ci.compiler.test_example_compilation` and/or `uv run pytest` when run as part of the full test suite.

## Error Details
```
Command failed: uv run python -m ci.compiler.test_example_compilation
###### ERROR ######
Test failed: test_example_compilation
###### ERROR ######
Tests failed with exit code 120
```

**OR** (depending on timing):
```
Command failed: uv run pytest -s ci/tests -xvs --durations=0 with return code 120
```

## Key Discovery: Isolation vs Full Suite
- ✅ **WORKS IN ISOLATION**: `uv run python -m ci.compiler.test_example_compilation` (exit code 0)
- ✅ **WORKS IN ISOLATION**: `uv run pytest -s ci/tests -xvs --durations=0` (28 passed, 8 skipped)
- ❌ **FAILS IN FULL SUITE**: Same commands fail with exit code 120 when run as part of `bash test`

## Successful Components
- ✅ Compilation: All 89 test files compiled and linked successfully (2.73s)
- ✅ Header validation: `uv run python ci/tests/no_using_namespace_fl_in_headers.py`
- ✅ Test runner: `uv run python -m ci.run_tests`
- ✅ Individual subprocess execution when run manually

## Three Possible Causes

### 1. Process Resource Contention and Interference
**Root Cause:** Multiple concurrent subprocess executions in the full test suite are creating resource contention, causing some processes to fail with exit code 120 (process terminated/interrupted).

**Evidence:**
- ✅ **Individual commands work perfectly**: Both `test_example_compilation` and `pytest` succeed when run manually
- ❌ **Same commands fail in test suite**: Exit code 120 occurs only when run as part of full `bash test`
- **Timing-dependent failure**: Sometimes `test_example_compilation` fails, sometimes `pytest` fails
- **Resource-intensive compilation phase**: 89 parallel compilation jobs completed just before failures

**Investigation Steps:**
- Monitor system resources (CPU, memory, file handles) during full test execution
- Check for process limits (max open files, process count limits)
- Add delays between subprocess launches in the test runner
- Use process monitoring tools to identify resource bottlenecks

### 2. Subprocess Process Group Management and Signal Handling
**Root Cause:** The test runner's subprocess management is not properly handling process groups, leading to premature termination or signal interference between concurrent processes.

**Evidence:**
- **Exit code 120**: Often indicates process interruption/termination by signal (SIGTERM equivalent)
- **Concurrent execution**: Multiple Python subprocesses running simultaneously in test suite
- **Git-bash environment**: Complex signal handling on Windows with git-bash can cause process management issues
- **Works in isolation**: Individual processes don't have group/signal conflicts

**Investigation Steps:**
- Review subprocess creation and process group handling in `ci/ci/test_runner.py`
- Check for signal handling issues in concurrent subprocess execution
- Test with different subprocess creation flags (process groups, signal handling)
- Try running full test suite in different terminal environments (PowerShell vs git-bash)

### 3. sccache/Compiler Cache Lock Contention
**Root Cause:** The sccache compilation cache is creating lock contention when multiple test processes try to access compiler resources simultaneously after the intensive compilation phase.

**Evidence:**
- **sccache usage**: Test output shows "Using sccache for unit tests: C:\Users\niteris\dev\fastled\.venv\Scripts\sccache.EXE"
- **Post-compilation failures**: Issues occur after successful 89-file compilation phase
- **Cache-related timing**: Failures happen when multiple processes might access compiler/cache resources
- **Isolated success**: Individual processes don't compete for cache locks

**Investigation Steps:**
- Disable sccache temporarily: test with `--no-cache` or similar flags
- Check sccache log files for lock contention or timeout errors
- Monitor file system locks during test execution
- Test with increased sccache timeout values or disabled caching

## Recommended Next Steps

### Immediate Debugging Actions
1. **Test Resource Contention**: Run `bash test` while monitoring system resources (Task Manager, Resource Monitor)
2. **Test sccache Theory**: Run tests with sccache disabled: `bash test --no-cache` (if supported) or temporarily disable sccache
3. **Test Sequential Execution**: Modify test runner to run subprocesses sequentially instead of concurrently to isolate timing issues
4. **Test Different Terminal**: Run `bash test` in PowerShell instead of git-bash to rule out terminal-specific issues

### Debugging Commands to Try
```bash
# Test without potential cache contention
sccache --stop-server
bash test

# Test with verbose process monitoring
bash test --verbose

# Test individual components that work in isolation
uv run python -m ci.compiler.test_example_compilation
uv run pytest -s ci/tests -xvs --durations=0

# Monitor for resource limits
ulimit -a  # Check process/file limits
```

### Code Investigation Areas
1. **Process Management**: Review `ci/ci/test_runner.py` subprocess handling, particularly concurrent execution patterns
2. **Signal Handling**: Check for proper signal handling and process group management in test runner
3. **Resource Cleanup**: Verify all subprocesses properly clean up resources and don't leave hanging processes
4. **Timing Dependencies**: Look for race conditions in subprocess execution order

### Expected Outcome
The most likely fix will involve either:
- Adding delays/sequencing between subprocess launches
- Improving subprocess process group and signal handling
- Disabling or reconfiguring sccache for concurrent test execution
- Adding resource limits or better resource management to the test runner
