# C++ Linter Architecture (Rust crate + Python holdover set)

**IMPORTANT: New C++ lint rules MUST be authored in the Rust crate (`ci/lint_cpp_rs/`). Adding new Python checkers to `ci/lint_cpp/` is no longer allowed except for the narrow cases listed in "When to still use Python" below.**

## Two-Tier Architecture

The C++ linter is split between two cooperating tiers:

1. **Rust crate `ci/lint_cpp_rs/`** — the default home for all new content-based / regex / line-state checkers. Ships as the `fastled-lint` binary. Files are read once and dispatched in parallel via `rayon` (`MultiCheckerFileProcessor::process_files_with_checkers` in `ci/lint_cpp_rs/src/lint_core/processor_registry_cli.rs`). ~60 checkers run in well under a second across the full src/, examples/, tests/ tree.
2. **Python orchestrator `ci/lint_cpp/run_all_checkers.py`** — owns the small remaining set of Python-only checkers (see "When to still use Python"), invokes the Rust binary via `ci/lint_cpp/rust_bridge.py`, and merges results. Also hosts the AST ratchets and cross-file structural checks that cannot be expressed as single-file regex passes.

The Python orchestrator is still the entrypoint `bash lint --cpp` calls, but the bulk of the work happens in Rust.

## How a Run Flows

1. `bash lint --cpp` calls `uv run python -m ci.lint_cpp.run_all_checkers`.
2. The orchestrator calls `ci.lint_cpp.rust_binary_cache.ensure_rust_lint_binary()`, which rebuilds the `fastled-lint` binary only when crate sources changed (otherwise reuses the cached build).
3. The orchestrator calls `run_rust_linter()` (in `rust_bridge.py`) to run the Rust binary in a single subprocess, parses its JSON output, and translates each `LintViolation` into the Python `CheckerResults` shape.
4. The orchestrator strips Rust-handled checker classes from the Python `checkers_by_scope` table (`remove_rust_supported_checkers()` keyed on `RUST_SUPPORTED_CHECKERS`) so they don't run twice.
5. The remaining Python-only checkers run, their results are merged with the Rust results, AST ratchets and cross-file passes are run last, and a single combined report is printed.

## Single-File Mode

- **Usage**: `uv run python ci/lint_cpp/run_all_checkers.py <file_path>`
- Runs all applicable checkers (Rust + Python holdover) on a single file in well under a second.
- Used by `ci/hooks/lint-on-save.py` for save-time validation and by IDE integrations.

## Creating a New C++ Linter (Rust — the default)

New checkers go in `ci/lint_cpp_rs/src/checkers/`. The existing files group checkers by policy area (see `ci/lint_cpp_rs/src/checkers/README.md`); add your struct to the file whose theme matches, or create a new file if no existing group fits. Good models:

- `ci/lint_cpp_rs/src/checkers/basic.rs` — simple substring/regex content checks (`SerialPrintfChecker`, `UsingNamespaceFlInExamplesChecker`).
- `ci/lint_cpp_rs/src/checkers/style.rs` — line-state checkers with comment tracking (`PlatformPragmaChecker`, `RawPragmaChecker`).
- `ci/lint_cpp_rs/src/checkers/preprocessor.rs` — preprocessor / include-rule checkers.

### Steps

**a) Define a struct and `impl FileContentChecker`.** The trait lives in `ci/lint_cpp_rs/src/lint_core/processor_registry_cli.rs`:

```rust
struct YourRuleChecker;

impl FileContentChecker for YourRuleChecker {
    fn name(&self) -> &'static str {
        "YourRuleChecker"
    }

    fn should_process_file(&self, file_path: &str, _project_root: &Path) -> bool {
        if !ends_with_any(file_path, &[".cpp", ".h", ".hpp", ".ino", ".cpp.hpp"]) {
            return false;
        }
        let normalized = normalize_path(file_path);
        !is_under_dir(&normalized, "third_party")
    }

    fn check_file_content(&self, file_content: &FileContent) -> Vec<(usize, String)> {
        let mut violations = Vec::new();
        let mut in_multiline_comment = false;

        for (index, line) in file_content.lines.iter().enumerate() {
            let stripped = line.trim();

            if line.contains("/*") { in_multiline_comment = true; }
            if line.contains("*/") { in_multiline_comment = false; continue; }
            if in_multiline_comment || stripped.starts_with("//") { continue; }

            let code = split_line_comment(line);
            // YOUR DETECTION LOGIC HERE
            // On violation:
            //   violations.push((index + 1, format!("message: {stripped}")));
        }

        violations
    }
}
```

Helpers like `ends_with_any`, `is_under_dir`, `normalize_path`, `split_line_comment`, and `strip_string_literals` live in `ci/lint_cpp_rs/src/lint_core/`. Pre-compiled regexes go in `ci/lint_cpp_rs/src/lint_core/regexes.rs`.

> **Crate layout note.** All `ci/lint_cpp_rs/src/lint_core/*.rs` helpers — plus the `FileContent` struct and the `FileContentChecker` trait — are declared with module-private visibility (`fn`, not `pub fn`). They reach your new checker only because `ci/lint_cpp_rs/src/lib.rs` `include!()`s every source file into a single compilation unit. Treat the whole crate as one big module: add new files under `ci/lint_cpp_rs/src/checkers/`, hook them up with another `include!()` in `lib.rs`, and rely on the existing helpers in-place — do not try to add `pub` qualifiers or import them via `use crate::...` paths. (See `ci/lint_cpp_rs/src/checkers/README.md` for the policy-area grouping.)

**b) Register it in `ci/lint_cpp_rs/src/lint_core/processor_registry_cli.rs`.** Add a short snake_case name and a class-style name in two places:

```rust
// In supported_checker_names()
&[
    ...,
    "your_rule",
    ...,
]

// In supported_python_checker_names()
&[
    ...,
    "YourRuleChecker",
    ...,
]

// In create_checkers()
let mut checkers: Vec<(&'static str, Box<dyn FileContentChecker>)> = vec![
    ...,
    ("your_rule", Box::new(YourRuleChecker)),
    ...,
];
```

**c) Add the class name to `RUST_SUPPORTED_CHECKERS` in `ci/lint_cpp/rust_bridge.py`.** This tells the Python orchestrator to skip any Python instance with that class name during the deprecation window. If there is no Python equivalent (a brand-new rule), still add the name — the set is the canonical list of "owned by Rust" checker classes.

**d) Add an inline Rust unit test.** Follow the pattern in `ci/lint_cpp_rs/src/lint_core/tests.rs`:

```rust
#[test]
fn your_rule_flags_violations() {
    let checker = YourRuleChecker;
    let hits = checker.check_file_content(&file(
        "src/fl/example.h",
        "offending line of code\n",
    ));
    assert_eq!(hits.len(), 1);
}

#[test]
fn your_rule_ignores_third_party() {
    let checker = YourRuleChecker;
    assert!(!checker.should_process_file(
        "src/third_party/foo.h",
        Path::new("."),
    ));
}
```

The `file(path, content)` helper at the top of `tests.rs` constructs a `FileContent` instance.

### Verify

```
uv run python ci/lint_cpp/rust_binary_cache.py   # rebuild the cached binary
bash lint --cpp                                   # full run, parity-checked
```

If a Python equivalent of your checker exists (a deprecation port), also run:

```
FL_LINT_AB=1 bash lint --cpp                      # A/B parity check vs Python oracle
```

## Why Rust?

- **Speed**: the entire ~60-checker set runs in well under a second across src/, examples/, and tests/ — fast enough that `ci/hooks/lint-on-save.py` runs the full Rust suite per save.
- **Parallelism**: file IO and per-file checker dispatch fan out over `rayon`'s worker pool with zero per-checker setup cost (`processor_registry_cli.rs::process_files_with_checkers`).
- **Parity-tested**: every Rust checker was ported from the corresponding Python checker and validated under `FL_LINT_AB=1` until parity was clean. The `RUST_SUPPORTED_CHECKERS` set is the trust boundary.

## When to still use Python

As of PR #3293, **all single-file content checkers and cross-file structural checks run in the Rust crate** (`ci/lint_cpp_rs/`). The Python tier is reserved exclusively for the two remaining Tier-4 AST ratchets (across three Python files) that need libclang / clang-query, which the Rust binary cannot model today:

- **AST ratchets only**: `run_noexcept_ast_check` and `run_array_param_ast_check` in `ci/lint_cpp/run_all_checkers.py`, backed by `ci/lint_cpp/noexcept_checker.py`, `ci/tools/check_noexcept.py`, and `ci/tools/check_array_params.py`. These compare AST query output against a checked-in baseline so the violation count can only drop, not grow.

Everything else — unity-build structure, test aggregation, PCH file shape, `BareLibmChecker`, `BareNoInlineChecker`, `BareSnprintfChecker`, `LegacyLogMacroChecker`, `PublicSettingsPatternChecker`, `FlNoUnderscoreChecker`, etc. — now lives under `ci/lint_cpp_rs/src/checkers/`.

If your rule is single-file and content-based, write it in Rust. Cross-file structural rules also belong in Rust (see `ci/lint_cpp_rs/src/checkers/unity_build.rs` and `structural_passes.rs`).

## DO NOT

- Add new single-file content checkers to `ci/lint_cpp/` — write them in Rust.
- Create standalone `test_*.py` scripts that scan files independently and bypass the orchestrator.
- Call standalone checker scripts from the `bash lint` driver.
- Load files multiple times for different checks. The Rust crate reads each file exactly once per run.
