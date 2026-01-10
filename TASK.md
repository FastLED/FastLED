# Task: Implement Debug Mode for Examples

## Executive Summary

**Objective**: Add debug mode support to FastLED examples, mirroring the existing implementation for unit tests. Examples should support all three build modes (quick, debug, release) with mode-specific build directories, automatic reconfiguration, and proper cleanup.

**Status**: Investigation complete (iteration 1). Planning complete (iteration 2). Ready for implementation (iteration 3+).

**Expected Impact**:
- Enable debugging of examples with full symbols (`-g3`) and sanitizers
- Maintain parity between test and example compilation capabilities
- No performance impact on default (quick) mode
- Seamless mode switching without manual cleanup

## Background

From `INVESTIGATION.md`:
- ✅ Unit tests have complete debug mode support
- ❌ Examples lack debug mode support
- **Solution**: Mirror `ci/util/meson_runner.py` patterns in `ci/util/meson_example_runner.py`

## Implementation Phases

### Phase 1: Core Infrastructure (Iteration 3)

#### Task 1.1: Add Debug Parameters to run_meson_build_examples()

**File**: `ci/util/meson_example_runner.py`

**Current signature** (lines 30-40, approximate):
```python
def run_meson_build_examples(
    source_dir: Path,
    build_dir: Path,
    example_names: list[str] | None = None,
    clean: bool = False,
    verbose: bool = False,
    no_parallel: bool = False,
    no_pch: bool = False,
    full: bool = False,
) -> MesonTestResult:
```

**New signature** (add two parameters):
```python
def run_meson_build_examples(
    source_dir: Path,
    build_dir: Path,
    example_names: list[str] | None = None,
    clean: bool = False,
    verbose: bool = False,
    debug: bool = False,              # NEW: Enable debug mode
    build_mode: str | None = None,    # NEW: Override build mode
    no_parallel: bool = False,
    no_pch: bool = False,
    full: bool = False,
) -> MesonTestResult:
```

**Add docstring updates**:
```python
"""
Compile and optionally execute host-based examples using Meson.

Args:
    source_dir: Root directory of the FastLED project
    build_dir: Base build directory (will be made mode-specific)
    example_names: List of example names to build (None = all)
    clean: Force clean rebuild
    verbose: Enable verbose output
    debug: Enable debug mode (full symbols + sanitizers)
    build_mode: Override build mode ('quick', 'debug', 'release')
    no_parallel: Disable parallel compilation
    no_pch: Disable precompiled headers (ignored, PCH always enabled)
    full: Execute examples after compilation

Returns:
    MesonTestResult object with success status and output
"""
```

#### Task 1.2: Implement Build Mode Logic

**Location**: Beginning of `run_meson_build_examples()` function body

**Add this code** (after parameter validation, before any build operations):

```python
    # Determine build mode (build_mode parameter takes precedence over debug flag)
    if build_mode is None:
        build_mode = "debug" if debug else "quick"

    # Validate build mode
    valid_modes = ["quick", "debug", "release"]
    if build_mode not in valid_modes:
        raise ValueError(
            f"Invalid build mode: {build_mode}. "
            f"Valid modes: {', '.join(valid_modes)}"
        )

    # Construct mode-specific build directory
    # This enables caching per mode when source unchanged but flags differ
    # Example: .build/meson -> .build/meson-debug
    original_build_dir = build_dir
    build_dir = build_dir.parent / f"{build_dir.name}-{build_mode}"

    _ts_print(f"[EXAMPLES] Using mode-specific build directory: {build_dir}")
    _ts_print(f"[EXAMPLES] Build mode: {build_mode}")
```

**Rationale**: This matches the pattern in `meson_runner.py:1624-1631` exactly.

#### Task 1.3: Pass Debug Flag to setup_meson_build()

**Location**: Where `setup_meson_build()` is called in `meson_example_runner.py`

**Current call** (approximate):
```python
    setup_meson_build(
        source_dir=source_dir,
        build_dir=build_dir,
        reconfigure=clean,
    )
```

**Updated call** (add debug parameter):
```python
    setup_meson_build(
        source_dir=source_dir,
        build_dir=build_dir,
        debug=(build_mode == "debug"),  # Convert mode string to boolean
        reconfigure=clean,
    )
```

**Verification**:
- `setup_meson_build()` in `meson_runner.py` already accepts `debug` parameter
- It handles marker files (`.debug_config`), reconfiguration, and cleanup automatically
- No changes needed to `setup_meson_build()` itself

---

### Phase 2: test.py Integration (Iteration 4)

#### Task 2.1: Verify Command-Line Flags

**File**: `test.py`

**Check for these flags** (likely already exist):
1. `--examples` - Run examples instead of tests
2. `--debug` - Enable debug mode
3. `--build-mode` - Override build mode

**If --build-mode doesn't exist**, add it:
```python
parser.add_argument(
    "--build-mode",
    type=str,
    choices=["quick", "debug", "release"],
    default=None,
    help="Override build mode (default: quick, or debug if --debug flag set)"
)
```

#### Task 2.2: Pass Flags to run_meson_build_examples()

**Location**: In `test.py`, find where `run_meson_build_examples()` is called

**Add two arguments** to the function call:
```python
    result = run_meson_build_examples(
        source_dir=source_dir,
        build_dir=build_dir,
        example_names=example_names,  # or args.examples, depending on implementation
        clean=args.clean if hasattr(args, 'clean') else False,
        verbose=args.verbose if hasattr(args, 'verbose') else False,
        debug=args.debug,              # NEW: Pass debug flag
        build_mode=args.build_mode if hasattr(args, 'build_mode') else None,  # NEW
        no_parallel=args.no_parallel if hasattr(args, 'no_parallel') else False,
        no_pch=args.no_pch if hasattr(args, 'no_pch') else False,
        full=args.full if hasattr(args, 'full') else False,
    )
```

**Testing**:
```bash
# Quick mode (default)
uv run test.py --examples

# Debug mode
uv run test.py --examples --debug

# Release mode
uv run test.py --examples --build-mode release

# Single example in debug mode
uv run test.py --examples Blink --debug
```

---

### Phase 3: Initial Testing (Iteration 5)

#### Task 3.1: Test Single Example - Quick Mode

**Purpose**: Establish baseline before testing debug mode

**Commands**:
```bash
# Clean slate
rm -rf .build/meson-*

# Compile Blink in quick mode (default)
uv run test.py --examples Blink
```

**Expected results**:
- Build directory: `.build/meson-quick/examples/`
- Executable: `.build/meson-quick/examples/example-Blink.exe`
- Marker file: `.build/meson-quick/.debug_config` (contains "False")
- Exit code: 0 (success)

#### Task 3.2: Test Single Example - Debug Mode

**Purpose**: Verify debug mode compilation works

**Commands**:
```bash
# Switch to debug mode (should trigger reconfiguration)
uv run test.py --examples Blink --debug
```

**Expected results**:
- Build directory: `.build/meson-debug/examples/`
- Executable: `.build/meson-debug/examples/example-Blink.exe`
- Marker file: `.build/meson-debug/.debug_config` (contains "True")
- Compilation includes `-O0 -g3 -fsanitize=address,undefined`
- Binary size larger than quick mode (debug symbols)
- Exit code: 0 (success)

**Verification**:
```bash
# Check that both directories exist
ls -ld .build/meson-quick/examples/
ls -ld .build/meson-debug/examples/

# Compare binary sizes
ls -lh .build/meson-quick/examples/example-Blink.exe
ls -lh .build/meson-debug/examples/example-Blink.exe
```

#### Task 3.3: Test Mode Switching

**Purpose**: Verify mode changes trigger cleanup and reconfiguration

**Commands**:
```bash
# Start in quick mode
uv run test.py --examples Blink

# Switch to debug mode
uv run test.py --examples Blink --debug

# Switch back to quick mode
uv run test.py --examples Blink

# Switch to release mode
uv run test.py --examples Blink --build-mode release
```

**Expected behavior**:
- Each mode switch prints: `"⚠️  Debug mode changed: X → Y"`
- Each mode switch prints: `"🔄 Forcing reconfigure"`
- Each mode switch prints: `"🗑️  Debug mode changed - cleaning all object files"`
- No linking errors occur
- All builds succeed

#### Task 3.4: Test Multiple Examples in Debug Mode

**Purpose**: Verify debug mode works for multiple examples

**Commands**:
```bash
# Compile several examples in debug mode
uv run test.py --examples Blink DemoReel100 Pride2015 --debug
```

**Expected results**:
- All three examples compile successfully
- All use `.build/meson-debug/examples/` directory
- All execute without sanitizer errors (if --full flag added)

---

### Phase 4: Full Suite Validation (Iteration 6)

#### Task 4.1: Test All Examples - Quick Mode

**Purpose**: Baseline performance and verify no regressions

**Command**:
```bash
time uv run test.py --examples
```

**Expected results**:
- All 96 examples compile successfully
- Compilation time ~0.24s (with PCH)
- Zero failures

#### Task 4.2: Test All Examples - Debug Mode

**Purpose**: Verify debug mode works for entire suite

**Command**:
```bash
time uv run test.py --examples --debug
```

**Expected results**:
- All 96 examples compile successfully
- Compilation time slower than quick mode (sanitizers add overhead)
- Zero failures
- No linking errors

#### Task 4.3: Test All Examples - Release Mode

**Purpose**: Verify release mode works for entire suite

**Command**:
```bash
time uv run test.py --examples --build-mode release
```

**Expected results**:
- All 96 examples compile successfully
- Uses `-O3 -DNDEBUG` flags
- Zero failures

#### Task 4.4: Verify Sanitizer Execution

**Purpose**: Confirm sanitizers work at runtime

**Commands**:
```bash
# Execute examples with sanitizers
uv run test.py --examples Blink DemoReel100 --debug --full
```

**Expected results**:
- Examples execute without sanitizer errors
- No memory leaks reported
- No undefined behavior detected
- Clean exit (code 0)

---

### Phase 5: Documentation (Iteration 7)

#### Task 5.1: Update examples/AGENTS.md

**File**: `examples/AGENTS.md`

**Add new section** after the existing compilation commands:

```markdown
## Debug Mode for Examples

Examples support three build modes, mirroring the unit test system:

### Quick Mode (Default)
- **Flags**: `-O0 -g1` (minimal debug info)
- **Use case**: Fast iteration and testing
- **Command**: `uv run test.py --examples`
- **Build directory**: `.build/meson-quick/examples/`

### Debug Mode
- **Flags**: `-O0 -g3` + AddressSanitizer + UndefinedBehaviorSanitizer
- **Use case**: Debugging crashes, memory issues, undefined behavior
- **Command**: `uv run test.py --examples --debug`
- **Build directory**: `.build/meson-debug/examples/`
- **Example**: `uv run test.py --examples Blink --debug --full`

### Release Mode
- **Flags**: `-O3 -DNDEBUG` (optimized)
- **Use case**: Performance testing
- **Command**: `uv run test.py --examples --build-mode release`
- **Build directory**: `.build/meson-release/examples/`

### Mode-Specific Build Directories

Examples use separate build directories per mode to enable caching and prevent flag conflicts:

- `.build/meson-quick/examples/` - Quick mode
- `.build/meson-debug/examples/` - Debug mode
- `.build/meson-release/examples/` - Release mode

When switching modes, the system automatically:
1. Detects the mode change via marker files
2. Forces Meson reconfiguration
3. Cleans all object files and archives
4. Rebuilds with new flags

This prevents linking errors from mixing objects compiled with different flags.

### Debugging Workflow

1. **Identify problematic example**:
   ```bash
   uv run test.py --examples ExampleName
   ```

2. **Compile with debug symbols and sanitizers**:
   ```bash
   uv run test.py --examples ExampleName --debug --full
   ```

3. **Analyze sanitizer output** for memory errors or undefined behavior

4. **Use GDB for deeper investigation** (if needed):
   ```bash
   gdb .build/meson-debug/examples/example-ExampleName.exe
   ```

### Performance Notes

- **Quick mode**: Fastest compilation, suitable for rapid iteration
- **Debug mode**: Slower compilation (sanitizers add overhead), larger binaries
- **Release mode**: Optimized for performance testing, no debug info
- **Mode switching**: One-time cleanup cost, then normal build speed
- **PCH**: Works in all modes (precompiled headers dramatically speed up compilation)
```

#### Task 5.2: Update CLAUDE.md

**File**: `CLAUDE.md`

**Update section**: "Example Compilation (Host-Based)"

**Add build mode commands** to the existing list:

```markdown
### Example Compilation (Host-Based)
FastLED supports fast host-based compilation of `.ino` examples using Meson build system:

- `uv run test.py --examples` - Compile and run all examples (quick mode, default)
- `uv run test.py --examples --debug` - Compile with debug symbols and sanitizers
- `uv run test.py --examples --build-mode release` - Compile optimized release builds
- `uv run test.py --examples Blink DemoReel100` - Compile specific examples
- `uv run test.py --examples Blink --debug --full` - Debug mode with execution
- `uv run test.py --examples --no-parallel` - Sequential compilation (easier debugging)
- `uv run test.py --examples --verbose` - Show detailed compilation output
- `uv run test.py --examples --clean` - Clean build cache and recompile

**Build Modes**:
- **quick** (default): Fast compilation with minimal debug info (`-O0 -g1`)
- **debug**: Full symbols and sanitizers (`-O0 -g3` + ASan + UBSan)
- **release**: Optimized production build (`-O3 -DNDEBUG`)

**Mode-Specific Directories**:
- `.build/meson-quick/examples/` - Quick mode builds
- `.build/meson-debug/examples/` - Debug mode builds
- `.build/meson-release/examples/` - Release mode builds
```

---

### Phase 6: Edge Cases and Polish (Iteration 8)

#### Task 6.1: Test Edge Case - Clean Flag with Mode

**Commands**:
```bash
# Build in quick mode
uv run test.py --examples Blink

# Force clean rebuild in debug mode
uv run test.py --examples Blink --debug --clean
```

**Expected**: Complete rebuild in debug mode, even if quick mode cached

#### Task 6.2: Test Edge Case - Invalid Build Mode

**Command**:
```bash
uv run test.py --examples --build-mode invalid
```

**Expected**: Clear error message:
```
Error: Invalid build mode: invalid. Valid modes: quick, debug, release
```

#### Task 6.3: Test Edge Case - Conflicting Flags

**Command**:
```bash
uv run test.py --examples --debug --build-mode quick
```

**Expected behavior** (implementation decision):
- **Option A**: `build_mode` parameter takes precedence (use quick mode)
- **Option B**: Raise error about conflicting flags

**Recommendation**: Option A (precedence) for flexibility

#### Task 6.4: Improve Error Messages

**Add helpful error messages** for common failure scenarios:

1. **Mode detection failure**:
   ```python
   if build_mode is None and not debug:
       # This shouldn't happen, but handle gracefully
       build_mode = "quick"
       print("Warning: Could not determine build mode, defaulting to 'quick'")
   ```

2. **Build directory creation failure**:
   ```python
   try:
       build_dir.mkdir(parents=True, exist_ok=True)
   except OSError as e:
       print(f"Error: Failed to create build directory: {build_dir}")
       print(f"Reason: {e}")
       return MesonTestResult(success=False, output=str(e))
   ```

3. **Marker file issues**:
   ```python
   try:
       debug_marker.write_text(str(debug))
   except (OSError, IOError) as e:
       print(f"Warning: Could not write debug marker file: {e}")
       print("Build will continue, but mode detection may be affected")
   ```

---

## Testing Matrix

### Smoke Tests (Fast - Run First)
```bash
# Single example, each mode
uv run test.py --examples Blink                    # quick
uv run test.py --examples Blink --debug            # debug
uv run test.py --examples Blink --build-mode release  # release

# Mode switching (same example)
uv run test.py --examples Blink                    # quick
uv run test.py --examples Blink --debug            # switch to debug
uv run test.py --examples Blink                    # switch back to quick
```

### Integration Tests (Medium - Run After Smoke Tests Pass)
```bash
# Multiple examples, each mode
uv run test.py --examples Blink DemoReel100 Pride2015
uv run test.py --examples Blink DemoReel100 Pride2015 --debug
uv run test.py --examples Blink DemoReel100 Pride2015 --build-mode release

# Execution with sanitizers
uv run test.py --examples Blink --debug --full
```

### Full Suite Tests (Slow - Run Before Final Validation)
```bash
# All examples, each mode
time uv run test.py --examples                     # ~0.24s expected
time uv run test.py --examples --debug             # slower, but should succeed
time uv run test.py --examples --build-mode release
```

### Regression Tests (Critical - Must Pass)
```bash
# Ensure no breakage to existing functionality
uv run test.py --cpp                               # Unit tests still work
uv run test.py --examples                          # Examples still work (default)
bash lint                                          # No new linter errors
```

---

## Implementation Checklist

### Phase 1: Core Infrastructure (Iteration 3)
- [ ] Add `debug` parameter to `run_meson_build_examples()`
- [ ] Add `build_mode` parameter to `run_meson_build_examples()`
- [ ] Implement build mode determination logic
- [ ] Implement mode-specific build directory construction
- [ ] Pass `debug` flag to `setup_meson_build()`
- [ ] Add docstring updates
- [ ] Verify code compiles without syntax errors

### Phase 2: test.py Integration (Iteration 4)
- [ ] Verify `--debug` flag exists in test.py
- [ ] Add `--build-mode` flag if missing
- [ ] Pass `debug` to `run_meson_build_examples()`
- [ ] Pass `build_mode` to `run_meson_build_examples()`
- [ ] Test CLI flags work correctly

### Phase 3: Initial Testing (Iteration 5)
- [ ] Test single example in quick mode
- [ ] Test single example in debug mode
- [ ] Verify mode-specific directories created
- [ ] Verify marker files created
- [ ] Test mode switching triggers reconfiguration
- [ ] Test multiple examples in debug mode

### Phase 4: Full Suite Validation (Iteration 6)
- [ ] Test all examples in quick mode
- [ ] Test all examples in debug mode
- [ ] Test all examples in release mode
- [ ] Verify sanitizer execution works
- [ ] Measure compilation times
- [ ] Verify no regressions

### Phase 5: Documentation (Iteration 7)
- [ ] Update `examples/AGENTS.md`
- [ ] Update `CLAUDE.md`
- [ ] Add usage examples
- [ ] Document debugging workflow

### Phase 6: Edge Cases (Iteration 8)
- [ ] Test clean flag with mode
- [ ] Test invalid build mode
- [ ] Test conflicting flags
- [ ] Add error messages
- [ ] Final regression testing

---

## Success Criteria

**The implementation is complete when ALL of these are verified:**

### Functionality
- [ ] `uv run test.py --examples` works (quick mode, default)
- [ ] `uv run test.py --examples --debug` works (debug mode)
- [ ] `uv run test.py --examples --build-mode release` works (release mode)
- [ ] Mode-specific build directories created correctly
- [ ] Mode switching triggers automatic cleanup and reconfiguration
- [ ] All 96 examples compile successfully in each mode
- [ ] Debug mode includes `-g3` symbols and sanitizers
- [ ] Examples execute without sanitizer errors

### Quality
- [ ] No regressions in existing functionality
- [ ] Unit tests still pass (`uv run test.py --cpp`)
- [ ] Linter passes (`bash lint`)
- [ ] Error messages are clear and helpful
- [ ] Edge cases handled gracefully

### Documentation
- [ ] `examples/AGENTS.md` updated with debug mode section
- [ ] `CLAUDE.md` updated with build mode commands
- [ ] Usage examples provided
- [ ] Debugging workflow documented

---

## Key Files

### Files to Modify
1. **ci/util/meson_example_runner.py** (PRIMARY)
   - Add `debug` and `build_mode` parameters
   - Implement mode-specific directory logic
   - Pass debug flag to `setup_meson_build()`

2. **test.py** (SECONDARY)
   - Verify/add `--build-mode` flag
   - Pass flags to `run_meson_build_examples()`

3. **examples/AGENTS.md** (DOCUMENTATION)
   - Add debug mode documentation
   - Add usage examples

4. **CLAUDE.md** (DOCUMENTATION)
   - Update quick reference

### Files to Reference (Do NOT Modify)
1. **ci/util/meson_runner.py**
   - Reference implementation (lines 1584-1972)
   - `setup_meson_build()` implementation (lines 327-1049)

2. **INVESTIGATION.md**
   - Technical details from iteration 1
   - Design patterns to follow

3. **meson.build**
   - Build mode flag definitions (lines 124-148)

4. **meson.options**
   - Build mode option definition

---

## Critical Patterns to Follow

### From INVESTIGATION.md:

1. **Mode-Specific Build Directories**
   ```python
   build_dir = build_dir.parent / f"{build_dir.name}-{build_mode}"
   ```
   - Prevents flag conflicts
   - Enables per-mode caching

2. **Debug Flag Conversion**
   ```python
   setup_meson_build(..., debug=(build_mode == "debug"))
   ```
   - Convert mode string to boolean
   - `setup_meson_build()` expects boolean, not string

3. **Mode Validation**
   ```python
   if build_mode not in ["quick", "debug", "release"]:
       raise ValueError(...)
   ```
   - Fail fast on invalid modes
   - Clear error messages

4. **Marker File Handling**
   - `setup_meson_build()` handles this automatically
   - No additional code needed in `meson_example_runner.py`

---

## Common Pitfalls to Avoid

### ❌ Don't Do This:
1. **Reuse same build directory for all modes** → Causes linking errors
2. **Forget to pass debug flag to setup_meson_build()** → No sanitizers applied
3. **Hardcode build directory paths** → Breaks mode-specific directories
4. **Skip mode validation** → Cryptic errors from Meson

### ✅ Do This Instead:
1. Use mode-specific directories: `build_dir / f"{name}-{mode}"`
2. Always pass debug flag: `debug=(build_mode == "debug")`
3. Use Path concatenation: `build_dir.parent / f"..."`
4. Validate early: Check mode before any build operations

---

## Performance Expectations

### Build Times (from INVESTIGATION.md)
- **Quick mode**: ~0.24s for 96 examples (with PCH)
- **Debug mode**: Slower (sanitizers add overhead), but should complete in reasonable time
- **Release mode**: Similar to quick mode (optimization happens at compile, not link)

### Binary Sizes
- **Quick mode**: Minimal debug info → smaller binaries
- **Debug mode**: Full symbols → significantly larger binaries
- **Release mode**: Optimized, no debug info → smallest binaries

### Mode Switching
- **First switch**: One-time cleanup cost (deletes .o, .a, .exe files)
- **Subsequent builds**: Normal build speed (cached if source unchanged)

---

## Definition of Done

Write `DONE.md` at project root when **ALL** of these are true:

1. ✅ Core infrastructure implemented and compiles
2. ✅ test.py integration complete
3. ✅ All smoke tests pass
4. ✅ All integration tests pass
5. ✅ Full suite tests pass (96 examples, all modes)
6. ✅ Regression tests pass (cpp, examples, lint)
7. ✅ Documentation updated (AGENTS.md, CLAUDE.md)
8. ✅ Edge cases handled gracefully
9. ✅ No known issues or blockers

**Until ALL criteria met**, continue iterations with incremental progress.

---

## Notes for Future Iterations

### Iteration 3 (Core Infrastructure)
- Focus: Modify `meson_example_runner.py`
- Verify: Code compiles without syntax errors
- Test: Import the module (`uv run python -c "from ci.util.meson_example_runner import run_meson_build_examples"`)

### Iteration 4 (test.py Integration)
- Focus: Update `test.py` to pass flags
- Verify: `--debug` and `--build-mode` flags recognized
- Test: `uv run test.py --examples --help` shows flags

### Iteration 5 (Initial Testing)
- Focus: Single example testing in each mode
- Verify: Mode-specific directories created
- Test: Blink in quick, debug, release modes

### Iteration 6 (Full Suite Validation)
- Focus: All 96 examples in each mode
- Verify: Zero failures
- Test: Full suite in quick, debug, release

### Iteration 7 (Documentation)
- Focus: Update AGENTS.md and CLAUDE.md
- Verify: Documentation clear and accurate
- Test: User can follow docs to use debug mode

### Iteration 8 (Edge Cases and Final Validation)
- Focus: Edge cases, error handling, regression testing
- Verify: All edge cases handled, all tests pass
- Test: Complete testing matrix

---

**END OF TASK SPECIFICATION**

Ready for iteration 3 to begin implementation.
