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
    test_aggregation_pass(project_root, &mut violations);
    unity_build_pass(project_root, &mut violations);
    violations
}

fn test_aggregation_pass(project_root: &Path, violations: &mut Vec<LintViolation>) {
    // Origin: ci/lint_cpp/test_aggregation_checker.py::check() (whole-project).
    // For each TEST_AGGREGATED_DIRS entry:
    //   - Find the parent `<dir>.cpp` aggregator. If missing → emit "Missing
    //     aggregator" violation.
    //   - Walk the dir for every `.hpp` file. If a file is not transitively
    //     included by any ancestor aggregator → emit "orphaned" violation.
    //   - Scan the direct aggregator for `#include "...cpp"` references that
    //     point inside the excluded dir → emit "should use .hpp" violation.
    let root_prefix = normalize_path(&path_to_string(project_root));

    let mut sorted_dirs: Vec<&'static str> = TEST_AGGREGATED_DIRS.iter().copied().collect();
    sorted_dirs.sort();

    for excluded_rel in sorted_dirs {
        let excluded_path = join_project_path(&root_prefix, excluded_rel);
        if !excluded_path.exists() {
            continue;
        }
        let aggregator_rel = test_aggregator_rel_for_dir(excluded_rel);
        let aggregator_path = join_project_path(&root_prefix, &aggregator_rel);

        let (resolved_aggregator_rel, included_files) =
            collect_test_aggregation_included_files(&root_prefix, excluded_rel);

        if resolved_aggregator_rel.is_none() {
            violations.push(LintViolation {
                checker: "TestAggregationChecker".to_string(),
                path: "test_aggregation".to_string(),
                line: 0,
                message: format!(
                    "Missing aggregator: {aggregator_rel} \
(no parent .cpp file for excluded directory {excluded_rel}/)"
                ),
            });
            continue;
        }

        // Orphan .hpp scan — walk the excluded dir, emit one violation per
        // .hpp not present in `included_files`. Sort for deterministic output
        // (Python uses `sorted(rglob("*.hpp"))`).
        let mut hpp_files: Vec<String> = Vec::new();
        for entry in WalkDir::new(&excluded_path).into_iter().filter_map(Result::ok) {
            if !entry.file_type().is_file() {
                continue;
            }
            let normalized = normalize_path(&path_to_string(entry.path()));
            if normalized.ends_with(".hpp") {
                hpp_files.push(normalized);
            }
        }
        hpp_files.sort();
        for hpp_path in hpp_files {
            if included_files.contains(&hpp_path) {
                continue;
            }
            let rel_file = project_relative_path(&hpp_path).unwrap_or(hpp_path.clone());
            violations.push(LintViolation {
                checker: "TestAggregationChecker".to_string(),
                path: "test_aggregation".to_string(),
                line: 0,
                message: format!(
                    "{aggregator_rel}: orphaned test file not #included by any aggregator: {rel_file}"
                ),
            });
        }

        // .cpp-include scan on the direct aggregator. Walks the includes in
        // sorted order to match the Python pass.
        if aggregator_path.exists() {
            // parse_aggregator_includes returns BTreeSet → iteration is
            // already lexicographically ordered, matching Python's
            // `sorted(_parse_includes(aggregator))`.
            for include in parse_aggregator_includes(&aggregator_path) {
                if !include.ends_with(".cpp") {
                    continue;
                }
                let inc_resolved =
                    resolve_test_include(&root_prefix, &aggregator_rel, &include);
                if project_relative_path(&inc_resolved)
                    .is_some_and(|rel| rel.starts_with(&format!("{excluded_rel}/")))
                {
                    violations.push(LintViolation {
                        checker: "TestAggregationChecker".to_string(),
                        path: "test_aggregation".to_string(),
                        line: 0,
                        message: format!(
                            "{aggregator_rel}: #include \"{include}\" should use .hpp \
(included test files must use .hpp extension)"
                        ),
                    });
                }
            }
        }
    }
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
