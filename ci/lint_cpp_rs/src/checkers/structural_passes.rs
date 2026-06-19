// Structural passes — checks that walk the filesystem once at lint time
// rather than dispatching per-file through MultiCheckerFileProcessor.
//
// Each pass returns `Vec<LintViolation>` and is invoked exactly once from
// `run_cli` after the per-file dispatch finishes. Per-file-checker output
// shape (checker name, path, line, message) is reused so the JSON / text
// output formats stay uniform across both styles.
//
// Today this file hosts PchFileChecker only — the per-file dispatcher cannot
// see `.pch` files (its glob is restricted to .cpp/.h/.hpp/.ino/.cpp.hpp).

pub(crate) fn run_structural_passes(project_root: &Path) -> Vec<LintViolation> {
    let mut violations = Vec::new();
    pch_file_pass(project_root, &mut violations);
    violations
}

fn pch_file_pass(project_root: &Path, violations: &mut Vec<LintViolation>) {
    // Origin: ci/lint_cpp/pch_file_checker.py. Scans tests/, src/, examples/
    // for any `.pch` artifact — precompiled-header binaries should never be
    // committed.
    const SCAN_DIRS: &[&str] = &["tests", "src", "examples"];
    for dir_name in SCAN_DIRS {
        let scan_dir = project_root.join(dir_name);
        if !scan_dir.exists() {
            continue;
        }
        for entry in WalkDir::new(&scan_dir)
            .into_iter()
            .filter_map(Result::ok)
        {
            if !entry.file_type().is_file() {
                continue;
            }
            let path = entry.path();
            let Some(ext) = path.extension().and_then(|value| value.to_str()) else {
                continue;
            };
            if ext.eq_ignore_ascii_case("pch") {
                let rel = path.strip_prefix(project_root).unwrap_or(path);
                let rel_display = normalize_path(&path_to_string(rel));
                // Python emits one violation per .pch with path="pch_files"
                // and line=0 — keep the shape identical so A/B parity holds.
                violations.push(LintViolation {
                    checker: "PchFileChecker".to_string(),
                    path: "pch_files".to_string(),
                    line: 0,
                    message: format!(
                        "{rel_display}: Precompiled header file (.pch) found — \
these are build artifacts and should not be in the repository. Delete this file."
                    ),
                });
            }
        }
    }
}
