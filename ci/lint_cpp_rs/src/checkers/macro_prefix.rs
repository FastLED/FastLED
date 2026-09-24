// New FastLED-owned macros use FL_. The FASTLED_* names in the centralized
// amnesty are retained only for source/user compatibility.

const LEGACY_MACRO_AMNESTY: &str = include_str!("../../legacy_macro_amnesty.txt");

struct MacroPrefixChecker {
    amnesty: &'static str,
}

impl MacroPrefixChecker {
    const fn production() -> Self {
        Self {
            amnesty: LEGACY_MACRO_AMNESTY,
        }
    }

    fn is_amnestied(&self, name: &str) -> bool {
        self.amnesty.lines().any(|line| line.trim() == name)
    }
}

impl FileContentChecker for MacroPrefixChecker {
    fn name(&self) -> &'static str {
        "MacroPrefixChecker"
    }

    fn should_process_file(&self, file_path: &str, project_root: &Path) -> bool {
        let normalized = normalize_path(file_path);
        is_under_project_subpath(&normalized, project_root, "src")
            && !is_under_dir(&normalized, "third_party")
            && ends_with_any(&normalized, &[".h", ".hpp", ".cpp", ".cpp.hpp"])
    }

    fn check_file_content(&self, file_content: &FileContent) -> Vec<(usize, String)> {
        if !file_content.content.contains("FASTLED_") {
            return Vec::new();
        }

        let mut violations = Vec::new();
        let mut lexical_state = CommentScanState::default();
        let mut raw_logical_line = String::new();
        let mut logical_start = 1;

        for (index, physical_line) in file_content.lines.iter().enumerate() {
            if raw_logical_line.is_empty() {
                logical_start = index + 1;
            }
            raw_logical_line.push_str(physical_line);

            // Preprocessing splices backslash-newline before comments and
            // literals are recognized. Preserve that ordering: otherwise a
            // continued string can hide the backslash and terminate this
            // logical directive too early.
            if raw_logical_line.trim_end().ends_with('\\') {
                let continuation = raw_logical_line
                    .rfind('\\')
                    .expect("trimmed logical line ends with a backslash");
                raw_logical_line.truncate(continuation);
                raw_logical_line.push(' ');
                continue;
            }

            let visible =
                mask_macro_prefix_literals_and_comments(&raw_logical_line, &mut lexical_state);
            let directive = visible.trim_start();
            if macro_prefix_directive().is_match(directive) {
                for found in macro_prefix_name().find_iter(directive) {
                    let name = found.as_str();
                    if !self.is_amnestied(name) {
                        violations.push((
                            logical_start,
                            format!(
                                "new macro name `{name}`; new FastLED-owned macros must use the FL_ prefix. Legacy compatibility names require explicit review in ci/lint_cpp_rs/legacy_macro_amnesty.txt"
                            ),
                        ));
                    }
                }
            }
            raw_logical_line.clear();
        }

        violations
    }
}

fn mask_macro_prefix_literals_and_comments(
    line: &str,
    state: &mut CommentScanState,
) -> String {
    let bytes = line.as_bytes();
    let mut visible = vec![b' '; bytes.len()];
    let mut cursor = 0;

    if let Some(delim) = state.in_raw_string.clone() {
        let close = format!("){delim}\"");
        let Some(pos) = find_subslice(bytes, close.as_bytes()) else {
            return String::from_utf8(visible).expect("spaces are valid UTF-8");
        };
        state.in_raw_string = None;
        cursor = pos + close.len();
    } else if let Some(quote) = state.in_quoted {
        let end = scan_quoted(bytes, 0, quote);
        if end == bytes.len() && bytes.last() == Some(&b'\\') {
            return String::from_utf8(visible).expect("spaces are valid UTF-8");
        }
        state.in_quoted = None;
        cursor = end;
    }

    while cursor < bytes.len() {
        if state.in_block_comment {
            if bytes[cursor..].starts_with(b"*/") {
                cursor += 2;
                state.in_block_comment = false;
            } else {
                cursor += 1;
            }
            continue;
        }

        if bytes[cursor..].starts_with(b"//") {
            break;
        }
        if bytes[cursor..].starts_with(b"/*") {
            cursor += 2;
            state.in_block_comment = true;
            continue;
        }
        if bytes[cursor] == b'"' && is_raw_string_open(bytes, cursor) {
            if let Some((delim, body_start)) = raw_string_delimiter(bytes, cursor) {
                let close = format!("){delim}\"");
                match find_subslice(&bytes[body_start..], close.as_bytes()) {
                    Some(pos) => cursor = body_start + pos + close.len(),
                    None => {
                        state.in_raw_string = Some(delim);
                        break;
                    }
                }
                continue;
            }
        }
        if bytes[cursor] == b'"'
            || (bytes[cursor] == b'\'' && !is_digit_separator_quote(bytes, cursor))
        {
            let quote = bytes[cursor];
            let end = scan_quoted(bytes, cursor + 1, quote);
            if end == bytes.len() && bytes.last() == Some(&b'\\') {
                state.in_quoted = Some(quote);
            }
            cursor = end;
            continue;
        }

        visible[cursor] = bytes[cursor];
        cursor += 1;
    }

    String::from_utf8(visible).expect("masking ASCII delimiters preserves UTF-8")
}
