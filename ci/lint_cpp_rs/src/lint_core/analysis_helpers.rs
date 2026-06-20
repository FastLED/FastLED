fn test_aggregator_rel_for_dir(excluded_dir_rel: &str) -> String {
    format!("{excluded_dir_rel}.cpp")
}

fn resolve_test_include(root_prefix: &str, aggregator_rel: &str, include_path: &str) -> String {
    let aggregator_parent = aggregator_rel
        .rsplit_once('/')
        .map_or("", |(parent, _)| parent);
    let from_aggregator = if aggregator_parent.is_empty() {
        include_path.to_string()
    } else {
        format!("{aggregator_parent}/{include_path}")
    };
    let candidate = join_project_path(root_prefix, &from_aggregator);
    if candidate.exists() {
        return normalize_path(&path_to_string(&candidate));
    }
    normalize_path(&path_to_string(&join_project_path(
        root_prefix,
        include_path,
    )))
}

fn collect_test_aggregation_included_files(
    root_prefix: &str,
    excluded_dir_rel: &str,
) -> (Option<String>, BTreeSet<String>) {
    let direct_aggregator_rel = test_aggregator_rel_for_dir(excluded_dir_rel);
    let primary_aggregator = join_project_path(root_prefix, &direct_aggregator_rel)
        .exists()
        .then_some(direct_aggregator_rel.clone());
    let mut included_files = BTreeSet::new();
    let mut check_dir = excluded_dir_rel.to_string();

    while check_dir.starts_with("tests/") && check_dir != "tests" {
        let aggregator_rel = test_aggregator_rel_for_dir(&check_dir);
        let aggregator = join_project_path(root_prefix, &aggregator_rel);
        if aggregator.exists() {
            for include in parse_aggregator_includes(&aggregator) {
                let resolved = resolve_test_include(root_prefix, &aggregator_rel, &include);
                if Path::new(&resolved).exists() {
                    included_files.insert(resolved);
                }
            }
        }
        let Some((parent, _)) = check_dir.rsplit_once('/') else {
            break;
        };
        check_dir = parent.to_string();
    }

    (primary_aggregator, included_files)
}

fn test_aggregation_check_single_file(file_path: &str) -> Vec<String> {
    let normalized = normalize_path(file_path);
    let root_prefix = project_root_prefix_for_file(&normalized);
    let Some(project_rel) = project_relative_path(&normalized) else {
        return Vec::new();
    };
    let resolved = normalize_path(&path_to_string(&join_project_path(
        &root_prefix,
        &project_rel,
    )));
    let mut violations = Vec::new();

    for excluded_dir in TEST_AGGREGATED_DIRS {
        let aggregator_rel = test_aggregator_rel_for_dir(excluded_dir);
        let aggregator_path = join_project_path(&root_prefix, &aggregator_rel);
        let aggregator_norm = normalize_path(&path_to_string(&aggregator_path));

        if aggregator_path.exists() && aggregator_norm == resolved {
            let (_, included_files) =
                collect_test_aggregation_included_files(&root_prefix, excluded_dir);
            let excluded_path = join_project_path(&root_prefix, excluded_dir);
            if excluded_path.exists() {
                for entry in WalkDir::new(&excluded_path)
                    .into_iter()
                    .filter_map(Result::ok)
                {
                    if !entry.file_type().is_file() {
                        continue;
                    }
                    let hpp_path = normalize_path(&path_to_string(entry.path()));
                    if !hpp_path.ends_with(".hpp") || included_files.contains(&hpp_path) {
                        continue;
                    }
                    let rel_file = project_relative_path(&hpp_path).unwrap_or(hpp_path);
                    violations.push(format!("{}: orphaned {}", aggregator_rel, rel_file));
                }
            }
            for include in parse_aggregator_includes(&aggregator_path) {
                let inc_resolved = resolve_test_include(&root_prefix, &aggregator_rel, &include);
                if project_relative_path(&inc_resolved)
                    .is_some_and(|rel| rel.starts_with(&format!("{excluded_dir}/")))
                    && include.ends_with(".cpp")
                {
                    violations.push(format!(
                        "{}: #include \"{include}\" should use .hpp",
                        aggregator_rel
                    ));
                }
            }
            return violations;
        }

        if project_rel == *excluded_dir
            || project_rel
                .strip_prefix(&format!("{excluded_dir}/"))
                .is_some()
        {
            if !project_rel.ends_with(".hpp") {
                continue;
            }
            let (aggregator, included_files) =
                collect_test_aggregation_included_files(&root_prefix, excluded_dir);
            let Some(aggregator_rel) = aggregator else {
                violations.push(format!("Missing aggregator: {}.cpp", excluded_dir));
                return violations;
            };
            if !included_files.contains(&resolved) {
                violations.push(format!("{}: orphaned {}", aggregator_rel, project_rel));
            }
            return violations;
        }
    }

    violations
}

fn required_fl_is_header(fl_is_macro: &str) -> Option<&'static str> {
    FL_IS_PREFIX_TO_HEADER
        .iter()
        .find_map(|(prefix, header)| fl_is_macro.starts_with(prefix).then_some(*header))
}

fn iwyu_is_top_level_platform_header(header_path: &str) -> bool {
    let Some(remainder) = header_path.strip_prefix("platforms/") else {
        return false;
    };
    !remainder.contains('/')
}

fn iwyu_classify_header(header_path: &str, include_line: &str) -> &'static str {
    if include_line.contains('<') && include_line.contains('>') {
        return "system";
    }
    if IWYU_INTERNAL_HEADER_PREFIXES
        .iter()
        .any(|prefix| header_path.starts_with(prefix))
    {
        return "internal";
    }
    if !header_path.contains('/') && ends_with_any(header_path, &[".h", ".hpp"]) {
        return "internal";
    }
    if header_path.starts_with("platforms/") {
        return "platform";
    }
    "system"
}

fn iwyu_format_violation(_header_path: &str, header_type: &str, include_line: &str) -> String {
    let type_desc = if header_type == "platform" {
        "Platform"
    } else {
        "System"
    };
    format!(
        "{type_desc} header not wrapped in IWYU pragma block: {include_line}\n  Wrap in:\n    // IWYU pragma: begin_keep\n    {include_line}\n    // IWYU pragma: end_keep\n  Or add inline pragma:\n    {include_line}  // IWYU pragma: keep"
    )
}

fn python_capitalize(part: &str) -> String {
    let mut chars = part.chars();
    let Some(first) = chars.next() else {
        return String::new();
    };
    let mut result = first.to_uppercase().collect::<String>();
    result.push_str(&chars.as_str().to_lowercase());
    result
}

fn convert_google_to_m_prefix(var_name: &str) -> String {
    let Some(base_name) = var_name.strip_suffix('_') else {
        return var_name.to_string();
    };
    let mut base_chars = base_name.chars();
    let Some(first) = base_chars.next() else {
        return "m".to_string();
    };
    if base_chars.as_str().chars().any(char::is_uppercase) {
        let first_upper = first.to_uppercase().collect::<String>();
        return format!("m{first_upper}{}", base_chars.as_str());
    }
    let converted = base_name
        .split('_')
        .filter(|part| !part.is_empty())
        .map(python_capitalize)
        .collect::<String>();
    format!("m{converted}")
}

fn convert_m_underscore_to_m_prefix(var_name: &str) -> String {
    let Some(remainder) = var_name.strip_prefix("m_") else {
        return var_name.to_string();
    };
    if remainder.is_empty() {
        return var_name.to_string();
    }
    if remainder.contains('_') {
        let converted = remainder
            .split('_')
            .filter(|part| !part.is_empty())
            .map(python_capitalize)
            .collect::<String>();
        return format!("m{converted}");
    }
    let mut chars = remainder.chars();
    let Some(first) = chars.next() else {
        return "m".to_string();
    };
    let first_upper = first.to_uppercase().collect::<String>();
    format!("m{first_upper}{}", chars.as_str())
}

fn is_inside_parens(code_part: &str, pos: usize) -> bool {
    let mut depth = 0_i32;
    for ch in code_part[..pos].chars() {
        if ch == '(' {
            depth += 1;
        } else if ch == ')' {
            depth -= 1;
        }
    }
    depth > 0
}

fn find_close_paren(text: &str, open_pos: usize) -> Option<usize> {
    let mut depth = 1_i32;
    for (offset, ch) in text[open_pos + 1..].char_indices() {
        if ch == '(' {
            depth += 1;
        } else if ch == ')' {
            depth -= 1;
            if depth == 0 {
                return Some(open_pos + 1 + offset);
            }
        }
    }
    None
}

fn find_close_paren_multiline(
    lines: &[String],
    start_idx: usize,
    open_col: usize,
) -> Option<(usize, usize)> {
    let line = &lines[start_idx];
    let mut depth = 1_i32;
    for (offset, ch) in line[open_col + 1..].char_indices() {
        if ch == '(' {
            depth += 1;
        } else if ch == ')' {
            depth -= 1;
            if depth == 0 {
                return Some((start_idx, open_col + 1 + offset));
            }
        }
    }
    for lookahead in 1..15 {
        let idx = start_idx + lookahead;
        if idx >= lines.len() {
            break;
        }
        for (col, ch) in lines[idx].char_indices() {
            if ch == '(' {
                depth += 1;
            } else if ch == ')' {
                depth -= 1;
                if depth == 0 {
                    return Some((idx, col));
                }
            }
        }
    }
    None
}

fn compute_block_comment_mask(lines: &[String]) -> Vec<bool> {
    let mut mask = Vec::with_capacity(lines.len());
    let mut inside = false;
    for line in lines {
        let line_inside = inside;
        let bytes = line.as_bytes();
        let mut index = 0;
        while index + 1 < bytes.len() {
            if inside {
                if bytes[index] == b'*' && bytes[index + 1] == b'/' {
                    inside = false;
                    index += 2;
                    continue;
                }
            } else if bytes[index] == b'/' && bytes[index + 1] == b'*' {
                inside = true;
                index += 2;
                continue;
            } else if bytes[index] == b'/' && bytes[index + 1] == b'/' {
                break;
            }
            index += 1;
        }
        mask.push(line_inside);
    }
    mask
}

fn strip_line_comment_code(line: &str) -> String {
    let mut in_string = false;
    let mut in_char = false;
    let mut previous = '\0';
    let chars: Vec<(usize, char)> = line.char_indices().collect();
    for (idx, ch) in &chars {
        if *ch == '"' && !in_char && previous != '\\' {
            in_string = !in_string;
        } else if *ch == '\'' && !in_string && previous != '\\' {
            in_char = !in_char;
        } else if !in_string && !in_char && *ch == '/' {
            let next_is_slash = line[*idx + ch.len_utf8()..].starts_with('/');
            if next_is_slash {
                return line[..*idx].to_string();
            }
        }
        previous = *ch;
    }
    line.to_string()
}

// Pre-compiled state for classify_noexcept_line: the single combined
// alternation regex over all class names + a vector of the same names
// so we can look up which name matched without re-running per-name regexes.
//
// Built ONCE per file in check_file_content; without this every non-blank
// line was paying O(class_names) calls to `Regex::new` which is
// milliseconds per call (zackees/fastled #2871 follow-up).
pub struct ClassNameRegex {
    pub names: Vec<String>,
    pub combined: Option<Regex>,
}

impl ClassNameRegex {
    pub fn build(class_names: &HashSet<String>) -> Self {
        let names: Vec<String> = class_names.iter().cloned().collect();
        let combined = if names.is_empty() {
            None
        } else {
            let alternation: Vec<String> =
                names.iter().map(|n| regex::escape(n)).collect();
            let pattern = format!(r"\b({})\s*\(", alternation.join("|"));
            Regex::new(&pattern).ok()
        };
        Self { names, combined }
    }
}

fn classify_noexcept_line(
    line: &str,
    class_names: &ClassNameRegex,
) -> Option<(&'static str, usize)> {
    let code = strip_line_comment_code(line);
    let stripped = code.trim();
    if stripped.is_empty() || stripped.starts_with('#') {
        return None;
    }

    if let Some(matched) = regex_destructor_decl().find(&code) {
        let tilde_pos = matched.start();
        let prefix = code[..tilde_pos].trim_end();
        if !prefix.ends_with("->") && !prefix.ends_with('.') {
            let paren = code[matched.start()..].find('(')? + matched.start();
            let close = find_close_paren(&code, paren)?;
            let inside = code[paren + 1..close].trim();
            if inside.is_empty() || inside == "void" {
                return Some(("destructor", paren));
            }
        }
    }

    if let Some(matched) = regex_operator_assign_decl().find(&code) {
        let paren = code[matched.start()..].find('(')? + matched.start();
        return Some(("assignment operator", paren));
    }

    let combined = class_names.combined.as_ref()?;
    for matched in combined.captures_iter(&code) {
        let outer = matched.get(0)?;
        let name_match = matched.get(1)?;
        let name = name_match.as_str();
        let prefix = code[..outer.start()].trim();
        if !NOEXCEPT_CTOR_QUALS.contains(&prefix) {
            continue;
        }
        let paren = outer.end() - 1;
        let rest = code[paren + 1..].trim_start();
        let copy_prefix = format!("const {name}");
        if rest.starts_with(&copy_prefix) {
            let after = rest[copy_prefix.len()..].trim_start();
            if after.starts_with('&') {
                return Some(("copy constructor", paren));
            }
        }
        if rest.starts_with(name) {
            let after = rest[name.len()..].trim_start();
            if after.starts_with("&&") {
                return Some(("move constructor", paren));
            }
        }
        if rest.starts_with(')')
            || rest.starts_with("void") && rest[4..].trim_start().starts_with(')')
        {
            return Some(("default constructor", paren));
        }
    }
    let _ = &class_names.names; // names retained for future per-match dispatch
    None
}

fn join_multiline_signature(lines: &[String], start: usize) -> Option<String> {
    let line = strip_line_comment_code(&lines[start]);
    if !line.contains('(') {
        return None;
    }
    let mut depth = 0_i32;
    for ch in line.chars() {
        if ch == '(' {
            depth += 1;
        } else if ch == ')' {
            depth -= 1;
        }
    }
    if depth <= 0 {
        return None;
    }
    let mut joined = line;
    for offset in 1..10 {
        let idx = start + offset;
        if idx >= lines.len() {
            break;
        }
        let next = strip_line_comment_code(&lines[idx]).trim().to_string();
        joined.push(' ');
        joined.push_str(&next);
        for ch in next.chars() {
            if ch == '(' {
                depth += 1;
            } else if ch == ')' {
                depth -= 1;
            }
        }
        if depth <= 0 {
            return Some(joined);
        }
    }
    None
}

fn signature_has_noexcept(lines: &[String], start: usize, open_paren: usize) -> bool {
    let Some((close_line, close_col)) = find_close_paren_multiline(lines, start, open_paren) else {
        return false;
    };
    let tail = &lines[close_line][close_col + 1..];
    if regex_has_noexcept_special().is_match(tail) {
        return true;
    }
    for offset in 1..6 {
        let idx = close_line + offset;
        if idx >= lines.len() {
            break;
        }
        let next = lines[idx].trim();
        if regex_has_noexcept_special().is_match(next) {
            return true;
        }
        if next.starts_with('{') || next.ends_with('{') || next == "};" {
            break;
        }
        if next.starts_with(':') && !next.starts_with("::") {
            break;
        }
        if next.ends_with(';') {
            break;
        }
    }
    false
}

fn platform_trampoline_suggestion(include_path: &str) -> &'static str {
    DEEP_PLATFORM_REPLACEMENTS
        .iter()
        .find_map(|(pattern, replacement)| include_path.contains(pattern).then_some(*replacement))
        .unwrap_or("platforms/*.h (appropriate trampoline)")
}

