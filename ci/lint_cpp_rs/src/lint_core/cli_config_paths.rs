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

fn split_line_comment(line: &str) -> &str {
    line.split("//").next().unwrap_or(line)
}

fn strip_block_comments_from_line(line: &str, in_block_comment: &mut bool) -> String {
    let bytes = line.as_bytes();
    let mut visible = String::with_capacity(line.len());
    let mut cursor = 0;
    let mut visible_start = 0;

    while cursor < bytes.len() {
        if *in_block_comment {
            if bytes[cursor..].starts_with(b"*/") {
                cursor += 2;
                visible_start = cursor;
                *in_block_comment = false;
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
            *in_block_comment = true;
            continue;
        }
        if bytes[cursor] == b'"'
            || (bytes[cursor] == b'\'' && !is_digit_separator_quote(bytes, cursor))
        {
            cursor = quoted_literal_end(bytes, cursor, bytes[cursor]);
            continue;
        }
        cursor += 1;
    }

    if !*in_block_comment {
        visible.push_str(&line[visible_start..]);
    }
    visible
}

fn quoted_literal_end(bytes: &[u8], start: usize, quote: u8) -> usize {
    let mut cursor = start + 1;
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

fn is_digit_separator_quote(bytes: &[u8], cursor: usize) -> bool {
    cursor > 0
        && cursor + 1 < bytes.len()
        && bytes[cursor - 1].is_ascii_hexdigit()
        && bytes[cursor + 1].is_ascii_hexdigit()
}

fn strip_string_literals(code: &str) -> String {
    let mut result = String::with_capacity(code.len());
    let mut quote: Option<char> = None;
    let mut escaped = false;

    for ch in code.chars() {
        match quote {
            None => {
                if ch == '"' || ch == '\'' {
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
