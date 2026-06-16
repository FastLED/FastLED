struct PlatformIncludesChecker;

impl FileContentChecker for PlatformIncludesChecker {
    fn name(&self) -> &'static str {
        "PlatformIncludesChecker"
    }

    fn should_process_file(&self, file_path: &str, _project_root: &Path) -> bool {
        let normalized = normalize_path(file_path);
        if !ends_with_any(&normalized, &[".h", ".hpp"]) {
            return false;
        }
        if is_excluded_file(&normalized) {
            return false;
        }
        if normalized.contains("/platforms/") || normalized.contains("/tests/platforms/") {
            return false;
        }
        PLATFORM_INCLUDE_LOCATIONS
            .iter()
            .any(|location| normalized.contains(location))
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
            let comment = comment_part.to_ascii_lowercase();
            let has_exception = comment.contains("ok platform headers")
                || comment.contains("ok -- platform headers");
            if has_exception {
                continue;
            }

            let Some(capture) = regex_deep_platform_header().captures(code_part) else {
                continue;
            };
            let include_statement = capture.get(1).unwrap().as_str().trim();
            violations.push((
                index + 1,
                format!(
                    "Forbidden deep platform header: {include_statement} - Code in fl/** must use top-level platform headers (platforms/*.h) or fl/ alternatives instead of deep platform-specific headers. Deep includes (platforms/{{platform}}/**/*.h) bypass the trampoline architecture. If this include is necessary, add '// ok platform headers' comment to suppress. See src/platforms/README.md for architecture details."
                ),
            ));
        }

        violations
    }
}

struct PlatformTrampolineChecker;

impl FileContentChecker for PlatformTrampolineChecker {
    fn name(&self) -> &'static str {
        "PlatformTrampolineChecker"
    }

    fn should_process_file(&self, file_path: &str, project_root: &Path) -> bool {
        let normalized = normalize_path(file_path);
        if !is_under_project_subpath(&normalized, project_root, "src") {
            return false;
        }
        let Some(parts_after_src) = normalized.split("/src/").nth(1) else {
            return false;
        };
        let parts: Vec<&str> = parts_after_src.split('/').collect();
        if parts.first() == Some(&"platforms") {
            return false;
        }
        if parts.first() == Some(&"fl") {
            return true;
        }
        parts.len() == 1 && ends_with_any(&normalized, &[".cpp", ".h", ".hpp"])
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
            let comment = comment_part.to_ascii_lowercase();
            let has_exception = comment.contains("ok platform headers")
                || comment.contains("ok -- platform headers");
            if has_exception {
                continue;
            }

            let Some(capture) = regex_deep_platform_include().captures(code_part) else {
                continue;
            };
            let include_path = format!(
                "platforms/{}/{}",
                capture.get(1).unwrap().as_str(),
                capture.get(2).unwrap().as_str()
            );
            let suggestion = platform_trampoline_suggestion(&include_path);
            violations.push((
                index + 1,
                format!(
                    "Forbidden deep platform header: #include \"{include_path}\" - Use \"{suggestion}\" instead (platform trampolines only). Deep includes bypass the trampoline architecture. If necessary, add \"// ok platform headers\" comment to suppress."
                ),
            ));
        }

        violations
    }
}

struct SimdIntrinsicsChecker;

impl FileContentChecker for SimdIntrinsicsChecker {
    fn name(&self) -> &'static str {
        "SimdIntrinsicsChecker"
    }

    fn should_process_file(&self, file_path: &str, project_root: &Path) -> bool {
        let normalized = normalize_path(file_path);
        if !is_under_project_subpath(&normalized, project_root, "src") {
            return false;
        }
        if !ends_with_any(&normalized, &[".cpp", ".h", ".hpp", ".cpp.hpp"]) {
            return false;
        }
        if is_excluded_file(&normalized)
            || normalized.contains("third_party")
            || normalized.contains("thirdparty")
        {
            return false;
        }
        !normalized.contains("/platforms/")
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
            if line.contains("// ok platform simd") {
                continue;
            }

            let code_part = split_line_comment(line);
            for (pattern, description) in SIMD_PATTERNS {
                if code_part.contains(pattern) {
                    violations.push((index + 1, format!("{stripped}  [{description}]")));
                    break;
                }
            }
        }

        violations
    }
}

struct CppHppHeaderPairChecker;

impl FileContentChecker for CppHppHeaderPairChecker {
    fn name(&self) -> &'static str {
        "CppHppHeaderPairChecker"
    }

    fn should_process_file(&self, file_path: &str, project_root: &Path) -> bool {
        let normalized = normalize_path(file_path);
        if !normalized.ends_with(".cpp.hpp") {
            return false;
        }
        if !is_under_project_subpath(&normalized, project_root, "src/fl") {
            return false;
        }
        let basename = normalized.rsplit('/').next().unwrap_or(&normalized);
        !basename.starts_with("_build")
    }

    fn check_file_content(&self, file_content: &FileContent) -> Vec<(usize, String)> {
        if file_content
            .lines
            .iter()
            .take(5)
            .any(|line| line.contains("// ok no header"))
        {
            return Vec::new();
        }
        let Some(header_path) = file_content.path.strip_suffix(".cpp.hpp") else {
            return Vec::new();
        };
        let expected_header = format!("{header_path}.h");
        if Path::new(&expected_header).exists() {
            return Vec::new();
        }
        let mut expected_display = project_relative_guess(&expected_header);
        if cfg!(windows) {
            expected_display = expected_display.replace('/', "\\");
        }
        vec![(
            1,
            format!("No corresponding header found: expected {expected_display}"),
        )]
    }
}

struct IsHeaderIncludeChecker;

impl FileContentChecker for IsHeaderIncludeChecker {
    fn name(&self) -> &'static str {
        "IsHeaderIncludeChecker"
    }

    fn should_process_file(&self, file_path: &str, project_root: &Path) -> bool {
        let normalized = normalize_path(file_path);
        if !is_under_project_subpath(&normalized, project_root, "src/platforms") {
            return false;
        }
        if !ends_with_any(&normalized, &[".cpp", ".h", ".hpp"]) {
            return false;
        }
        let file_name = normalized.rsplit('/').next().unwrap_or(&normalized);
        if file_name.starts_with("is_") {
            return false;
        }
        let file_name_lower = file_name.to_lowercase();
        !file_name_lower.contains("compile_test") && !file_name_lower.contains("core_detection")
    }

    fn check_file_content(&self, file_content: &FileContent) -> Vec<(usize, String)> {
        let mut macros = BTreeSet::new();
        let mut in_block_comment = false;

        for line in &file_content.lines {
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
            let stripped = line.trim();
            if stripped.starts_with("//") || !regex_preprocessor_conditional().is_match(stripped) {
                continue;
            }
            let code_part = split_line_comment(line);
            for token in regex_fl_is_token().find_iter(code_part) {
                macros.insert(token.as_str().to_string());
            }
        }

        if macros.is_empty() {
            return Vec::new();
        }

        let mut included_names = HashSet::new();
        for line in &file_content.lines {
            if let Some(capture) = regex_include_any_path().captures(line) {
                let include_path = &capture[1];
                let include_name = include_path.rsplit('/').next().unwrap_or(include_path);
                included_names.insert(include_name.to_string());
            }
        }
        let has_is_platform = included_names.contains("is_platform.h");

        let mut required: BTreeMap<&str, BTreeSet<String>> = BTreeMap::new();
        for macro_name in macros {
            if let Some(header) = required_fl_is_header(&macro_name) {
                required.entry(header).or_default().insert(macro_name);
            }
        }

        let mut violations = Vec::new();
        for (header, macros) in required {
            if included_names.contains(header) || has_is_platform {
                continue;
            }
            let macros_str = macros.into_iter().collect::<Vec<_>>().join(", ");
            violations.push((
                0,
                format!(
                    "File uses {macros_str} but does not include '{header}' or 'is_platform.h'. Add: #include \"{header}\""
                ),
            ));
        }
        violations
    }
}

struct IwyuPragmaBlockChecker;

impl FileContentChecker for IwyuPragmaBlockChecker {
    fn name(&self) -> &'static str {
        "IwyuPragmaBlockChecker"
    }

    fn should_process_file(&self, file_path: &str, project_root: &Path) -> bool {
        let normalized = normalize_path(file_path);
        if !is_under_project_subpath(&normalized, project_root, "src/fl")
            && !is_under_project_subpath(&normalized, project_root, "src/platforms")
        {
            return false;
        }
        if !ends_with_any(&normalized, &[".h", ".hpp", ".hh", ".hxx"]) {
            return false;
        }
        !normalized.contains("/third_party/") && !normalized.contains("/platforms/stub/")
    }

    fn check_file_content(&self, file_content: &FileContent) -> Vec<(usize, String)> {
        let normalized_path = normalize_path(&file_content.path);
        let is_fl_file = normalized_path.contains("/src/fl/");
        let mut violations = Vec::new();
        let mut in_pragma_block = false;

        for (index, line) in file_content.lines.iter().enumerate() {
            if regex_iwyu_begin().is_match(line) {
                in_pragma_block = true;
                continue;
            }
            if regex_iwyu_end().is_match(line) {
                in_pragma_block = false;
                continue;
            }

            let Some(capture) = regex_iwyu_include().captures(line) else {
                continue;
            };
            let header_path = &capture[1];
            let comment = capture.get(2).map_or("", |value| value.as_str());
            if in_pragma_block || regex_iwyu_keep_inline().is_match(comment) {
                continue;
            }

            let header_type = iwyu_classify_header(header_path, line);
            if header_type == "internal" {
                continue;
            }
            if header_type == "platform" {
                if iwyu_is_top_level_platform_header(header_path) || !is_fl_file {
                    continue;
                }
                let message = iwyu_format_violation(header_path, header_type, line.trim());
                violations.push((index + 1, message));
            } else if header_type == "system" {
                let message = iwyu_format_violation(header_path, header_type, line.trim());
                violations.push((index + 1, message));
            }
        }

        violations
    }
}

struct MemberStyleChecker;

impl FileContentChecker for MemberStyleChecker {
    fn name(&self) -> &'static str {
        "MemberStyleChecker"
    }

    fn should_process_file(&self, file_path: &str, _project_root: &Path) -> bool {
        let normalized = normalize_path(file_path);
        if !ends_with_any(&normalized, &[".h", ".hpp", ".cpp"]) {
            return false;
        }
        if is_excluded_file(&normalized) || normalized.contains("third_party") {
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

            let mut code_part = split_line_comment(line).to_string();
            if code_part.trim().is_empty() {
                continue;
            }

            let has_trailing_underscore = code_part.contains("_;")
                || code_part.contains("_=")
                || code_part.contains("_,")
                || code_part.contains("_)")
                || code_part.contains("_ ");
            let has_m_underscore = code_part.contains("m_");
            if !has_trailing_underscore && !has_m_underscore {
                continue;
            }

            if code_part.contains('"') {
                code_part = regex_string_literal()
                    .replace_all(&code_part, "\"\"")
                    .to_string();
            }

            if has_trailing_underscore {
                let code_for_positions = code_part.clone();
                for capture in regex_member_trailing_underscore().captures_iter(&code_part) {
                    let Some(var_match) = capture.get(1) else {
                        continue;
                    };
                    let var_name = var_match.as_str();
                    if is_inside_parens(&code_for_positions, var_match.start()) {
                        continue;
                    }
                    if code_part.trim_start().starts_with('#') {
                        continue;
                    }
                    if var_name.contains("__")
                        || code_part.contains("COUNTER")
                        || code_part.contains("LINE")
                    {
                        continue;
                    }
                    let base = var_name.trim_end_matches('_');
                    if base
                        .chars()
                        .all(|ch| !ch.is_alphabetic() || ch.is_uppercase())
                        && var_name.chars().count() > 2
                    {
                        continue;
                    }
                    let suggested_name = convert_google_to_m_prefix(var_name);
                    violations.push((
                        index + 1,
                        format!(
                            "{var_name} -> {suggested_name}: {}",
                            stripped.chars().take(80).collect::<String>()
                        ),
                    ));
                }
            }

            if has_m_underscore {
                let code_for_positions = code_part.clone();
                for capture in regex_member_m_underscore().captures_iter(&code_part) {
                    let Some(var_match) = capture.get(1) else {
                        continue;
                    };
                    let var_name = var_match.as_str();
                    if is_inside_parens(&code_for_positions, var_match.start()) {
                        continue;
                    }
                    if code_part.trim_start().starts_with('#') {
                        continue;
                    }
                    if var_name.contains("__")
                        || code_part.contains("COUNTER")
                        || code_part.contains("LINE")
                    {
                        continue;
                    }
                    let suggested_name = convert_m_underscore_to_m_prefix(var_name);
                    violations.push((
                        index + 1,
                        format!(
                            "{var_name} -> {suggested_name}: {}",
                            stripped.chars().take(80).collect::<String>()
                        ),
                    ));
                }
            }
        }

        violations
    }
}

