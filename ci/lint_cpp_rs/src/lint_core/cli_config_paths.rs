// CLI argument parsing, input-file collection and the path/comment
// helpers shared by the checkers.
//
// Split out of processor_registry_cli.rs to satisfy the 1,000-line
// ceiling enforced by `rust_lint_source_files_stay_under_one_thousand_lines`.
// Included at crate root by lib.rs, same as the file it came from.

#[derive(Debug)]
struct CliConfig {
    output_format: OutputFormat,
    project_root: PathBuf,
    selected_checkers: Option<HashSet<String>>,
    show_help: bool,
    list_checkers: bool,
    paths: Vec<String>,
}

#[derive(Debug, Clone, Copy)]
enum OutputFormat {
    Json,
    Text,
}

impl CliConfig {
    fn parse<I>(args: I) -> Result<Self, DynError>
    where
        I: IntoIterator<Item = String>,
    {
        let mut output_format = OutputFormat::Text;
        let mut project_root = std::env::current_dir()?;
        let mut selected_checkers = HashSet::new();
        let mut list_checkers = false;
        let mut paths = Vec::new();

        let mut iter = args.into_iter();
        while let Some(arg) = iter.next() {
            match arg.as_str() {
                "--format" => {
                    let value = iter.next().ok_or("--format requires json or text")?;
                    output_format = match value.as_str() {
                        "json" => OutputFormat::Json,
                        "text" => OutputFormat::Text,
                        other => return Err(format!("unsupported --format value: {other}").into()),
                    };
                }
                "--project-root" => {
                    let value = iter.next().ok_or("--project-root requires a path")?;
                    project_root = PathBuf::from(value).canonicalize()?;
                }
                "--checker" => {
                    let value = iter.next().ok_or("--checker requires a checker name")?;
                    for checker in value.split(',') {
                        let checker = checker.trim();
                        if !checker.is_empty() {
                            selected_checkers.insert(checker.to_string());
                        }
                    }
                }
                "--list-checkers" => list_checkers = true,
                "--help" | "-h" => {
                    print_help();
                    return Ok(Self {
                        output_format,
                        project_root,
                        selected_checkers: None,
                        show_help: true,
                        list_checkers: false,
                        paths,
                    });
                }
                _ if arg.starts_with("--") => {
                    return Err(format!("unknown argument: {arg}").into());
                }
                _ => paths.push(arg),
            }
        }

        let selected_checkers = if selected_checkers.is_empty() {
            None
        } else {
            Some(selected_checkers)
        };

        Ok(Self {
            output_format,
            project_root,
            selected_checkers,
            show_help: false,
            list_checkers,
            paths,
        })
    }
}

fn print_help() {
    println!(
        "fastled-lint\n\
\n\
Usage:\n\
  fastled-lint [--format text|json] [--checker name[,name...]] [--project-root PATH] [files-or-globs...]\n\
\n\
When no files are supplied, scans src/, examples/, and tests/ under --project-root.\n\
Use --list-checkers to print the Rust-supported checker names."
    );
}

fn collect_input_files(project_root: &Path, inputs: &[String]) -> Result<Vec<PathBuf>, DynError> {
    let mut files = BTreeSet::new();

    if inputs.is_empty() {
        for dir in ["src", "examples", "tests"] {
            let root = project_root.join(dir);
            if root.exists() {
                collect_directory_files(&root, &mut files);
            }
        }
    } else {
        for input in inputs {
            let mut input_files = BTreeSet::new();
            if input.contains('*') || input.contains('?') || input.contains('[') {
                let mut matched = false;
                for entry in glob(input)? {
                    matched = true;
                    let path = entry?;
                    collect_path(&path, &mut input_files, false);
                }
                if !matched || input_files.is_empty() {
                    return Err(format!("input pattern matched no files: {input}").into());
                }
            } else {
                let path = PathBuf::from(input);
                if !path.exists() {
                    return Err(format!("input path not found: {input}").into());
                }
                collect_path(&path, &mut input_files, true);
                if input_files.is_empty() {
                    return Err(format!("input path produced no lintable files: {input}").into());
                }
            }
            files.extend(input_files);
        }
    }

    Ok(files.into_iter().collect())
}

fn collect_path(path: &Path, files: &mut BTreeSet<PathBuf>, allow_any_file: bool) {
    if path.is_dir() {
        collect_directory_files(path, files);
    } else if path.is_file()
        && (allow_any_file || is_cpp_like_path(&normalize_path(&path_to_string(path))))
    {
        files.insert(path.to_path_buf());
    }
}

fn collect_directory_files(root: &Path, files: &mut BTreeSet<PathBuf>) {
    for entry in WalkDir::new(root)
        .into_iter()
        .filter_entry(|entry| should_visit_directory(entry.path()))
        .filter_map(Result::ok)
    {
        if !entry.file_type().is_file() {
            continue;
        }
        let path = entry.path();
        if parent_has_cpp_no_lint_marker(path) {
            continue;
        }
        if is_cpp_like_path(&normalize_path(&path_to_string(path))) {
            files.insert(path.to_path_buf());
        }
    }
}

fn should_visit_directory(path: &Path) -> bool {
    if !path.is_dir() {
        return true;
    }
    let Some(name) = path.file_name().and_then(|value| value.to_str()) else {
        return true;
    };
    !name.starts_with(".build")
        && !matches!(
            name,
            "build" | ".venv" | ".cache" | "__pycache__" | "node_modules"
        )
}

fn parent_has_cpp_no_lint_marker(path: &Path) -> bool {
    path.parent()
        .map(|parent| parent.join(".cpp_no_lint").exists())
        .unwrap_or(false)
}

fn is_cpp_like_path(path: &str) -> bool {
    CPP_EXTENSIONS
        .iter()
        .any(|extension| path.ends_with(&format!(".{extension}")))
}

fn normalize_path(path: &str) -> String {
    let normalized = path.replace('\\', "/");
    if let Some(rest) = normalized.strip_prefix("//?/UNC/") {
        return format!("//{rest}");
    }
    if let Some(rest) = normalized.strip_prefix("//?/") {
        return rest.to_string();
    }
    normalized
}

fn path_to_string(path: &Path) -> String {
    path.to_string_lossy().to_string()
}

fn relative_display_path(path: &str, project_root: &Path) -> String {
    let normalized_root = normalize_path(&path_to_string(project_root));
    let normalized_path = normalize_path(path);
    normalized_path
        .strip_prefix(&format!("{normalized_root}/"))
        .unwrap_or(&normalized_path)
        .to_string()
}

fn is_under_dir(path: &str, dir: &str) -> bool {
    let normalized = normalize_path(path);
    normalized == dir
        || normalized.starts_with(&format!("{dir}/"))
        || normalized.contains(&format!("/{dir}/"))
}

fn is_under_project_subpath(path: &str, project_root: &Path, subpath: &str) -> bool {
    let normalized_path = normalize_path(path);
    let normalized_root = normalize_path(&path_to_string(project_root));
    let subpath = subpath.trim_matches('/');
    normalized_path == format!("{normalized_root}/{subpath}")
        || normalized_path.starts_with(&format!("{normalized_root}/{subpath}/"))
        || normalized_path == subpath
        || normalized_path.starts_with(&format!("{subpath}/"))
}

fn ends_with_any(path: &str, suffixes: &[&str]) -> bool {
    suffixes.iter().any(|suffix| path.ends_with(suffix))
}

fn is_excluded_file(path: &str) -> bool {
    EXCLUDED_FILES
        .iter()
        .any(|excluded| path.ends_with(excluded))
}

/// Byte index of the first `//` on `line` that is not inside a literal.
///
/// `line` has already had block comments removed, so only string, character
/// and raw-string literals can hide a `//` here. Scanning them is what keeps
/// `R"(//)"; FL_IRAM ...` from truncating away the real code that follows
/// (FastLED#3960).
fn line_comment_start(line: &str) -> Option<usize> {
    let bytes = line.as_bytes();
    let mut cursor = 0;

    while cursor < bytes.len() {
        if bytes[cursor..].starts_with(b"//") {
            return Some(cursor);
        }
        if bytes[cursor] == b'"' && is_raw_string_open(bytes, cursor) {
            if let Some((delim, body_start)) = raw_string_delimiter(bytes, cursor) {
                let close = format!("){delim}\"");
                cursor = match find_subslice(&bytes[body_start..], close.as_bytes()) {
                    Some(pos) => body_start + pos + close.len(),
                    // Unterminated on this line: the rest is literal data.
                    None => bytes.len(),
                };
                continue;
            }
            // Malformed delimiter: not a raw string after all, so fall
            // through and read it as an ordinary string literal.
        }
        if bytes[cursor] == b'"'
            || (bytes[cursor] == b'\'' && !is_digit_separator_quote(bytes, cursor))
        {
            cursor = scan_quoted(bytes, cursor + 1, bytes[cursor]);
            continue;
        }
        cursor += 1;
    }

    None
}

fn split_line_comment(line: &str) -> &str {
    match line_comment_start(line) {
        Some(index) => &line[..index],
        None => line,
    }
}

/// Where a physical line begins, for the comment scanner.
///
/// A single `bool` cannot express this. A raw string or a
/// backslash-continued literal carries text across a line boundary, and that
/// text may contain `/*` or `//` that are data, not comment introducers.
/// Tracking only `in_block_comment` meant an unterminated `/*` inside such a
/// literal swallowed every following line of real code, silently hiding
/// diagnostics from every checker that funnels through this helper
/// (FastLED#3960).
#[derive(Clone, Debug, Default, PartialEq, Eq)]
struct CommentScanState {
    /// Inside `/* ... */`.
    in_block_comment: bool,
    /// Inside `R"delim( ... )delim"`, holding `delim`.
    in_raw_string: Option<String>,
    /// Inside a `"` or `'` literal continued by a trailing backslash.
    in_quoted: Option<u8>,
}

fn find_subslice(haystack: &[u8], needle: &[u8]) -> Option<usize> {
    if needle.is_empty() || haystack.len() < needle.len() {
        return None;
    }
    haystack.windows(needle.len()).position(|window| window == needle)
}

/// Scan for the unescaped `quote` that closes a literal, starting at `from`.
///
/// Returns the index just past the closing quote, or `bytes.len()` when the
/// literal does not close on this line.
fn scan_quoted(bytes: &[u8], from: usize, quote: u8) -> usize {
    let mut cursor = from;
    while cursor < bytes.len() {
        if bytes[cursor] == b'\\' {
            cursor = (cursor + 2).min(bytes.len());
        } else if bytes[cursor] == quote {
            return cursor + 1;
        } else {
            cursor += 1;
        }
    }
    bytes.len()
}

/// True when the `"` at `quote_pos` opens a raw string.
///
/// Only `R`, `LR`, `uR`, `UR` and `u8R` introduce one, and the whole prefix
/// must start at a non-identifier character: maximal munch lexes
/// `nameLR"..."` as the identifier `nameLR` followed by an ordinary string,
/// not as a raw string.
fn is_raw_string_open(bytes: &[u8], quote_pos: usize) -> bool {
    if quote_pos == 0 || bytes[quote_pos - 1] != b'R' {
        return false;
    }

    // Longest encoding prefix that can sit in front of the `R`.
    let prefix_len = if quote_pos >= 3 && &bytes[quote_pos - 3..quote_pos] == b"u8R" {
        3
    } else if quote_pos >= 2 && matches!(bytes[quote_pos - 2], b'L' | b'u' | b'U') {
        2
    } else {
        1
    };

    match quote_pos.checked_sub(prefix_len + 1).map(|index| bytes[index]) {
        // The prefix starts the line.
        None => true,
        Some(byte) => !is_ascii_identifier_byte(byte),
    }
}

fn is_ascii_identifier_byte(byte: u8) -> bool {
    byte.is_ascii_alphanumeric() || byte == b'_'
}

/// True for a character the standard allows in a raw-string delimiter.
///
/// [lex.string] admits any basic-source character except space, `(`, `)`,
/// `\` and the control characters. `"` *is* admitted, so `R"""(body)"""` is a
/// well-formed raw string with the two-character delimiter `""`. `$`, `@` and
/// backtick are not basic-source characters.
fn is_raw_delimiter_byte(byte: u8) -> bool {
    byte.is_ascii_graphic() && !matches!(byte, b'(' | b')' | b'\\' | b'$' | b'@' | b'`')
}

/// Read the delimiter of the raw string whose `"` sits at `quote_pos`.
///
/// Returns the delimiter and the index just past the opening `(`.
fn raw_string_delimiter(bytes: &[u8], quote_pos: usize) -> Option<(String, usize)> {
    let mut cursor = quote_pos + 1;
    let mut delim = String::new();
    while cursor < bytes.len() {
        match bytes[cursor] {
            b'(' => return Some((delim, cursor + 1)),
            // The standard caps the delimiter at 16 characters; anything
            // outside the d-char set means this was not a raw string.
            byte if is_raw_delimiter_byte(byte) && delim.len() < 16 => {
                delim.push(byte as char);
                cursor += 1;
            }
            _ => return None,
        }
    }
    None
}

fn strip_block_comments_from_line(line: &str, state: &mut CommentScanState) -> String {
    let bytes = line.as_bytes();
    let mut visible = String::with_capacity(line.len());
    let mut cursor = 0;
    let mut visible_start = 0;

    // Finish a literal carried over from the previous physical line before
    // looking for any comment introducer: everything up to its terminator is
    // literal data.
    if let Some(delim) = state.in_raw_string.clone() {
        let close = format!("){delim}\"");
        match find_subslice(bytes, close.as_bytes()) {
            Some(pos) => {
                state.in_raw_string = None;
                cursor = pos + close.len();
            }
            None => return line.to_string(),
        }
    } else if let Some(quote) = state.in_quoted {
        let end = scan_quoted(bytes, 0, quote);
        if end == bytes.len() && bytes.last() == Some(&b'\\') {
            return line.to_string();
        }
        state.in_quoted = None;
        cursor = end;
    }

    while cursor < bytes.len() {
        if state.in_block_comment {
            if bytes[cursor..].starts_with(b"*/") {
                cursor += 2;
                visible_start = cursor;
                state.in_block_comment = false;
            } else {
                cursor += 1;
            }
            continue;
        }

        if bytes[cursor..].starts_with(b"//") {
            visible.push_str(&line[visible_start..]);
            return visible;
        }
        if bytes[cursor..].starts_with(b"/*") {
            visible.push_str(&line[visible_start..cursor]);
            cursor += 2;
            state.in_block_comment = true;
            continue;
        }
        if bytes[cursor] == b'"' && is_raw_string_open(bytes, cursor) {
            let raw_start = cursor;
            match raw_string_delimiter(bytes, cursor) {
                Some((delim, body_start)) => {
                    let close = format!("){delim}\"");
                    match find_subslice(&bytes[body_start..], close.as_bytes()) {
                        Some(pos) => cursor = body_start + pos + close.len(),
                        None => {
                            // Runs on to the next line; nothing after this is
                            // code on this line.
                            state.in_raw_string = Some(delim);
                            cursor = bytes.len();
                        }
                    }
                }
                // Malformed delimiter: not a raw string after all, so fall
                // through and read it as an ordinary string literal.
                None => {}
            }
            if cursor != raw_start {
                continue;
            }
        }
        if bytes[cursor] == b'"'
            || (bytes[cursor] == b'\'' && !is_digit_separator_quote(bytes, cursor))
        {
            let quote = bytes[cursor];
            let end = scan_quoted(bytes, cursor + 1, quote);
            if end == bytes.len() && bytes.last() == Some(&b'\\') {
                // Backslash-continued literal: the rest is on the next line.
                state.in_quoted = Some(quote);
            }
            cursor = end;
            continue;
        }
        cursor += 1;
    }

    if !state.in_block_comment {
        visible.push_str(&line[visible_start..]);
    }
    visible
}

fn is_digit_separator_quote(bytes: &[u8], cursor: usize) -> bool {
    cursor > 0
        && cursor + 1 < bytes.len()
        && bytes[cursor - 1].is_ascii_hexdigit()
        && bytes[cursor + 1].is_ascii_hexdigit()
}

fn strip_string_literals(code: &str) -> String {
    let bytes = code.as_bytes();
    let mut result = String::with_capacity(code.len());
    let mut quote: Option<char> = None;
    let mut escaped = false;

    for (cursor, ch) in code.char_indices() {
        match quote {
            None => {
                if ch == '"' || (ch == '\'' && !is_digit_separator_quote(bytes, cursor)) {
                    quote = Some(ch);
                }
                result.push(ch);
            }
            Some(active_quote) => {
                if escaped {
                    escaped = false;
                    result.push(' ');
                } else if ch == '\\' {
                    escaped = true;
                    result.push(' ');
                } else if ch == active_quote {
                    quote = None;
                    result.push(ch);
                } else {
                    result.push(' ');
                }
            }
        }
    }

    result
}
