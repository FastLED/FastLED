---
name: new-cpp-lint
description: Create a new C++ lint rule, test it, run it across the codebase, and selectively apply fixes. Usage - /new-cpp-lint <rule description>
argument-hint: <rule description, e.g. "no raw new/delete — use fl::unique_ptr" or "all functions must use FL_NOEXCEPT">
context: fork
agent: cpp-linter-builder-agent
---

# Create New C++ Lint Rule

You are creating a new C++ lint rule for the FastLED codebase. Follow this exact workflow.

## Input

$ARGUMENTS

## Phase 1: Design the Rule

1. **Parse the rule description** from the input
2. **Research the codebase**: Grep for existing patterns, violations, and edge cases
3. **Choose detection strategy** — **prefer the simplest approach that works**:
   - **Regex (Python)**: Use when the pattern is a **keyword, token, or textual pattern** that can be reliably detected with word-boundary regex. Examples: banning `noexcept` keyword, detecting `std::` namespace usage, finding `#pragma` directives, flagging raw `new`/`delete`. Most lint rules fall here. This is fast, simple, and easy to maintain.
   - **clang-query (AST)**: Use **only** when the rule requires **semantic understanding** that regex cannot provide — e.g., type-aware checks, matching function signatures, detecting inheritance patterns, or analyzing template instantiations. AST parsing is heavy and slow; don't use it when regex suffices.
4. **Define scope**: Which directories should be checked (src/fl/, platforms/, examples/, tests/)
5. **Identify exemptions**: Comments, macros, templates, platform-guarded code that should be allowed

Output:
```
## Rule Design

**Rule**: [one-line rule statement]
**Detection**: regex / clang-query
**Scope**: [directories]
**Exemptions**: [what should NOT be flagged]
**Suppression**: [comment pattern to suppress, e.g. "// nolint" or "// ok no X"]
```

## Phase 2: Write the Checker + Tests

1. **Read reference checkers**: Study 1-2 similar existing checkers in `ci/lint_cpp/`
2. **Create checker**: `ci/lint_cpp/{rule_name}_checker.py` inheriting from `FileContentChecker`
3. **Create tests**: `ci/lint_cpp/test_{rule_name}_checker.py` with:
   - Tests for violations (should flag)
   - Tests for correct code (should pass)
   - Tests for exemptions (comments, macros, suppression)
   - Tests for edge cases (multi-line, templates, nested scopes)
4. **Run tests**: `uv run python -m pytest ci/lint_cpp/test_{rule_name}_checker.py -v`

## Phase 3: Run Across Codebase (Dry Run)

1. **Run standalone**: `uv run python -m ci.lint_cpp.{rule_name}_checker` (if it has a `main()`)
2. **Count violations**: Report how many files/lines are affected
3. **Sample review**: Show 5-10 representative violations to verify correctness
4. **Check for false positives**: If any look wrong, refine the detection logic and re-run

Output:
```
## Dry Run Results

**Violations found**: [N] across [M] files
**Sample violations**:
- file.h:42: [violation text]
- file.cpp:100: [violation text]
**False positives**: [none / list of issues found and how they were fixed]
```

## Phase 4: Register in Lint Pipeline

1. **Add import** to `ci/lint_cpp/run_all_checkers.py`
2. **Add to scope** in `create_checkers()` — choose the right scope for the rule
3. **Verify integration**: Run `bash lint --cpp` — ensure it runs without breaking other checks
4. **If violations are expected**: Add suppression comments to known exceptions, or report them

## Phase 5: Apply Fixes (Selective)

**Only if the rule has a clear autofix pattern:**

1. **Create fixer script** (if needed) in `ci/tools/` for batch-applying fixes
2. **Apply to one file first**: Verify the fix is correct
3. **Run tests**: `bash test --cpp` after each batch of fixes
4. **Apply incrementally**: Fix one directory at a time, testing after each
5. **Do NOT auto-fix ambiguous cases** — report them for manual review

**If no autofix is appropriate:** Report the violation list and let the user decide.

## Phase 6: Summary

```
## New Lint Rule Created

**Rule**: [description]
**Checker**: ci/lint_cpp/{name}_checker.py
**Tests**: ci/lint_cpp/test_{name}_checker.py
**Scope**: [which scope in run_all_checkers.py]
**Detection**: [regex/clang-query]

**Violations**: [N] found, [M] fixed, [K] remaining
**Files modified**: [list]

**Suppression**: Use `// [suppression comment]` to suppress individual lines
```

## Key Rules

- **Test FIRST** — never register a checker without passing tests
- **Dry run FIRST** — never auto-fix without reviewing the violation list
- **Incremental fixes** — fix one directory at a time, test after each
- **Stay in project root** — never `cd` to subdirectories
- **Use `bash test --cpp`** and `bash lint --cpp` — never bare build commands
- **Store violations in `self.violations`** dict — never return them
- **Support suppression** — always allow a comment to suppress individual lines
- **Handle Windows paths** — normalize with `replace("\\", "/")`
