fn is_top_level_include(include_path: &str) -> bool {
    !include_path.contains('/') && !include_path.contains('\\')
}

fn include_path_looks_like_fastled_code(include_path: &str) -> bool {
    if include_path.ends_with(".hpp") || include_path.ends_with(".cpp") {
        return true;
    }

    let filename = include_path.rsplit('/').next().unwrap_or(include_path);
    let filename = filename.to_ascii_lowercase();
    [
        "clockless",
        "fastpin",
        "fastspi",
        "led_sysdefs",
        "compile_test",
        "spi_output",
        "is_",
    ]
    .iter()
    .any(|pattern| filename.contains(pattern))
}

fn is_external_sdk_header(include_path: &str) -> bool {
    if EXTERNAL_SDK_PREFIXES
        .iter()
        .any(|prefix| include_path.starts_with(prefix))
    {
        return true;
    }

    for prefix in AMBIGUOUS_INCLUDE_PREFIXES {
        if include_path.starts_with(prefix) {
            return !include_path_looks_like_fastled_code(include_path);
        }
    }

    false
}

fn is_fastled_platform_relative(include_path: &str) -> bool {
    if FASTLED_PLATFORM_SUBDIRS
        .iter()
        .any(|subdir| include_path.starts_with(subdir))
    {
        return true;
    }

    AMBIGUOUS_INCLUDE_PREFIXES.iter().any(|prefix| {
        include_path.starts_with(prefix) && include_path_looks_like_fastled_code(include_path)
    })
}

fn banned_subpath_replacement(include_path: &str) -> Option<String> {
    BANNED_INTERNAL_SUBPATHS
        .iter()
        .find_map(|(banned_prefix, replacement_prefix)| {
            include_path
                .strip_prefix(banned_prefix)
                .map(|suffix| format!("{replacement_prefix}{suffix}"))
        })
}

fn is_valid_include_path(include_path: &str) -> bool {
    if is_top_level_include(include_path) || is_external_sdk_header(include_path) {
        return true;
    }
    if banned_subpath_replacement(include_path).is_some() {
        return false;
    }
    VALID_INCLUDE_PREFIXES
        .iter()
        .any(|prefix| include_path.starts_with(prefix))
}

fn is_relative_include_path(include_path: &str) -> bool {
    include_path.starts_with("./") || include_path.starts_with("../")
}

fn header_exists_for_file(file_path: &str, include_path: &str) -> bool {
    if is_relative_include_path(include_path) || is_external_sdk_header(include_path) {
        return true;
    }

    let normalized_path = normalize_path(file_path);
    let project_root = if let Some(index) = normalized_path.find("/src/") {
        PathBuf::from(&normalized_path[..index])
    } else {
        PathBuf::from(".")
    };

    project_root.join("src").join(include_path).exists()
}

fn typo_include_suggestion(include_path: &str) -> Option<&'static str> {
    TYPO_INCLUDE_PREFIXES
        .iter()
        .find_map(|(typo, correct)| include_path.starts_with(typo).then_some(*correct))
}

fn standard_attribute_replacement(attribute: &str) -> Option<&'static str> {
    ATTRIBUTE_MAPPINGS
        .iter()
        .find_map(|(name, replacement)| (*name == attribute).then_some(*replacement))
}

fn numeric_limit_suggestion(macro_name: &str) -> &'static str {
    match macro_name {
        "UINT8_MAX" => "fl::numeric_limits<uint8_t>::max()",
        "UINT16_MAX" => "fl::numeric_limits<uint16_t>::max()",
        "UINT32_MAX" => "fl::numeric_limits<uint32_t>::max()",
        "UINT64_MAX" => "fl::numeric_limits<uint64_t>::max()",
        "INT8_MAX" => "fl::numeric_limits<int8_t>::max()",
        "INT8_MIN" => "fl::numeric_limits<int8_t>::min()",
        "INT16_MAX" => "fl::numeric_limits<int16_t>::max()",
        "INT16_MIN" => "fl::numeric_limits<int16_t>::min()",
        "INT32_MAX" => "fl::numeric_limits<int32_t>::max()",
        "INT32_MIN" => "fl::numeric_limits<int32_t>::min()",
        "INT64_MAX" => "fl::numeric_limits<int64_t>::max()",
        "INT64_MIN" => "fl::numeric_limits<int64_t>::min()",
        "SIZE_MAX" => "fl::numeric_limits<size_t>::max()",
        "UINTMAX_MAX" => "fl::numeric_limits<uintmax_t>::max()",
        "UINTPTR_MAX" => "fl::numeric_limits<uintptr_t>::max()",
        "INTMAX_MAX" => "fl::numeric_limits<intmax_t>::max()",
        "INTMAX_MIN" => "fl::numeric_limits<intmax_t>::min()",
        "INTPTR_MAX" => "fl::numeric_limits<intptr_t>::max()",
        "INTPTR_MIN" => "fl::numeric_limits<intptr_t>::min()",
        "UCHAR_MAX" => "fl::numeric_limits<unsigned char>::max()",
        "USHRT_MAX" => "fl::numeric_limits<unsigned short>::max()",
        "UINT_MAX" => "fl::numeric_limits<unsigned int>::max()",
        "ULONG_MAX" => "fl::numeric_limits<unsigned long>::max()",
        "ULLONG_MAX" => "fl::numeric_limits<unsigned long long>::max()",
        "CHAR_MAX" => "fl::numeric_limits<char>::max()",
        "CHAR_MIN" => "fl::numeric_limits<char>::min()",
        "SHRT_MAX" => "fl::numeric_limits<short>::max()",
        "SHRT_MIN" => "fl::numeric_limits<short>::min()",
        "INT_MAX" => "fl::numeric_limits<int>::max()",
        "INT_MIN" => "fl::numeric_limits<int>::min()",
        "LONG_MAX" => "fl::numeric_limits<long>::max()",
        "LONG_MIN" => "fl::numeric_limits<long>::min()",
        "LLONG_MAX" => "fl::numeric_limits<long long>::max()",
        "LLONG_MIN" => "fl::numeric_limits<long long>::min()",
        _ => "fl::numeric_limits<T>::max/min()",
    }
}

fn is_std_bridge_file(file_path: &str) -> bool {
    let normalized = normalize_path(file_path);
    STD_BRIDGE_FILE_WHITELIST
        .iter()
        .any(|bridge_path| normalized.ends_with(bridge_path))
}

fn line_has_only_allowed_std_symbols(line: &str) -> bool {
    let code_part = split_line_comment(line);
    if !code_part.contains("std::") {
        return true;
    }

    regex_std_usage().find_iter(code_part).all(|usage| {
        ALLOWED_STD_SYMBOLS
            .iter()
            .any(|allowed| *allowed == usage.as_str())
    })
}

fn serial_replacement(method: &str) -> &'static str {
    match method {
        "print" => "fl::print(...) or fl::cout << ...",
        "println" => "fl::println(...) or fl::cout << ... << fl::endl",
        "printf" => "fl::cout << ... (build typed values) or fl::println(formatted)",
        "write" => "fl::write_bytes(buf, size)",
        "read" => "fl::read()",
        "available" => "fl::available()",
        "peek" => "fl::peek()",
        "readStringUntil" => "fl::readLine(delim) (returns fl::optional<fl::string>)",
        "flush" => "fl::flush(timeoutMs)",
        "begin" => "fl::serial_begin(baudRate)",
        _ => "an fl:: variant",
    }
}

fn strip_inline_block_comments(line: &str) -> String {
    let mut result = String::new();
    let mut rest = line;
    while let Some(start) = rest.find("/*") {
        result.push_str(&rest[..start]);
        let after_start = &rest[start + 2..];
        let Some(end) = after_start.find("*/") else {
            result.push_str(&rest[start..]);
            return result;
        };
        rest = &after_start[end + 2..];
    }
    result.push_str(rest);
    result
}

fn strip_comments_preserving_lines(lines: &[String]) -> String {
    let mut in_block_comment = false;
    lines
        .iter()
        .map(|line| {
            let visible = strip_block_comments_from_line(line, &mut in_block_comment);
            split_line_comment(&visible).to_string()
        })
        .collect::<Vec<_>>()
        .join("\n")
}

fn is_ascii_word_byte(byte: u8) -> bool {
    byte.is_ascii_alphanumeric() || byte == b'_'
}

fn contains_ascii_word(code: &str, word: &str) -> bool {
    let bytes = code.as_bytes();
    let mut search_start = 0;
    while let Some(relative_pos) = code[search_start..].find(word) {
        let pos = search_start + relative_pos;
        let end = pos + word.len();
        search_start = end;
        if (pos == 0 || !is_ascii_word_byte(bytes[pos - 1]))
            && (end >= bytes.len() || !is_ascii_word_byte(bytes[end]))
        {
            return true;
        }
    }
    false
}

fn banned_header_recommendation(header: &str) -> &'static str {
    BANNED_HEADER_RECOMMENDATIONS
        .iter()
        .find_map(|(candidate, recommendation)| (*candidate == header).then_some(*recommendation))
        .unwrap_or("Use fl/ alternatives instead of standard library headers")
}

fn private_libcpp_header_recommendation(header: &str) -> &'static str {
    if let Some((_, recommendation)) = PRIVATE_LIBCPP_HEADER_MAPPINGS_RS
        .iter()
        .find(|(candidate, _)| *candidate == header)
    {
        return *recommendation;
    }
    PRIVATE_LIBCPP_HEADER_MAPPINGS_RS
        .iter()
        .find_map(|(prefix, recommendation)| header.starts_with(prefix).then_some(*recommendation))
        .unwrap_or("\"appropriate FastLED header\" (check ci/iwyu/stdlib.imp)")
}

fn banned_header_matches_exception(file_path: &str, header: &str) -> bool {
    let normalized = normalize_path(file_path);
    BANNED_HEADER_EXCEPTIONS
        .iter()
        .filter(|(exception_header, _)| *exception_header == header)
        .any(|(_, pattern)| normalized == *pattern || normalized.ends_with(&format!("/{pattern}")))
}

fn last_dot_extension(path: &str) -> &str {
    path.rsplit('.').next().unwrap_or(path)
}

fn namespace_include_snippet(line: &str) -> String {
    let text = line.trim();
    if text.chars().count() <= 50 {
        return text.to_string();
    }
    text.chars().take(47).collect::<String>() + "..."
}

fn namespace_include_message(
    include_snippet: &str,
    namespace_info: Option<(usize, &str)>,
) -> String {
    if let Some((line, snippet)) = namespace_info {
        format!("{include_snippet} (namespace declared at line {line}: {snippet})")
    } else {
        include_snippet.to_string()
    }
}

fn namespace_braces_are_balanced(lines: &[String]) -> bool {
    let stripped = strip_comments_preserving_lines(lines);
    stripped.matches('{').count() == stripped.matches('}').count()
}

fn namespace_include_thorough_violations(lines: &[String]) -> Vec<(usize, String)> {
    let stripped_content = strip_comments_preserving_lines(lines);
    let stripped_lines: Vec<&str> = stripped_content.split('\n').collect();
    let mut violations = Vec::new();
    let mut namespace_stack: Vec<(usize, String)> = Vec::new();
    let mut last_namespace: Option<(usize, String)> = None;

    for (index, original_line) in lines.iter().enumerate() {
        let line_number = index + 1;
        let line_no_comments = stripped_lines.get(index).copied().unwrap_or("");
        let stripped = line_no_comments.trim();
        if stripped.is_empty() {
            continue;
        }

        let open_braces = line_no_comments.matches('{').count();
        let close_braces = line_no_comments.matches('}').count();

        if stripped.contains("using namespace")
            && regex_namespace_include_using().is_match(stripped)
        {
            let info = (line_number, namespace_include_snippet(original_line));
            last_namespace = Some(info.clone());
            if namespace_stack.is_empty() {
                namespace_stack.push(info);
            }
        }

        if stripped.contains("namespace")
            && stripped.contains('{')
            && regex_namespace_include_open().is_match(stripped)
        {
            let info = (line_number, namespace_include_snippet(original_line));
            last_namespace = Some(info.clone());
            namespace_stack.push(info);
        }

        if stripped.contains("#include")
            && regex_namespace_include_directive().is_match(stripped)
            && !namespace_stack.is_empty()
        {
            // Includes inside namespace blocks are handled by IncludeAfterNamespaceChecker.
        } else if stripped.contains("#include")
            && regex_namespace_include_directive().is_match(stripped)
            && !original_line.contains("// nolint")
            && last_namespace.is_some()
        {
            let include_snippet = namespace_include_snippet(original_line);
            let namespace_info = last_namespace
                .as_ref()
                .map(|(namespace_line, snippet)| (*namespace_line, snippet.as_str()));
            violations.push((
                line_number,
                namespace_include_message(&include_snippet, namespace_info),
            ));
        }

        if close_braces > open_braces {
            for _ in 0..(close_braces - open_braces) {
                if !namespace_stack.is_empty() {
                    namespace_stack.pop();
                }
            }
        }
    }

    violations
}

fn ctype_functions() -> impl Iterator<Item = &'static str> {
    CTYPE_FUNCTIONS
        .iter()
        .chain(CSTRING_FUNCTIONS.iter())
        .copied()
}

fn ctype_header(func: &str) -> &'static str {
    if CTYPE_FUNCTIONS.contains(&func) {
        "fl/stl/cctype.h"
    } else {
        "fl/stl/cstring.h"
    }
}

fn find_ctype_calls(code_part: &str) -> Vec<(&'static str, bool)> {
    let bytes = code_part.as_bytes();
    let mut calls = Vec::new();

    for func in ctype_functions() {
        let mut search_start = 0;
        while let Some(relative_pos) = code_part[search_start..].find(func) {
            let pos = search_start + relative_pos;
            let end = pos + func.len();
            search_start = end;

            if pos > 0 && is_ascii_word_byte(bytes[pos - 1]) {
                continue;
            }
            if end < bytes.len() && is_ascii_word_byte(bytes[end]) {
                continue;
            }

            let mut call_pos = end;
            while call_pos < bytes.len() && bytes[call_pos].is_ascii_whitespace() {
                call_pos += 1;
            }
            if call_pos >= bytes.len() || bytes[call_pos] != b'(' {
                continue;
            }

            let prefix = &code_part[..pos];
            if prefix.ends_with("fl::") {
                continue;
            }
            let is_global_qualified = pos >= 2
                && &bytes[pos - 2..pos] == b"::"
                && (pos == 2 || !is_ascii_word_byte(bytes[pos - 3]));
            calls.push((func, is_global_qualified));
        }
    }

    calls
}

fn find_stdint_matches(code_part: &str) -> Vec<&'static str> {
    let bytes = code_part.as_bytes();
    let mut matches = Vec::new();

    for (type_name, _) in STDINT_TYPE_MAPPINGS {
        if *type_name == "unsigned int" || *type_name == "signed int" {
            continue;
        }
        let mut search_start = 0;
        while let Some(relative_pos) = code_part[search_start..].find(type_name) {
            let pos = search_start + relative_pos;
            let end = pos + type_name.len();
            search_start = end;

            if (pos == 0 || !is_ascii_word_byte(bytes[pos - 1]))
                && (end >= bytes.len() || !is_ascii_word_byte(bytes[end]))
            {
                matches.push(*type_name);
            }
        }
    }

    for type_name in ["unsigned int", "signed int"] {
        let mut search_start = 0;
        while let Some(relative_pos) = code_part[search_start..].find(type_name) {
            let pos = search_start + relative_pos;
            let end = pos + type_name.len();
            search_start = end;

            let Some(next) = bytes.get(end).copied() else {
                continue;
            };
            if !matches!(next, b' ' | b'*' | b'&' | b',' | b')' | b']') {
                continue;
            }
            if (pos == 0 || !is_ascii_word_byte(bytes[pos - 1])) && !is_ascii_word_byte(next) {
                matches.push(type_name);
            }
        }
    }

    matches
}

fn stdint_fl_type(type_name: &str) -> &'static str {
    STDINT_TYPE_MAPPINGS
        .iter()
        .find_map(|(name, replacement)| (*name == type_name).then_some(*replacement))
        .unwrap_or("u32")
}

fn unit_test_fl_macro(macro_name: &str) -> Option<&'static str> {
    UNIT_TEST_BANNED_MACROS
        .iter()
        .find_map(|(bare, replacement)| (*bare == macro_name).then_some(*replacement))
}

fn project_relative_guess(path: &str) -> String {
    let normalized = normalize_path(path);
    for marker in ["/src/", "/tests/", "/examples/"] {
        if let Some(index) = normalized.find(marker) {
            return normalized[index + 1..].to_string();
        }
    }
    for prefix in ["src/", "tests/", "examples/"] {
        if normalized.starts_with(prefix) {
            return normalized;
        }
    }
    normalized
}

fn project_root_prefix_for_file(path: &str) -> String {
    let normalized = normalize_path(path);
    for marker in ["/tests/", "/src/", "/examples/"] {
        if let Some(index) = normalized.find(marker) {
            return normalized[..index].to_string();
        }
    }
    String::new()
}

fn join_project_path(root_prefix: &str, rel_path: &str) -> PathBuf {
    if root_prefix.is_empty() {
        PathBuf::from(rel_path)
    } else {
        PathBuf::from(format!(
            "{root_prefix}/{}",
            rel_path.trim_start_matches('/')
        ))
    }
}

fn project_relative_path(path: &str) -> Option<String> {
    let normalized = normalize_path(path);
    for marker in ["/tests/", "/src/", "/examples/"] {
        if let Some(index) = normalized.find(marker) {
            return Some(normalized[index + 1..].to_string());
        }
    }
    for prefix in ["tests/", "src/", "examples/"] {
        if normalized.starts_with(prefix) {
            return Some(normalized);
        }
    }
    None
}

fn tests_relative_path(path: &str) -> Option<String> {
    let normalized = normalize_path(path);
    if let Some(index) = normalized.find("/tests/") {
        return Some(normalized[index + "/tests/".len()..].to_string());
    }
    normalized
        .strip_prefix("tests/")
        .map(|value| value.to_string())
}

fn path_without_extension(path: &str) -> String {
    for suffix in [".cpp.hpp", ".cpp", ".hpp", ".h"] {
        if let Some(stripped) = path.strip_suffix(suffix) {
            return stripped.to_string();
        }
    }
    path.to_string()
}

fn python_path_display(rel_path: &str) -> String {
    if cfg!(windows) {
        rel_path.replace('/', "\\")
    } else {
        rel_path.to_string()
    }
}

fn is_under_config_excluded_test_dir(path: &str) -> bool {
    let Some(project_rel) = project_relative_path(path) else {
        return false;
    };
    TEST_CONFIG_EXCLUDED_DIRS
        .iter()
        .any(|dir| project_rel == *dir || project_rel.strip_prefix(&format!("{dir}/")).is_some())
}

fn top_level_headers(root_prefix: &str, dir: &str) -> HashSet<String> {
    let mut headers = HashSet::new();
    let root = join_project_path(root_prefix, dir);
    let Ok(entries) = fs::read_dir(root) else {
        return headers;
    };
    for entry in entries.flatten() {
        let path = entry.path();
        if !path.is_file() {
            continue;
        }
        let name = path
            .file_name()
            .and_then(|value| value.to_str())
            .unwrap_or("");
        if name.ends_with(".h") || name.ends_with(".hpp") {
            headers.insert(name.to_string());
        }
    }
    headers
}

fn all_test_header_filenames(root_prefix: &str) -> HashSet<String> {
    let root = join_project_path(root_prefix, "tests");
    let mut filenames = HashSet::new();
    if !root.exists() {
        return filenames;
    }
    for entry in WalkDir::new(root).into_iter().filter_map(Result::ok) {
        if !entry.file_type().is_file() {
            continue;
        }
        let path = entry.path();
        let path_str = normalize_path(&path_to_string(path));
        if !ends_with_any(&path_str, &[".h", ".hpp", ".cpp.hpp"]) {
            continue;
        }
        if let Some(name) = path.file_name().and_then(|value| value.to_str()) {
            filenames.insert(name.to_string());
        }
    }
    filenames
}

fn source_mirror_dir_has_headers(root_prefix: &str, test_dir_path: &str) -> bool {
    let src_dir = join_project_path(root_prefix, &format!("src/{test_dir_path}"));
    let Ok(entries) = fs::read_dir(src_dir) else {
        return false;
    };
    entries.flatten().any(|entry| {
        let path = entry.path();
        path.is_file()
            && path
                .file_name()
                .and_then(|value| value.to_str())
                .is_some_and(|name| {
                    name.ends_with(".h") || name.ends_with(".hpp") || name.ends_with(".cpp.hpp")
                })
    })
}

fn parse_aggregator_includes(path: &Path) -> BTreeSet<String> {
    let mut includes = BTreeSet::new();
    let Ok(content) = fs::read_to_string(path) else {
        return includes;
    };
    for line in content.lines() {
        if let Some(capture) = regex_quoted_include_line().captures(line) {
            includes.insert(capture[1].to_string());
        }
    }
    includes
}

