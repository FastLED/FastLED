struct SleepForChecker;

impl FileContentChecker for SleepForChecker {
    fn name(&self) -> &'static str {
        "SleepForChecker"
    }

    fn should_process_file(&self, file_path: &str, _project_root: &Path) -> bool {
        if !ends_with_any(file_path, &[".cpp", ".h", ".hpp", ".ino", ".cpp.hpp"]) {
            return false;
        }
        if is_excluded_file(file_path) {
            return false;
        }
        if file_path.contains("third_party") || file_path.contains("thirdparty") {
            return false;
        }
        let normalized = normalize_path(file_path);
        !SLEEP_FOR_WHITELISTED_SUFFIXES
            .iter()
            .any(|suffix| normalized.ends_with(suffix))
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
            if line.contains("// ok sleep for") || line.contains("// okay sleep for") {
                continue;
            }

            let code_part = split_line_comment(line);
            if !code_part.contains("sleep_for") {
                continue;
            }
            if regex_sleep_for().is_match(code_part) {
                violations.push((
                    index + 1,
                    format!(
                        "⚠️  CRITICAL: sleep_for() BLOCKS async/scheduler pumping! Async operations HANG, tasks FREEZE, UI becomes UNRESPONSIVE. USE fl::delay(ms) INSTEAD! Only suppress with '// ok sleep for' in core infrastructure: {stripped}"
                    ),
                ));
            }
        }

        violations
    }
}

struct ThreadLocalKeywordChecker;

impl FileContentChecker for ThreadLocalKeywordChecker {
    fn name(&self) -> &'static str {
        "ThreadLocalKeywordChecker"
    }

    fn should_process_file(&self, file_path: &str, _project_root: &Path) -> bool {
        ends_with_any(file_path, &[".cpp", ".h", ".hpp", ".ino"])
    }

    fn check_file_content(&self, file_content: &FileContent) -> Vec<(usize, String)> {
        if !file_content.content.contains("thread_local") {
            return Vec::new();
        }

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
            let code_without_strings = strip_string_literals(code_part);
            if regex_thread_local().is_match(&code_without_strings)
                && !line.contains("// ok thread_local")
            {
                violations.push((
                    index + 1,
                    format!(
                        "❌ Raw 'thread_local' keyword is banned — use fl::SingletonThreadLocal<T>::instance() instead (portable, never-destroyed, LSAN-safe): {stripped}"
                    ),
                ));
            }
        }

        violations
    }
}

struct SpanFromPointerChecker;

impl FileContentChecker for SpanFromPointerChecker {
    fn name(&self) -> &'static str {
        "SpanFromPointerChecker"
    }

    fn should_process_file(&self, file_path: &str, _project_root: &Path) -> bool {
        if !ends_with_any(file_path, &[".cpp", ".h", ".hpp", ".ino", ".cpp.hpp"]) {
            return false;
        }
        !file_path.contains("third_party") && !file_path.contains("thirdparty")
    }

    fn check_file_content(&self, file_content: &FileContent) -> Vec<(usize, String)> {
        if !file_content.content.contains("span<") && !file_content.content.contains("span <") {
            return Vec::new();
        }

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
            if line.contains("// ok span from pointer") {
                continue;
            }

            let code_part = split_line_comment(line);
            if !code_part.contains(".data()") || !code_part.contains(".size()") {
                continue;
            }
            if !code_part.contains("span<") && !code_part.contains("span <") {
                continue;
            }
            if regex_span_data_size().is_match(code_part) {
                violations.push((
                    index + 1,
                    format!(
                        "{stripped}  →  Use span<T>(container) instead of span<T>(container.data(), container.size()). Suppress with '// ok span from pointer'"
                    ),
                ));
            }
        }

        violations
    }
}

struct RelativeIncludeChecker;

impl FileContentChecker for RelativeIncludeChecker {
    fn name(&self) -> &'static str {
        "RelativeIncludeChecker"
    }

    fn should_process_file(&self, file_path: &str, project_root: &Path) -> bool {
        is_under_project_subpath(file_path, project_root, "src")
            && ends_with_any(file_path, &[".cpp", ".h", ".hpp", ".cc", ".cxx", ".ino"])
    }

    fn check_file_content(&self, file_content: &FileContent) -> Vec<(usize, String)> {
        const ALLOWED_RELATIVE_INCLUDE_FILES: &[&str] = &[
            "src/platforms/win/run_example.hpp",
            "src/platforms/posix/run_example.hpp",
            "src/platforms/apple/run_example.hpp",
        ];

        let normalized_path = normalize_path(&file_content.path);
        if ALLOWED_RELATIVE_INCLUDE_FILES
            .iter()
            .any(|allowed_file| normalized_path.ends_with(allowed_file))
        {
            return Vec::new();
        }

        let mut violations = Vec::new();
        let mut in_block_comment = false;
        for (index, line) in file_content.lines.iter().enumerate() {
            let visible_line = strip_block_comments_from_line(line, &mut in_block_comment);
            if regex_relative_include().is_match(&visible_line)
                && !line.contains("// ok relative include")
            {
                violations.push((index + 1, format!("Relative include: {}", line.trim())));
            }
        }

        violations
    }
}

struct FastLEDHeaderUsageChecker;

impl FileContentChecker for FastLEDHeaderUsageChecker {
    fn name(&self) -> &'static str {
        "FastLEDHeaderUsageChecker"
    }

    fn should_process_file(&self, file_path: &str, project_root: &Path) -> bool {
        if !is_under_project_subpath(file_path, project_root, "src") {
            return false;
        }
        if !ends_with_any(file_path, &[".h", ".hpp"]) {
            return false;
        }
        let normalized = normalize_path(file_path);
        let basename = normalized.rsplit('/').next().unwrap_or(&normalized);
        !matches!(basename, "FastLED.h" | "fastspi.h")
    }

    fn check_file_content(&self, file_content: &FileContent) -> Vec<(usize, String)> {
        let mut violations = Vec::new();
        let mut in_block_comment = false;
        let visible_lines: Vec<String> = file_content
            .lines
            .iter()
            .map(|line| strip_block_comments_from_line(line, &mut in_block_comment))
            .collect();

        for (index, visible_line) in visible_lines.iter().enumerate() {
            if !regex_fastled_h_include().is_match(visible_line) {
                continue;
            }
            let line = &file_content.lines[index];
            let lower = line.to_ascii_lowercase();
            if lower.contains("// ok include") {
                continue;
            }

            let lookback = (index).min(5);
            let has_internal_define = (1..=lookback)
                .map(|offset| &visible_lines[index - offset])
                .any(|prev_line| regex_fastled_internal_define().is_match(prev_line));
            if !has_internal_define {
                violations.push((
                    index + 1,
                    format!(
                        "Use 'fl/system/fastled.h' instead of 'FastLED.h': {}",
                        line.trim()
                    ),
                ));
            }
        }

        violations
    }
}

struct ArduinoMacroUsageChecker;

impl FileContentChecker for ArduinoMacroUsageChecker {
    fn name(&self) -> &'static str {
        "ArduinoMacroUsageChecker"
    }

    fn should_process_file(&self, file_path: &str, project_root: &Path) -> bool {
        if !ends_with_any(file_path, &[".cpp", ".h", ".hpp", ".ino", ".cpp.hpp"]) {
            return false;
        }
        if !is_under_project_subpath(file_path, project_root, "src") {
            return false;
        }
        if is_under_dir(file_path, "platforms") || is_under_dir(file_path, "third_party") {
            return false;
        }
        true
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
            if regex_arduino_preprocessor().is_match(code_part)
                || regex_arduino_scoped_enum().is_match(code_part)
                || regex_arduino_enum_member().is_match(code_part)
            {
                continue;
            }

            for name in ARDUINO_BANNED_MACROS {
                let Some(found) = regex_arduino_macro()
                    .captures_iter(code_part)
                    .find(|capture| capture.get(1).is_some_and(|m| m.as_str() == *name))
                else {
                    continue;
                };
                let macro_name = found.get(1).unwrap().as_str();
                violations.push((
                    index + 1,
                    format!(
                        "Banned Arduino macro '{macro_name}' used: {stripped}\n      These macros pollute the global namespace and conflict with Windows headers.\n      Use platform-specific APIs or define local constants instead."
                    ),
                ));
            }
        }

        violations
    }
}

struct AttributeChecker;

impl FileContentChecker for AttributeChecker {
    fn name(&self) -> &'static str {
        "AttributeChecker"
    }

    fn should_process_file(&self, file_path: &str, _project_root: &Path) -> bool {
        if !ends_with_any(file_path, &[".cpp", ".h", ".hpp", ".ino", ".cpp.hpp"]) {
            return false;
        }
        let normalized = normalize_path(file_path);
        if normalized.ends_with("compiler_control.h") {
            return false;
        }
        if is_under_dir(&normalized, "third_party") {
            return false;
        }
        !ends_with_any(&normalized, &["doctest.h", "catch.hpp", "gtest.h"])
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
            if code_part.contains("#define") && code_part.contains("FL_") {
                continue;
            }
            if !code_part.contains("[[") && !code_part.contains("alignas") {
                continue;
            }

            if code_part.contains("[[") {
                for capture in regex_standard_attribute().captures_iter(code_part) {
                    let Some(attribute) = capture.get(1).map(|m| m.as_str()) else {
                        continue;
                    };
                    if let Some(fl_macro) = standard_attribute_replacement(attribute) {
                        violations.push((
                            index + 1,
                            format!("Use {fl_macro} instead of [[{attribute}]]: {stripped}"),
                        ));
                    }
                }
            }

            if code_part.contains("alignas")
                && regex_alignas().is_match(code_part)
                && !code_part.contains("FL_ALIGNAS")
            {
                violations.push((
                    index + 1,
                    format!("Use FL_ALIGNAS instead of alignas: {stripped}"),
                ));
            }
        }

        violations
    }
}

struct FlIsDefinedChecker;

impl FileContentChecker for FlIsDefinedChecker {
    fn name(&self) -> &'static str {
        "FlIsDefinedChecker"
    }

    fn should_process_file(&self, file_path: &str, _project_root: &Path) -> bool {
        ends_with_any(file_path, &[".cpp", ".h", ".hpp", ".ino"])
    }

    fn check_file_content(&self, file_content: &FileContent) -> Vec<(usize, String)> {
        if !file_content.content.contains("FL_IS_") {
            return Vec::new();
        }

        let mut violations = Vec::new();
        let mut in_multiline_comment = false;

        for (index, line) in file_content.lines.iter().enumerate() {
            if line.contains("/*") {
                in_multiline_comment = true;
            }
            if line.contains("*/") {
                in_multiline_comment = false;
                continue;
            }
            if in_multiline_comment {
                continue;
            }

            let stripped = line.trim();
            if stripped.starts_with("//") || !regex_preprocessor_if_elif().is_match(stripped) {
                continue;
            }

            let code_part = split_line_comment(line);
            if !regex_fl_is_token().is_match(code_part) {
                continue;
            }
            let stripped_code = regex_defined_fl_is().replace_all(code_part, "");
            for token in regex_fl_is_token().find_iter(&stripped_code) {
                violations.push((
                    index + 1,
                    format!(
                        "Bare '{}' in preprocessor conditional. FL_IS_* macros are defined/undefined (no value). Use '#ifdef {}' or '#if defined({})' instead.",
                        token.as_str(),
                        token.as_str(),
                        token.as_str()
                    ),
                ));
            }
        }

        violations
    }
}

struct NumericLimitMacroChecker;

impl FileContentChecker for NumericLimitMacroChecker {
    fn name(&self) -> &'static str {
        "NumericLimitMacroChecker"
    }

    fn should_process_file(&self, file_path: &str, _project_root: &Path) -> bool {
        if !ends_with_any(file_path, &[".cpp", ".h", ".hpp", ".ino"]) {
            return false;
        }
        if is_excluded_file(file_path) {
            return false;
        }
        let normalized = normalize_path(file_path);
        !NUMERIC_LIMIT_EXCLUDED_SUFFIXES
            .iter()
            .any(|suffix| normalized.ends_with(suffix))
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

            if line.contains("// okay numeric limit macro") {
                continue;
            }
            let code_part = split_line_comment(line);
            if !code_part.contains("_MAX") && !code_part.contains("_MIN") {
                continue;
            }

            let Some(capture) = regex_numeric_limit_macro().captures(code_part) else {
                continue;
            };
            let macro_name = capture.get(1).unwrap().as_str();
            let suggestion = numeric_limit_suggestion(macro_name);
            violations.push((index + 1, format!("{stripped} (use {suggestion} instead)")));
        }

        violations
    }
}

