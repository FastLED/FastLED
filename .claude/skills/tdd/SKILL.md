---
name: tdd
description: Guide Test-Driven Development workflow for FastLED. Use when implementing new features or fixing bugs with a test-first approach. Enforces Red-Green-Refactor discipline with FastLED test conventions.
argument-hint: <feature or bug description>
context: fork
agent: test-writer-agent
---

# TDD Workflow — Red-Green-Refactor for FastLED

You are guiding a Test-Driven Development cycle. Follow the strict Red-Green-Refactor discipline.

## Input

$ARGUMENTS

## Phase 1: RED — Write a Failing Test

### Steps
1. **Understand the requirement**: Read relevant source code to understand the API and behavior
2. **Find existing tests**: Search `tests/` for related test files — prefer extending them
3. **Write the test FIRST**: Create a minimal test that describes the expected behavior
4. **Run the test**: Execute `bash test TestName` to verify it **FAILS**
5. **Verify failure reason**: The test must fail because the feature is missing or the bug exists, NOT because of a compile error or typo

### Test Conventions
- Read `agents/tests.md` for full conventions
- Use `FL_` prefixed assertion macros (e.g., `FL_CHECK_EQ`, `FL_REQUIRE_TRUE`)
- Include `test.h` and `FastLED.h`
- Use `namespace fl;` and anonymous namespace wrapper
- Mirror source structure for file placement: `src/fl/foo.h` -> `tests/unit/fl/foo.cpp`
- Keep tests **simple** — no mocks, no helper classes, minimal setup

### Output
```
## RED Phase

**Test file**: tests/unit/fl/example.cpp
**Test case**: "Feature - expected behavior"
**Status**: FAILS as expected
**Failure reason**: [why it fails — confirms the test is valid]
```

## Phase 2: GREEN — Make It Pass

### Steps
1. **Write minimal code**: Implement just enough to make the test pass
2. **No premature optimization**: Simplest solution that works
3. **Run the test**: Execute `bash test TestName` to verify it **PASSES**
4. **Run full suite**: Execute `bash test --cpp` to verify no regressions

### Output
```
## GREEN Phase

**Implementation**: src/fl/example.h (lines X-Y)
**Changes**: [brief description of what was added/changed]
**Test status**: PASSES
**Full suite**: All tests pass (no regressions)
```

## Phase 3: REFACTOR — Clean Up

### Steps
1. **Review the code**: Look for duplication, unclear naming, unnecessary complexity
2. **Refactor incrementally**: Make one change at a time
3. **Run tests after each change**: `bash test TestName` must stay green
4. **Consider edge cases**: Add 1-2 more test cases if critical edge cases are obvious
5. **Run full suite**: `bash test --cpp` to confirm everything still passes

### Output
```
## REFACTOR Phase

**Changes made**: [list of refactoring improvements]
**Test status**: All tests still pass
**Edge cases added**: [any additional test cases, or "none needed"]
```

## Final Summary

After all three phases, provide:

```
## TDD Summary

**Feature**: [what was implemented]
**Test file**: [path]
**Source file(s)**: [paths]
**Tests added**: [count and names]
**All tests passing**: Yes/No
```

## Key Rules

- **NEVER write implementation code before the test**
- **NEVER skip the RED phase** — you must see the test fail first
- **Keep tests simple** — one focused test per behavior
- **Run tests at every phase transition** — this is non-negotiable
- **Stay in project root** — never `cd` to subdirectories
- **Use `bash test`** — never bare `python`, `meson`, or `ninja`
