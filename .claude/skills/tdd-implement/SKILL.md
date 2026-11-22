---
name: tdd-implement
description: Implement a feature or fix a bug using strict TDD. Analyzes the requirement, creates tests first, implements minimal code, then refactors. Use for feature requests, bug fixes, or enhancements that need test coverage.
argument-hint: <feature request, bug description, or issue reference>
context: fork
agent: test-writer-agent
---

# TDD Implementation — Feature-Driven Test-First Development

You are implementing a feature or fixing a bug using strict Test-Driven Development. This is a more structured workflow than `/tdd` — you analyze the requirement, break it into testable behaviors, and implement each one through a full Red-Green-Refactor cycle.

## Input

$ARGUMENTS

## Step 1: Analyze the Requirement

### Actions
1. **Read the requirement** carefully — identify what behavior needs to change or be added
2. **Find relevant source code**: Use Grep/Glob to locate the files that need to change
3. **Read existing tests**: Find any existing test coverage for the affected code
4. **Identify testable behaviors**: Break the requirement into 1-5 discrete, testable units

### Output
```
## Requirement Analysis

**Requirement**: [one-line summary]
**Source files**: [list of files that will be modified]
**Existing tests**: [list of related test files, or "none found"]
**Testable behaviors**:
1. [behavior 1 — what it does, how to verify]
2. [behavior 2 — what it does, how to verify]
3. [behavior 3 — if needed]
```

## Step 2: Implement Each Behavior via TDD

For EACH testable behavior, execute a full Red-Green-Refactor cycle:

### RED: Write Failing Test
1. Write a minimal test for this specific behavior
2. Follow all conventions from `agents/tests.md`:
   - `FL_` prefixed macros (`FL_CHECK_EQ`, `FL_REQUIRE_TRUE`, etc.)
   - Include `test.h` and `FastLED.h`
   - `using namespace fl;` + anonymous namespace
   - Mirror source path for test file placement
3. Run `bash test TestName` — verify it FAILS for the right reason

### GREEN: Minimal Implementation
1. Write the minimum code to make this test pass
2. Do NOT implement other behaviors yet
3. Run `bash test TestName` — verify it PASSES
4. Run `bash test --cpp` — verify no regressions

### REFACTOR: Clean Up
1. Improve code quality without changing behavior
2. Run tests after each change to verify they stay green

### Report Each Cycle
```
### Behavior N: [description]
- RED: Test written at tests/unit/fl/foo.cpp — FAILS (expected: [reason])
- GREEN: Implemented in src/fl/foo.h — PASSES
- REFACTOR: [changes made, or "clean as-is"]
```

## Step 3: Integration Verification

After all behaviors are implemented:

1. **Run full test suite**: `bash test --cpp`
2. **Check for regressions**: Verify ALL tests pass, not just new ones
3. **Review changes holistically**: Ensure the implementation is cohesive
4. **Run code review**: Check changes against FastLED coding standards

### Output
```
## Integration Verification

**Full test suite**: [X/X] tests pass
**New tests added**: [count]
**Source files modified**: [list]
**Regressions**: None / [details if any]
```

## Step 4: Final Summary

```
## TDD Implementation Complete

**Requirement**: [what was implemented]
**Approach**: [brief description of the solution]

### Files Changed
| File | Change Type | Description |
|------|-------------|-------------|
| tests/unit/fl/foo.cpp | Added | 3 test cases for [feature] |
| src/fl/foo.h | Modified | Added [function/method] |

### Test Coverage
- [Test case 1]: [what it verifies]
- [Test case 2]: [what it verifies]
- [Test case 3]: [what it verifies]

### All Tests Passing: Yes
```

## Key Rules

- **Test FIRST, implement SECOND** — this order is absolute
- **One behavior per cycle** — don't batch multiple behaviors
- **Minimal implementation** — write the simplest code that passes
- **Run tests at EVERY transition** — RED->GREEN->REFACTOR each verified
- **Stay in project root** — never `cd` to subdirectories
- **Use `bash test`** wrapper — never bare `python`, `meson`, or `ninja`
- **Extend existing test files** — don't create new ones unless necessary
- **No mocks** — use real objects and values
