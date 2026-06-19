// Single-line regex bans ported from the Python checker fleet
// (FastLED #3297 follow-up to #3288). Each struct here mirrors its
// Python counterpart's `should_process_file` + `check_file_content`
// semantics. The block-comment state machine follows the Python
// "enter only on /* without */, exit only when the next line has */"
// rule — see the per-checker tests in ../lint_core/tests.rs.

// --- BareNoInlineChecker -----------------------------------------------------
//
// Bans bare `__attribute__((noinline))` / `__declspec(noinline)` in src/.
// Origin: ci/lint_cpp/bare_noinline_checker.py (FastLED #2773 item 2.1).

const BARE_NOINLINE_EXEMPT_SUFFIXES: &[&str] = &[
    "fl/stl/compiler_control.h",
    "fl/stl/compiler_control.cpp.hpp",
    "platforms/arm/nrf52/led_sysdefs_arm_nrf52.h",
    "platforms/arm/teensy/coroutine_teensy.impl.hpp",
];

struct BareNoInlineChecker;

impl FileContentChecker for BareNoInlineChecker {
    fn name(&self) -> &'static str {
        "BareNoInlineChecker"
    }

    fn should_process_file(&self, file_path: &str, _project_root: &Path) -> bool {
        if !is_under_dir(file_path, "src") {
            return false;
        }
        if !ends_with_any(file_path, &[".cpp", ".h", ".hpp"]) {
            return false;
        }
        let normalized = normalize_path(file_path);
        if BARE_NOINLINE_EXEMPT_SUFFIXES
            .iter()
            .any(|suffix| normalized.ends_with(suffix))
        {
            return false;
        }
        !normalized.contains("/third_party/")
    }

    fn check_file_content(&self, file_content: &FileContent) -> Vec<(usize, String)> {
        const SUPPRESS: &str = "// ok noinline";
        let mut violations = Vec::new();
        let mut in_block_comment = false;
        for (index, line) in file_content.lines.iter().enumerate() {
            if !python_style_pass_through_line(line, &mut in_block_comment, SUPPRESS) {
                continue;
            }
            let code = split_line_comment(line);
            if regex_bare_noinline().is_match(code) {
                violations.push((index + 1, line.trim_end().to_string()));
            }
        }
        violations
    }
}

// --- BareSnprintfChecker -----------------------------------------------------
//
// Bans bare C ::snprintf / ::printf / ::sprintf family in src/.
// Origin: ci/lint_cpp/bare_snprintf_checker.py (FastLED #2773 item 1.5).

const BARE_SNPRINTF_EXEMPT_SUFFIXES: &[&str] = &[
    "fl/stl/stdio.h",
    "fl/stl/stdio.cpp.hpp",
    "fl/stl/cstdio.h",
    "fl/stl/cstdio.cpp.hpp",
];

struct BareSnprintfChecker;

impl FileContentChecker for BareSnprintfChecker {
    fn name(&self) -> &'static str {
        "BareSnprintfChecker"
    }

    fn should_process_file(&self, file_path: &str, _project_root: &Path) -> bool {
        if !is_under_dir(file_path, "src") {
            return false;
        }
        if !ends_with_any(file_path, &[".cpp", ".h", ".hpp"]) {
            return false;
        }
        let normalized = normalize_path(file_path);
        if BARE_SNPRINTF_EXEMPT_SUFFIXES
            .iter()
            .any(|suffix| normalized.ends_with(suffix))
        {
            return false;
        }
        if normalized.contains("/third_party/") {
            return false;
        }
        // Host-only unit-test watchdog scaffolding never links into ESP32
        // firmware, so the binary-size argument doesn't apply.
        !normalized.ends_with("/run_unit_test.hpp")
    }

    fn check_file_content(&self, file_content: &FileContent) -> Vec<(usize, String)> {
        const SUPPRESS: &str = "// ok snprintf";
        let mut violations = Vec::new();
        let mut in_block_comment = false;
        for (index, line) in file_content.lines.iter().enumerate() {
            if !python_style_pass_through_line(line, &mut in_block_comment, SUPPRESS) {
                continue;
            }
            let code = split_line_comment(line);
            for mat in regex_bare_snprintf().find_iter(code) {
                if printf_family_has_qualifier_prefix(code, mat.start()) {
                    continue;
                }
                violations.push((index + 1, line.trim_end().to_string()));
                break;
            }
        }
        violations
    }
}

// --- BareLibmChecker ---------------------------------------------------------
//
// Bans bare libm calls (sin/cos/sqrtf/atan2/ldexpf/...) in src/.
// Origin: ci/lint_cpp/bare_libm_checker.py (FastLED #3002 / #3012).

const BARE_LIBM_EXEMPT_SUFFIXES: &[&str] = &["fl/math/math.cpp.hpp", "fl/math/math.h"];

const BARE_LIBM_EXEMPT_PREFIXES: &[&str] = &[
    "fl/audio/",
    "fl/fx/",
    "fl/gfx/",
    "fl/math/fixed_point",
    "fl/math/line_simplification",
    "fl/math/screenmap",
    "fl/math/transform",
    "platforms/esp/",
    "platforms/wasm/",
];

struct BareLibmChecker;

impl FileContentChecker for BareLibmChecker {
    fn name(&self) -> &'static str {
        "BareLibmChecker"
    }

    fn should_process_file(&self, file_path: &str, _project_root: &Path) -> bool {
        if !is_under_dir(file_path, "src") {
            return false;
        }
        if !ends_with_any(file_path, &[".cpp", ".h", ".hpp"]) {
            return false;
        }
        let normalized = normalize_path(file_path);
        if BARE_LIBM_EXEMPT_SUFFIXES
            .iter()
            .any(|suffix| normalized.ends_with(suffix))
        {
            return false;
        }
        if normalized.contains("/third_party/") {
            return false;
        }
        for prefix in BARE_LIBM_EXEMPT_PREFIXES {
            if normalized.contains(&format!("/src/{prefix}")) || normalized.starts_with(prefix) {
                return false;
            }
        }
        true
    }

    fn check_file_content(&self, file_content: &FileContent) -> Vec<(usize, String)> {
        const SUPPRESS: &str = "// ok libm";
        let mut violations = Vec::new();
        let mut in_block_comment = false;
        for (index, line) in file_content.lines.iter().enumerate() {
            if !python_style_pass_through_line(line, &mut in_block_comment, SUPPRESS) {
                continue;
            }
            let code = split_line_comment(line);
            for mat in regex_bare_libm().find_iter(code) {
                if libm_has_qualifier_prefix(code, mat.start()) {
                    continue;
                }
                violations.push((index + 1, line.trim_end().to_string()));
                break;
            }
        }
        violations
    }
}

// --- FlNoUnderscoreChecker ---------------------------------------------------
//
// Enforces the FL_NO_<WORD> macro-naming convention; bans bare FL_NO<WORD>.
// Origin: ci/lint_cpp/fl_no_underscore_checker.py (FastLED #3283).

const FL_NO_UNDERSCORE_WHITELIST: &[&str] = &["FL_NOP", "FL_NOP2"];
const FL_NO_UNDERSCORE_SKIP_PREFIXES: &[&str] = &["FL_NOT_"];

struct FlNoUnderscoreChecker;

impl FileContentChecker for FlNoUnderscoreChecker {
    fn name(&self) -> &'static str {
        "FlNoUnderscoreChecker"
    }

    fn should_process_file(&self, file_path: &str, _project_root: &Path) -> bool {
        if !is_under_dir(file_path, "src") {
            return false;
        }
        if !ends_with_any(
            file_path,
            &[".cpp", ".h", ".hpp", ".cc", ".hh", ".inl", ".ino"],
        ) {
            return false;
        }
        let normalized = normalize_path(file_path);
        !normalized.contains("/third_party/")
    }

    fn check_file_content(&self, file_content: &FileContent) -> Vec<(usize, String)> {
        const SUPPRESS: &str = "// ok no-underscore";
        let mut violations = Vec::new();
        let mut in_block_comment = false;
        for (index, line) in file_content.lines.iter().enumerate() {
            if !python_style_pass_through_line(line, &mut in_block_comment, SUPPRESS) {
                continue;
            }
            let code = split_line_comment(line);
            for mat in regex_fl_no_underscore().find_iter(code) {
                let token = mat.as_str();
                if FL_NO_UNDERSCORE_WHITELIST.contains(&token) {
                    continue;
                }
                if FL_NO_UNDERSCORE_SKIP_PREFIXES
                    .iter()
                    .any(|prefix| token.starts_with(prefix))
                {
                    continue;
                }
                violations.push((index + 1, line.trim_end().to_string()));
                break;
            }
        }
        violations
    }
}

// --- LegacyLogMacroChecker ---------------------------------------------------
//
// Bans legacy stream-style FastLED logging macros (FL_WARN, FL_PRINT, FL_DBG,
// FL_ERROR, FL_LOG_*) in src/. Each call site should use the `_F` printf-style
// variant so the formatter lives in fl::printf instead of a fresh fl::sstream
// chain per call site. Origin: ci/lint_cpp/legacy_log_macro_checker.py.

struct LegacyLogMacroChecker;

impl FileContentChecker for LegacyLogMacroChecker {
    fn name(&self) -> &'static str {
        "LegacyLogMacroChecker"
    }

    fn should_process_file(&self, file_path: &str, project_root: &Path) -> bool {
        if !is_under_dir(file_path, "src") {
            return false;
        }
        if !ends_with_any(file_path, &[".cpp", ".h", ".hpp"]) {
            return false;
        }
        let normalized = normalize_path(file_path);
        if normalized.contains("/third_party/") {
            return false;
        }
        // log.h itself defines the macros — exempt it.
        let log_header_suffix = "fl/log/log.h";
        if normalized.ends_with(log_header_suffix) {
            return false;
        }
        // Belt-and-suspenders: also catch the absolute path form when a
        // non-canonical project_root is supplied (mirrors the Python
        // Path.resolve() == LOG_HEADER comparison).
        let project_normalized = normalize_path(&path_to_string(project_root));
        let absolute = format!("{project_normalized}/src/{log_header_suffix}");
        normalized != absolute
    }

    fn check_file_content(&self, file_content: &FileContent) -> Vec<(usize, String)> {
        let mut violations = Vec::new();
        let mut in_block_comment = false;
        for (index, line) in file_content.lines.iter().enumerate() {
            // Same block-comment + // line skip as the others, but with an
            // ADDITIONAL skip on `#define` lines (the original macro
            // definitions). No `// ok legacy log` suppression by design.
            let trimmed = line.trim();
            if in_block_comment {
                if line.contains("*/") {
                    in_block_comment = false;
                }
                continue;
            }
            if line.contains("/*") && !line.contains("*/") {
                in_block_comment = true;
                continue;
            }
            if trimmed.starts_with("//") || trimmed.starts_with("#define") {
                continue;
            }
            let code = split_line_comment(line);
            let Some(captures) = regex_legacy_log_macro().captures(code) else {
                continue;
            };
            let macro_name = captures.get(1).map(|m| m.as_str()).unwrap_or("");
            let replacement = legacy_log_replacement(macro_name);
            violations.push((
                index + 1,
                format!(
                    "{}  [use {replacement} instead of {macro_name}]",
                    line.trim_end()
                ),
            ));
        }
        violations
    }
}

fn legacy_log_replacement(macro_name: &str) -> String {
    if macro_name.ends_with("_ASYNC_MAIN") {
        return format!("{macro_name}_F");
    }
    if macro_name == "FL_LOG_ASYNC" {
        return "FL_LOG_ASYNC_F".to_string();
    }
    if let Some(prefix) = macro_name.strip_suffix("_FMT_IF") {
        return format!("{prefix}_F_IF");
    }
    if let Some(prefix) = macro_name.strip_suffix("_FMT") {
        return format!("{prefix}_F");
    }
    if let Some(prefix) = macro_name.strip_suffix("_IF") {
        return format!("{prefix}_F_IF");
    }
    if let Some(prefix) = macro_name.strip_suffix("_ONCE") {
        return format!("{prefix}_F_ONCE");
    }
    if let Some(prefix) = macro_name.strip_suffix("_EVERY") {
        return format!("{prefix}_F_EVERY");
    }
    format!("{macro_name}_F")
}

// --- Shared helpers ----------------------------------------------------------
//
// All five checkers share the same Python-style "skip line on block comment
// open or // line comment or // <suppress>" idiom. Centralizing it here keeps
// the per-checker bodies focused on the rule.

fn python_style_pass_through_line(
    line: &str,
    in_block_comment: &mut bool,
    suppress: &str,
) -> bool {
    if *in_block_comment {
        if line.contains("*/") {
            *in_block_comment = false;
        }
        return false;
    }
    if line.contains("/*") && !line.contains("*/") {
        *in_block_comment = true;
        return false;
    }
    if line.trim_start().starts_with("//") {
        return false;
    }
    if line.contains(suppress) {
        return false;
    }
    true
}

// `\b` in Rust regex covers identifier-char boundary, but it ALSO allows `:`
// and `.` as the preceding non-word char. The Python lookbehind excluded
// those too, so we manually re-check after the regex finds a match.
fn printf_family_has_qualifier_prefix(code: &str, start: usize) -> bool {
    if start == 0 {
        return false;
    }
    let preceding = &code[..start];
    preceding.ends_with(':') || preceding.ends_with('.')
}

// BareLibm allows an OPTIONAL `::` prefix as part of the match (Python
// `(?:::)?<func>`). The lookbehind only excluded identifier-chars and `:`,
// so `obj.sqrtf(...)` does match in Python (and is therefore flagged).
// We mirror that: only the `:` and identifier-char preceding chars suppress
// the match. The leading `::` of a global-qualified call is consumed by the
// regex itself.
fn libm_has_qualifier_prefix(code: &str, start: usize) -> bool {
    if start == 0 {
        return false;
    }
    let preceding = &code[..start];
    // Strip the optional `::` that's part of our own match.
    let preceding = preceding.trim_end_matches(':');
    // After stripping `::`, an identifier-char or another `:` preceding it
    // means this was a member / qualified call — skip.
    let Some(last) = preceding.chars().last() else {
        return false;
    };
    last.is_ascii_alphanumeric() || last == '_' || last == ':'
}
