struct BareUsingChecker;

impl FileContentChecker for BareUsingChecker {
    fn name(&self) -> &'static str {
        "BareUsingChecker"
    }

    fn should_process_file(&self, file_path: &str, project_root: &Path) -> bool {
        let normalized = normalize_path(file_path);
        if !is_under_project_subpath(&normalized, project_root, "src/fl") {
            return false;
        }
        if normalized.contains("/third_party/") {
            return false;
        }
        let basename = normalized.rsplit('/').next().unwrap_or(&normalized);
        if basename == "FastLED.h" {
            return false;
        }
        ends_with_any(&normalized, &[".h", ".hpp", ".cpp.hpp"])
    }

    fn check_file_content(&self, file_content: &FileContent) -> Vec<(usize, String)> {
        let mut violations = Vec::new();
        let mut scope_stack: Vec<&str> = Vec::new();
        let mut in_block_comment = false;

        for (index, line) in file_content.lines.iter().enumerate() {
            if in_block_comment {
                if line.contains("*/") {
                    in_block_comment = false;
                }
                continue;
            }

            let line_before_line_comment = split_line_comment(line);
            if let Some(start_pos) = line_before_line_comment.find("/*") {
                let rest = &line[start_pos + 2..];
                if !rest.contains("*/") {
                    in_block_comment = true;
                }
                continue;
            }

            if regex_preprocessor_line().is_match(line) {
                continue;
            }

            let without_strings = strip_string_literals(line);
            let clean = split_line_comment(&without_strings).to_string();
            let mut open_braces = clean.matches('{').count();
            let close_braces = clean.matches('}').count();
            let mut scopes_to_push: Vec<&str> = Vec::new();

            if open_braces > 0 {
                if regex_anon_namespace_open().is_match(&clean) {
                    scopes_to_push.push("local");
                    open_braces -= 1;
                } else if regex_named_namespace_open().is_match(&clean) {
                    scopes_to_push.push("namespace");
                    open_braces -= 1;
                } else if regex_extern_c_open().is_match(&clean) {
                    scopes_to_push.push("namespace");
                    open_braces -= 1;
                } else if regex_local_scope_keyword().is_match(&clean) {
                    scopes_to_push.push("local");
                    open_braces -= 1;
                }
                scopes_to_push.extend(std::iter::repeat("local").take(open_braces));
            }

            scope_stack.extend(scopes_to_push);
            for _ in 0..close_braces {
                if !scope_stack.is_empty() {
                    scope_stack.pop();
                }
            }

            if regex_bare_using_decl().is_match(&clean) {
                if regex_bare_using_suppress().is_match(line) {
                    continue;
                }
                if scope_stack.iter().all(|scope| *scope == "namespace") {
                    violations.push((index + 1, line.trim().to_string()));
                }
            }
        }

        violations
    }
}

struct CtypeGlobalChecker;

impl FileContentChecker for CtypeGlobalChecker {
    fn name(&self) -> &'static str {
        "CtypeGlobalChecker"
    }

    fn should_process_file(&self, file_path: &str, project_root: &Path) -> bool {
        let normalized = normalize_path(file_path);
        if !is_under_project_subpath(&normalized, project_root, "src")
            && !is_under_project_subpath(&normalized, project_root, "tests")
        {
            return false;
        }
        if !ends_with_any(&normalized, &[".cpp", ".h", ".hpp", ".ino", ".cpp.hpp"]) {
            return false;
        }
        if is_excluded_file(&normalized)
            || normalized.contains("third_party")
            || normalized.contains("thirdparty")
        {
            return false;
        }
        !CTYPE_WHITELISTED_SUFFIXES
            .iter()
            .any(|suffix| normalized.ends_with(suffix))
    }

    fn check_file_content(&self, file_content: &FileContent) -> Vec<(usize, String)> {
        // Whole-file pre-flight gate. The combined static regex matches in
        // a single pass over the content instead of 28 separate `contains`
        // scans (one per ctype/cstring function name) - the old code paid
        // O(28 * content_len) per file just to decide "no work to do".
        if !ctype_any_function_regex().is_match(&file_content.content) {
            return Vec::new();
        }

        let mut violations = Vec::new();
        let mut in_multiline_comment = false;
        let mut fl_namespace_depth = 0_i32;
        let mut brace_depth_at_fl_namespace: Vec<i32> = Vec::new();
        let mut brace_depth = 0_i32;

        for (index, line) in file_content.lines.iter().enumerate() {
            let stripped = line.trim();
            if line.contains("/*") {
                in_multiline_comment = true;
            }
            if line.contains("*/") {
                in_multiline_comment = false;
                continue;
            }
            if in_multiline_comment || stripped.starts_with("//") {
                continue;
            }

            let code_part = split_line_comment(line);
            if code_part.contains("namespace")
                && regex_namespace_fl_declaration().is_match(code_part)
            {
                fl_namespace_depth += 1;
                brace_depth_at_fl_namespace.push(brace_depth);
            }
            for ch in code_part.chars() {
                if ch == '{' {
                    brace_depth += 1;
                } else if ch == '}' {
                    brace_depth -= 1;
                    if brace_depth_at_fl_namespace
                        .last()
                        .is_some_and(|depth| brace_depth == *depth)
                    {
                        fl_namespace_depth -= 1;
                        brace_depth_at_fl_namespace.pop();
                    }
                }
            }

            if line.contains("// ok ctype") || line.contains("// okay ctype") {
                continue;
            }
            // Same single-regex gate at the per-line level - skip the
            // 28-substring-scan loop unless the line plausibly mentions
            // one of the watched functions.
            if !ctype_any_function_regex().is_match(code_part) {
                continue;
            }

            let calls = find_ctype_calls(code_part);
            if calls.is_empty() {
                continue;
            }

            if fl_namespace_depth > 0 {
                let global_funcs: BTreeSet<&str> = calls
                    .iter()
                    .filter_map(|(func, is_global)| (*is_global).then_some(*func))
                    .collect();
                for func in global_funcs {
                    let header = ctype_header(func);
                    violations.push((
                        index + 1,
                        format!(
                            "Use fl::{func}() instead of ::{func}() — see {header}: {stripped}"
                        ),
                    ));
                }
                continue;
            }

            let funcs: BTreeSet<&str> = calls.iter().map(|(func, _)| *func).collect();
            for func in funcs {
                let header = ctype_header(func);
                violations.push((
                    index + 1,
                    format!(
                        "Use fl::{func}() instead of {func}() or ::{func}() — see {header}: {stripped}"
                    ),
                ));
            }
        }

        violations
    }
}

struct StdintTypeChecker;

impl FileContentChecker for StdintTypeChecker {
    fn name(&self) -> &'static str {
        "StdintTypeChecker"
    }

    fn should_process_file(&self, file_path: &str, project_root: &Path) -> bool {
        let normalized = normalize_path(file_path);
        if !is_under_project_subpath(&normalized, project_root, "src") {
            return false;
        }
        if !ends_with_any(&normalized, &[".cpp", ".h", ".hpp", ".cpp.hpp"]) {
            return false;
        }
        let filename = normalized.rsplit('/').next().unwrap_or(&normalized);
        if STDINT_EXCLUDED_FILENAMES.contains(&filename) {
            return false;
        }
        if normalized.contains("/third_party/") || normalized.contains("/fl/stl/") {
            return false;
        }
        if normalized.contains("/platforms/") {
            if filename.starts_with("int_") && filename.ends_with(".h") {
                return false;
            }
            if filename == "Arduino.h" || filename == "Arduino.cpp.hpp" {
                return false;
            }
            if normalized.contains("/stub/") || normalized.contains("/test/") {
                return false;
            }
        }
        true
    }

    fn check_file_content(&self, file_content: &FileContent) -> Vec<(usize, String)> {
        if !file_content.content.contains("int") {
            return Vec::new();
        }

        let mut violations = Vec::new();
        let mut in_multiline_comment = false;
        let mut in_extern_c_block = false;

        for (index, line) in file_content.lines.iter().enumerate() {
            if !in_multiline_comment
                && !line.contains("int")
                && !line.contains("/*")
                && !line.contains("*/")
            {
                continue;
            }

            let trimmed_start = line.trim_start_matches(|ch| ch == ' ' || ch == '\t');
            let is_line_comment = trimmed_start.starts_with("//");
            if is_line_comment && !in_multiline_comment {
                continue;
            }

            if line.contains("extern \"C\"") {
                if line.contains('{') {
                    in_extern_c_block = true;
                } else if line.contains(';') {
                    continue;
                }
            }
            if in_extern_c_block && line.contains('}') {
                in_extern_c_block = false;
                continue;
            }
            if in_extern_c_block {
                continue;
            }

            if in_multiline_comment {
                if line.contains("*/") {
                    in_multiline_comment = false;
                }
                continue;
            }

            let code_for_comment_tracking = split_line_comment(line);
            if let Some(open_pos) = code_for_comment_tracking.find("/*") {
                let close_pos = code_for_comment_tracking[open_pos + 2..].find("*/");
                if close_pos.is_none() {
                    in_multiline_comment = true;
                    continue;
                }
            }

            if !line.contains("int") {
                continue;
            }

            let mut code_part = split_line_comment(line).to_string();
            while let Some(open_pos) = code_part.find("/*") {
                let Some(close_relative) = code_part[open_pos + 2..].find("*/") else {
                    code_part.truncate(open_pos);
                    break;
                };
                let close_pos = open_pos + 2 + close_relative;
                code_part.replace_range(open_pos..close_pos + 2, " ");
            }

            for match_type in find_stdint_matches(&code_part) {
                let fl_type = stdint_fl_type(match_type);
                violations.push((
                    index + 1,
                    format!(
                        "Use '{fl_type}' from fl/stl/int.h instead of '{match_type}': {}",
                        line.trim()
                    ),
                ));
            }
        }

        violations
    }
}

struct SubdirNamespaceChecker {
    subdir: &'static str,
}

impl FileContentChecker for SubdirNamespaceChecker {
    fn name(&self) -> &'static str {
        "SubdirNamespaceChecker"
    }

    fn should_process_file(&self, file_path: &str, project_root: &Path) -> bool {
        let subpath = format!("src/fl/{}", self.subdir);
        is_under_project_subpath(file_path, project_root, &subpath) && file_path.ends_with(".h")
    }

    fn check_file_content(&self, file_content: &FileContent) -> Vec<(usize, String)> {
        let expected = ["fl", self.subdir];
        let mut found_parts: HashSet<String> = HashSet::new();
        let mut has_any_namespace = false;

        for line in &file_content.lines {
            let stripped = line.trim();
            if stripped.starts_with("//") || stripped.starts_with("/*") || stripped.starts_with('*')
            {
                continue;
            }
            let code = split_line_comment(stripped);
            for capture in regex_subdir_namespace_decl().captures_iter(code) {
                has_any_namespace = true;
                for part in capture[1].split("::").filter(|part| !part.is_empty()) {
                    found_parts.insert(part.to_string());
                }
            }
        }

        if !has_any_namespace {
            return Vec::new();
        }

        let missing: Vec<&str> = expected
            .iter()
            .copied()
            .filter(|part| !found_parts.contains(*part))
            .collect();
        if missing.is_empty() {
            return Vec::new();
        }

        let expected_ns = expected.join("::");
        vec![(
            1,
            format!(
                "Header in fl/{}/ must use namespace {expected_ns}; missing namespace part(s): {}",
                self.subdir,
                missing.join(", ")
            ),
        )]
    }
}

struct UnitTestChecker;

impl FileContentChecker for UnitTestChecker {
    fn name(&self) -> &'static str {
        "UnitTestChecker"
    }

    fn should_process_file(&self, file_path: &str, project_root: &Path) -> bool {
        let normalized = normalize_path(file_path);
        if !is_under_project_subpath(&normalized, project_root, "tests") {
            return false;
        }
        if !ends_with_any(&normalized, &[".cpp", ".h", ".hpp"]) {
            return false;
        }
        let basename = normalized.rsplit('/').next().unwrap_or(&normalized);
        !UNIT_TEST_EXEMPT_FILES.contains(&basename)
    }

    fn check_file_content(&self, file_content: &FileContent) -> Vec<(usize, String)> {
        let mut violations = Vec::new();

        for (index, line) in file_content.lines.iter().enumerate() {
            let stripped = line.trim();
            if stripped.starts_with("//") {
                continue;
            }

            if stripped.contains("#include \"doctest.h\"")
                || stripped.contains("#include <doctest.h>")
            {
                violations.push((
                    index + 1,
                    "Use #include \"test.h\" instead of #include \"doctest.h\"".to_string(),
                ));
            }

            if !UNIT_TEST_FAST_PREFIXES
                .iter()
                .any(|prefix| line.contains(prefix))
            {
                continue;
            }

            for capture in regex_unit_test_macro_call().captures_iter(line) {
                let macro_name = &capture[1];
                if let Some(fl_macro) = unit_test_fl_macro(macro_name) {
                    violations.push((
                        index + 1,
                        format!("Use {fl_macro}() instead of bare {macro_name}()"),
                    ));
                }
            }
        }

        violations
    }
}

struct HeadersExistChecker;

impl FileContentChecker for HeadersExistChecker {
    fn name(&self) -> &'static str {
        "HeadersExistChecker"
    }

    fn should_process_file(&self, file_path: &str, project_root: &Path) -> bool {
        let normalized = normalize_path(file_path);
        if !is_under_project_subpath(&normalized, project_root, "tests") {
            return false;
        }
        if !ends_with_any(&normalized, &[".cpp", ".hpp"]) {
            return false;
        }
        let name = normalized.rsplit('/').next().unwrap_or(&normalized);
        if HEADERS_EXIST_EXCLUDED_FILES.contains(&name) {
            return false;
        }
        let Some(rel) = tests_relative_path(&normalized) else {
            return false;
        };
        let parts: Vec<&str> = rel.split('/').collect();
        if parts
            .iter()
            .any(|part| matches!(*part, "core" | "shared" | "test_utils"))
        {
            return false;
        }
        !parts.iter().any(|part| {
            *part == "build"
                || part.starts_with(".build-")
                || *part == "example_compile_direct"
                || *part == "CMakeFiles"
        })
    }

    fn check_file_content(&self, file_content: &FileContent) -> Vec<(usize, String)> {
        let normalized = normalize_path(&file_content.path);
        let root_prefix = project_root_prefix_for_file(&normalized);
        let Some(test_rel) = tests_relative_path(&normalized) else {
            return Vec::new();
        };
        let first_part = test_rel.split('/').next().unwrap_or("");
        if first_part == "misc" || first_part == "profile" {
            return Vec::new();
        }
        if file_content
            .lines
            .iter()
            .take(5)
            .any(|line| line.to_lowercase().contains("// ok standalone"))
        {
            return Vec::new();
        }

        let includes: Vec<String> = file_content
            .lines
            .iter()
            .filter_map(|line| regex_quoted_include_line().captures(line))
            .filter_map(|capture| {
                let header = capture[1].to_string();
                (!header.contains("test.h")
                    && !header.starts_with("testing/")
                    && !header.starts_with("test_utils/"))
                .then_some(header)
            })
            .collect();

        let rel_no_ext = path_without_extension(&test_rel);
        let base_name = normalized
            .rsplit('/')
            .next()
            .and_then(|name| name.rsplit_once('.').map(|(stem, _)| stem))
            .unwrap_or("");
        let parent_rel = rel_no_ext.rsplit_once('/').map_or("", |(parent, _)| parent);
        let primary_rel = if parent_rel.is_empty() {
            format!("src/{base_name}.h")
        } else {
            format!("src/{parent_rel}/{base_name}.h")
        };
        let primary_exists = join_project_path(&root_prefix, &primary_rel).exists();
        let fallback_candidates: Vec<String> = includes
            .iter()
            .map(|include| format!("src/{include}"))
            .filter(|candidate| candidate != &primary_rel)
            .collect();
        let fallback_exists = fallback_candidates
            .iter()
            .any(|candidate| join_project_path(&root_prefix, candidate).exists());

        if !primary_exists && !fallback_exists {
            let test_full_rel = project_relative_path(&normalized).unwrap_or(test_rel.clone());
            let mut parts = vec![
                format!(
                    "Test file {} has no corresponding header in src/",
                    python_path_display(&test_full_rel)
                ),
                format!("  Expected: {}", python_path_display(&primary_rel)),
            ];
            if includes.is_empty() {
                parts.push("  No project headers included!".to_string());
            } else {
                parts.push(format!(
                    "  Includes: {}",
                    includes
                        .iter()
                        .take(3)
                        .cloned()
                        .collect::<Vec<_>>()
                        .join(", ")
                ));
                if includes.len() > 3 {
                    parts.push(format!("  ... and {} more", includes.len() - 3));
                }
            }
            return vec![(0, parts.join("\n"))];
        }

        if !primary_exists && fallback_exists && !includes.is_empty() {
            let test_dir_path = parent_rel;
            let mut any_include_matches = false;
            let mut first_mismatched_include_dir: Option<String> = None;
            for include in &includes {
                let include_dir = include.rsplit_once('/').map_or("", |(dir, _)| dir);
                if !test_dir_path.is_empty()
                    && !include_dir.is_empty()
                    && test_dir_path == include_dir
                {
                    any_include_matches = true;
                    break;
                }
                if first_mismatched_include_dir.is_none()
                    && !test_dir_path.is_empty()
                    && !include_dir.is_empty()
                    && test_dir_path != include_dir
                {
                    first_mismatched_include_dir = Some(include_dir.to_string());
                }
            }
            if !any_include_matches
                && first_mismatched_include_dir.is_some()
                && !source_mirror_dir_has_headers(&root_prefix, test_dir_path)
            {
                let include_dir = first_mismatched_include_dir.unwrap();
                let test_full_rel = project_relative_path(&normalized).unwrap_or(test_rel);
                return vec![(
                    0,
                    format!(
                        "⚠️  Test file {} may be in wrong directory:\n  Test location: tests/{test_dir_path}/\n  Includes headers from: src/{include_dir}/\n  Expected location: tests/{include_dir}/",
                        python_path_display(&test_full_rel)
                    ),
                )];
            }
        }

        Vec::new()
    }
}

