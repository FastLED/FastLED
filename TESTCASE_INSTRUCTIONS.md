# FastLED TEST_CASE Execution Guide

## Overview

FastLED uses the **doctest** framework for C++ unit testing. Tests are organized in files named `test_*.cpp` in the `tests/` directory. Each file can contain multiple `TEST_CASE` macros that define individual test functions.

## Test Structure

- **Test Files**: Located in `tests/test_*.cpp` (e.g., `test_algorithm.cpp`, `test_easing.cpp`)
- **TEST_CASEs**: Individual test functions defined with `TEST_CASE("name")` macro
- **SUBCASEs**: Nested test sections within TEST_CASEs using `SUBCASE("name")`
- **Assertions**: `CHECK()`, `CHECK_EQ()`, `CHECK_LT()`, etc. for validation

## Quick Start

### Using the MCP Server (Recommended)

1. **Start the MCP server:**
   ```bash
   uv run mcp_server.py
   ```

2. **Available MCP tools:**
   - `run_tests`: Run tests with various options
   - `list_test_cases`: List available TEST_CASEs  
   - `test_instructions`: Show detailed instructions

### Command Line Usage

1. **Run all tests:**
   ```bash
   uv run test.py
   # or using the user rule:
   bash test
   ```

2. **Run only C++ tests:**
   ```bash
   uv run test.py --cpp
   ```

3. **Run specific test file:**
   ```bash
   uv run test.py --cpp algorithm
   # This runs test_algorithm.cpp
   ```

## Running Specific TEST_CASEs

### Method 1: Direct Executable (Most Precise)

First, compile the tests, then run the specific executable with doctest filtering:

```bash
# 1. Compile the test (if not already compiled)
uv run test.py --cpp algorithm

# 2. Run specific TEST_CASE
./tests/.build/bin/test_algorithm --test-case="reverse an int list"

# 3. Run with verbose output
./tests/.build/bin/test_algorithm --test-case="reverse an int list" --verbose
```

### Method 2: Using MCP Server

Use the `run_tests` tool with specific parameters:
- `test_type`: "specific"
- `specific_test`: "algorithm" (without test_ prefix)
- `test_case`: "reverse an int list"
- `verbose`: true (optional)

### Method 3: List Available TEST_CASEs First

```bash
# Using MCP list_test_cases tool, or manually:
grep -r "TEST_CASE" tests/test_algorithm.cpp
```

## Common Test Files and Their Purpose

| File | Purpose | Example TEST_CASEs |
|------|---------|-------------------|
| `test_algorithm.cpp` | Algorithm utilities | "reverse an int list" |
| `test_easing.cpp` | Easing functions | "8-bit easing functions", "easeInOutQuad16" |
| `test_hsv16.cpp` | HSV color space | "HSV to RGB conversion", "HSV hue wraparound" |
| `test_math.cpp` | Mathematical functions | "sin32 accuracy", "random number generation" |
| `test_vector.cpp` | Vector container | "vector push_back", "vector iteration" |
| `test_fx.cpp` | Effects framework | "fx engine initialization" |
| `test_apa102_hd.cpp` | APA102 HD driver | "APA102 color mapping" |
| `test_bitset.cpp` | Bitset operations | "bitset operations" |

## Example Workflows

### Debug a Failing Test

1. **List TEST_CASEs in a specific file:**
   ```bash
   # Using MCP server list_test_cases tool with test_file: "easing"
   ```

2. **Run with verbose output:**
   ```bash
   uv run test.py --cpp easing --verbose
   ```

3. **Run specific failing TEST_CASE:**
   ```bash
   ./tests/.build/bin/test_easing --test-case="8-bit easing functions" --verbose
   ```

### Test Development Workflow

1. **Clean build and run:**
   ```bash
   uv run test.py --cpp mytest --clean --verbose
   ```

2. **Run only failed tests:**
   ```bash
   cd tests/.build && ctest --rerun-failed
   ```

3. **Test with different compilers:**
   ```bash
   uv run test.py --cpp mytest --clang
   ```

### Finding Specific Tests

1. **Search for TEST_CASEs by pattern:**
   ```bash
   # Using MCP list_test_cases tool with search_pattern: "easing"
   ```

2. **Manual search:**
   ```bash
   grep -r "TEST_CASE.*easing" tests/
   ```

## Doctest Command Line Options

When running test executables directly, you can use these doctest options:

- `--test-case=<name>`: Run specific TEST_CASE
- `--test-case-exclude=<name>`: Exclude specific TEST_CASE  
- `--subcase=<name>`: Run specific SUBCASE
- `--subcase-exclude=<name>`: Exclude specific SUBCASE
- `--list-test-cases`: List all TEST_CASEs in the executable
- `--list-test-suites`: List all test suites
- `--verbose`: Show detailed output including successful assertions
- `--success`: Show successful assertions too
- `--no-colors`: Disable colored output
- `--force-colors`: Force colored output
- `--no-breaks`: Disable breaking into debugger
- `--no-skip`: Don't skip any tests

### Examples:

```bash
# List all TEST_CASEs in a test file
./tests/.build/bin/test_easing --list-test-cases

# Run multiple TEST_CASEs with pattern
./tests/.build/bin/test_easing --test-case="*quad*"

# Exclude specific TEST_CASEs
./tests/.build/bin/test_easing --test-case-exclude="*16-bit*"

# Run with full verbose output
./tests/.build/bin/test_easing --success --verbose
```

## Understanding Test Output

### Successful Test:
```
[doctest] run with "--help" for options
===============================================================================
TEST_CASE:  reverse an int list
===============================================================================
tests/test_algorithm.cpp:12
all tests passed!
===============================================================================
[doctest] test cases:      1 |      1 passed |      0 failed |      0 skipped
[doctest] assertions:      5 |      5 passed |      0 failed |
[doctest] Status: SUCCESS!
```

### Failed Test:
```
===============================================================================
TEST_CASE:  reverse an int list
===============================================================================
tests/test_algorithm.cpp:12
tests/test_algorithm.cpp:20: ERROR: CHECK_EQ(vec[0], 6) is NOT correct!
  values: CHECK_EQ(5, 6)

===============================================================================
[doctest] test cases:      1 |      0 passed |      1 failed |      0 skipped
[doctest] assertions:      5 |      4 passed |      1 failed |
[doctest] Status: FAILURE!
```

## Tips and Best Practices

1. **Use `--verbose`** to see detailed test output and assertions
2. **Use `--clean`** when testing after significant code changes
3. **List TEST_CASEs first** to see what's available before running specific tests
4. **Individual TEST_CASE execution** is useful for debugging specific functionality
5. **Check test output carefully** - doctest provides detailed failure information with line numbers
6. **Use pattern matching** with `--test-case="*pattern*"` to run related tests
7. **Combine with debugger** - tests are compiled with debug symbols (`-g3`)

## Environment Setup

- Tests are compiled with Debug flags (`-g3`, `-O0`) for better debugging
- GDB integration available for crash analysis
- Static analysis warnings enabled as errors
- Cross-platform support (Linux, macOS, Windows)
- Uses zig/clang as the default compiler for consistency

## Troubleshooting

### Common Issues:

1. **Test executable not found:**
   ```bash
   # Make sure tests are compiled first
   uv run test.py --cpp <test_name>
   ```

2. **TEST_CASE name not found:**
   ```bash
   # List available TEST_CASEs
   ./tests/.build/bin/test_<name> --list-test-cases
   ```

3. **Compilation errors:**
   ```bash
   # Clean build
   uv run test.py --cpp <test_name> --clean
   ```

4. **Permission denied on executable:**
   ```bash
   chmod +x tests/.build/bin/test_<name>
   ```

## Integration with Development

### Git Hooks
Consider adding a pre-commit hook to run relevant tests:
```bash
#!/bin/bash
# Run tests for changed files
uv run test.py --cpp
```

### IDE Integration
Most IDEs can be configured to run individual TEST_CASEs directly from the editor by configuring the test executable path and doctest arguments.