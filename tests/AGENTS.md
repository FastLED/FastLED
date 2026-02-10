# FastLED Testing Agent Guidelines

## 📚 Main Documentation

**For comprehensive guidelines, read these files:**

| Topic | Read |
|-------|------|
| Test commands and execution | `docs/agents/testing-commands.md` |
| Build system restrictions | `docs/agents/build-system.md` |
| C++ coding standards | `docs/agents/cpp-standards.md` |
| MCP server tools | `docs/agents/mcp-tools.md` |
| Debugging strategies | `docs/agents/debugging.md` |
| LLDB debugging guide | `docs/agents/lldb-debugging.md` |

**This file contains only test-directory-specific conventions.**

---

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

---

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

---

## Test Macro Naming Standards

**🚨 CRITICAL: Always use FL_ prefixed trampolines instead of direct doctest macros**

### Overview
All test files must use `FL_CHECK` and `FL_REQUIRE` macro trampolines instead of calling doctest macros directly. This provides a consistent abstraction layer between test code and the underlying test framework (doctest).

### Trampoline Pattern
The FastLED test suite uses a **trampoline layer** defined in `tests/test.h` that wraps doctest macros:

```cpp
// tests/test.h defines trampolines
#define FL_CHECK(...)           CHECK(__VA_ARGS__)
#define FL_CHECK_EQ(a, b)       CHECK_EQ(a, b)
#define FL_REQUIRE(...)         REQUIRE(__VA_ARGS__)
// ... and 30+ more variants
```

### Usage Examples

**✅ CORRECT - Use FL_ prefixed trampolines:**
```cpp
#include "test.h"  // Includes trampolines, not doctest.h directly

TEST_CASE("Example test") {
    FL_CHECK_EQ(expected, actual);
    FL_REQUIRE_LT(index, max_size);
    FL_CHECK_TRUE(condition);
    FL_CHECK_FALSE(error_flag);
}
```

**❌ INCORRECT - Do NOT use doctest macros directly:**
```cpp
#include "doctest.h"  // Wrong - use test.h instead

TEST_CASE("Example test") {
    CHECK_EQ(expected, actual);      // Missing FL_ prefix
    REQUIRE_LT(index, max_size);     // Missing FL_ prefix
    CHECK(condition == true);        // Missing FL_ prefix
}
```

### Complete Trampoline List

All 35+ trampolines follow the pattern: `FL_<DOCTEST_MACRO_NAME>`

**Basic Assertions:**
- `FL_CHECK(...)` - Basic check
- `FL_REQUIRE(...)` - Required check (stops test on failure)

**Equality Comparisons:**
- `FL_CHECK_EQ(a, b)` / `FL_REQUIRE_EQ(a, b)` - Equal
- `FL_CHECK_NE(a, b)` / `FL_REQUIRE_NE(a, b)` - Not equal

**Relational Comparisons:**
- `FL_CHECK_LT(a, b)` / `FL_REQUIRE_LT(a, b)` - Less than
- `FL_CHECK_LE(a, b)` / `FL_REQUIRE_LE(a, b)` - Less than or equal
- `FL_CHECK_GT(a, b)` / `FL_REQUIRE_GT(a, b)` - Greater than
- `FL_CHECK_GE(a, b)` / `FL_REQUIRE_GE(a, b)` - Greater than or equal

**Boolean Assertions:**
- `FL_CHECK_TRUE(x)` / `FL_REQUIRE_TRUE(x)` - Must be true
- `FL_CHECK_FALSE(x)` / `FL_REQUIRE_FALSE(x)` - Must be false

**Floating Point:**
- `FL_CHECK_DOUBLE_EQ(a, b)` / `FL_REQUIRE_DOUBLE_EQ(a, b)` - Double equality
- `FL_CHECK_DOUBLE_NE(a, b)` / `FL_REQUIRE_DOUBLE_NE(a, b)` - Double inequality

**String Comparisons:**
- `FL_CHECK_STREQ(a, b)` / `FL_REQUIRE_STREQ(a, b)` - String equal
- `FL_CHECK_STRNE(a, b)` / `FL_REQUIRE_STRNE(a, b)` - String not equal

**Type Traits:**
- `FL_CHECK_TRAIT(expr)` / `FL_REQUIRE_TRAIT(expr)` - Type trait validation

**Unary Checks:**
- `FL_CHECK_UNARY(expr)` / `FL_REQUIRE_UNARY(expr)` - Unary expression
- `FL_CHECK_UNARY_FALSE(expr)` / `FL_REQUIRE_UNARY_FALSE(expr)` - Unary false

**Throwing Assertions:**
- `FL_CHECK_THROWS(...)` / `FL_REQUIRE_THROWS(...)` - Must throw any exception
- `FL_CHECK_THROWS_AS(expr, type)` / `FL_REQUIRE_THROWS_AS(expr, type)` - Must throw specific type
- `FL_CHECK_THROWS_WITH(expr, str)` / `FL_REQUIRE_THROWS_WITH(expr, str)` - Must throw with message
- `FL_CHECK_THROWS_WITH_AS(expr, str, type)` / `FL_REQUIRE_THROWS_WITH_AS(expr, str, type)` - Combined
- `FL_CHECK_NOTHROW(...)` / `FL_REQUIRE_NOTHROW(...)` - Must not throw

### Exceptions (DO NOT Convert)

**These macros should NOT use FL_ prefix:**
- `CHECK_CLOSE` / `REQUIRE_CLOSE` - Custom floating point comparison macros (not doctest)
- `TEST_CASE` / `SUBCASE` / `TEST_SUITE` - Test structure macros (remain unchanged)
- `DOCTEST_CONFIG_*` - Configuration macros (remain unchanged)

### Template Argument Wrapping

**🚨 CRITICAL: Wrap template expressions with commas in parentheses**

When passing template expressions containing commas to FL_ macros, the preprocessor treats commas as argument separators. Wrap the entire expression in parentheses:

**❌ PROBLEM - Preprocessor error:**
```cpp
// ERROR: Preprocessor sees 3 arguments instead of 2
FL_CHECK_EQ(int_scale<T1, T2>(arg), expected)
//                     ^^^ comma treated as macro argument separator
```

**✅ SOLUTION - Wrap in parentheses:**
```cpp
// Correct: Parentheses protect the comma from preprocessor
FL_CHECK_EQ((int_scale<T1, T2>(arg)), expected)
//          ^                      ^
//          wrap template expression
```

**More examples:**
```cpp
// Multiple template arguments
FL_CHECK_TRUE((std::is_same<A, B>::value))

// Template function calls
FL_CHECK_EQ((convert<uint8_t, uint16_t>(input)), output)

// Nested templates
FL_REQUIRE_LT((map_range<int, int, long>(x, 0, 100, 0, 1000)), max_value)
```

**When wrapping is needed:**
- Template instantiations: `func<T1, T2>(...)`
- Template type traits: `std::is_same<A, B>::value`
- Template member access: `Container<K, V>::size()`
- Any expression containing commas that aren't function argument separators

**When wrapping is NOT needed:**
- Function calls with multiple arguments: `FL_CHECK_EQ(func(a, b, c), expected)` ✅ (commas are function args)
- Initializer lists: `FL_CHECK_EQ(vec, {1, 2, 3})` ✅ (braces protect commas)

---

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

---

## Test Simplicity Principle

**🚨 CRITICAL: KEEP TESTS AS SIMPLE AS POSSIBLE**

When writing or updating tests in `tests/*`, prioritize **absolute simplicity** over comprehensive coverage:

### SIMPLICITY RULES
- ✅ **MINIMAL:** Test only the critical functionality - one focused test is better than many complex ones
- ✅ **DIRECT:** Avoid mocks, helpers, and abstraction layers unless absolutely necessary
- ✅ **READABLE:** Code should be immediately understandable without documentation
- ✅ **CONCISE:** Shorter tests with clear intent are better than exhaustive test suites

### AVOID OVER-ENGINEERING
- ❌ **Mock frameworks** - Use real objects/values whenever possible
- ❌ **Helper classes** - Write inline test code instead of creating test infrastructure
- ❌ **Excessive test cases** - One well-designed test > ten redundant variations
- ❌ **Complex setup/teardown** - Keep test state simple and explicit

### EXAMPLE: Good vs Bad Test Design

**❌ BAD - Over-engineered with mocks and helpers:**
```cpp
class MockTime {
    static uint32_t value;
    static void set(uint32_t v) { value = v; }
    static void advance(uint32_t d) { value += d; }
    static uint32_t get() { return value; }
};

TEST_CASE("Timeout - comprehensive") {
    SUBCASE("basic") { /* ... */ }
    SUBCASE("rollover case 1") { /* ... */ }
    SUBCASE("rollover case 2") { /* ... */ }
    SUBCASE("edge case 1") { /* ... */ }
    // ... 20 more subcases
}
```

**✅ GOOD - Simple, direct, focused:**
```cpp
TEST_CASE("Timeout - rollover test") {
    // Test critical rollover: starts before 0xFFFFFFFF, ends after 0x00000000
    uint32_t start = 0xFFFFFF00;
    Timeout timeout(start, 512);

    CHECK_FALSE(timeout.done(start));
    CHECK(timeout.done(start + 512));  // Works across rollover
}
```

**Why the good example is better:**
- No mock infrastructure needed
- Tests the critical behavior (rollover) directly
- Obvious what's being tested from reading the code
- Easy to modify and maintain
- Compiles faster

### REFACTORING EXISTING TESTS
When updating existing tests that violate these principles:
1. **Identify the critical behavior** being tested
2. **Remove mock/helper infrastructure** if it can be replaced with direct values
3. **Consolidate** multiple test cases into one focused test
4. **Simplify** assertions and test logic

**Remember:** A simple test that catches real bugs is infinitely more valuable than a complex test suite that's hard to maintain.

---

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

---

## Test Namespace Convention

**🚨 CRITICAL: Anonymous namespaces in test files should match the test name:**

For test files in `tests/**/*`, use an **anonymous namespace** named after the test:

```cpp
// File: tests/fl/chipsets/test_ucs7604.cpp

#include "test.h"
#include "FastLED.h"

using namespace fl;

namespace {  // Anonymous namespace for test_ucs7604

// Helper functions and test fixtures specific to this test
class MockController { /*...*/ };
void helperFunction() { /*...*/ }

TEST_CASE("UCS7604 - feature test") {
    // Test implementation
}

}  // anonymous namespace
```

**Why:**
- **Prevents symbol conflicts** between tests
- **Scopes test helpers** to avoid polluting global namespace
- **Improves readability** - clear organization of test-specific code
- **Follows C++ best practices** for internal linkage

**Rules:**
- ✅ Use anonymous namespace `namespace { ... }` for all test helpers and fixtures
- ✅ Close namespace with descriptive comment: `} // anonymous namespace`
- ✅ Place `using namespace fl;` **before** the anonymous namespace
- ❌ Don't create named namespaces in test files (e.g., `namespace test_ucs7604 { ... }`)
- ❌ Don't leave test helpers in global scope

---

## Memory Refresh Rule
**🚨 ALL AGENTS: Read docs/agents/testing-commands.md and relevant docs/agents/*.md files before concluding testing work to refresh memory about test execution requirements and validation rules.**
