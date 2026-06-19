---
name: new-cpp-lint
description: Create a new C++ lint rule, test it, run it across the codebase, and selectively apply fixes. Usage - /new-cpp-lint <rule description>
argument-hint: <rule description, e.g. "no raw new/delete — use fl::unique_ptr" or "all functions must use FL_NOEXCEPT">
context: fork
agent: cpp-linter-builder-agent
---

# Create New C++ Lint Rule

You are creating a new C++ lint rule for the FastLED codebase. The default home for the new rule is the Rust crate `ci/lint_cpp_rs/`; the legacy Python tier (`ci/lint_cpp/`) is reserved for AST ratchets and cross-file structural checks. Follow this workflow.

## Input

$ARGUMENTS

## Phase 1: Design the Rule

1. **Parse the rule description** from the input
2. **Research the codebase**: Grep for existing patterns, violations, and edge cases
3. **Choose detection strategy** — **prefer the simplest approach that works**:
   - **Rust regex checker (default)**: Use when the pattern is a **keyword, token, or textual pattern** that can be reliably detected with word-boundary regex / per-line state. Examples: banning a keyword, detecting `std::` namespace usage, finding `#pragma` directives, flagging raw `new`/`delete`. The overwhelming majority of lint rules belong here. Fast, parallel, no libclang dependency.
   - **Python AST ratchet (libclang)**: Use **only** when the rule requires **semantic understanding** that regex cannot provide — e.g., type-aware checks, matching function signatures, detecting inheritance patterns, or analyzing template instantiations. AST parsing is heavy and slow; don't use it when a Rust regex checker suffices. See `ci/tools/check_noexcept.py` for the canonical pattern.
4. **Define scope**: Which directories should be checked (src/fl/, platforms/, examples/, tests/)
5. **Identify exemptions**: Comments, macros, templates, third_party/, platform-guarded code that should be allowed

Output:
```
## Rule Design

**Rule**: [one-line rule statement]
**Detection**: rust-regex / python-ast
**Scope**: [directories]
**Exemptions**: [what should NOT be flagged]
**Suppression**: [comment pattern to suppress, e.g. "// nolint" or "// ok no X"]
```

## Phase 2: Write the Checker + Tests

**Default (Rust regex) path:**

1. **Read reference checkers**: Study 1-2 similar existing checkers in `ci/lint_cpp_rs/src/checkers/` (see `ci/lint_cpp_rs/src/checkers/README.md` for the policy-area grouping)
2. **Add a checker struct** to the matching file under `ci/lint_cpp_rs/src/checkers/` (e.g. `basic.rs`, `style.rs`, `preprocessor.rs`). Implement `FileContentChecker` (trait in `ci/lint_cpp_rs/src/lint_core/processor_registry_cli.rs`):
   - `name()` returns the class-style name (e.g. `"YourRuleChecker"`)
   - `should_process_file(file_path, project_root)` filters by extension + scope
   - `check_file_content(file_content)` returns `Vec<(usize, String)>` of `(line_number, message)`
3. **Add inline Rust tests** to `ci/lint_cpp_rs/src/lint_core/tests.rs` with:
   - Tests for violations (should flag)
   - Tests for correct code (should pass)
   - Tests for exemptions (comments, macros, suppression marker)
   - Tests for edge cases (multi-line, templates, nested scopes)
4. **Pre-compile any regex** in `ci/lint_cpp_rs/src/lint_core/regexes.rs` — never `Regex::new` inside the hot loop
5. **Build + run the Rust tests**: `uv run python ci/lint_cpp/rust_binary_cache.py` (rebuilds the cached binary, which runs the inline tests during cargo build)

**Python AST ratchet path** (only when libclang semantics are required):

1. Add a new tool under `ci/tools/check_<rule>.py` following the pattern in `ci/tools/check_noexcept.py` (translation unit + clang-query + baseline diff)
2. Wire it into `ci/lint_cpp/run_all_checkers.py` next to `run_noexcept_ast_check` / `run_array_param_ast_check`
3. Add a checked-in baseline so the ratchet can only ratchet down

## Phase 3: Run Across Codebase (Dry Run)

1. **Run the Rust binary directly against the tree**:
   `uv run python ci/lint_cpp/rust_binary_cache.py` then `./ci/lint_cpp_rs/target/release/fastled-lint --checker your_rule`
   (or just `bash lint --cpp` and grep for your checker name)
2. **Count violations**: Report how many files/lines are affected
3. **Sample review**: Show 5-10 representative violations to verify correctness
4. **Check for false positives**: If any look wrong, refine `should_process_file` or the detection logic and re-run

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

Four edits, all small:

1. **`ci/lint_cpp_rs/src/lint_core/processor_registry_cli.rs`**:
   - Add the snake_case name to `supported_checker_names()`
   - Add the class-style name to `supported_python_checker_names()`
   - Add `("your_rule", Box::new(YourRuleChecker))` to the `checkers` vec in `create_checkers()`
2. **`ci/lint_cpp/rust_bridge.py`**:
   - Add `"YourRuleChecker"` to the `RUST_SUPPORTED_CHECKERS` frozenset
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
**Checker**: ci/lint_cpp_rs/src/checkers/<file>.rs::YourRuleChecker
**Tests**: ci/lint_cpp_rs/src/lint_core/tests.rs (#[test] fn your_rule_*)
**Registration**:
  - ci/lint_cpp_rs/src/lint_core/processor_registry_cli.rs (3 sites)
  - ci/lint_cpp/rust_bridge.py (RUST_SUPPORTED_CHECKERS)
**Detection**: [rust-regex/python-ast]

**Violations**: [N] found, [M] fixed, [K] remaining
**Files modified**: [list]

**Suppression**: Use `// [suppression comment]` to suppress individual lines
```

## Key Rules

- **Default tier is Rust** — only fall back to Python for AST ratchets or cross-file structural checks
- **Test FIRST** — never register a checker without passing inline `#[test]` functions
- **Dry run FIRST** — never auto-fix without reviewing the violation list
- **Incremental fixes** — fix one directory at a time, test after each
- **Stay in project root** — never `cd` to subdirectories
- **Use `bash test --cpp`** and `bash lint --cpp` — never bare `cargo`, `meson`, or build commands
- **Return violations from `check_file_content`** as `Vec<(usize, String)>` — no shared mutable state, dispatch is `rayon`-parallel
- **Support suppression** — always allow a comment marker (e.g. `// nolint`) to suppress individual lines
- **Handle Windows paths** — call `normalize_path()` before path comparisons
