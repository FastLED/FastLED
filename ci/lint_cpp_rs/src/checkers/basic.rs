struct SerialPrintfChecker;

impl FileContentChecker for SerialPrintfChecker {
    fn name(&self) -> &'static str {
        "SerialPrintfChecker"
    }

    fn should_process_file(&self, file_path: &str, _project_root: &Path) -> bool {
        is_under_dir(file_path, "examples")
            && ends_with_any(file_path, &[".cpp", ".h", ".hpp", ".ino"])
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
            if code_part.contains("Serial.printf") {
                violations.push((index + 1, stripped.to_string()));
            }
        }

        violations
    }
}

struct UsingNamespaceFlInExamplesChecker;

impl FileContentChecker for UsingNamespaceFlInExamplesChecker {
    fn name(&self) -> &'static str {
        "UsingNamespaceFlInExamplesChecker"
    }

    fn should_process_file(&self, file_path: &str, _project_root: &Path) -> bool {
        is_under_dir(file_path, "examples")
            && ends_with_any(
                file_path,
                &[".cpp", ".h", ".hpp", ".ino", ".c", ".cc", ".cxx"],
            )
    }

    fn check_file_content(&self, file_content: &FileContent) -> Vec<(usize, String)> {
        file_content
            .lines
            .iter()
            .enumerate()
            .filter_map(|(index, line)| {
                if regex_using_namespace_fl().is_match(line) {
                    Some((index + 1, line.trim_end().to_string()))
                } else {
                    None
                }
            })
            .collect()
    }
}

struct BannedMacrosChecker;

impl FileContentChecker for BannedMacrosChecker {
    fn name(&self) -> &'static str {
        "BannedMacrosChecker"
    }

    fn should_process_file(&self, file_path: &str, _project_root: &Path) -> bool {
        if !ends_with_any(file_path, &[".cpp", ".h", ".hpp", ".ino", ".cpp.hpp"]) {
            return false;
        }
        if file_path.ends_with("has_include.h")
            || file_path.ends_with("compiler_control.h")
            || file_path.ends_with("cpp_compat.h")
            || file_path.ends_with("static_assert.h")
        {
            return false;
        }
        if is_under_dir(file_path, "third_party") {
            return false;
        }
        if ends_with_any(file_path, &["doctest.h", "catch.hpp", "gtest.h"]) {
            return false;
        }
        if ends_with_any(file_path, &[".md", ".txt"]) {
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
            if code_part.contains("#define") && code_part.contains("FL_HAS_INCLUDE") {
                continue;
            }

            let code_no_strings = strip_string_literals(code_part);
            if !code_no_strings.contains("__has_include")
                && !code_no_strings.contains("static_assert")
            {
                continue;
            }

            if regex_has_include().is_match(&code_no_strings) {
                if code_no_strings.contains("#ifndef") || code_no_strings.contains("#ifdef") {
                    continue;
                }
                violations.push((
                    index + 1,
                    format!(
                        "Use FL_HAS_INCLUDE instead of __has_include: {stripped}\n      \
Rationale: __has_include is not universally supported. FL_HAS_INCLUDE provides a portable wrapper with fallback for older compilers.\n      \
Include 'fl/stl/has_include.h' and replace __has_include(...) with FL_HAS_INCLUDE(...)"
                    ),
                ));
            }

            if regex_static_assert().is_match(&code_no_strings) {
                violations.push((
                    index + 1,
                    format!(
                        "Use FL_STATIC_ASSERT instead of raw static_assert: {stripped}\n      \
Rationale: FL_STATIC_ASSERT keeps compile-time assertions portable on old embedded toolchains.\n      \
Include 'fl/stl/static_assert.h' when needed and replace static_assert(...) with FL_STATIC_ASSERT(...)"
                    ),
                ));
            }
        }

        violations
    }
}

struct BareAllocationChecker;

impl FileContentChecker for BareAllocationChecker {
    fn name(&self) -> &'static str {
        "BareAllocationChecker"
    }

    fn should_process_file(&self, file_path: &str, _project_root: &Path) -> bool {
        if !ends_with_any(file_path, &[".cpp", ".h", ".hpp", ".ino", ".cpp.hpp"]) {
            return false;
        }
        if is_excluded_file(file_path) {
            return false;
        }
        let lower = file_path.to_ascii_lowercase();
        if lower.contains("third_party") || lower.contains("thirdparty") {
            return false;
        }
        !BARE_ALLOCATION_WHITELISTED_SUFFIXES
            .iter()
            .any(|suffix| file_path.ends_with(suffix))
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
            if line.contains("// ok bare allocation") || line.contains("// okay bare allocation") {
                continue;
            }
            if !line.contains("new")
                && !line.contains("delete")
                && !line.contains("malloc")
                && !line.contains("calloc")
                && !line.contains("realloc")
                && !line.contains("free")
            {
                continue;
            }

            let code_part = split_line_comment(line);
            let code_part = regex_string_literal().replace_all(code_part, "\"\"");

            if code_part.contains("operator new") || code_part.contains("operator delete") {
                continue;
            }
            if regex_deleted_func().is_match(&code_part)
                || regex_deleted_func_eol().is_match(code_part.trim_end())
            {
                continue;
            }

            if regex_new_alloc().is_match(&code_part) {
                violations.push((
                    index + 1,
                    format!("bare 'new': {stripped} — {}", suggestion_new()),
                ));
                continue;
            }

            if regex_delete().is_match(&code_part) {
                violations.push((
                    index + 1,
                    format!("bare 'delete': {stripped} — {}", suggestion_delete()),
                ));
                continue;
            }

            if let Some(mat) = regex_malloc_family()
                .find_iter(&code_part)
                .find(|mat| !has_forbidden_prefix(&code_part, mat.start()))
            {
                let func = regex_malloc_family()
                    .captures(mat.as_str())
                    .and_then(|captures| captures.get(1))
                    .map(|capture| capture.as_str())
                    .unwrap_or("malloc");
                violations.push((
                    index + 1,
                    format!("bare '{func}': {stripped} — {}", suggestion_malloc()),
                ));
                continue;
            }

            if regex_free()
                .find_iter(&code_part)
                .any(|mat| !has_forbidden_prefix(&code_part, mat.start()))
            {
                violations.push((
                    index + 1,
                    format!("bare 'free': {stripped} — {}", suggestion_free()),
                ));
            }
        }

        violations
    }
}

fn has_forbidden_prefix(code: &str, start: usize) -> bool {
    if start == 0 {
        return false;
    }
    let prefix = &code[..start];
    prefix.ends_with("::") || prefix.ends_with('.')
}

fn suggestion_new() -> &'static str {
    "Use fl::make_unique<T>() or fl::make_shared<T>() instead of bare 'new', or add '// ok bare allocation' to suppress"
}

fn suggestion_delete() -> &'static str {
    "Use fl::unique_ptr or fl::shared_ptr (automatic cleanup) instead of bare 'delete', or add '// ok bare allocation' to suppress"
}

fn suggestion_malloc() -> &'static str {
    "Use fl::make_unique<T>() or fl::make_shared<T>() instead, or fl::malloc() if raw memory is required, or add '// ok bare allocation' to suppress"
}

fn suggestion_free() -> &'static str {
    "Use fl::unique_ptr or fl::shared_ptr (automatic cleanup) instead, or fl::free() if raw memory is required, or add '// ok bare allocation' to suppress"
}

struct StaticInHeaderChecker;

impl FileContentChecker for StaticInHeaderChecker {
    fn name(&self) -> &'static str {
        "StaticInHeaderChecker"
    }

    fn should_process_file(&self, file_path: &str, _project_root: &Path) -> bool {
        if !ends_with_any(file_path, &[".h", ".hpp"]) {
            return false;
        }
        if file_path.ends_with(".cpp.hpp") {
            return false;
        }
        !is_excluded_file(file_path)
    }

    fn check_file_content(&self, file_content: &FileContent) -> Vec<(usize, String)> {
        if !file_content.content.contains("static") {
            return Vec::new();
        }

        let mut violations = Vec::new();
        let mut in_multiline_comment = false;
        let mut brace_depth: isize = 0;
        let mut in_function = false;
        let mut in_template_function = false;

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
            let open_braces = code_part.matches('{').count() as isize;
            let close_braces = code_part.matches('}').count() as isize;

            if code_part.contains("template") && regex_template().is_match(code_part) {
                in_template_function = true;
            }

            if code_part.contains('(')
                && code_part.contains('{')
                && regex_inline_func().is_match(code_part)
            {
                in_function = true;
                brace_depth = open_braces - close_braces;
            } else if in_function {
                brace_depth += open_braces - close_braces;
            }

            if in_function && brace_depth <= 0 {
                in_function = false;
                in_template_function = false;
                brace_depth = 0;
            }

            if in_function
                && brace_depth > 0
                && !in_template_function
                && code_part.contains("static")
                && regex_static_var().is_match(code_part)
                && !regex_static_func().is_match(code_part)
                && !line.contains("// okay static in header")
            {
                violations.push((index + 1, stripped.to_string()));
            }
        }

        violations
    }
}

struct IncludePathsChecker;

impl FileContentChecker for IncludePathsChecker {
    fn name(&self) -> &'static str {
        "IncludePathsChecker"
    }

    fn should_process_file(&self, file_path: &str, project_root: &Path) -> bool {
        if !ends_with_any(file_path, &[".cpp", ".h", ".hpp", ".c"]) {
            return false;
        }
        if is_excluded_file(file_path) {
            return false;
        }

        is_under_project_subpath(file_path, project_root, "src/fl")
            || is_under_project_subpath(file_path, project_root, "src/platforms")
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
            if comment_part
                .to_ascii_lowercase()
                .contains("ok include path")
            {
                continue;
            }

            let Some(captures) = regex_include_path().captures(code_part) else {
                continue;
            };
            let include_path = captures.get(1).map(|match_| match_.as_str()).unwrap_or("");

            if include_path.starts_with('<') {
                continue;
            }

            if is_valid_include_path(include_path)
                && !header_exists_for_file(&file_content.path, include_path)
            {
                violations.push((
                    index + 1,
                    format!(
                        "Header not found: #include \"{include_path}\"\n  Expected: src/{include_path}\n"
                    ),
                ));
                continue;
            }

            if is_valid_include_path(include_path) {
                continue;
            }

            if let Some(banned_replacement) = banned_subpath_replacement(include_path) {
                violations.push((
                    index + 1,
                    format!(
                        "Invalid include path: #include \"{include_path}\" - Use #include \"{banned_replacement}\" instead. Add '// ok include path' comment to suppress."
                    ),
                ));
            } else if is_relative_include_path(include_path) {
                violations.push((
                    index + 1,
                    format!(
                        "Invalid include path: #include \"{include_path}\" - Relative paths (../ or ./) are not allowed. Use paths relative to src/ instead (e.g., \"fl/foo.h\" or \"platforms/bar.h\"). Add '// ok include path' comment to suppress."
                    ),
                ));
            } else if is_fastled_platform_relative(include_path) {
                violations.push((
                    index + 1,
                    format!(
                        "Invalid include path: #include \"{include_path}\" - FastLED platform includes must use \"platforms/\" prefix. Use #include \"platforms/{include_path}\" instead. Add '// ok include path' comment to suppress."
                    ),
                ));
            } else if let Some(typo_correction) = typo_include_suggestion(include_path) {
                let typo_prefix = include_path
                    .split_once('/')
                    .map_or(include_path, |(prefix, _)| prefix);
                let typo_prefix = format!("{typo_prefix}/");
                let rest_of_path = include_path.strip_prefix(&typo_prefix).unwrap_or("");
                let corrected_path = format!("{typo_correction}{rest_of_path}");
                violations.push((
                    index + 1,
                    format!(
                        "Invalid include path: #include \"{include_path}\" - \"{typo_prefix}\" looks like a typo. Use #include \"{corrected_path}\" instead. Add '// ok include path' comment to suppress."
                    ),
                ));
            }
        }

        violations
    }
}

struct BuiltinMemcpyChecker;

impl FileContentChecker for BuiltinMemcpyChecker {
    fn name(&self) -> &'static str {
        "BuiltinMemcpyChecker"
    }

    fn should_process_file(&self, file_path: &str, _project_root: &Path) -> bool {
        if !ends_with_any(file_path, &[".cpp", ".h", ".hpp", ".ino", ".cpp.hpp"]) {
            return false;
        }
        if file_path.ends_with("compiler_control.h") {
            return false;
        }
        !normalize_path(file_path).contains("/third_party/")
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
            if code_part.contains("#define") && code_part.contains("FL_BUILTIN_MEMCPY") {
                continue;
            }
            if !code_part.contains("__builtin_memcpy") {
                continue;
            }
            if regex_builtin_memcpy().is_match(code_part) {
                violations.push((
                    index + 1,
                    format!(
                        "Use FL_BUILTIN_MEMCPY instead of __builtin_memcpy: {stripped}\n      \
Rationale: FL_BUILTIN_MEMCPY wraps __builtin_memcpy for portability.\n      \
Include 'fl/stl/compiler_control.h' and replace __builtin_memcpy(...) with FL_BUILTIN_MEMCPY(...)"
                    ),
                ));
            }
        }

        violations
    }
}

