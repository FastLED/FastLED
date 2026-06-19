// PublicSettingsPatternChecker — Tier-2 port from
// ci/lint_cpp/public_settings_pattern_checker.py (FastLED #3297).
//
// Flags fl:: free functions whose name matches `(set|enable|disable|use)_[a-z]`,
// declared at namespace-fl root scope in src/fl/**/*.h, that LACK a matching
// CFastLED method wrapper in src/FastLED.h. The wrapper presence check is a
// one-time cross-file load — FastLED.h is read once via a static OnceLock and
// reused across every file scanned in this process.

// Grandfathered names — predate the rule, never flagged.
const PUBLIC_SETTINGS_GRANDFATHERED: &[&str] = &[
    "set_rgbw_colorimetric_profile",
    "set_input_gamut",
    "enable_rgbw_colorimetric_lut",
    "disable_rgbw_colorimetric_lut",
    "set_rgbww_colorimetric_profile",
];

// Function-pointer installers — the per-object heuristic can't catch
// function-pointer first-params cleanly, so we name them explicitly.
const PUBLIC_SETTINGS_FUNCTION_POINTER_SETTERS: &[&str] = &[
    "set_rgb_2_rgbw_function",
    "set_rgb_2_rgbww_function",
];

// Fundamental C++ types — first-parameter check uses this set to tell
// per-object setters (which take a non-const ptr/ref to a user-defined type)
// apart from global setters (which take scalars or const ptr/ref).
const PUBLIC_SETTINGS_FUNDAMENTAL_TYPES: &[&str] = &[
    "bool", "char", "short", "int", "long", "float", "double", "void", "unsigned",
    "signed", "u8", "u16", "u32", "u64", "s8", "s16", "s32", "s64",
    "uint8_t", "uint16_t", "uint32_t", "uint64_t",
    "int8_t", "int16_t", "int32_t", "int64_t",
    "size_t", "ssize_t", "ptrdiff_t",
];

static FASTLED_H_CONTENT: std::sync::OnceLock<String> = std::sync::OnceLock::new();

fn fastled_h_content_for(project_root: &Path) -> &'static str {
    FASTLED_H_CONTENT
        .get_or_init(|| {
            let path = project_root.join("src").join("FastLED.h");
            std::fs::read_to_string(&path).unwrap_or_default()
        })
        .as_str()
}

struct PublicSettingsPatternChecker;

impl FileContentChecker for PublicSettingsPatternChecker {
    fn name(&self) -> &'static str {
        "PublicSettingsPatternChecker"
    }

    fn should_process_file(&self, file_path: &str, project_root: &Path) -> bool {
        // Seed the FastLED.h cache on first eligible call. Doing it here
        // (rather than inside `check_file_content` which doesn't see the
        // project root) means the OnceLock fills before any check runs.
        let _ = fastled_h_content_for(project_root);

        let normalized = normalize_path(file_path);
        if !normalized.ends_with(".h") {
            return false;
        }
        if normalized.ends_with(".cpp.hpp") {
            return false;
        }
        if !normalized.contains("/src/fl/") && !normalized.starts_with("src/fl/") {
            return false;
        }
        !normalized.contains("/third_party/")
    }

    fn check_file_content(&self, file_content: &FileContent) -> Vec<(usize, String)> {
        let content = &file_content.content;
        // Fast skip: bail before the per-line work if no trigger prefix appears.
        if !content.contains("set_")
            && !content.contains("enable_")
            && !content.contains("disable_")
            && !content.contains("use_")
        {
            return Vec::new();
        }

        let fastled_h = FASTLED_H_CONTENT.get().map(String::as_str).unwrap_or("");
        let mut violations = Vec::new();
        let mut in_block_comment = false;

        // Scope stack: one entry per `{`, pop one per `}`.
        // Entry kinds mirror the Python checker:
        //   "fl"      — inside `namespace fl { ... }` (target scope)
        //   "ns_sub"  — named sub-namespace (fl::detail, fl::isr, ...)
        //   "ns_anon" — anonymous `namespace { ... }`
        //   "class"   — class/struct body
        //   "other"   — everything else (enum, function body, if, ...)
        let mut scope_stack: Vec<&'static str> = Vec::new();

        for (line_number_zero_indexed, line) in file_content.lines.iter().enumerate() {
            let line_number = line_number_zero_indexed + 1;

            // Python collapses `/*` and `*/` independently; mirror that exactly
            // (any line containing both opens then closes within the same line,
            // leaving in_block_comment=false at end).
            if line.contains("/*") {
                in_block_comment = true;
            }
            if line.contains("*/") {
                in_block_comment = false;
                continue;
            }
            if in_block_comment {
                continue;
            }

            let trimmed = line.trim_start();
            if trimmed.starts_with("//") {
                continue;
            }

            let code = split_line_comment(line);
            let opens = code.matches('{').count();
            let closes = code.matches('}').count();

            if opens > 0 {
                let first_kind = classify_scope_open(code.trim(), &scope_stack);
                scope_stack.push(first_kind);
                for _ in 0..opens - 1 {
                    scope_stack.push("other");
                }
            }
            for _ in 0..closes {
                scope_stack.pop();
            }

            // Only a free function declared at exactly `namespace fl {` qualifies.
            if scope_stack.as_slice() != ["fl"] {
                continue;
            }

            let code_stripped = code.trim();
            if code_stripped.is_empty() || code_stripped.starts_with('#') {
                continue;
            }

            if !(code_stripped.contains("set_")
                || code_stripped.contains("enable_")
                || code_stripped.contains("disable_")
                || code_stripped.contains("use_"))
            {
                continue;
            }

            let Some(func_name) = match_setter_decl(code_stripped) else {
                continue;
            };

            if regex_public_settings_suppression().is_match(line) {
                continue;
            }
            if PUBLIC_SETTINGS_GRANDFATHERED.contains(&func_name.as_str()) {
                continue;
            }
            if PUBLIC_SETTINGS_FUNCTION_POINTER_SETTERS.contains(&func_name.as_str()) {
                continue;
            }
            if is_per_object_setter(code_stripped) {
                continue;
            }
            if is_wrapped_in_fastled_h(&func_name, fastled_h) {
                continue;
            }

            let wrapper_name = snake_to_camel(&func_name);
            violations.push((
                line_number,
                format!(
                    "fl::{func_name}(...) declared here is a public global setter \
but has no CFastLED::{wrapper_name}() wrapper in src/FastLED.h. \
New global setters must follow the Public Settings Pattern — add an `inline` \
one-liner on CFastLED that delegates to this free function. See exemplar at \
src/FastLED.h:1455 (setPowerModel -> fl::set_power_model) and the rule at \
agents/docs/cpp-standards.md -> 'Public Settings Pattern'."
                ),
            ));
        }

        violations
    }
}

fn classify_scope_open(code_line: &str, scope_stack: &[&str]) -> &'static str {
    if let Some(captures) = regex_named_namespace().captures(code_line) {
        let name = captures.get(1).map(|m| m.as_str()).unwrap_or("");
        if scope_stack == ["fl"] || scope_stack.is_empty() {
            if name == "fl" {
                return "fl";
            }
            return "ns_sub";
        }
        return "ns_sub";
    }
    if regex_anonymous_namespace().is_match(code_line) {
        return "ns_anon";
    }
    if regex_class_or_struct_keyword().is_match(code_line)
        && !regex_class_or_struct_forward_decl().is_match(code_line)
    {
        return "class";
    }
    "other"
}

fn match_setter_decl(code_line: &str) -> Option<String> {
    let captures = regex_public_setter_decl().captures(code_line)?;
    let name = captures.name("name")?.as_str();
    Some(name.to_string())
}

fn is_per_object_setter(decl_line: &str) -> bool {
    // Extract the first parameter (everything inside the outer `()`, stopping
    // at the first comma not inside `<>`).
    let Some(open) = decl_line.find('(') else {
        return false;
    };
    let after_open = &decl_line[open + 1..];
    let Some(close) = find_matching_paren(after_open) else {
        return false;
    };
    let params_raw = after_open[..close].trim();
    if params_raw.is_empty() {
        return false; // no params → global setter (e.g. disable_foo())
    }

    let mut first_param = String::new();
    let mut depth = 0i32;
    for ch in params_raw.chars() {
        match ch {
            '<' => depth += 1,
            '>' => depth -= 1,
            ',' if depth == 0 => break,
            _ => {}
        }
        first_param.push(ch);
    }
    let first_param = first_param.trim().to_string();
    if first_param.contains("(*") {
        return false; // function pointer — handled by allowlist
    }

    // Strip the trailing parameter NAME token (last `[a-zA-Z_]\w*`) so we're
    // left with just the type tokens.
    let mut tokens: Vec<&str> = first_param.split_whitespace().collect();
    if let Some(last) = tokens.last() {
        if regex_simple_identifier().is_match(last) {
            tokens.pop();
        }
    }
    let param_type_str = tokens.join(" ");

    let is_const = param_type_str.contains("const");
    let has_ptr_or_ref = param_type_str.contains('*') || param_type_str.contains('&');
    if !has_ptr_or_ref || is_const {
        return false;
    }

    // Get base type word. Strip *, &, const; pick first token.
    let mut base = String::new();
    for ch in param_type_str.chars() {
        if ch == '*' || ch == '&' {
            base.push(' ');
        } else {
            base.push(ch);
        }
    }
    let base = base.replace("const", "");
    let base = base.trim().to_string();
    let Some(first_token) = base.split_whitespace().next() else {
        return false;
    };
    let base_type = first_token
        .trim_start_matches(':')
        .rsplit("::")
        .next()
        .unwrap_or(first_token);
    !PUBLIC_SETTINGS_FUNDAMENTAL_TYPES.contains(&base_type)
}

fn find_matching_paren(s: &str) -> Option<usize> {
    let mut depth = 1i32;
    for (index, ch) in s.char_indices() {
        match ch {
            '(' => depth += 1,
            ')' => {
                depth -= 1;
                if depth == 0 {
                    return Some(index);
                }
            }
            _ => {}
        }
    }
    None
}

fn is_wrapped_in_fastled_h(func_name: &str, fastled_h: &str) -> bool {
    if fastled_h.is_empty() {
        return false;
    }
    // Escape the function name and look for it as a bare word on any non-
    // comment, non-include line — same heuristic as the Python implementation.
    let escaped = regex::escape(func_name);
    let pattern = match regex::Regex::new(&format!(r"\b{escaped}\b")) {
        Ok(re) => re,
        Err(_) => return false,
    };
    for line in fastled_h.lines() {
        let stripped = line.trim_start();
        if stripped.starts_with("#include") {
            continue;
        }
        if stripped.starts_with("//") || stripped.starts_with('*') {
            continue;
        }
        if pattern.is_match(line) {
            return true;
        }
    }
    false
}

fn snake_to_camel(snake: &str) -> String {
    let mut parts = snake.split('_');
    let head = parts.next().unwrap_or("").to_string();
    let mut camel = head;
    for part in parts {
        let mut chars = part.chars();
        if let Some(first) = chars.next() {
            camel.push(first.to_ascii_uppercase());
            for c in chars {
                camel.push(c);
            }
        }
    }
    camel
}
