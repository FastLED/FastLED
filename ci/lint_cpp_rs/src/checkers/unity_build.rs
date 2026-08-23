// UnityBuildChecker — Tier-3 port from ci/lint_cpp/test_unity_build.py
// (FastLED #3297). Hosts both:
//   1. `unity_build_pass()` — whole-project structural pass over src/.
//   2. `UnityBuildChecker` (struct impl FileContentChecker) — targeted
//      single-file mode that validates just the file's immediate
//      neighborhood (used by the on-save hook).
//
// 11 distinct checks ported verbatim:
//   - Legacy _build.hpp naming (must be _build.cpp.hpp).
//   - Hierarchical includes (immediate children only).
//   - .cpp.hpp completeness (every .cpp.hpp referenced in its _build.cpp.hpp).
//   - Include ordering + section comments (same-level first, then subdir).
//   - Subdir completeness (every immediate _build.cpp.hpp subdir referenced).
//   - Alphabetical ordering within each section.
//   - No cross-directory includes.
//   - No invalid include types (only .cpp.hpp + .h).
//   - Build-file pre-headers (platforms/new.h, fl/system/arduino.h).
//   - Build-file content (flat = all .cpp.hpp; recursive = one _build.cpp.hpp).
//   - Build-file naming + library.json srcFilter.

const UNITY_BUILD_HPP: &str = "_build.cpp.hpp";
const UNITY_BUILD_LEGACY_HPP: &str = "_build.hpp";
const UNITY_SECTION_CURRENT_DIR: &str = "// begin current directory includes";
const UNITY_SECTION_SUB_DIR: &str = "// begin sub directory includes";
const UNITY_REQUIRED_PRE_HEADERS: &[&str] = &["platforms/new.h", "fl/system/arduino.h"];

const UNITY_EXPECTED_BUILD_FILES: &[&str] = &[
    "fl/build/src.cpp",
    "fl/build/fl.asset+.cpp",
    "fl/build/fl.audio+.cpp",
    "fl/build/fl.channels+.cpp",
    "fl/build/fl.chipsets+.cpp",
    "fl/build/fl.codec+.cpp",
    "fl/build/fl.fled+.cpp",
    "fl/build/fl.font+.cpp",
    "fl/build/fl.fx+.cpp",
    "fl/build/fl.gfx+.cpp",
    "fl/build/fl.log+.cpp",
    "fl/build/fl.math+.cpp",
    "fl/build/fl.net+.cpp",
    "fl/build/fl.control+.cpp",
    "fl/build/fl.remote+.cpp",
    "fl/build/fl.sensors+.cpp",
    "fl/build/fl.stl+.cpp",
    "fl/build/fl.system+.cpp",
    "fl/build/fl.fs+.cpp",
    "fl/build/fl.fs.sd+.cpp",
    "fl/build/fl.task+.cpp",
    "fl/build/fl.test+.cpp",
    "fl/build/fl.ui+.cpp",
    "fl/build/fl.video+.cpp",
    "fl/build/fl.wdt+.cpp",
    "fl/build/platforms+.cpp",
    "fl/build/third_party+.cpp",
    "fl/build/extras+.cpp",
];

const UNITY_DANGEROUS_WILDCARDS: &[&str] = &[
    "+<*.cpp>",
    "+<platforms/**/*.cpp>",
    "+<third_party/**/*.cpp>",
    "+<fl/**/*.cpp>",
];
const UNITY_EXCLUDE_PATTERN: &str = "-<**/*.cpp>";
const UNITY_EXPECTED_SRC_FILTER: &str = "+<fl/build/*.cpp>";

// ---------------------------------------------------------------------------
// Whole-project pass (called from structural_passes.rs).
// ---------------------------------------------------------------------------

pub(crate) fn unity_build_pass(project_root: &Path, violations: &mut Vec<LintViolation>) {
    let src_dir = project_root.join("src");
    if !src_dir.exists() {
        return;
    }

    let build_hpps = collect_files(&src_dir, |path| {
        path.file_name().and_then(|name| name.to_str()) == Some(UNITY_BUILD_HPP)
    });
    let cpp_hpps_by_dir = collect_cpp_hpps_by_dir(&src_dir);
    let independently_compiled = compute_independently_compiled_dirs(&src_dir);

    push_violations(
        violations,
        check_build_hpp_naming(&src_dir, project_root),
    );
    push_violations(
        violations,
        check_hierarchy(&src_dir, &build_hpps, project_root),
    );
    push_violations(
        violations,
        check_cpp_hpp_files(&src_dir, &cpp_hpps_by_dir, project_root),
    );
    push_violations(
        violations,
        check_build_include_order(&src_dir, &build_hpps, project_root),
    );
    push_violations(
        violations,
        check_subdir_completeness(
            &src_dir,
            &build_hpps,
            &independently_compiled,
            project_root,
        ),
    );
    push_violations(
        violations,
        check_alphabetical_order(&src_dir, &build_hpps, project_root),
    );
    push_violations(
        violations,
        check_no_cross_directory_includes(&src_dir, &build_hpps, project_root),
    );
    push_violations(
        violations,
        check_no_invalid_include_types(&src_dir, &build_hpps, project_root),
    );
    push_violations(
        violations,
        check_build_file_naming(&src_dir, project_root),
    );
    push_violations(
        violations,
        check_build_file_preheaders(&src_dir, project_root),
    );
    push_violations(
        violations,
        check_build_file_content(&src_dir, &cpp_hpps_by_dir, project_root),
    );
    push_violations(
        violations,
        check_no_orphan_cpp_files(&src_dir, project_root),
    );
    push_violations(violations, check_library_json_src_filter(project_root));
}

fn push_violations(target: &mut Vec<LintViolation>, mut new_violations: Vec<String>) {
    for message in new_violations.drain(..) {
        target.push(LintViolation {
            checker: "UnityBuildChecker".to_string(),
            path: "unity_build_structure".to_string(),
            line: 0,
            message,
        });
    }
}

// ---------------------------------------------------------------------------
// Per-file UnityBuildChecker (single-file mode).
// ---------------------------------------------------------------------------

struct UnityBuildChecker;

impl FileContentChecker for UnityBuildChecker {
    fn name(&self) -> &'static str {
        "UnityBuildChecker"
    }

    fn should_process_file(&self, file_path: &str, project_root: &Path) -> bool {
        let normalized = normalize_path(file_path);
        if !is_under_project_subpath(&normalized, project_root, "src") {
            return false;
        }
        normalized.ends_with(".cpp.hpp") || normalized.ends_with(".cpp")
    }

    fn check_file_content(&self, file_content: &FileContent) -> Vec<(usize, String)> {
        unity_build_check_single_file(&file_content.path)
            .into_iter()
            .map(|message| (0, message))
            .collect()
    }
}

fn unity_build_check_single_file(file_path: &str) -> Vec<String> {
    let normalized = normalize_path(file_path);
    let root_prefix = project_root_prefix_for_file(&normalized);
    let Some(project_rel) = project_relative_path(&normalized) else {
        return Vec::new();
    };

    let project_root_path = PathBuf::from(&root_prefix);
    let src_dir = project_root_path.join("src");
    let build_dir = src_dir.join("fl").join("build");

    // Build-file path under src/fl/build/<name>.cpp ?
    if project_rel.starts_with("fl/build/") && project_rel.ends_with(".cpp") {
        let mut violations = Vec::new();
        let cpp_hpps_by_dir = collect_cpp_hpps_by_dir(&src_dir);
        violations.extend(check_build_file_preheaders(&src_dir, &project_root_path));
        violations.extend(check_build_file_content(
            &src_dir,
            &cpp_hpps_by_dir,
            &project_root_path,
        ));
        return violations;
    }

    // Only proceed for .cpp.hpp files past this point.
    if !normalized.ends_with(".cpp.hpp") {
        return Vec::new();
    }
    let file_path_buf = PathBuf::from(&normalized);
    let Some(file_dir) = file_path_buf.parent() else {
        return Vec::new();
    };
    let file_name = file_path_buf
        .file_name()
        .and_then(|name| name.to_str())
        .unwrap_or("");

    if file_name == UNITY_BUILD_HPP {
        // Editing a _build.cpp.hpp: every sibling .cpp.hpp must be referenced.
        let siblings = collect_files(file_dir, |path| {
            path.file_name()
                .and_then(|name| name.to_str())
                .is_some_and(|name| name.ends_with(".cpp.hpp"))
                && path.parent() == Some(file_dir)
        });
        let mut single_dir_map = HashMap::new();
        single_dir_map.insert(file_dir.to_path_buf(), siblings);
        return check_cpp_hpp_files(&src_dir, &single_dir_map, &project_root_path);
    }

    // Regular .cpp.hpp: its parent _build.cpp.hpp must include it.
    let build_hpp = file_dir.join(UNITY_BUILD_HPP);
    if !build_hpp.exists() {
        let rel_dir_normalized = normalize_path(&path_to_string(file_dir));
        let rel_dir = project_relative_path(&rel_dir_normalized).unwrap_or_else(|| {
            normalize_path(&path_to_string(file_dir))
                .strip_prefix(&format!("{}/", normalize_path(&root_prefix)))
                .map(str::to_string)
                .unwrap_or_else(|| rel_dir_normalized.clone())
        });
        return vec![format!("Missing {UNITY_BUILD_HPP} in {rel_dir}/")];
    }

    let Some(rel_path) = pathdiff_normalized(&normalized, &src_dir) else {
        return Vec::new();
    };
    let Ok(content) = std::fs::read_to_string(&build_hpp) else {
        return Vec::new();
    };
    if !content.contains(&rel_path) {
        // File-level opt-out: `UNITY_BUILD_EXCLUDE(<reason>)` in the
        // first 15 lines of the .cpp.hpp declares that this file is
        // intentionally NOT part of the unity build (e.g. Stage-4 impl
        // whose symbol set collides with a restored classic driver —
        // see `platforms/esp/32/drivers/i2s/i2s_peripheral_esp32dev_esp.cpp.hpp`).
        if let Ok(file_content) = std::fs::read_to_string(&file_path_buf) {
            if file_content
                .lines()
                .take(15)
                .any(|l| l.contains("UNITY_BUILD_EXCLUDE"))
            {
                return Vec::new();
            }
        }
        let build_hpp_norm = normalize_path(&path_to_string(&build_hpp));
        let rel_build = project_relative_path(&build_hpp_norm).unwrap_or(build_hpp_norm);
        return vec![format!("{rel_build}: missing {rel_path}")];
    }
    Vec::new()
}

// ---------------------------------------------------------------------------
// Individual whole-project checks.
// ---------------------------------------------------------------------------

fn check_build_hpp_naming(src_dir: &Path, project_root: &Path) -> Vec<String> {
    let mut violations = Vec::new();
    let legacy_files = collect_files(src_dir, |path| {
        path.file_name().and_then(|name| name.to_str()) == Some(UNITY_BUILD_LEGACY_HPP)
    });
    for legacy in legacy_files {
        let Ok(content) = std::fs::read_to_string(&legacy) else {
            continue;
        };
        if regex_unity_cpp_hpp_include().is_match(&content) {
            let rel = rel_from_project(&legacy, project_root);
            let expected = legacy
                .parent()
                .map(|parent| parent.join(UNITY_BUILD_HPP))
                .unwrap_or_else(|| PathBuf::from(UNITY_BUILD_HPP));
            let rel_expected = rel_from_project(&expected, project_root);
            violations.push(format!(
                "{rel}: Legacy _build.hpp file includes .cpp.hpp files. \
Should be renamed to '{rel_expected}' to follow implementation file convention."
            ));
        }
    }
    violations
}

fn check_hierarchy(
    src_dir: &Path,
    build_hpps: &[PathBuf],
    project_root: &Path,
) -> Vec<String> {
    let mut violations = Vec::new();
    for build_hpp in build_hpps {
        let Some(namespace_path) = unity_namespace_path(build_hpp, src_dir) else {
            continue;
        };
        let Ok(content) = std::fs::read_to_string(build_hpp) else {
            continue;
        };
        let rel_file = rel_from_project(build_hpp, project_root);
        for capture in regex_unity_build_hpp_include().captures_iter(&content) {
            let Some(included) = capture.get(1) else { continue; };
            let included_path = included.as_str();
            let depth = unity_calculate_include_depth(included_path, &namespace_path);
            let line_num = unity_line_number(&content, included.start());
            if depth == 0 {
                violations.push(format!(
                    "{rel_file}:{line_num}: Include '{included_path}' doesn't match expected prefix '{namespace_path}/'"
                ));
            } else if depth != 1 {
                violations.push(format!(
                    "{rel_file}:{line_num}: Include '{included_path}' is {depth} levels deep, but should be exactly 1 level. \
Expected pattern: '{namespace_path}/<dir>/_build.hpp'"
                ));
            }
        }
    }
    violations
}

fn check_cpp_hpp_files(
    src_dir: &Path,
    cpp_hpps_by_dir: &HashMap<PathBuf, Vec<PathBuf>>,
    project_root: &Path,
) -> Vec<String> {
    let mut violations = Vec::new();
    let build_dir = src_dir.join("fl").join("build");
    let mut sorted: Vec<(&PathBuf, &Vec<PathBuf>)> = cpp_hpps_by_dir.iter().collect();
    sorted.sort_by(|a, b| a.0.cmp(b.0));
    for (dir_path, cpp_hpp_files) in sorted {
        if dir_path == &build_dir || dir_path.starts_with(&build_dir) {
            continue;
        }
        let build_hpp = dir_path.join(UNITY_BUILD_HPP);
        if !build_hpp.exists() {
            let rel_dir = rel_from_project(dir_path, project_root);
            violations.push(format!("Missing {UNITY_BUILD_HPP} in {rel_dir}/"));
            continue;
        }
        let Ok(content) = std::fs::read_to_string(&build_hpp) else {
            continue;
        };
        let mut sorted_files = cpp_hpp_files.clone();
        sorted_files.sort();
        for cpp_hpp in &sorted_files {
            let name = cpp_hpp
                .file_name()
                .and_then(|name| name.to_str())
                .unwrap_or("");
            if name == UNITY_BUILD_HPP {
                continue;
            }
            // File-level opt-out: if the .cpp.hpp declares
            // `UNITY_BUILD_EXCLUDE(<reason>)` in its first 15 lines,
            // skip the "missing include" check. Used for files that
            // intentionally must NOT participate in the unity build
            // (e.g. Stage-4 impls whose symbol set collides with a
            // restored classic driver — see
            // `platforms/esp/32/drivers/i2s/i2s_peripheral_esp32dev_esp.cpp.hpp`).
            if let Ok(file_content) = std::fs::read_to_string(cpp_hpp) {
                if file_content
                    .lines()
                    .take(15)
                    .any(|l| l.contains("UNITY_BUILD_EXCLUDE"))
                {
                    continue;
                }
            }
            let Some(rel_path) = pathdiff_normalized(&path_to_string(cpp_hpp), src_dir) else {
                continue;
            };
            if !content.contains(&rel_path) {
                let rel_build = rel_from_project(&build_hpp, project_root);
                violations.push(format!("{rel_build}: missing {rel_path}"));
            }
        }
    }
    violations
}

fn check_build_include_order(
    _src_dir: &Path,
    build_hpps: &[PathBuf],
    project_root: &Path,
) -> Vec<String> {
    let mut violations = Vec::new();
    for build_hpp in build_hpps {
        let Ok(content) = std::fs::read_to_string(build_hpp) else {
            continue;
        };
        let lines: Vec<&str> = content.lines().collect();
        let rel_file = rel_from_project(build_hpp, project_root);

        let mut same_level_lines = Vec::new();
        let mut subdir_lines = Vec::new();
        for capture in regex_unity_cpp_hpp_include_capture().captures_iter(&content) {
            let Some(included) = capture.get(1) else { continue; };
            let path = included.as_str();
            let line_num = unity_line_number(&content, included.start());
            if path.ends_with(UNITY_BUILD_HPP) {
                subdir_lines.push(line_num);
            } else {
                same_level_lines.push(line_num);
            }
        }

        let has_both = !same_level_lines.is_empty() && !subdir_lines.is_empty();
        if has_both {
            let last_same = *same_level_lines.iter().max().unwrap();
            let first_subdir = *subdir_lines.iter().min().unwrap();
            if last_same > first_subdir {
                violations.push(format!(
                    "{rel_file}: Same-level .cpp.hpp includes (last at line {last_same}) \
must come BEFORE subdirectory _build.cpp.hpp includes (first at line {first_subdir}). \
Required order: same-level *.cpp.hpp first, then subdir/_build.cpp.hpp last."
                ));
            }
        }
        if !has_both {
            continue;
        }

        let mut current_dir_comment_line: Option<usize> = None;
        let mut sub_dir_comment_line: Option<usize> = None;
        for (idx, line) in lines.iter().enumerate() {
            let stripped = line.trim();
            let line_one_based = idx + 1;
            if stripped == UNITY_SECTION_CURRENT_DIR {
                current_dir_comment_line = Some(line_one_based);
            } else if stripped == UNITY_SECTION_SUB_DIR {
                sub_dir_comment_line = Some(line_one_based);
            }
        }
        let first_same = *same_level_lines.iter().min().unwrap();
        let first_subdir = *subdir_lines.iter().min().unwrap();

        match current_dir_comment_line {
            None => violations.push(format!(
                "{rel_file}: Missing \"{UNITY_SECTION_CURRENT_DIR}\" comment before \
same-level includes (first at line {first_same}). \
Add it on the line immediately before the first same-level #include."
            )),
            Some(comment_line) => {
                if comment_line != first_same.saturating_sub(1) {
                    let expected = first_same.saturating_sub(1);
                    violations.push(format!(
                        "{rel_file}:{comment_line}: \"{UNITY_SECTION_CURRENT_DIR}\" \
must be on line {expected} (immediately before first same-level include at line {first_same}), not line {comment_line}."
                    ));
                }
                if comment_line >= 2 {
                    let prev_idx = comment_line - 2;
                    let prev_line = lines.get(prev_idx).copied().unwrap_or("");
                    let prev_stripped = prev_line.trim();
                    if !prev_stripped.is_empty() && !prev_stripped.starts_with("///") {
                        violations.push(format!(
                            "{rel_file}:{comment_line}: \"{UNITY_SECTION_CURRENT_DIR}\" \
must have a blank line before it (line {} is not blank).",
                            comment_line - 1
                        ));
                    }
                }
            }
        }

        match sub_dir_comment_line {
            None => violations.push(format!(
                "{rel_file}: Missing \"{UNITY_SECTION_SUB_DIR}\" comment before \
subdirectory includes (first at line {first_subdir}). \
Add it on the line immediately before the first subdirectory #include."
            )),
            Some(comment_line) => {
                if comment_line != first_subdir.saturating_sub(1) {
                    let expected = first_subdir.saturating_sub(1);
                    violations.push(format!(
                        "{rel_file}:{comment_line}: \"{UNITY_SECTION_SUB_DIR}\" \
must be on line {expected} (immediately before first subdirectory include at line {first_subdir}), not line {comment_line}."
                    ));
                }
                if comment_line >= 2 {
                    let prev_idx = comment_line - 2;
                    let prev_line = lines.get(prev_idx).copied().unwrap_or("");
                    if !prev_line.trim().is_empty() {
                        violations.push(format!(
                            "{rel_file}:{comment_line}: \"{UNITY_SECTION_SUB_DIR}\" \
must have a blank line before it (line {} is not blank).",
                            comment_line - 1
                        ));
                    }
                }
            }
        }
    }
    violations
}

fn check_subdir_completeness(
    src_dir: &Path,
    build_hpps: &[PathBuf],
    independently_compiled: &HashSet<String>,
    project_root: &Path,
) -> Vec<String> {
    let mut violations = Vec::new();
    for build_hpp in build_hpps {
        let Some(dir_path) = build_hpp.parent() else {
            continue;
        };
        let Ok(content) = std::fs::read_to_string(build_hpp) else {
            continue;
        };
        let rel_file = rel_from_project(build_hpp, project_root);

        let mut subdirs: Vec<PathBuf> = match std::fs::read_dir(dir_path) {
            Ok(reader) => reader
                .filter_map(Result::ok)
                .filter(|entry| entry.file_type().map(|ty| ty.is_dir()).unwrap_or(false))
                .map(|entry| entry.path())
                .collect(),
            Err(_) => continue,
        };
        subdirs.sort();
        for subdir in subdirs {
            let sub_build = subdir.join(UNITY_BUILD_HPP);
            if !sub_build.exists() {
                continue;
            }
            let Some(subdir_rel) = pathdiff_normalized(&path_to_string(&subdir), src_dir)
            else {
                continue;
            };
            if independently_compiled.contains(&subdir_rel) {
                continue;
            }
            let Some(include_path) =
                pathdiff_normalized(&path_to_string(&sub_build), src_dir)
            else {
                continue;
            };
            if !content.contains(&include_path) {
                violations.push(format!(
                    "{rel_file}: Missing subdirectory include '{include_path}'. \
All immediate subdirectories with {UNITY_BUILD_HPP} must be included."
                ));
            }
        }
    }
    violations
}

fn check_alphabetical_order(
    _src_dir: &Path,
    build_hpps: &[PathBuf],
    project_root: &Path,
) -> Vec<String> {
    let mut violations = Vec::new();
    for build_hpp in build_hpps {
        let Ok(content) = std::fs::read_to_string(build_hpp) else {
            continue;
        };
        // File-level opt-out for intentionally non-alphabetical include
        // order: `UNITY_BUILD_ORDER(<reason>)` in the file suppresses
        // the alphabetical check. Used when a compile-order dependency
        // between two cpp.hpp files (e.g. runtime-before-engine when the
        // engine instantiates a runtime class as a member) forces a
        // non-sorted order.
        if content.contains("UNITY_BUILD_ORDER") {
            continue;
        }
        let rel_file = rel_from_project(build_hpp, project_root);

        let mut same_level: Vec<(String, usize)> = Vec::new();
        let mut subdir: Vec<(String, usize)> = Vec::new();
        for capture in regex_unity_cpp_hpp_include_capture().captures_iter(&content) {
            let Some(included) = capture.get(1) else { continue; };
            let path = included.as_str().to_string();
            let line_num = unity_line_number(&content, included.start());
            if path.ends_with(UNITY_BUILD_HPP) {
                subdir.push((path, line_num));
            } else {
                same_level.push((path, line_num));
            }
        }
        for (name, section) in [
            ("current directory", &same_level),
            ("sub directory", &subdir),
        ] {
            if section.len() < 2 {
                continue;
            }
            let paths: Vec<&str> = section.iter().map(|(p, _)| p.as_str()).collect();
            let mut sorted_paths: Vec<&str> = paths.clone();
            sorted_paths.sort();
            if paths != sorted_paths {
                for i in 1..paths.len() {
                    if paths[i] < paths[i - 1] {
                        let line_num = section[i].1;
                        violations.push(format!(
                            "{rel_file}:{line_num}: {name} includes not alphabetically sorted. \
'{}' comes before '{}' but should come after it.",
                            paths[i],
                            paths[i - 1]
                        ));
                        break;
                    }
                }
            }
        }
    }
    violations
}

fn check_no_cross_directory_includes(
    src_dir: &Path,
    build_hpps: &[PathBuf],
    project_root: &Path,
) -> Vec<String> {
    let mut violations = Vec::new();
    for build_hpp in build_hpps {
        let Ok(content) = std::fs::read_to_string(build_hpp) else {
            continue;
        };
        let rel_file = rel_from_project(build_hpp, project_root);
        let Some(namespace_path) = unity_namespace_path(build_hpp, src_dir) else {
            continue;
        };
        let expected_prefix = if namespace_path.is_empty() {
            String::new()
        } else {
            format!("{namespace_path}/")
        };
        for capture in regex_unity_any_include().captures_iter(&content) {
            let Some(included) = capture.get(1) else { continue; };
            let included_path = included.as_str();
            let line_num = unity_line_number(&content, included.start());
            if !included_path.ends_with(".cpp.hpp") {
                continue;
            }
            if !expected_prefix.is_empty() && !included_path.starts_with(&expected_prefix) {
                violations.push(format!(
                    "{rel_file}:{line_num}: Cross-directory include '{included_path}' \
does not belong to this directory (expected prefix '{expected_prefix}'). \
_build.cpp.hpp should only include files from its own directory."
                ));
                continue;
            }
            if !included_path.ends_with(UNITY_BUILD_HPP) {
                let relative_part = &included_path[expected_prefix.len()..];
                if relative_part.contains('/') {
                    violations.push(format!(
                        "{rel_file}:{line_num}: Include '{included_path}' is from a \
subdirectory but is not a _build.cpp.hpp. Same-level includes \
must be directly in '{namespace_path}/', not in a child folder."
                    ));
                }
            }
        }
    }
    violations
}

fn check_no_invalid_include_types(
    _src_dir: &Path,
    build_hpps: &[PathBuf],
    project_root: &Path,
) -> Vec<String> {
    let mut violations = Vec::new();
    for build_hpp in build_hpps {
        let Ok(content) = std::fs::read_to_string(build_hpp) else {
            continue;
        };
        let rel_file = rel_from_project(build_hpp, project_root);
        for capture in regex_unity_any_include().captures_iter(&content) {
            let Some(included) = capture.get(1) else { continue; };
            let included_path = included.as_str();
            let line_num = unity_line_number(&content, included.start());
            if included_path.ends_with(".cpp.hpp")
                || included_path.ends_with(".h")
                || included_path.ends_with(".hpp")
            {
                continue;
            }
            violations.push(format!(
                "{rel_file}:{line_num}: Invalid include type '{included_path}'. \
_build.cpp.hpp should only include *.cpp.hpp and *.h files."
            ));
        }
    }
    violations
}
