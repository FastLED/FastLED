struct WeakAttributeChecker;

impl FileContentChecker for WeakAttributeChecker {
    fn name(&self) -> &'static str {
        "WeakAttributeChecker"
    }

    fn should_process_file(&self, file_path: &str, _project_root: &Path) -> bool {
        if !ends_with_any(file_path, &[".cpp", ".h", ".hpp", ".ino", ".cpp.hpp"]) {
            return false;
        }
        !file_path.ends_with("compiler_control.h")
    }

    fn check_file_content(&self, file_content: &FileContent) -> Vec<(usize, String)> {
        // Whole-file early exit: the regex only matches if both
        // "__attribute__" and "weak" appear; most files have neither.
        if !file_content.content.contains("weak") {
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
            if code_part.contains("#define") && code_part.contains("FL_LINK_WEAK") {
                continue;
            }
            if regex_weak_attribute().is_match(code_part) {
                violations.push((
                    index + 1,
                    format!("Use FL_LINK_WEAK instead of __attribute__((weak)): {stripped}"),
                ));
            }
        }

        violations
    }
}

struct BannedDefineChecker;

impl FileContentChecker for BannedDefineChecker {
    fn name(&self) -> &'static str {
        "BannedDefineChecker"
    }

    fn should_process_file(&self, file_path: &str, _project_root: &Path) -> bool {
        ends_with_any(file_path, &[".cpp", ".h", ".hpp", ".cpp.hpp", ".ino"])
    }

    fn check_file_content(&self, file_content: &FileContent) -> Vec<(usize, String)> {
        const WRONG_DEFINES: &[(&str, &str)] = &[
            ("#if ESP32", "Use #ifdef ESP32 instead of #if ESP32"),
            (
                "#if defined(FASTLED_RMT5)",
                "Use #ifdef FASTLED_RMT5 instead of #if defined(FASTLED_RMT5)",
            ),
            (
                "#if defined(FASTLED_ESP_HAS_CLOCKLESS_SPI)",
                "Use #ifdef FASTLED_ESP_HAS_CLOCKLESS_SPI instead of #if defined(FASTLED_ESP_HAS_CLOCKLESS_SPI)",
            ),
        ];

        // Whole-file early exit: every needle starts with "#if " and
        // most files have no `#if` at all (or only `#ifdef`/`#ifndef`).
        // The per-line walk would burn nontrivial time on these.
        if !file_content.content.contains("#if ") {
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
            for (needle, message) in WRONG_DEFINES {
                if code_part.contains(needle) {
                    violations.push((index + 1, (*message).to_string()));
                }
            }
        }

        violations
    }
}

struct EmAsmClangFormatChecker;

impl FileContentChecker for EmAsmClangFormatChecker {
    fn name(&self) -> &'static str {
        "EmAsmClangFormatChecker"
    }

    fn should_process_file(&self, file_path: &str, project_root: &Path) -> bool {
        // Ported from ci/tests/test_wasm_clang_format.py: walks src/platforms/wasm/
        // and checks files whose names end in ".h" or ".cpp".
        if !ends_with_any(file_path, &[".h", ".cpp"]) {
            return false;
        }
        let normalized = normalize_path(file_path);
        is_under_project_subpath(&normalized, project_root, "src/platforms/wasm")
    }

    fn check_file_content(&self, file_content: &FileContent) -> Vec<(usize, String)> {
        // The original pytest fails when a file contains "EM_ASM_" anywhere AND
        // does NOT contain "clang-format off" anywhere. We surface a single
        // violation at line 1 to mirror the Python failure message exactly.
        if !file_content.content.contains("EM_ASM_") {
            return Vec::new();
        }
        if file_content.content.contains("clang-format off") {
            return Vec::new();
        }
        vec![(
            1,
            format!(
                "Missing clang-format off in {} (file uses EM_ASM_ macros; \
add `// clang-format off` somewhere in the file)",
                file_content.path
            ),
        )]
    }
}

struct BannedNamespaceChecker;

impl FileContentChecker for BannedNamespaceChecker {
    fn name(&self) -> &'static str {
        "BannedNamespaceChecker"
    }

    fn should_process_file(&self, file_path: &str, _project_root: &Path) -> bool {
        let skip_patterns = [".build", ".pio", ".venv", "third_party", "vendor"];
        if skip_patterns
            .iter()
            .any(|pattern| file_path.contains(pattern))
        {
            return false;
        }
        ends_with_any(file_path, &[".cpp", ".h", ".hpp", ".cc"])
    }

    fn check_file_content(&self, file_content: &FileContent) -> Vec<(usize, String)> {
        if !file_content.content.contains("fl::fl") {
            return Vec::new();
        }

        let mut violations = Vec::new();
        for (index, line) in file_content.lines.iter().enumerate() {
            let stripped = line.trim();
            if stripped.is_empty() || stripped.starts_with("//") {
                continue;
            }
            let code_part = split_line_comment(line);
            if !code_part.contains("fl::fl") {
                continue;
            }
            if regex_banned_namespace_fl_fl().is_match(code_part) {
                violations.push((index + 1, stripped.to_string()));
            }
        }
        violations
    }
}

struct CppIncludeChecker;

impl FileContentChecker for CppIncludeChecker {
    fn name(&self) -> &'static str {
        "CppIncludeChecker"
    }

    fn should_process_file(&self, file_path: &str, _project_root: &Path) -> bool {
        ends_with_any(file_path, &[".cpp", ".h", ".hpp", ".ino"]) && !is_excluded_file(file_path)
    }

    fn check_file_content(&self, file_content: &FileContent) -> Vec<(usize, String)> {
        if file_content
            .lines
            .iter()
            .take(5)
            .any(|line| line.to_ascii_lowercase().contains("// ok cpp include"))
        {
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
            if let Some(captures) = regex_cpp_include().captures(code_part) {
                if let Some(cpp_file) = captures.get(1) {
                    violations.push((index + 1, format!("#include \"{}\"", cpp_file.as_str())));
                }
            }
        }

        violations
    }
}

struct CppHppIncludesChecker;

impl FileContentChecker for CppHppIncludesChecker {
    fn name(&self) -> &'static str {
        "CppHppIncludesChecker"
    }

    fn should_process_file(&self, file_path: &str, project_root: &Path) -> bool {
        let is_src = is_under_project_subpath(file_path, project_root, "src");
        let is_tests = is_under_project_subpath(file_path, project_root, "tests");

        if !is_src && !is_tests {
            return false;
        }

        if is_src && !ends_with_any(file_path, &[".cpp", ".h", ".hpp", ".cpp.hpp"]) {
            return false;
        }
        if is_tests && !ends_with_any(file_path, &[".cpp", ".h", ".hpp"]) {
            return false;
        }

        if is_src {
            if file_path.ends_with("_build.hpp")
                || file_path.ends_with("_build.cpp")
                || file_path.ends_with("_build.cpp.hpp")
            {
                return false;
            }
            if is_under_project_subpath(file_path, project_root, "src/fl/build") {
                return false;
            }
        }

        true
    }

    fn check_file_content(&self, file_content: &FileContent) -> Vec<(usize, String)> {
        let mut violations = Vec::new();
        let mut in_multiline_comment = false;
        let normalized = normalize_path(&file_content.path);
        let is_test = normalized.contains("/tests/") || normalized.starts_with("tests/");

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
            let Some(captures) = regex_cpp_hpp_include().captures(code_part) else {
                continue;
            };
            let included_file = captures.get(1).map(|m| m.as_str()).unwrap_or("");
            if is_test {
                let h_header = included_file
                    .strip_suffix(".cpp.hpp")
                    .map(|prefix| format!("{prefix}.h"))
                    .unwrap_or_else(|| included_file.to_string());
                let h_header = h_header
                    .strip_suffix(".impl.h")
                    .map(|prefix| format!("{prefix}.h"))
                    .unwrap_or(h_header);
                violations.push((
                    index + 1,
                    format!(
                        "{stripped} - Including *.cpp.hpp files in tests is banned (hard ban, no opt-out). Include the public header instead: #include \"{h_header}\""
                    ),
                ));
            } else {
                violations.push((
                    index + 1,
                    format!(
                        "{stripped} - *.cpp.hpp files should ONLY be included by _build.* files (hard ban, no opt-out). Found include of '{included_file}' in non-build file. Move this include to the appropriate _build.hpp file."
                    ),
                ));
            }
        }

        violations
    }
}

struct ImplHppIncludesChecker;

impl FileContentChecker for ImplHppIncludesChecker {
    fn name(&self) -> &'static str {
        "ImplHppIncludesChecker"
    }

    fn should_process_file(&self, file_path: &str, project_root: &Path) -> bool {
        if !is_under_project_subpath(file_path, project_root, "src")
            && !is_under_project_subpath(file_path, project_root, "tests")
        {
            return false;
        }
        if !ends_with_any(
            file_path,
            &[".cpp", ".h", ".hpp", ".cpp.hpp", ".impl.cpp.hpp"],
        ) {
            return false;
        }
        !file_path.ends_with(".impl.cpp.hpp")
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
            if let Some(captures) = regex_impl_hpp_include().captures(code_part) {
                let included_file = captures.get(1).map(|m| m.as_str()).unwrap_or("");
                violations.push((
                    index + 1,
                    format!(
                        "{stripped} - *.impl.hpp files must ONLY be included by *.impl.cpp.hpp router files. Found include of '{included_file}' in non-router file."
                    ),
                ));
            }
        }

        violations
    }
}

struct AsmJsLocationChecker;

impl FileContentChecker for AsmJsLocationChecker {
    fn name(&self) -> &'static str {
        "AsmJsLocationChecker"
    }

    fn should_process_file(&self, file_path: &str, project_root: &Path) -> bool {
        let normalized = normalize_path(file_path);
        if !is_under_project_subpath(&normalized, project_root, "src/fl")
            && !is_under_project_subpath(&normalized, project_root, "src/platforms")
        {
            return false;
        }
        if normalized.contains("/third_party/") {
            return false;
        }
        ends_with_any(&normalized, &[".h", ".hpp", ".cpp", ".cpp.hpp"])
    }

    fn check_file_content(&self, file_content: &FileContent) -> Vec<(usize, String)> {
        let normalized = normalize_path(&file_content.path);
        if normalized.ends_with(".js.cpp.hpp") {
            return Vec::new();
        }
        if !["EM_JS(", "EM_ASYNC_JS(", "EM_ASM("]
            .iter()
            .any(|token| file_content.content.contains(token))
        {
            return Vec::new();
        }

        let mut violations = Vec::new();
        let mut in_multiline_comment = false;
        for (index, line) in file_content.lines.iter().enumerate() {
            let stripped = line.trim();
            if line.contains("/*") {
                in_multiline_comment = true;
            }
            if in_multiline_comment && line.contains("*/") {
                in_multiline_comment = false;
                continue;
            }
            if in_multiline_comment || stripped.starts_with("//") {
                continue;
            }

            let code = stripped.split("//").next().unwrap_or(stripped).trim();
            if code.is_empty() {
                continue;
            }
            if regex_asm_js_macro().is_match(code) {
                violations.push((
                    index + 1,
                    format!(
                        "Emscripten JS glue macros must live in a *.js.cpp.hpp file: {stripped}"
                    ),
                ));
            }
        }

        violations
    }
}

struct ReinterpretCastChecker;

impl FileContentChecker for ReinterpretCastChecker {
    fn name(&self) -> &'static str {
        "ReinterpretCastChecker"
    }

    fn should_process_file(&self, file_path: &str, project_root: &Path) -> bool {
        let normalized = normalize_path(file_path);
        if !is_under_project_subpath(&normalized, project_root, "src/fl")
            && !is_under_project_subpath(&normalized, project_root, "src/platforms")
        {
            return false;
        }
        if !ends_with_any(file_path, &[".cpp", ".h", ".hpp", ".ino"]) {
            return false;
        }
        if is_excluded_file(file_path) {
            return false;
        }
        !file_path.contains("third_party") && !file_path.contains("thirdparty")
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
            if !code_part.contains("reinterpret_cast") {
                continue;
            }
            if code_part.contains("fl::reinterpret_cast_") {
                continue;
            }
            if line.contains("// ok reinterpret cast") || line.contains("// okay reinterpret cast")
            {
                continue;
            }
            violations.push((index + 1, stripped.to_string()));
        }

        violations
    }
}

struct PragmaOnceChecker;

impl FileContentChecker for PragmaOnceChecker {
    fn name(&self) -> &'static str {
        "PragmaOnceChecker"
    }

    fn should_process_file(&self, file_path: &str, project_root: &Path) -> bool {
        if file_path.ends_with(".cpp.hpp") {
            return false;
        }
        if !ends_with_any(file_path, &[".cpp", ".h", ".hpp"]) {
            return false;
        }
        let normalized = normalize_path(file_path);
        if !is_under_project_subpath(&normalized, project_root, "src") {
            return false;
        }
        !normalized.contains("/third_party/") && !normalized.contains("/platforms/")
    }

    fn check_file_content(&self, file_content: &FileContent) -> Vec<(usize, String)> {
        let has_pragma_once = file_content.content.contains("#pragma once");
        let is_header = ends_with_any(&file_content.path, &[".h", ".hpp"]);
        let is_cpp = file_content.path.ends_with(".cpp");

        if is_header && !has_pragma_once {
            return vec![(
                1,
                "Missing #pragma once directive. Add '#pragma once' at the top of the header file."
                    .to_string(),
            )];
        }

        if is_cpp && has_pragma_once {
            for (index, line) in file_content.lines.iter().enumerate() {
                if line.contains("#pragma once") {
                    return vec![(
                        index + 1,
                        "Incorrect #pragma once in .cpp file. Remove '#pragma once' from source files."
                            .to_string(),
                    )];
                }
            }
        }

        Vec::new()
    }
}

struct EspRomPrintfChecker;

impl FileContentChecker for EspRomPrintfChecker {
    fn name(&self) -> &'static str {
        "EspRomPrintfChecker"
    }

    fn should_process_file(&self, file_path: &str, _project_root: &Path) -> bool {
        if !ends_with_any(file_path, &[".cpp", ".h", ".hpp", ".ino", ".cpp.hpp"]) {
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

            let (code_part, comment_part) = line
                .split_once("//")
                .map_or((line.as_str(), ""), |(code, comment)| (code, comment));
            if !code_part.contains("esp_rom_printf") {
                continue;
            }
            if !regex_esp_rom_printf().is_match(code_part) {
                continue;
            }
            if comment_part
                .to_ascii_lowercase()
                .contains("ok esp_rom_printf")
            {
                continue;
            }

            violations.push((
                index + 1,
                format!(
                    "Avoid `esp_rom_printf` — use FastLED logging (FL_LOG_INFO, FL_WARN, FL_PRINT) or fl::io instead: {stripped}\n      Rationale: esp_rom_printf bypasses FastLED's logging facilities, blocks on a slow peripheral, and proliferates platform-specific I/O into general code.\n      If this call is genuinely required (e.g. early bootstrap before logging is up), suppress with `// ok esp_rom_printf - <reason>` on the same line."
                ),
            ));
        }

        violations
    }
}

