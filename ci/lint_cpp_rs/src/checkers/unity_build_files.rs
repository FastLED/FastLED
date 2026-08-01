// UnityBuildChecker, part 2 — the `src/fl/build/` build-file checks and the
// shared filesystem helpers.
//
// Split out of checkers/unity_build.rs, which had grown past the crate's own
// 1,000-line-per-file rule (pinned by
// `rust_lint_source_files_stay_under_one_thousand_lines` in lint_core/tests.rs).
// That test had been failing for some time because CI never ran `cargo test`
// for this crate at all -- see the CI step added alongside this split.
//
// Checkers are `include!`d into lib.rs, so this file shares one flat scope
// with unity_build.rs: no `mod`/`pub` wiring, and the functions here are
// called from `unity_build_pass()` exactly as before.
//
// Contains:
//   - Build-file naming + library.json srcFilter.
//   - Build-file pre-headers (platforms/new.h, fl/system/arduino.h).
//   - Build-file content (flat = all .cpp.hpp; recursive = one _build.cpp.hpp).
//   - Orphan .cpp file detection.
//   - collect_files / collect_cpp_hpps_by_dir / compute_independently_compiled_dirs.

fn check_build_file_naming(src_dir: &Path, _project_root: &Path) -> Vec<String> {
    let mut violations = Vec::new();
    let build_dir = src_dir.join("fl").join("build");
    if !build_dir.exists() {
        violations.push("Missing build directory: src/fl/build/".to_string());
        return violations;
    }
    let actual: HashSet<String> = match std::fs::read_dir(&build_dir) {
        Ok(reader) => reader
            .filter_map(Result::ok)
            .filter_map(|entry| {
                let path = entry.path();
                if path.extension().and_then(|ext| ext.to_str()) == Some("cpp") {
                    path.file_name()
                        .and_then(|name| name.to_str())
                        .map(str::to_string)
                } else {
                    None
                }
            })
            .collect(),
        Err(_) => HashSet::new(),
    };
    let expected: HashSet<String> = UNITY_EXPECTED_BUILD_FILES
        .iter()
        .map(|name| name.rsplit('/').next().unwrap_or(*name).to_string())
        .collect();

    let mut unexpected: Vec<&String> = actual.difference(&expected).collect();
    unexpected.sort();
    for name in unexpected {
        violations.push(format!(
            "src/fl/build/{name}: Unexpected build file. \
Not in EXPECTED_BUILD_FILES list."
        ));
    }
    let mut missing: Vec<&String> = expected.difference(&actual).collect();
    missing.sort();
    for name in missing {
        violations.push(format!(
            "src/fl/build/{name}: Expected build file is missing."
        ));
    }

    let mut build_files: Vec<PathBuf> = std::fs::read_dir(&build_dir)
        .map(|reader| {
            reader
                .filter_map(Result::ok)
                .map(|entry| entry.path())
                .filter(|path| path.extension().and_then(|ext| ext.to_str()) == Some("cpp"))
                .collect()
        })
        .unwrap_or_default();
    build_files.sort();
    for build_file in build_files {
        let Some(name) = build_file.file_name().and_then(|name| name.to_str()) else {
            continue;
        };
        let (dir_path, _) = parse_build_filename(name);
        let mapped = if dir_path.is_empty() {
            src_dir.to_path_buf()
        } else {
            src_dir.join(&dir_path)
        };
        if !mapped.exists() {
            let rel_file = normalize_path(&path_to_string(&build_file));
            let display = if dir_path.is_empty() { "src/" } else { &dir_path };
            violations.push(format!(
                "{rel_file}: Maps to non-existent directory '{display}'."
            ));
        }
    }
    violations
}

fn check_build_file_preheaders(src_dir: &Path, project_root: &Path) -> Vec<String> {
    let mut violations = Vec::new();
    let build_dir = src_dir.join("fl").join("build");
    if !build_dir.exists() {
        return violations;
    }
    let mut build_files: Vec<PathBuf> = std::fs::read_dir(&build_dir)
        .map(|reader| {
            reader
                .filter_map(Result::ok)
                .map(|entry| entry.path())
                .filter(|path| path.extension().and_then(|ext| ext.to_str()) == Some("cpp"))
                .collect()
        })
        .unwrap_or_default();
    build_files.sort();
    for build_file in build_files {
        let Ok(content) = std::fs::read_to_string(&build_file) else {
            continue;
        };
        let rel_file = rel_from_project(&build_file, project_root);
        let includes: Vec<(String, usize)> = regex_unity_any_include()
            .captures_iter(&content)
            .filter_map(|capture| {
                let mat = capture.get(1)?;
                Some((mat.as_str().to_string(), mat.start()))
            })
            .collect();
        if includes.is_empty() {
            continue;
        }
        let first_impl_line = includes
            .iter()
            .find(|(path, _)| path.ends_with(".cpp.hpp"))
            .map(|(_, start)| unity_line_number(&content, *start));
        let Some(first_impl_line) = first_impl_line else {
            violations.push(format!(
                "{rel_file}: Build file has no .cpp.hpp includes — this file serves no purpose."
            ));
            continue;
        };
        for pre_header in UNITY_REQUIRED_PRE_HEADERS {
            let mut found = false;
            for (include_path, start) in &includes {
                if include_path == pre_header {
                    let pre_line = unity_line_number(&content, *start);
                    if pre_line < first_impl_line {
                        found = true;
                    } else {
                        violations.push(format!(
                            "{rel_file}:{pre_line}: Pre-header '{pre_header}' must appear \
BEFORE first .cpp.hpp include (line {first_impl_line})."
                        ));
                        found = true;
                    }
                    break;
                }
            }
            if !found {
                violations.push(format!(
                    "{rel_file}: Missing required pre-header '#include \"{pre_header}\"'. \
All build files must include platform pre-headers before implementation includes."
                ));
            }
        }
    }
    violations
}

fn check_build_file_content(
    src_dir: &Path,
    cpp_hpps_by_dir: &HashMap<PathBuf, Vec<PathBuf>>,
    project_root: &Path,
) -> Vec<String> {
    let mut violations = Vec::new();
    let build_dir = src_dir.join("fl").join("build");
    if !build_dir.exists() {
        return violations;
    }
    let mut build_files: Vec<PathBuf> = std::fs::read_dir(&build_dir)
        .map(|reader| {
            reader
                .filter_map(Result::ok)
                .map(|entry| entry.path())
                .filter(|path| path.extension().and_then(|ext| ext.to_str()) == Some("cpp"))
                .collect()
        })
        .unwrap_or_default();
    build_files.sort();
    for build_file in build_files {
        let Ok(content) = std::fs::read_to_string(&build_file) else {
            continue;
        };
        let rel_file = rel_from_project(&build_file, project_root);
        let Some(name) = build_file.file_name().and_then(|name| name.to_str()) else {
            continue;
        };
        let (dir_path, is_recursive) = parse_build_filename(name);

        let impl_includes: Vec<String> = regex_unity_cpp_hpp_include_capture()
            .captures_iter(&content)
            .filter_map(|capture| capture.get(1).map(|mat| mat.as_str().to_string()))
            .collect();

        if is_recursive {
            let expected_build_hpp = if dir_path.is_empty() {
                UNITY_BUILD_HPP.to_string()
            } else {
                format!("{dir_path}/{UNITY_BUILD_HPP}")
            };
            let build_hpp_includes: Vec<&String> = impl_includes
                .iter()
                .filter(|name| name.ends_with(UNITY_BUILD_HPP))
                .collect();
            let non_build_includes: Vec<&String> = impl_includes
                .iter()
                .filter(|name| !name.ends_with(UNITY_BUILD_HPP))
                .collect();

            if build_hpp_includes.is_empty() {
                violations.push(format!(
                    "{rel_file}: Recursive build file must include '{expected_build_hpp}'."
                ));
            } else if build_hpp_includes.len() > 1 {
                let display: Vec<String> = build_hpp_includes
                    .iter()
                    .map(|name| format!("'{name}'"))
                    .collect();
                violations.push(format!(
                    "{rel_file}: Recursive build file must include exactly one _build.cpp.hpp, \
found {}: [{}]",
                    build_hpp_includes.len(),
                    display.join(", ")
                ));
            } else if build_hpp_includes[0] != &expected_build_hpp {
                violations.push(format!(
                    "{rel_file}: Expected include '{expected_build_hpp}', \
found '{}'.",
                    build_hpp_includes[0]
                ));
            }
            if !non_build_includes.is_empty() {
                let display: Vec<String> = non_build_includes
                    .iter()
                    .map(|name| format!("'{name}'"))
                    .collect();
                violations.push(format!(
                    "{rel_file}: Recursive build file should only include one _build.cpp.hpp, \
found extra .cpp.hpp includes: [{}]",
                    display.join(", ")
                ));
            }
        } else {
            let mapped_dir = if dir_path.is_empty() {
                src_dir.to_path_buf()
            } else {
                src_dir.join(&dir_path)
            };
            if !mapped_dir.exists() {
                let display = if dir_path.is_empty() { "src/" } else { &dir_path };
                violations.push(format!(
                    "{rel_file}: Mapped directory '{display}' does not exist."
                ));
                continue;
            }

            let build_hpp_includes: HashSet<&String> = impl_includes
                .iter()
                .filter(|name| name.ends_with(UNITY_BUILD_HPP))
                .collect();
            if !build_hpp_includes.is_empty() {
                let display: Vec<String> = build_hpp_includes
                    .iter()
                    .map(|name| format!("'{name}'"))
                    .collect();
                violations.push(format!(
                    "{rel_file}: Flat build file must NOT include _build.cpp.hpp files \
(found: [{}]). Use individual .cpp.hpp includes instead.",
                    display.join(", ")
                ));
            }

            let mut expected_files: HashSet<String> = HashSet::new();
            if let Some(cpp_hpp_files) = cpp_hpps_by_dir.get(&mapped_dir) {
                for cpp_hpp in cpp_hpp_files {
                    if cpp_hpp.file_name().and_then(|name| name.to_str()) == Some(UNITY_BUILD_HPP) {
                        continue;
                    }
                    if let Some(rel) =
                        pathdiff_normalized(&path_to_string(cpp_hpp), src_dir)
                    {
                        expected_files.insert(rel);
                    }
                }
            }
            let actual: HashSet<String> = impl_includes
                .iter()
                .filter(|name| !name.ends_with(UNITY_BUILD_HPP))
                .cloned()
                .collect();
            let mut missing: Vec<&String> = expected_files.difference(&actual).collect();
            missing.sort();
            for m in missing {
                violations.push(format!("{rel_file}: Missing include '{m}'."));
            }
            let mut extras: Vec<&String> = actual.difference(&expected_files).collect();
            extras.sort();
            for e in extras {
                let display = if dir_path.is_empty() { "src/" } else { &dir_path };
                violations.push(format!(
                    "{rel_file}: Include '{e}' does not belong in this flat build file \
(expected only files from '{display}')."
                ));
            }
        }
    }
    violations
}

fn check_no_orphan_cpp_files(src_dir: &Path, project_root: &Path) -> Vec<String> {
    let mut violations = Vec::new();
    let build_dir = src_dir.join("fl").join("build");
    let cpps = collect_files(src_dir, |path| {
        let Some(name) = path.file_name().and_then(|name| name.to_str()) else {
            return false;
        };
        path.extension().and_then(|ext| ext.to_str()) == Some("cpp") && !name.ends_with(".cpp.hpp")
    });
    for cpp_file in cpps {
        if cpp_file.starts_with(&build_dir) {
            continue;
        }
        let rel_file = rel_from_project(&cpp_file, project_root);
        violations.push(format!(
            "{rel_file}: .cpp file found outside src/fl/build/. \
All .cpp files must be in src/fl/build/ (unity build entry points). \
Implementation files should use .cpp.hpp extension."
        ));
    }
    violations
}

fn check_library_json_src_filter(project_root: &Path) -> Vec<String> {
    let mut violations = Vec::new();
    let library_json = project_root.join("library.json");
    if !library_json.exists() {
        violations.push("Missing library.json file".to_string());
        return violations;
    }
    let content = match std::fs::read_to_string(&library_json) {
        Ok(value) => value,
        Err(error) => {
            violations.push(format!("library.json: Invalid JSON - {error}"));
            return violations;
        }
    };
    let parsed: serde_json::Value = match serde_json::from_str(&content) {
        Ok(value) => value,
        Err(error) => {
            violations.push(format!("library.json: Invalid JSON - {error}"));
            return violations;
        }
    };
    let Some(build) = parsed.get("build") else {
        violations.push("library.json: Missing build.srcFilter configuration".to_string());
        return violations;
    };
    let Some(src_filter) = build.get("srcFilter").and_then(|value| value.as_array()) else {
        violations.push("library.json: Missing build.srcFilter configuration".to_string());
        return violations;
    };
    let entries: Vec<&str> = src_filter
        .iter()
        .filter_map(|value| value.as_str())
        .collect();
    let mut has_exclude = false;
    let mut has_build_glob = false;
    for entry in &entries {
        if *entry == UNITY_EXCLUDE_PATTERN {
            has_exclude = true;
        } else if *entry == UNITY_EXPECTED_SRC_FILTER {
            has_build_glob = true;
        }
    }
    if !has_exclude {
        violations.push(format!(
            "library.json: Missing {UNITY_EXCLUDE_PATTERN} exclude pattern in srcFilter"
        ));
    }
    if !has_build_glob {
        violations.push(format!(
            "library.json: Missing {UNITY_EXPECTED_SRC_FILTER} in srcFilter. \
This pattern auto-discovers all build files in src/fl/build/."
        ));
    }
    for entry in &entries {
        if UNITY_DANGEROUS_WILDCARDS.contains(entry) {
            violations.push(format!(
                "library.json: Found wildcard pattern '{entry}' - \
this will compile individual .cpp files instead of unity builds. \
Remove this pattern (unity builds are already included)."
            ));
        }
    }
    violations
}

// ---------------------------------------------------------------------------
// Helpers.
// ---------------------------------------------------------------------------

fn collect_files<F>(root: &Path, predicate: F) -> Vec<PathBuf>
where
    F: Fn(&Path) -> bool,
{
    let mut results = Vec::new();
    for entry in WalkDir::new(root).into_iter().filter_map(Result::ok) {
        if !entry.file_type().is_file() {
            continue;
        }
        let path = entry.into_path();
        if predicate(&path) {
            results.push(path);
        }
    }
    results
}

fn collect_cpp_hpps_by_dir(src_dir: &Path) -> HashMap<PathBuf, Vec<PathBuf>> {
    let mut map: HashMap<PathBuf, Vec<PathBuf>> = HashMap::new();
    for entry in WalkDir::new(src_dir).into_iter().filter_map(Result::ok) {
        if !entry.file_type().is_file() {
            continue;
        }
        let path = entry.into_path();
        if !path
            .file_name()
            .and_then(|name| name.to_str())
            .is_some_and(|name| name.ends_with(".cpp.hpp"))
        {
            continue;
        }
        if let Some(parent) = path.parent() {
            map.entry(parent.to_path_buf()).or_default().push(path);
        }
    }
    map
}

fn compute_independently_compiled_dirs(src_dir: &Path) -> HashSet<String> {
    let mut dirs = HashSet::new();
    let build_dir = src_dir.join("fl").join("build");
    let Ok(reader) = std::fs::read_dir(&build_dir) else {
        return dirs;
    };
    for entry in reader.filter_map(Result::ok) {
        let path = entry.path();
        if path.extension().and_then(|ext| ext.to_str()) != Some("cpp") {
            continue;
        }
        let Some(name) = path.file_name().and_then(|name| name.to_str()) else {
            continue;
        };
        let (dir_path, _) = parse_build_filename(name);
        dirs.insert(dir_path.clone());
        let mut parent = dir_path;
        while let Some((head, _)) = parent.rsplit_once('/') {
            parent = head.to_string();
            dirs.insert(parent.clone());
        }
    }
    dirs
}

fn parse_build_filename(name: &str) -> (String, bool) {
    let stem = name.strip_suffix(".cpp").unwrap_or(name);
    let (stem, recursive) = match stem.strip_suffix('+') {
        Some(without) => (without, true),
        None => (stem, false),
    };
    if stem == "src" {
        return (String::new(), recursive);
    }
    (stem.replace('.', "/"), recursive)
}

fn unity_namespace_path(file_path: &Path, src_dir: &Path) -> Option<String> {
    let normalized_file = normalize_path(&path_to_string(file_path));
    let normalized_src = normalize_path(&path_to_string(src_dir));
    let suffix = normalized_file.strip_prefix(&format!("{normalized_src}/"))?;
    let mut parts: Vec<&str> = suffix.split('/').collect();
    parts.pop(); // drop the filename
    Some(parts.join("/"))
}

fn unity_calculate_include_depth(included_path: &str, expected_prefix: &str) -> usize {
    let needle = format!("{expected_prefix}/");
    let Some(remaining) = included_path.strip_prefix(&needle) else {
        return 0;
    };
    let mut count = remaining.split('/').count();
    if count == 0 {
        return 0;
    }
    count -= 1;
    count
}

fn unity_line_number(content: &str, match_start: usize) -> usize {
    content[..match_start].matches('\n').count() + 1
}

fn rel_from_project(path: &Path, project_root: &Path) -> String {
    let normalized = normalize_path(&path_to_string(path));
    if let Some(rel) = project_relative_path(&normalized) {
        return rel;
    }
    let normalized_root = normalize_path(&path_to_string(project_root));
    normalized
        .strip_prefix(&format!("{normalized_root}/"))
        .map(str::to_string)
        .unwrap_or(normalized)
}

fn pathdiff_normalized(path: &str, base: &Path) -> Option<String> {
    let normalized_path = normalize_path(path);
    let normalized_base = normalize_path(&path_to_string(base));
    normalized_path
        .strip_prefix(&format!("{normalized_base}/"))
        .map(str::to_string)
}
