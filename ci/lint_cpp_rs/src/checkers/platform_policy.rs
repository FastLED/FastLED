struct EnumClassChecker;

impl FileContentChecker for EnumClassChecker {
    fn name(&self) -> &'static str {
        "EnumClassChecker"
    }

    fn should_process_file(&self, file_path: &str, project_root: &Path) -> bool {
        let normalized = normalize_path(file_path);
        if !ends_with_any(&normalized, &[".cpp", ".h", ".hpp", ".ino", ".cpp.hpp"]) {
            return false;
        }
        if is_excluded_file(&normalized)
            || normalized.contains("third_party")
            || normalized.contains("thirdparty")
            || normalized.contains("/examples/")
        {
            return false;
        }
        is_under_project_subpath(&normalized, project_root, "src/fl")
            || is_under_project_subpath(&normalized, project_root, "src/platforms")
    }

    fn check_file_content(&self, file_content: &FileContent) -> Vec<(usize, String)> {
        let mut violations = Vec::new();
        let mut in_multiline_comment = false;
        let mut brace_depth = 0_i32;
        let mut class_struct_enter_depths: Vec<i32> = Vec::new();
        let mut pending_class_struct = false;

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
            if regex_class_struct().is_match(code_part) {
                let before_brace = code_part.split('{').next().unwrap_or(code_part);
                if !before_brace.contains(';') {
                    pending_class_struct = true;
                }
            }

            for ch in code_part.chars() {
                if ch == '{' {
                    if pending_class_struct {
                        class_struct_enter_depths.push(brace_depth);
                        pending_class_struct = false;
                    }
                    brace_depth += 1;
                } else if ch == '}' {
                    brace_depth -= 1;
                    if class_struct_enter_depths
                        .last()
                        .is_some_and(|depth| brace_depth == *depth)
                    {
                        class_struct_enter_depths.pop();
                    }
                }
            }

            if line.contains("// ok plain enum") || line.contains("// okay plain enum") {
                continue;
            }
            if !code_part.contains("enum") || !class_struct_enter_depths.is_empty() {
                continue;
            }
            if regex_named_enum().is_match(code_part) {
                violations.push((
                    index + 1,
                    format!(
                        "Plain enum detected - use 'enum class' for type safety. Plain enums leak names into enclosing scope and allow implicit integer conversions. Suppress with '// ok plain enum' for legacy enums: {stripped}"
                    ),
                ));
            }
        }

        violations
    }
}

struct PlatformsFlNamespaceChecker;

impl FileContentChecker for PlatformsFlNamespaceChecker {
    fn name(&self) -> &'static str {
        "PlatformsFlNamespaceChecker"
    }

    fn should_process_file(&self, file_path: &str, project_root: &Path) -> bool {
        let normalized = normalize_path(file_path);
        if !is_under_project_subpath(&normalized, project_root, "src/platforms") {
            return false;
        }
        if normalized.contains("/examples/") || normalized.contains("/tests/") {
            return false;
        }
        // Vendored upstream code under any `/third_party/` directory is
        // byte-identical with its source and uses vendor-native namespacing
        // (e.g. no `namespace fl`). Skip.
        if normalized.contains("/third_party/") {
            return false;
        }
        if ends_with_any(&normalized, &["_build.hpp", "_build.cpp", "_build.cpp.hpp"]) {
            return false;
        }
        ends_with_any(&normalized, &[".h", ".cpp", ".hpp"])
    }

    fn check_file_content(&self, file_content: &FileContent) -> Vec<(usize, String)> {
        if file_content.content.contains("namespace fl")
            || file_content.content.contains("// ok no namespace fl")
        {
            return Vec::new();
        }
        vec![(
            0,
            "Missing 'namespace fl {' or '// ok no namespace fl' comment".to_string(),
        )]
    }
}

struct NamespacePlatformsChecker;

impl FileContentChecker for NamespacePlatformsChecker {
    fn name(&self) -> &'static str {
        "NamespacePlatformsChecker"
    }

    fn should_process_file(&self, file_path: &str, project_root: &Path) -> bool {
        let normalized = normalize_path(file_path);
        is_under_project_subpath(&normalized, project_root, "src/platforms")
            && ends_with_any(&normalized, &[".cpp", ".h", ".hpp"])
    }

    fn check_file_content(&self, file_content: &FileContent) -> Vec<(usize, String)> {
        let mut violations = Vec::new();
        let mut in_multiline_comment = false;

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
            if regex_namespace_platform_singular().is_match(code_part) {
                violations.push((index + 1, stripped.to_string()));
            }
        }

        violations
    }
}

struct LoggingInIramChecker;

impl FileContentChecker for LoggingInIramChecker {
    fn name(&self) -> &'static str {
        "LoggingInIramChecker"
    }

    fn should_process_file(&self, file_path: &str, _project_root: &Path) -> bool {
        let normalized = normalize_path(file_path);
        if is_excluded_file(&normalized) || !ends_with_any(&normalized, &[".cpp", ".h", ".hpp"]) {
            return false;
        }
        normalized.contains("/platforms/") || normalized.contains("/fl/")
    }

    fn check_file_content(&self, file_content: &FileContent) -> Vec<(usize, String)> {
        let full_content = file_content.lines.join("\n");
        if !full_content.contains("FL_IRAM") {
            return Vec::new();
        }

        let cleaned_content = strip_comments_preserving_lines(&file_content.lines);
        let mut violations = Vec::new();

        for function_match in regex_iram_function().captures_iter(&cleaned_content) {
            let Some(whole_match) = function_match.get(0) else {
                continue;
            };
            let func_name = function_match.get(2).map_or("", |value| value.as_str());
            let func_start = whole_match.end();
            let mut brace_depth = 1_i32;
            let mut func_body_end = func_start;
            for (offset, ch) in cleaned_content[func_start..].char_indices() {
                if ch == '{' {
                    brace_depth += 1;
                } else if ch == '}' {
                    brace_depth -= 1;
                    if brace_depth == 0 {
                        func_body_end = func_start + offset;
                        break;
                    }
                }
            }
            let func_body = &cleaned_content[func_start..func_body_end];

            for macro_match in regex_iram_banned_macro().captures_iter(func_body) {
                let macro_name = macro_match.get(1).unwrap().as_str();
                let match_pos = func_start + macro_match.get(0).unwrap().start();
                let line_number = cleaned_content[..match_pos].matches('\n').count() + 1;
                let original_line = file_content
                    .lines
                    .get(line_number.saturating_sub(1))
                    .map_or("", |line| line.trim());
                violations.push((
                    line_number,
                    format!(
                        "Found '{macro_name}' in FL_IRAM function '{func_name}'\n  Line: {}\n  Logging macros cannot be used in ISR functions marked with FL_IRAM.",
                        original_line.chars().take(100).collect::<String>()
                    ),
                ));
            }

            for log_match in regex_fl_log_macro().find_iter(func_body) {
                let macro_call = log_match.as_str();
                let macro_name = macro_call.split('(').next().unwrap_or(macro_call).trim();
                if macro_name.contains("ASYNC") {
                    continue;
                }
                let match_pos = func_start + log_match.start();
                let line_number = cleaned_content[..match_pos].matches('\n').count() + 1;
                let original_line = file_content
                    .lines
                    .get(line_number.saturating_sub(1))
                    .map_or("", |line| line.trim());
                violations.push((
                    line_number,
                    format!(
                        "Found '{macro_name}' in FL_IRAM function '{func_name}'\n  Line: {}\n  Logging macros cannot be used in ISR functions marked with FL_IRAM.",
                        original_line.chars().take(100).collect::<String>()
                    ),
                ));
            }
        }

        violations
    }
}

