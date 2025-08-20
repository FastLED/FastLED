# FastLED Testing Agent Guidelines

## 🚨 CRITICAL REQUIREMENTS FOR BACKGROUND AGENTS

### MANDATORY PRE-COMPLETION VALIDATION
**ALL BACKGROUND AGENTS MUST FOLLOW THESE REQUIREMENTS BEFORE INDICATING COMPLETION:**

1. **🚨 ALWAYS RUN `bash test` BEFORE INDICATING COMPLETION**
   - This is MANDATORY and NON-NEGOTIABLE for all background agents
   - The command `bash test` runs the full test suite including unit tests and compilation checks
   - Background agents MUST NOT indicate they are "done" until ALL tests pass

2. **🚨 USE MCP SERVER VALIDATION TOOL**
   - Use the `validate_completion` tool from the MCP server: `uv run mcp_server.py`
   - This tool runs `bash test` and validates that all tests pass
   - Only indicate completion if this validation tool returns success

3. **🚨 ZERO TOLERANCE FOR TEST FAILURES**
   - If ANY test fails, the background agent MUST fix the issues before completion
   - Do NOT indicate completion with failing tests
   - Do NOT ignore test errors or warnings

### Background Agent Completion Checklist:
- [ ] All code changes have been made
- [ ] `bash test` has been run and ALL tests pass
- [ ] No compilation errors or warnings
- [ ] MCP server `validate_completion` tool shows success
- [ ] Only then indicate task completion

**FAILURE TO FOLLOW THESE REQUIREMENTS WILL RESULT IN BROKEN CODE SUBMISSIONS.**

## Test System Overview

The project uses a comprehensive test suite including:
- C++ unit tests
- Platform compilation tests  
- Code quality checks (ruff, clang-format)
- Example compilation verification

## Test Execution Format

**🚨 CRITICAL: Always use the correct test execution format:**
- ✅ **CORRECT**: `bash test <test_name>` (e.g., `bash test audio_json_parsing`)
- ❌ **INCORRECT**: `./.build/bin/test_<test_name>.exe`
- ❌ **INCORRECT**: Running executables directly

**Examples:**
- `bash test` - Run all tests (includes debug symbols)
- `bash test audio_json_parsing` - Run specific test
- `bash test xypath` - Run test_xypath.cpp
- `bash compile uno --examples Blink` - Compile examples

**Quick Build Options:**
- `bash test --quick --cpp` - Quick C++ tests only (when no *.py changes)
- `bash test --quick` - Quick tests including Python (when *.py changes)

**Why:** The `bash test` wrapper handles platform differences, environment setup, and proper test execution across all supported systems.

## Test Assertion Macros

**🚨 CRITICAL: Always use the proper assertion macros for better error messages and debugging:**

### Equality Assertions
- ✅ **CORRECT**: `CHECK_EQ(A, B)` - for equality comparisons
- ❌ **INCORRECT**: `CHECK(A == B)` - provides poor error messages

### Inequality Assertions
- ✅ **CORRECT**: `CHECK_LT(A, B)` - for less than comparisons
- ✅ **CORRECT**: `CHECK_LE(A, B)` - for less than or equal comparisons
- ✅ **CORRECT**: `CHECK_GT(A, B)` - for greater than comparisons
- ✅ **CORRECT**: `CHECK_GE(A, B)` - for greater than or equal comparisons
- ❌ **INCORRECT**: `CHECK(A < B)`, `CHECK(A <= B)`, `CHECK(A > B)`, `CHECK(A >= B)`

### Boolean Assertions
- ✅ **CORRECT**: `CHECK_TRUE(condition)` - for true conditions
- ✅ **CORRECT**: `CHECK_FALSE(condition)` - for false conditions
- ❌ **INCORRECT**: `CHECK(condition)` - for boolean checks

### String Assertions
- ✅ **CORRECT**: `CHECK_STREQ(str1, str2)` - for string equality
- ✅ **CORRECT**: `CHECK_STRNE(str1, str2)` - for string inequality
- ❌ **INCORRECT**: `CHECK(str1 == str2)` - for string comparisons

### Floating Point Assertions
- ✅ **CORRECT**: `CHECK_DOUBLE_EQ(a, b)` - for floating point equality
- ✅ **CORRECT**: `CHECK_DOUBLE_NE(a, b)` - for floating point inequality
- ❌ **INCORRECT**: `CHECK(a == b)` - for floating point comparisons

### Examples
```cpp
// Good assertion usage
CHECK_EQ(expected_value, actual_value);
CHECK_LT(current_index, max_index);
CHECK_GT(temperature, 0.0);
CHECK_TRUE(is_initialized);
CHECK_FALSE(has_error);
CHECK_STREQ("expected", actual_string);
CHECK_DOUBLE_EQ(3.14159, pi_value, 0.001);

// Bad assertion usage
CHECK(expected_value == actual_value);  // Poor error messages
CHECK(current_index < max_index);       // Poor error messages
CHECK(is_initialized);                  // Unclear intent
CHECK("expected" == actual_string);     // Wrong comparison type
```

**Why:** Using the proper assertion macros provides:
- **Better error messages** with actual vs expected values
- **Clearer intent** about what is being tested
- **Consistent debugging** across all test failures
- **Type safety** for different comparison types

## Test File Creation Guidelines

**🚨 CRITICAL: Minimize test file proliferation - Consolidate tests whenever possible**

### PREFERRED APPROACH
- ✅ **CONSOLIDATE:** Add new test cases to existing related test files
- ✅ **EXTEND:** Expand existing `TEST_CASE()` blocks with additional scenarios
- ✅ **REUSE:** Leverage existing test infrastructure and helper functions
- ✅ **COMPREHENSIVE:** Create single comprehensive test files that cover entire feature areas

### CREATE NEW TEST FILES ONLY WHEN
- ✅ **Testing completely new subsystems** with no existing related tests
- ✅ **Isolated feature areas** that don't fit logically into existing test structure
- ✅ **Complex integration tests** that require dedicated setup/teardown

### AVOID
- ❌ **Creating new test files for minor bug fixes** - add to existing tests
- ❌ **One test case per file** - consolidate related functionality
- ❌ **Duplicate test patterns** across multiple files
- ❌ **Scattered feature testing** - keep related tests together

### DEVELOPMENT WORKFLOW
1. **During Development/Bug Fixing:** Temporary test files are acceptable for rapid iteration
2. **Near Completion:** **MANDATORY** - Consolidate temporary tests into existing files
3. **Final Review:** Remove temporary test files and ensure comprehensive coverage in main test files

### CONSOLIDATION CHECKLIST
- [ ] Can this test be added to an existing `TEST_CASE` in the same file?
- [ ] Does an existing test file cover the same functional area?
- [ ] Would this test fit better as a sub-section of a comprehensive test?
- [ ] Are there duplicate test patterns that can be eliminated?

**Why:** Maintaining a clean, consolidated test suite:
- **Easier maintenance** - fewer files to manage and update
- **Better organization** - related functionality tested together
- **Faster builds** - fewer compilation units
- **Cleaner repository** - less file clutter
- **Improved discoverability** - easier to find existing test coverage

## Blank Test Template

**✅ CORRECT blank test file structure:**

```cpp
// Unit tests for general allocator functionality and integration tests

#include "test.h"
#include "FastLED.h"

using namespace fl;


TEST_CASE("New test - fill in") {

} 
```

**❌ WRONG patterns to avoid:**
- Missing descriptive file header comment
- Missing proper includes (`test.h`, `FastLED.h`)
- Missing `using namespace fl;` declaration
- Empty or non-descriptive test case names
- Inconsistent formatting and spacing

## Debugging and Stack Traces

### Stack Trace Setup
The FastLED project supports enhanced debugging through stack trace functionality for crash analysis and debugging.

**For Background Agents**: Use the MCP server tool `setup_stack_traces` to automatically install and configure stack trace support:

```bash
# Via MCP server (recommended for background agents)
uv run mcp_server.py
# Then use setup_stack_traces tool with method: "auto"
```

**Manual Installation**:

**Ubuntu/Debian**:
```bash
sudo apt-get update
sudo apt-get install -y libunwind-dev build-essential cmake
```

**CentOS/RHEL/Fedora**:
```bash
sudo yum install -y libunwind-devel gcc-c++ cmake  # CentOS/RHEL  
sudo dnf install -y libunwind-devel gcc-c++ cmake  # Fedora
```

**macOS**:
```bash
brew install libunwind
```

### Available Stack Trace Methods
1. **LibUnwind** (Recommended) - Enhanced stack traces with symbol resolution
2. **Execinfo** (Fallback) - Basic stack traces using standard glibc
3. **Windows** (On Windows) - Windows-specific debugging APIs
4. **No-op** (Last resort) - Minimal crash handling

The build system automatically detects and configures the best available option.

### Testing Stack Traces
```bash
cd tests
cmake . && make crash_test_standalone crash_test_execinfo

# Test libunwind version
./.build/bin/crash_test_standalone manual   # Manual stack trace
./.build/bin/crash_test_standalone nullptr  # Crash test

# Test execinfo version  
./.build/bin/crash_test_execinfo manual     # Manual stack trace
./.build/bin/crash_test_execinfo nullptr    # Crash test
```

### Using in Code
```cpp
#include "tests/crash_handler.h"

int main() {
    setup_crash_handler();  // Enable crash handling
    // Your code here...
    return 0;
}
```

## Testing Infrastructure

**Test Configuration**: `tests/cmake/TestConfiguration.cmake`
- Defines test targets and dependencies
- Configures test execution parameters
- Sets up coverage and profiling

**Test Execution**:
- `bash test` - Comprehensive test runner (preferred)
- `uv run test.py` - Python test interface
- Individual test executables in `.build/bin/`

## Python Build System Commands for Testing

**Unit Test Compilation:**
```bash
# Fast Python API (default)
bash test --unit --verbose  # Uses PCH optimization

# Disable PCH if needed  
bash test --unit --no-pch --verbose

# Legacy CMake system (8x slower)
bash test --unit --legacy --verbose  
```

**Available Build Options:**
- `--no-pch` - Disable precompiled headers 
- `--clang` - Use Clang compiler (recommended for speed)
- `--clean` - Force full rebuild
- `--verbose` - Show detailed compilation output
- `--legacy` - Use old CMake system (discouraged)

## Memory Refresh Rule
**🚨 ALL AGENTS: Read tests/AGENTS.md before concluding testing work to refresh memory about current test execution requirements and validation rules.**