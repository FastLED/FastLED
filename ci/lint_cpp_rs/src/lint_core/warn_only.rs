// Warn-only checker policy.
//
// Checkers listed here have their findings reported but do NOT fail the run.
//
// This exists so a checker can be rolled out against a tree that already
// violates it: the findings are visible from day one while the migration
// lands incrementally, and the checker is promoted to hard-fail once the
// count reaches zero. Without it, enabling a checker on a dirty tree is
// all-or-nothing -- either CI breaks immediately, or the checker stays
// switched off and silently guards nothing, which is the worse failure
// because it still looks installed.
//
// Must stay in sync with WARN_ONLY_CHECKERS in
// ci/lint_cpp/run_all_checkers.py. The Rust binary owns its own exit code
// and the Python orchestrator tallies the same findings a second time to
// decide its own, so both layers have to agree or the stricter one wins
// silently. ci/tests/test_warn_only_checkers_sync.py asserts they match.

/// Checker names whose violations are reported but never block.
pub const WARN_ONLY_CHECKERS: &[&str] = &[
    // FastLED#3482 -- pre-existing namespace-scope globals; migration to
    // fl::Singleton<T> is tracked by the sub-issues under FastLED#3481.
    // Promote to hard-fail via FastLED#3492 once the count reaches zero.
    "SingletonElisionChecker",
    // FastLED#3483 -- integer constants split out of SingletonElisionChecker.
    // Same pre-existing tree, same rollout: report while the ~30 sites gain
    // `constexpr`, then promote. Fixing a hit silences both rules at once.
    "PreferConstexprChecker",
    // FastLED#3287 -- taking a raw pointer into a CONTIGUOUS fl:: container
    // is not UB today, only brittle. The non-contiguous tier
    // (ContainerNonContiguousPtrChecker) hard-fails; this one reports while
    // the handful of existing sites migrate to fl::span / iterators.
    "ContainerElementAddressChecker",
];

/// True when `checker`'s findings must not affect the exit code.
pub fn is_warn_only(checker: &str) -> bool {
    WARN_ONLY_CHECKERS.contains(&checker)
}

fn print_text_results(violations: &[LintViolation], project_root: &Path) {
    let (warnings, errors): (Vec<_>, Vec<_>) = violations
        .iter()
        .partition(|violation| is_warn_only(&violation.checker));

    let print_group = |group: &[&LintViolation]| {
        let mut current_checker = "";
        for violation in group {
            if violation.checker != current_checker {
                current_checker = &violation.checker;
                println!("\n[{current_checker}]");
            }
            let display_path = relative_display_path(&violation.path, project_root);
            println!("  {display_path}:{}: {}", violation.line, violation.message);
        }
    };

    if !warnings.is_empty() {
        print_group(&warnings);
        println!(
            "\n{} warn-only violation(s) above -- reported, not failing the build.",
            warnings.len()
        );
    }

    if errors.is_empty() {
        if warnings.is_empty() {
            println!("All Rust C++ linting checks passed!");
        } else {
            println!("No blocking Rust C++ lint violations.");
        }
        return;
    }

    print_group(&errors);
}
