struct TestIncludePathsChecker;

impl FileContentChecker for TestIncludePathsChecker {
    fn name(&self) -> &'static str {
        "TestIncludePathsChecker"
    }

    fn should_process_file(&self, file_path: &str, project_root: &Path) -> bool {
        let normalized = normalize_path(file_path);
        is_under_project_subpath(&normalized, project_root, "tests")
            && ends_with_any(&normalized, &[".cpp", ".h", ".hpp", ".cc", ".cxx"])
    }

    fn check_file_content(&self, file_content: &FileContent) -> Vec<(usize, String)> {
        let root_prefix = project_root_prefix_for_file(&file_content.path);
        let mut allowed_bare = top_level_headers(&root_prefix, "tests");
        allowed_bare.extend(top_level_headers(&root_prefix, "src"));
        let all_test_filenames = all_test_header_filenames(&root_prefix);
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
            if line.to_lowercase().contains("ok test include") {
                continue;
            }
            let Some(capture) = regex_quoted_include_line().captures(stripped) else {
                continue;
            };
            let include_path = &capture[1];
            if include_path.starts_with("../") || include_path.starts_with("./") {
                violations.push((
                    index + 1,
                    format!(
                        "Relative include not allowed in tests: #include \"{include_path}\" — use a fully-qualified path (e.g., tests/fl/...). Add '// ok test include' to suppress."
                    ),
                ));
                continue;
            }
            if TEST_INCLUDE_VALID_PREFIXES
                .iter()
                .any(|prefix| include_path.starts_with(prefix))
            {
                continue;
            }
            if !include_path.contains('/') && !include_path.contains('\\') {
                if allowed_bare.contains(include_path) || !all_test_filenames.contains(include_path)
                {
                    continue;
                }
                violations.push((
                    index + 1,
                    format!(
                        "Bare include not allowed in tests: #include \"{include_path}\" — use a fully-qualified path (e.g., fl/... or tests/...). Add '// ok test include' to suppress."
                    ),
                ));
                continue;
            }

            let file_dir = Path::new(&file_content.path)
                .parent()
                .map(Path::to_path_buf)
                .unwrap_or_default();
            if file_dir.join(include_path).exists() {
                violations.push((
                    index + 1,
                    format!(
                        "Sub-path include not allowed in tests: #include \"{include_path}\" — use a fully-qualified path (e.g., tests/fl/...). Add '// ok test include' to suppress."
                    ),
                ));
                continue;
            }
            let first_component = include_path.split('/').next().unwrap_or("");
            if matches!(
                first_component,
                "fl" | "misc" | "platforms" | "shared" | "profile"
            ) && !include_path.starts_with("tests/")
            {
                violations.push((
                    index + 1,
                    format!(
                        "Include missing tests/ prefix: #include \"{include_path}\" — use #include \"tests/{include_path}\" instead. Add '// ok test include' to suppress."
                    ),
                ));
            }
        }

        violations
    }
}

struct TestPathStructureChecker;

fn test_path_matching_source_exists(root_prefix: &str, test_name_no_ext: &str) -> bool {
    let expected_h = format!("src/{test_name_no_ext}.h");
    let expected_hpp = format!("src/{test_name_no_ext}.hpp");
    let expected_cpp_hpp = format!("src/{test_name_no_ext}.cpp.hpp");
    join_project_path(root_prefix, &expected_h).exists()
        || join_project_path(root_prefix, &expected_hpp).exists()
        || join_project_path(root_prefix, &expected_cpp_hpp).exists()
}

fn test_path_split_source_exists(root_prefix: &str, test_name_no_ext: &str) -> bool {
    let mut candidate = test_name_no_ext.to_string();
    while let Some((prefix, suffix)) = candidate.rsplit_once('_') {
        let slash_in_suffix = suffix.contains('/');
        if slash_in_suffix || prefix.is_empty() {
            break;
        }
        candidate = prefix.to_string();
        if test_path_matching_source_exists(root_prefix, &candidate) {
            return true;
        }
    }
    false
}

impl FileContentChecker for TestPathStructureChecker {
    fn name(&self) -> &'static str {
        "TestPathStructureChecker"
    }

    fn should_process_file(&self, file_path: &str, project_root: &Path) -> bool {
        let normalized = normalize_path(file_path);
        if !is_under_project_subpath(&normalized, project_root, "tests")
            || !ends_with_any(&normalized, &[".cpp", ".hpp"])
        {
            return false;
        }
        let basename = normalized.rsplit('/').next().unwrap_or(&normalized);
        if TEST_PATH_EXCLUDED_FILES.contains(&basename) {
            return false;
        }
        let Some(rel) = tests_relative_path(&normalized) else {
            return false;
        };
        let parts: Vec<&str> = rel.split('/').collect();
        if parts
            .first()
            .is_some_and(|part| matches!(*part, "misc" | "profile" | "shared"))
            || parts.contains(&"test_utils")
        {
            return false;
        }
        !is_under_config_excluded_test_dir(&normalized)
    }

    fn check_file_content(&self, file_content: &FileContent) -> Vec<(usize, String)> {
        let normalized = normalize_path(&file_content.path);
        let root_prefix = project_root_prefix_for_file(&normalized);
        let Some(rel_from_tests) = tests_relative_path(&normalized) else {
            return Vec::new();
        };
        let test_name_no_ext = path_without_extension(&rel_from_tests);
        let expected_h = format!("src/{test_name_no_ext}.h");
        let expected_hpp = format!("src/{test_name_no_ext}.hpp");
        let expected_cpp_hpp = format!("src/{test_name_no_ext}.cpp.hpp");
        if test_path_matching_source_exists(&root_prefix, &test_name_no_ext) {
            return Vec::new();
        }
        if test_path_split_source_exists(&root_prefix, &test_name_no_ext) {
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
        let project_rel = project_relative_path(&normalized).unwrap_or_else(|| normalized.clone());
        let test_name = normalized.rsplit('/').next().unwrap_or(&normalized);
        vec![(
            1,
            format!(
                "Test file has no corresponding source file at matching path. Test is at '{}' but no source file found at 'src/{}', 'src/{}', or 'src/{}'. \n\nREQUIRED ACTIONS (in order of preference):\n  1. RENAME the test to match the source file it's testing (best option)\n  2. MERGE this test into an existing test file that tests the same source — each test\n     file costs compile time, so consolidating into fewer files is strongly preferred\n  3. MOVE to 'tests/misc/{test_name}' if this truly doesn't test a specific source file\n\n⚠️  DO NOT add '// ok standalone' unless absolutely necessary. This amnesty is a last\nresort for rare infrastructure files that genuinely cannot be organized. AI agents\nshould NEVER add this comment — instead fix the path or consolidate tests.\n\nAvoid creating tests in 'tests/misc/' - prefer mirroring source directory structure.\nTest organization should mirror source organization for maintainability.\nNote: Source matcher checks .h, .hpp, and .cpp.hpp files.",
                python_path_display(&project_rel),
                python_path_display(&format!("{test_name_no_ext}.h")),
                python_path_display(&format!("{test_name_no_ext}.hpp")),
                python_path_display(&format!("{test_name_no_ext}.cpp.hpp")),
            ),
        )]
    }
}

struct TestAggregationChecker;

impl FileContentChecker for TestAggregationChecker {
    fn name(&self) -> &'static str {
        "TestAggregationChecker"
    }

    fn should_process_file(&self, file_path: &str, project_root: &Path) -> bool {
        let normalized = normalize_path(file_path);
        is_under_project_subpath(&normalized, project_root, "tests")
            && ends_with_any(&normalized, &[".cpp", ".hpp"])
    }

    fn check_file_content(&self, file_content: &FileContent) -> Vec<(usize, String)> {
        test_aggregation_check_single_file(&file_content.path)
            .into_iter()
            .map(|message| (0, message))
            .collect()
    }
}

#[derive(Clone, Copy)]
enum BannedHeadersScope {
    Fl,
    Lib8tion,
    FxSensorsPlatformsShared,
    Platforms,
    Examples,
    ThirdParty,
    Tests,
}

struct BannedHeadersChecker {
    banned_headers: &'static [&'static str],
    strict_mode: bool,
    scope: BannedHeadersScope,
}

impl BannedHeadersChecker {
    fn scope_matches(&self, path: &str, project_root: &Path) -> bool {
        match self.scope {
            BannedHeadersScope::Fl => is_under_project_subpath(path, project_root, "src/fl"),
            BannedHeadersScope::Lib8tion => {
                is_under_project_subpath(path, project_root, "src/lib8tion")
            }
            BannedHeadersScope::FxSensorsPlatformsShared => {
                is_under_project_subpath(path, project_root, "src/fx")
                    || is_under_project_subpath(path, project_root, "src/sensors")
                    || is_under_project_subpath(path, project_root, "src/platforms/shared")
            }
            BannedHeadersScope::Platforms => {
                is_under_project_subpath(path, project_root, "src/platforms")
            }
            BannedHeadersScope::Examples => {
                is_under_project_subpath(path, project_root, "examples")
            }
            BannedHeadersScope::ThirdParty => {
                is_under_project_subpath(path, project_root, "src/third_party")
            }
            BannedHeadersScope::Tests => is_under_project_subpath(path, project_root, "tests"),
        }
    }
}

impl FileContentChecker for BannedHeadersChecker {
    fn name(&self) -> &'static str {
        "BannedHeadersChecker"
    }

    fn should_process_file(&self, file_path: &str, project_root: &Path) -> bool {
        let normalized = normalize_path(file_path);
        if !ends_with_any(&normalized, &[".cpp", ".h", ".hpp", ".cpp.hpp", ".ino"]) {
            return false;
        }
        if is_excluded_file(&normalized) {
            return false;
        }
        self.scope_matches(&normalized, project_root)
    }

    fn check_file_content(&self, file_content: &FileContent) -> Vec<(usize, String)> {
        let file_ext = last_dot_extension(&file_content.path);
        let mut violations = Vec::new();

        for (index, line) in file_content.lines.iter().enumerate() {
            let stripped = line.trim();
            if stripped.starts_with("//") {
                continue;
            }

            if let Some(capture) = regex_private_libcpp_header().captures(line) {
                let header = capture.get(1).unwrap().as_str();
                let public_header = private_libcpp_header_recommendation(header);
                violations.push((
                    index + 1,
                    format!(
                        "Found private libc++ header \"{header}\" - Use {public_header} instead. Private libc++ headers (starting with __) are internal implementation details and should never be included directly. They are unstable across libc++ versions and break portability."
                    ),
                ));
            }

            let Some(capture) = regex_banned_header_include().captures(line) else {
                continue;
            };
            let header = capture.get(1).unwrap().as_str();
            if !self.banned_headers.contains(&header) {
                continue;
            }
            if banned_header_matches_exception(&file_content.path, header) {
                continue;
            }

            let recommendation = banned_header_recommendation(header);
            let has_bypass_comment =
                line.contains("// ok include") || line.contains("// OK include");
            if matches!(file_ext, "h" | "hpp") {
                violations.push((
                    index + 1,
                    format!(
                        "Found banned header '{header}' - Use {recommendation} instead (banned in header files). Try moving the #include to the corresponding .cpp implementation file, or find the equivalent header in fl/stl in the fl:: namespace."
                    ),
                ));
            } else if self.strict_mode && !has_bypass_comment {
                violations.push((
                    index + 1,
                    format!(
                        "Found banned header '{header}' - Use {recommendation} instead (strict mode, no bypass allowed). Try moving the #include to a .cpp implementation file, or find the equivalent header in fl/stl in the fl:: namespace."
                    ),
                ));
            } else if !self.strict_mode && !has_bypass_comment {
                violations.push((
                    index + 1,
                    format!(
                        "Found banned header '{header}' - Use {recommendation} instead. Try finding the equivalent header in fl/stl in the fl:: namespace, or if you need this header in a .cpp file, add '// ok include' comment to suppress this warning."
                    ),
                ));
            }
        }

        violations
    }
}

struct NamespaceIncludesChecker;

impl FileContentChecker for NamespaceIncludesChecker {
    fn name(&self) -> &'static str {
        "NamespaceIncludesChecker"
    }

    fn should_process_file(&self, file_path: &str, project_root: &Path) -> bool {
        let normalized = normalize_path(file_path);
        if !is_under_project_subpath(&normalized, project_root, "src") {
            return false;
        }
        let lower = normalized.to_ascii_lowercase();
        if [
            ".build",
            ".pio",
            ".venv",
            "libdeps",
            "third_party",
            "vendor",
            "tests",
        ]
        .iter()
        .any(|part| lower.contains(part))
        {
            return false;
        }
        ends_with_any(&normalized, &[".h", ".hpp"])
    }

    fn check_file_content(&self, file_content: &FileContent) -> Vec<(usize, String)> {
        if file_content
            .lines
            .iter()
            .any(|line| regex_allow_include_after_namespace().is_match(line))
        {
            return Vec::new();
        }

        let mut current_namespace: Option<(usize, String)> = None;
        let mut seen_namespace = false;
        let mut seen_include_after_namespace = false;

        for (index, line) in file_content.lines.iter().enumerate() {
            let stripped = line.trim();
            if stripped.is_empty() || stripped.starts_with("//") || stripped.starts_with("/*") {
                continue;
            }
            if current_namespace.is_some() && stripped.starts_with('}') {
                current_namespace = None;
            }
            if stripped.contains("using namespace")
                && regex_namespace_include_using().is_match(stripped)
            {
                current_namespace = Some((index + 1, namespace_include_snippet(line)));
                seen_namespace = true;
            }
            if stripped.contains("namespace")
                && stripped.contains('{')
                && regex_namespace_include_open().is_match(stripped)
            {
                current_namespace = Some((index + 1, namespace_include_snippet(line)));
                seen_namespace = true;
            }
            if stripped.contains("#include")
                && regex_namespace_include_directive().is_match(stripped)
            {
                if line.contains("// nolint") {
                    continue;
                }
                if current_namespace.is_some() {
                    // The Python checker stores this as a provisional violation, then replaces it
                    // with the thorough pass whenever a namespace/include pair was seen.
                }
                if seen_namespace {
                    seen_include_after_namespace = true;
                }
            }
        }

        if seen_namespace && seen_include_after_namespace {
            if !namespace_braces_are_balanced(&file_content.lines) {
                return Vec::new();
            }
            return namespace_include_thorough_violations(&file_content.lines);
        }

        Vec::new()
    }
}

struct NativePlatformDefinesChecker;

impl FileContentChecker for NativePlatformDefinesChecker {
    fn name(&self) -> &'static str {
        "NativePlatformDefinesChecker"
    }

    fn should_process_file(&self, file_path: &str, project_root: &Path) -> bool {
        let normalized = normalize_path(file_path);
        if !is_under_project_subpath(&normalized, project_root, "src") {
            return false;
        }
        if !ends_with_any(&normalized, &[".cpp", ".h", ".hpp"]) {
            return false;
        }
        if is_under_project_subpath(&normalized, project_root, "src/third_party") {
            return false;
        }

        let file_name = normalized.rsplit('/').next().unwrap_or(&normalized);
        let file_name_lower = file_name.to_lowercase();
        if file_name.starts_with("is_")
            || file_name_lower.contains("core_detection")
            || file_name_lower.contains("compile_test")
            || NATIVE_COMPILER_ABSTRACTION_FILES.contains(&file_name)
        {
            return false;
        }

        let src_root_file = project_relative_path(&normalized)
            .is_some_and(|rel| rel.starts_with("src/") && rel.matches('/').count() == 1);
        if src_root_file && NATIVE_ROOT_DISPATCH_HEADERS.contains(&file_name) {
            return false;
        }
        if NATIVE_DISPATCH_CONFIG_FILES.contains(&file_name)
            && normalized.contains("/src/lib8tion/")
        {
            return false;
        }

        if is_under_project_subpath(&normalized, project_root, "src/platforms") {
            let platforms_root_file = project_relative_path(&normalized).is_some_and(|rel| {
                rel.starts_with("src/platforms/")
                    && rel["src/platforms/".len()..].matches('/').count() == 0
            });
            if platforms_root_file {
                return false;
            }
            if file_name.starts_with("fastpin_") || file_name.starts_with("fastspi_") {
                return false;
            }
            if file_name.starts_with("led_sysdefs_") && file_name.matches('_').count() <= 2 {
                return false;
            }
            if file_name_lower.contains("dispatch") || file_name == "fastpin_legacy.h" {
                return false;
            }
        }

        true
    }

    fn check_file_content(&self, file_content: &FileContent) -> Vec<(usize, String)> {
        if !regex_preprocessor_conditional().is_match(&file_content.content) {
            return Vec::new();
        }
        if !NATIVE_TO_MODERN_DEFINES
            .iter()
            .any(|(native, _)| file_content.content.contains(native))
        {
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
            if stripped.starts_with("//") || !regex_preprocessor_conditional().is_match(stripped) {
                continue;
            }
            let code_part = split_line_comment(line);
            for (native_define, modern_define) in NATIVE_TO_MODERN_DEFINES {
                if !code_part.contains(native_define)
                    || !contains_ascii_word(code_part, native_define)
                {
                    continue;
                }
                violations.push((
                    index + 1,
                    format!(
                        "Native platform define '{native_define}' found in preprocessor conditional. Use '#ifdef {modern_define}' or '#if defined({modern_define})' instead (FL_IS_* macros are defined/undefined, never use bare '#if {modern_define}')."
                    ),
                ));
            }
        }

        violations
    }
}

struct NoexceptSpecialMembersChecker;

impl FileContentChecker for NoexceptSpecialMembersChecker {
    fn name(&self) -> &'static str {
        "NoexceptSpecialMembersChecker"
    }

    fn should_process_file(&self, file_path: &str, project_root: &Path) -> bool {
        let normalized = normalize_path(file_path);
        if !ends_with_any(&normalized, &[".h", ".hpp", ".cpp", ".cpp.hpp"]) {
            return false;
        }
        if !is_under_project_subpath(&normalized, project_root, "src/fl") {
            return false;
        }
        if normalized.ends_with("/noexcept.h") || normalized.ends_with("fl/stl/noexcept.h") {
            return false;
        }
        !normalized.contains("/third_party/")
    }

    fn check_file_content(&self, file_content: &FileContent) -> Vec<(usize, String)> {
        let class_names: HashSet<String> = regex_noexcept_class_def()
            .captures_iter(&file_content.content)
            .map(|capture| capture[1].to_string())
            .collect();
        if class_names.is_empty() {
            return Vec::new();
        }

        let comment_mask = compute_block_comment_mask(&file_content.lines);
        let mut violations = Vec::new();

        for (index, line) in file_content.lines.iter().enumerate() {
            let stripped = line.trim();
            if stripped.is_empty() || stripped.starts_with("//") || stripped.starts_with('#') {
                continue;
            }
            if comment_mask.get(index).copied().unwrap_or(false) {
                continue;
            }
            if stripped.starts_with("/*") || stripped.starts_with('*') {
                continue;
            }
            if regex_noexcept_suppress_special().is_match(line) {
                continue;
            }

            let mut info = classify_noexcept_line(line, &class_names);
            if info.is_none() {
                if let Some(joined) = join_multiline_signature(&file_content.lines, index) {
                    if let Some((kind, open_paren)) = classify_noexcept_line(&joined, &class_names)
                    {
                        let original_open = line.find('(').unwrap_or(open_paren);
                        info = Some((kind, original_open));
                    }
                }
            }
            let Some((kind, open_paren)) = info else {
                continue;
            };
            if signature_has_noexcept(&file_content.lines, index, open_paren) {
                continue;
            }
            violations.push((
                index + 1,
                format!("Missing FL_NOEXCEPT on {kind}: {stripped}"),
            ));
        }

        violations
    }
}
