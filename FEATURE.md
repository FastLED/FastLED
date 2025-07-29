# Test.py Flag System Refactoring Plan

## Overview

Refactor `test.py` to support granular test execution control with new flag system:
- `--unit` - Run only C++ unit tests 
- `--examples` - Run only example compilation tests
- `--py` - Run only Python tests
- No arguments - Run all tests (equivalent to `--unit --examples --py`)
- `--cpp` - Modified behavior to default to `--unit --examples` (suppress Python tests)

## Current State Analysis

### Existing Flag Behavior
- `--cpp` - Currently runs C++ tests only, but includes Python tests in default mode
- `--examples` - Currently auto-enables `--cpp` mode and runs example compilation
- No explicit Python test control flags
- Default behavior runs all tests in parallel

### Current Test Categories
1. **C++ Unit Tests** (`cmd_str_cpp` via `ci/cpp_test_run.py`)
2. **Example Compilation** (`ci/test_example_compilation.py`)  
3. **Python Tests** (`pytest` via `ci/tests`)
4. **Namespace Check** (`ci/tests/no_using_namespace_fl_in_headers.py`)
5. **Native Compilation** (`ci/ci-compile-native.py`)
6. **PIO Check** (`_make_pio_check_cmd()`)
7. **UNO Compilation** (conditional based on source changes)

## Proposed Changes

### 1. New Argument Parser Additions

Add new flags to `parse_args()`:
```python
parser.add_argument("--unit", action="store_true", help="Run C++ unit tests only")
parser.add_argument("--py", action="store_true", help="Run Python tests only")
# --examples already exists but will be modified
```

### 2. Flag Processing Logic

Add flag validation and default behavior logic:
```python
def process_test_flags(args):
    """Process and validate test execution flags"""
    
    # Check for conflicting specific test flags
    specific_flags = [args.unit, args.examples is not None, args.py]
    specific_count = sum(bool(flag) for flag in specific_flags)
    
    # If --cpp is provided, default to --unit --examples (no Python)
    if args.cpp and specific_count == 0:
        args.unit = True
        args.examples = []  # Empty list means run all examples
        print("--cpp mode: Running unit tests and examples (Python tests suppressed)")
        return args
    
    # If no specific flags, run everything (backward compatibility)
    if specific_count == 0:
        args.unit = True
        args.examples = []  # Empty list means run all examples
        args.py = True
        print("No test flags specified: Running all tests")
        return args
        
    return args
```

### 3. Test Execution Refactoring

#### Current Structure Issue
Current code has two paths:
- `if args.cpp:` - Handles C++ tests and early returns for examples
- Default path - Runs all tests in parallel

#### Proposed Structure
Replace with clear test category execution:

```python
def main():
    # ... existing setup code ...
    
    args = parse_args()
    args = process_test_flags(args)
    
    # Determine which test categories to run
    test_categories = determine_test_categories(args)
    
    # Execute test categories
    if test_categories['unit_only'] or test_categories['examples_only']:
        run_cpp_tests(args, test_categories)
    elif test_categories['py_only']:
        run_python_tests(args)
    else:
        run_all_tests(args, test_categories)

def determine_test_categories(args):
    """Determine which test categories should run based on flags"""
    return {
        'unit': args.unit,
        'examples': args.examples is not None,
        'py': args.py,
        'unit_only': args.unit and not args.examples and not args.py,
        'examples_only': args.examples is not None and not args.unit and not args.py,
        'py_only': args.py and not args.unit and not args.examples
    }
```

### 4. Test Execution Functions

#### Unit Tests Function
```python
def run_unit_tests(args, enable_stack_trace):
    """Run C++ unit tests only"""
    print("Running C++ unit tests...")
    
    # Namespace check (always run with C++ tests)
    run_namespace_check(enable_stack_trace)
    
    # Build and run unit tests
    cmd_list = build_cpp_test_command(args)
    proc = RunningProcess(cmd_list, enable_stack_trace=enable_stack_trace)
    proc.wait()
    if proc.returncode != 0:
        sys.exit(proc.returncode)
```

#### Examples Tests Function  
```python
def run_examples_tests(args, enable_stack_trace):
    """Run example compilation tests only"""
    print("Running example compilation tests...")
    
    # Handle specific vs all examples
    cmd = ["uv", "run", "ci/test_example_compilation.py"]
    if args.examples:
        cmd.extend(args.examples)
    if args.clean:
        cmd.append("--clean")
    if args.no_pch:
        cmd.append("--no-pch")
    if args.cache:
        cmd.append("--cache")
        
    proc = RunningProcess(cmd, echo=True, auto_run=True, enable_stack_trace=enable_stack_trace)
    proc.wait()
    if proc.returncode != 0:
        sys.exit(proc.returncode)
```

#### Python Tests Function
```python
def run_python_tests(args, enable_stack_trace):
    """Run Python tests only"""
    print("Running Python tests...")
    
    pytest_proc = RunningProcess(
        "uv run pytest -s ci/tests -xvs --durations=0",
        echo=True,
        auto_run=True,
        enable_stack_trace=enable_stack_trace
    )
    pytest_proc.wait()
    if pytest_proc.returncode != 0:
        sys.exit(pytest_proc.returncode)
```

### 5. Modified --cpp Behavior

Change `--cpp` flag behavior:
- **OLD**: `--cpp` runs only C++ tests but still includes Python in default mode
- **NEW**: `--cpp` defaults to `--unit --examples` (explicitly excludes Python)

### 6. Backward Compatibility

Ensure existing usage patterns continue to work:
- `bash test` - Runs all tests (same as before)
- `bash test specific_test` - Runs specific C++ test (same as before) 
- `bash test --cpp` - Now suppresses Python tests (new behavior)
- `bash test --examples` - Runs examples only (new capability)

### 7. Help Text Updates

Update argument parser help text to reflect new functionality:
```python
parser.add_argument("--cpp", action="store_true", 
                   help="Run C++ tests only (equivalent to --unit --examples, suppresses Python tests)")
parser.add_argument("--unit", action="store_true", 
                   help="Run C++ unit tests only")
parser.add_argument("--examples", nargs="*", 
                   help="Run example compilation tests only (optionally specify example names)")
parser.add_argument("--py", action="store_true", 
                   help="Run Python tests only")
```

## Implementation Steps

### Phase 1: Core Flag Processing
1. Add new argument parser flags (`--unit`, `--py`)
2. Implement `process_test_flags()` function
3. Implement `determine_test_categories()` function
4. Update help text

### Phase 2: Test Execution Refactoring
1. Extract current C++ test logic into `run_unit_tests()`
2. Extract example compilation logic into `run_examples_tests()`
3. Extract Python test logic into `run_python_tests()`
4. Create `run_cpp_tests()` wrapper for C++ scenarios

### Phase 3: Main Function Restructuring
1. Replace existing `if args.cpp:` logic with category-based execution
2. Implement single-category execution paths
3. Maintain existing parallel execution for default behavior
4. Ensure proper cleanup and error handling

### Phase 4: Testing and Validation
1. Test all new flag combinations
2. Verify backward compatibility
3. Update documentation and help text
4. Validate performance characteristics

## Expected Flag Behaviors

| Command | Unit Tests | Examples | Python Tests | Notes |
|---------|-----------|----------|--------------|-------|
| `bash test` | ✓ | ✓ | ✓ | Default - all tests |
| `bash test --unit` | ✓ | ✗ | ✗ | C++ unit tests only |
| `bash test --examples` | ✗ | ✓ | ✗ | Example compilation only |
| `bash test --py` | ✗ | ✗ | ✓ | Python tests only |
| `bash test --cpp` | ✓ | ✓ | ✗ | **Modified behavior** |
| `bash test --unit --py` | ✓ | ✗ | ✓ | Combined flags |
| `bash test specific_test` | ✓ | ✗ | ✗ | Specific C++ test (auto-enables --unit) |

## Benefits

1. **Granular Control** - Developers can run specific test categories
2. **Faster Iteration** - Skip lengthy test categories when not needed  
3. **Clear Intent** - Explicit flags show exactly what's being tested
4. **CI Optimization** - Allow CI to run test categories in parallel
5. **Debugging** - Isolate test failures to specific categories
6. **Backward Compatible** - Existing workflows continue to work

## Additional Considerations

### Error Handling
- Validate that at least one test category is specified
- Provide clear error messages for invalid flag combinations
- Ensure proper exit codes for each test category

### Performance Impact
- Single-category execution should be faster than full test suite
- Maintain parallel execution for default behavior to preserve performance
- Consider adding timing information for each test category

### Future Enhancements
- Add `--quick` support for individual test categories
- Consider adding `--verbose` support for specific categories
- Potential for test result caching based on category
