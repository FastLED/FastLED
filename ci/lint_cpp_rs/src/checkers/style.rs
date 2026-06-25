struct PlatformPragmaChecker;

impl FileContentChecker for PlatformPragmaChecker {
    fn name(&self) -> &'static str {
        "PlatformPragmaChecker"
    }

    fn should_process_file(&self, file_path: &str, _project_root: &Path) -> bool {
        if !ends_with_any(file_path, &[".cpp", ".h", ".hpp", ".ino", ".cpp.hpp"]) {
            return false;
        }
        let normalized = normalize_path(file_path);
        !is_under_dir(&normalized, "third_party")
            && !is_under_dir(&normalized, "platforms")
            && !normalized.ends_with("compiler_control.h")
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

            if index >= 1 && file_content.lines[index - 1].contains("FL_ALLOW_PLATFORM_PRAGMA") {
                continue;
            }
            if line.contains("FL_ALLOW_PLATFORM_PRAGMA") {
                continue;
            }
            if regex_platform_pragma().is_match(line) {
                violations.push((
                    index + 1,
                    format!(
                        "Raw platform-specific pragma: {stripped}\n      Use FL_DISABLE_WARNING_PUSH / FL_DISABLE_WARNING(<name>) / FL_DISABLE_WARNING_POP from fl/stl/compiler_control.h.\n      If the FL macros cannot express this pragma, place FL_ALLOW_PLATFORM_PRAGMA on the preceding line as an escape hatch."
                    ),
                ));
            }
        }

        violations
    }
}

struct RawPragmaChecker;

impl FileContentChecker for RawPragmaChecker {
    fn name(&self) -> &'static str {
        "RawPragmaChecker"
    }

    fn should_process_file(&self, file_path: &str, _project_root: &Path) -> bool {
        if !ends_with_any(file_path, &[".cpp", ".h", ".hpp", ".ino", ".cpp.hpp"]) {
            return false;
        }
        let normalized = normalize_path(file_path);
        !normalized.ends_with("compiler_control.h") && !is_under_dir(&normalized, "third_party")
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

            if regex_raw_pragma().is_match(line) {
                violations.push((
                    index + 1,
                    format!(
                        "Raw _Pragma() usage: {stripped}\n      Use FL_DISABLE_WARNING_PUSH / FL_DISABLE_WARNING_* / FL_DISABLE_WARNING_POP from fl/stl/compiler_control.h instead.\n      _Pragma() is not portable across all target compilers."
                    ),
                ));
            }
        }

        violations
    }
}

struct RawNoexceptChecker;

impl FileContentChecker for RawNoexceptChecker {
    fn name(&self) -> &'static str {
        "RawNoexceptChecker"
    }

    fn should_process_file(&self, file_path: &str, project_root: &Path) -> bool {
        if !is_under_project_subpath(file_path, project_root, "src") {
            return false;
        }
        if !ends_with_any(file_path, &[".h", ".hpp", ".cpp", ".cpp.hpp"]) {
            return false;
        }
        let normalized = normalize_path(file_path);
        !normalized.ends_with("fl/stl/noexcept.h") && !is_under_dir(&normalized, "third_party")
    }

    fn check_file_content(&self, file_content: &FileContent) -> Vec<(usize, String)> {
        if !file_content.content.contains("noexcept") {
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

            let code = split_line_comment(stripped).trim();
            if code.is_empty()
                || regex_preprocessor_include().is_match(code)
                || regex_define_fl_noexcept().is_match(code)
                || !regex_raw_noexcept().is_match(code)
            {
                continue;
            }

            let temp = code.replace("FL_NO_EXCEPT", "");
            let remaining: Vec<_> = regex_raw_noexcept().find_iter(&temp).collect();
            let all_operator_form = remaining
                .iter()
                .all(|m| temp[m.end()..].trim_start().starts_with('('));
            if !remaining.is_empty() && all_operator_form {
                continue;
            }
            if regex_noexcept_suppression().is_match(line) {
                continue;
            }

            violations.push((
                index + 1,
                format!(
                    "Raw 'noexcept' keyword — use FL_NO_EXCEPT macro instead (defined in fl/stl/noexcept.h, currently a noop everywhere for cross-platform compatibility): {stripped}"
                ),
            ));
        }

        violations
    }
}

struct SingletonInHeadersChecker;

impl FileContentChecker for SingletonInHeadersChecker {
    fn name(&self) -> &'static str {
        "SingletonInHeadersChecker"
    }

    fn should_process_file(&self, file_path: &str, _project_root: &Path) -> bool {
        if !ends_with_any(file_path, &[".h", ".hpp"]) {
            return false;
        }
        let normalized = normalize_path(file_path);
        if normalized.ends_with("fl/stl/singleton.h") {
            return false;
        }
        if normalized.ends_with(".hpp") {
            return false;
        }
        !is_excluded_file(&normalized)
    }

    fn check_file_content(&self, file_content: &FileContent) -> Vec<(usize, String)> {
        let is_cpp_hpp = file_content.path.ends_with(".cpp.hpp");
        let is_private_header = file_content
            .lines
            .iter()
            .take(50)
            .any(|line| line.contains("IWYU pragma: private"));
        if is_cpp_hpp || is_private_header {
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
            if !code_part.contains("Singleton") {
                continue;
            }
            if is_cpp_hpp {
                if regex_singleton_shared().is_match(code_part) {
                    violations.push((
                        index + 1,
                        format!("Use Singleton<T> instead of SingletonShared<T> in .cpp.hpp: {stripped}"),
                    ));
                }
            } else if regex_singleton().is_match(code_part) {
                if regex_friend_class().is_match(code_part) {
                    continue;
                }
                violations.push((
                    index + 1,
                    format!(
                        "Use SingletonShared<T> instead of Singleton<T> in headers: {stripped}"
                    ),
                ));
            }
        }

        violations
    }
}

struct StdNamespaceChecker;

impl FileContentChecker for StdNamespaceChecker {
    fn name(&self) -> &'static str {
        "StdNamespaceChecker"
    }

    fn should_process_file(&self, file_path: &str, project_root: &Path) -> bool {
        if !ends_with_any(file_path, &[".cpp", ".h", ".hpp", ".ino"]) {
            return false;
        }
        if !is_under_project_subpath(file_path, project_root, "src")
            && !is_under_project_subpath(file_path, project_root, "tests")
        {
            return false;
        }
        if is_excluded_file(file_path) {
            return false;
        }
        let normalized = normalize_path(file_path);
        if normalized.contains("third_party") || normalized.contains("thirdparty") {
            return false;
        }
        !is_std_bridge_file(&normalized)
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
            if !code_part.contains("std::")
                || line.contains("// okay std namespace")
                || line_has_only_allowed_std_symbols(line)
            {
                continue;
            }
            violations.push((index + 1, stripped.to_string()));
        }

        violations
    }
}

struct ExampleSerialChecker;

impl FileContentChecker for ExampleSerialChecker {
    fn name(&self) -> &'static str {
        "ExampleSerialChecker"
    }

    fn should_process_file(&self, file_path: &str, _project_root: &Path) -> bool {
        if !ends_with_any(file_path, &[".cpp", ".h", ".hpp", ".ino"]) {
            return false;
        }
        normalize_path(file_path).contains("examples/AutoResearch/")
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

            let (code_part, comment_part) = line
                .split_once("//")
                .map_or((line.as_str(), ""), |(code, comment)| (code, comment));
            if !code_part.contains("Serial.") {
                continue;
            }
            let Some(capture) = regex_serial_method().captures(code_part) else {
                continue;
            };
            let method = capture.get(1).unwrap().as_str();
            if ALLOWED_SERIAL_METHODS.contains(&method)
                || !FORBIDDEN_SERIAL_METHODS.contains(&method)
            {
                continue;
            }
            if comment_part.to_ascii_lowercase().contains("ok serial") {
                continue;
            }

            let replacement = serial_replacement(method);
            violations.push((
                index + 1,
                format!(
                    "Avoid `Serial.{method}(...)` in enforced examples — use `{replacement}` instead.\n      Rationale: fl:: wrappers carry the non-blocking HWCDC fixes from FastLED #2669 (setTxTimeoutMs=0, guarded flush, host-presence skip). Raw `Serial.{method}` bypasses them.\n      Line: {stripped}\n      If this call is genuinely required (platform-specific config with no fl:: equivalent), suppress with `// ok serial - <reason>` on the same line."
                ),
            ));
        }

        violations
    }
}

struct AutoResearchRuntimeOutputChecker;

impl FileContentChecker for AutoResearchRuntimeOutputChecker {
    fn name(&self) -> &'static str {
        "AutoResearchRuntimeOutputChecker"
    }

    fn should_process_file(&self, file_path: &str, _project_root: &Path) -> bool {
        if !ends_with_any(file_path, &[".cpp", ".h", ".hpp", ".ino"]) {
            return false;
        }
        let normalized = normalize_path(file_path);
        normalized.ends_with("examples/AutoResearch/AutoResearch.ino")
            || normalized.contains("examples/AutoResearch/AutoResearchRemote")
    }

    fn check_file_content(&self, file_content: &FileContent) -> Vec<(usize, String)> {
        if !self.should_process_file(&file_content.path, Path::new(".")) {
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

            let (code_part, comment_part) = line
                .split_once("//")
                .map_or((line.as_str(), ""), |(code, comment)| (code, comment));
            let code_without_strings =
                regex_string_literal().replace_all(code_part, "\"\"").to_string();
            if !regex_autoresearch_forbidden_runtime_output().is_match(&code_without_strings) {
                continue;
            }
            if comment_part
                .to_ascii_lowercase()
                .contains("ok autoresearch rpc serial")
            {
                continue;
            }

            violations.push((
                index + 1,
                format!(
                    "AutoResearch runtime output must use JSON-RPC result fields, not direct logging/serial output.\n      Line: {stripped}\n      If this is the RPC transport boundary itself, suppress with `// ok autoresearch rpc serial - <reason>`."
                ),
            ));
        }

        violations
    }
}

struct IncludeAfterNamespaceChecker;

impl FileContentChecker for IncludeAfterNamespaceChecker {
    fn name(&self) -> &'static str {
        "IncludeAfterNamespaceChecker"
    }

    fn should_process_file(&self, file_path: &str, _project_root: &Path) -> bool {
        if INCLUDE_AFTER_NAMESPACE_SKIP_PATTERNS
            .iter()
            .any(|pattern| file_path.contains(pattern))
        {
            return false;
        }
        ends_with_any(file_path, &[".cpp", ".h", ".hpp", ".cc", ".ino"])
    }

    fn check_file_content(&self, file_content: &FileContent) -> Vec<(usize, String)> {
        if file_content
            .lines
            .iter()
            .any(|line| regex_allow_include_after_namespace().is_match(line))
        {
            return Vec::new();
        }

        let mut namespace_started = false;
        let mut violations = Vec::new();

        for (index, line) in file_content.lines.iter().enumerate() {
            if regex_namespace_declaration().is_match(line) {
                namespace_started = true;
                continue;
            }
            if namespace_started && regex_any_include().is_match(line) {
                if regex_nolint().is_match(line) {
                    continue;
                }
                violations.push((index + 1, line.trim_end_matches('\n').to_string()));
            }
        }

        violations
    }
}

struct NamespaceFlDeclarationChecker;

impl FileContentChecker for NamespaceFlDeclarationChecker {
    fn name(&self) -> &'static str {
        "NamespaceFlDeclarationChecker"
    }

    fn should_process_file(&self, file_path: &str, project_root: &Path) -> bool {
        let path = Path::new(file_path);
        if path
            .parent()
            .and_then(Path::file_name)
            .and_then(|name| name.to_str())
            != Some("src")
        {
            return false;
        }
        let Ok(src_root) = project_root.join("src").canonicalize() else {
            return false;
        };
        let Ok(parent) = path
            .parent()
            .unwrap_or_else(|| Path::new(""))
            .canonicalize()
        else {
            return false;
        };
        if parent != src_root {
            return false;
        }
        matches!(
            path.extension().and_then(|value| value.to_str()),
            Some("h" | "cpp")
        )
    }

    fn check_file_content(&self, file_content: &FileContent) -> Vec<(usize, String)> {
        let mut violations = Vec::new();
        for (index, line) in file_content.lines.iter().enumerate() {
            if line.trim().starts_with("//") {
                continue;
            }
            if regex_namespace_fl_declaration().is_match(line) {
                violations.push((index + 1, line.trim().to_string()));
            }
        }
        violations
    }
}

struct UsingNamespaceFlChecker;

impl FileContentChecker for UsingNamespaceFlChecker {
    fn name(&self) -> &'static str {
        "UsingNamespaceFlChecker"
    }

    fn should_process_file(&self, file_path: &str, project_root: &Path) -> bool {
        let normalized = normalize_path(file_path);
        if !is_under_project_subpath(&normalized, project_root, "src") {
            return false;
        }
        if normalized.contains("/examples/") || normalized.contains("/tests/") {
            return false;
        }
        if normalized.contains("FastLED.h") {
            return false;
        }
        ends_with_any(&normalized, &[".h", ".hpp", ".cpp", ".cpp.hpp"])
    }

    fn check_file_content(&self, file_content: &FileContent) -> Vec<(usize, String)> {
        let mut violations = Vec::new();
        for (index, line) in file_content.lines.iter().enumerate() {
            if line.starts_with("//") {
                continue;
            }
            if line.contains("using namespace fl;") {
                violations.push((index + 1, line.trim().to_string()));
            }
        }
        violations
    }
}

struct UsingNamespaceChecker;

impl FileContentChecker for UsingNamespaceChecker {
    fn name(&self) -> &'static str {
        "UsingNamespaceChecker"
    }

    fn should_process_file(&self, file_path: &str, project_root: &Path) -> bool {
        let normalized = normalize_path(file_path);
        if !is_under_project_subpath(&normalized, project_root, "src/fl") {
            return false;
        }
        if normalized.contains("/examples/") || normalized.contains("/tests/") {
            return false;
        }
        ends_with_any(&normalized, &[".h", ".hpp", ".hxx", ".hh"])
    }

    fn check_file_content(&self, file_content: &FileContent) -> Vec<(usize, String)> {
        let mut violations = Vec::new();
        for (index, line) in file_content.lines.iter().enumerate() {
            let without_line_comment = split_line_comment(line);
            let line_clean = strip_inline_block_comments(without_line_comment);
            if !regex_using_namespace().is_match(&line_clean) {
                continue;
            }
            if regex_define_using_namespace().is_match(&line_clean) {
                continue;
            }

            let mut is_conditional = false;
            for previous_index in index.saturating_sub(5)..index {
                if regex_force_use_namespace().is_match(file_content.lines[previous_index].trim()) {
                    is_conditional = true;
                    break;
                }
            }
            if !is_conditional {
                violations.push((index + 1, line.trim().to_string()));
            }
        }
        violations
    }
}
