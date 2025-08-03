# BUG REPORT: Sketch Runner Integration Not Working for `bash test --examples --verbose | grep BLINK`

## Issue Summary

The command `bash test --examples --verbose | grep BLINK` does not produce "BLINK" output, even though the Blink.ino example contains `Serial.println("BLINK")` statements. The sketch runner execution phase is not being triggered when using the `--examples` flag alone.

## Expected Behavior

When running `bash test --examples --verbose | grep BLINK`, the system should:
1. Discover `examples/Blink/Blink.ino`
2. Compile it with sketch runner infrastructure
3. Link it into an executable with proper exports
4. **Execute the sketch and capture output**
5. Display "BLINK" messages from `Serial.println("BLINK")` calls
6. Allow `grep BLINK` to find the output

## Actual Behavior

The command only performs compilation and linking, but **never executes** the sketches. Therefore:
- No "BLINK" output is produced
- `grep BLINK` finds nothing
- The sketch runner execution phase is completely skipped

## Root Cause Analysis

### Critical Logic Error in `ci/util/test_runner.py`

**Location:** `ci/util/test_runner.py`, lines 365-366

```python
def create_examples_test_process(args: TestArgs, enable_stack_trace: bool) -> RunningProcess:
    cmd = ["uv", "run", "python", "ci/compiler/test_example_compilation.py"]
    # ... other flags ...
    if args.full and args.examples is not None:  # ❌ BUG: Requires BOTH conditions
        cmd.append("--full")
```

**The Problem:** This condition requires **BOTH** `args.full=True` AND `args.examples is not None` for the `--full` flag to be passed to the example compilation script.

**When user runs:** `bash test --examples --verbose`
- `args.examples = []` (not None, but list)
- `args.full = False` (not set by user)
- **Result:** Condition fails, `--full` flag is NOT passed

### Execution Dependency in `ci/compiler/test_example_compilation.py`

**Location:** `ci/compiler/test_example_compilation.py`, lines 1427-1431

```python
def handle_execution(self, results: CompilationTestResults) -> CompilationTestResults:
    """Execute linked programs when full compilation is requested."""
    if (
        not self.config.full_compilation  # ❌ This is False without --full flag
        or results.linking_failed_count > 0
        or results.linked_count == 0
    ):
        return results  # ❌ Execution is skipped
```

**The Problem:** Execution only happens when `self.config.full_compilation=True`, which requires the `--full` flag to be passed from the test runner.

## Evidence of Complete Implementation

The sketch runner execution infrastructure is **fully implemented** and working:

### 1. Execution Code Exists
- ✅ `handle_execution()` method in `test_example_compilation.py`
- ✅ Proper executable discovery in `.build/examples/`
- ✅ `subprocess.run()` with output capture
- ✅ `Serial.println()` output forwarding with `[EXECUTION]` prefix

### 2. Generated Code is Correct
- ✅ `create_main_cpp_for_example()` generates proper `main.cpp` with sketch runner exports
- ✅ Includes `setup()` and `loop()` calls with printf logging
- ✅ Generated code contains: `"RUNNER: Starting sketch execution"`

### 3. Example File is Correct
- ✅ `examples/Blink/Blink.ino` contains `Serial.println("BLINK");` in `loop()`
- ✅ Also contains `Serial.println("BLINK setup starting");` and `Serial.println("BLINK setup complete");`

## Verification of Issue

The infrastructure works correctly when using the proper command:
- ✅ **WORKS:** `bash test --examples --full --verbose | grep BLINK`
- ❌ **FAILS:** `bash test --examples --verbose | grep BLINK`

The difference is the `--full` flag requirement.

## Impact Assessment

**Severity:** HIGH - Breaks user expectation and documented behavior

**User Impact:**
- Users cannot use the intuitive command `bash test --examples --verbose | grep BLINK`
- Forces users to learn about the `--full` flag requirement
- Creates confusion between compilation-only vs execution testing
- Documentation mismatch (if docs suggest `--examples` should execute)

**Development Impact:**
- End-to-end integration test in `ci/tests/test_sketch_runner_execution.py` likely also affected
- CI/CD pipelines may not be testing actual sketch execution
- False sense of completeness when only compilation is tested

## Proposed Solutions

### Option 1: Auto-Enable Full Compilation for Examples (Recommended)
**Change `ci/util/test_runner.py` line 365-366:**

```python
# BEFORE (broken):
if args.full and args.examples is not None:
    cmd.append("--full")

# AFTER (fixed):
if args.examples is not None:  # Always enable full compilation for examples
    cmd.append("--full")
```

**Rationale:** When users specify `--examples`, they expect sketches to actually run, not just compile.

### Option 2: Explicit Documentation and User Education
- Update documentation to clearly state that `--examples --full` is required for execution
- Add helpful error messages when `--examples` is used without `--full`
- Create alias commands for common use cases

### Option 3: Add New Flag for Execution Control
- Create `--execute` flag for explicit execution control
- Maintain backward compatibility with current behavior
- Allow fine-grained control over compilation vs execution

## Recommended Fix

**Implement Option 1** for the following reasons:
1. **User expectation:** `--examples` should test examples completely (including execution)
2. **Simplicity:** Eliminates confusing flag combinations
3. **Safety:** Execution is already sandboxed with 30-second timeouts
4. **Completeness:** Provides actual end-to-end testing of sketch functionality

## Test Plan

After implementing the fix:

1. **Verify basic functionality:**
   ```bash
   bash test --examples --verbose | grep BLINK
   # Should find multiple "BLINK" outputs
   ```

2. **Verify integration test:**
   ```bash
   uv run python ci/tests/test_sketch_runner_execution.py
   # Should pass without requiring --full flag
   ```

3. **Verify backward compatibility:**
   ```bash
   bash test --examples --full --verbose | grep BLINK
   # Should still work with explicit --full flag
   ```

4. **Verify multiple examples:**
   ```bash
   bash test --examples DemoReel100 Blink --verbose
   # Should execute both examples
   ```

## File Inventory

**Files with sketch runner implementation:**
- ✅ `ci/compiler/test_example_compilation.py` - Complete execution infrastructure
- ✅ `ci/tests/test_sketch_runner_execution.py` - End-to-end integration test
- ✅ `examples/Blink/Blink.ino` - Contains proper Serial.println() calls
- ❌ `ci/util/test_runner.py` - **BUG: Incorrect flag logic**

**Files that can be eliminated (now redundant):**
- ❌ `tests/test_sketch_runner.cpp` - Mock unit test, superseded by real integration
- ❌ `tests/sketch_runner_demo.cpp` - Standalone demo, superseded by real integration

## Priority

**HIGH PRIORITY** - This is a critical integration bug that makes the sketch runner feature appear broken when it's actually complete and functional. The fix is simple (one line change) but has significant impact on user experience.
