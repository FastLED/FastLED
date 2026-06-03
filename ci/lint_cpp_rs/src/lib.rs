use glob::glob;
use rayon::prelude::*;
use regex::Regex;
use serde::{Deserialize, Serialize};
use std::collections::{BTreeSet, HashMap, HashSet};
use std::error::Error;
use std::fs;
use std::path::{Path, PathBuf};
use std::sync::OnceLock;
use walkdir::WalkDir;

type DynError = Box<dyn Error + Send + Sync>;

const CPP_EXTENSIONS: &[&str] = &["cpp", "h", "hpp", "ino", "cpp.hpp"];

const EXCLUDED_FILES: &[&str] = &[
    "stub_main.cpp",
    "doctest_main.cpp",
    "fltest.h",
    "doctest.h",
    "crash_handler_execinfo.h",
    "crash_handler_libunwind.h",
    "crash_handler_noop.h",
    "crash_handler_win.h",
    "run_example.h",
    "run_unit_test.h",
    "platforms/apple/run_example.hpp",
    "platforms/apple/run_unit_test.hpp",
    "platforms/posix/run_example.hpp",
    "platforms/posix/run_unit_test.hpp",
    "platforms/win/run_example.hpp",
    "platforms/win/run_unit_test.hpp",
];

const BARE_ALLOCATION_WHITELISTED_SUFFIXES: &[&str] = &[
    "fl/stl/allocator.h",
    "fl/stl/allocator.cpp.hpp",
    "fl/stl/malloc.h",
    "fl/stl/malloc.cpp.hpp",
    "fl/stl/new.h",
    "fl/stl/unique_ptr.h",
    "fl/stl/shared_ptr.h",
    "fl/stl/shared_ptr.cpp.hpp",
    "fl/stl/scoped_ptr.h",
    "fl/stl/weak_ptr.h",
    "fl/stl/detail/string_holder.cpp.hpp",
    "fl/system/heap.h",
    "fl/system/heap.cpp.hpp",
];

const VALID_INCLUDE_PREFIXES: &[&str] = &["fl/", "platforms/", "fx/", "sensors/", "third_party/"];

const EXTERNAL_SDK_PREFIXES: &[&str] = &[
    "driver/",
    "esp_",
    "esp32/",
    "freertos/",
    "hal/",
    "soc/",
    "rom/",
    "sdkconfig",
    "xtensa/",
    "riscv/",
    "hardware/",
    "pico/",
    "boards/",
    "cmsis/",
    "class/",
    "tusb_",
    "Arduino.h",
    "Wire.h",
    "SPI.h",
    "pins_arduino.h",
    "util/",
    "core_",
    "cmsis_",
    "nrf_",
    "nrf52",
    "nrfx",
    "stm32",
    "core_pins.h",
    "usb_",
    "kinetis.h",
    "imxrt.h",
    "RP2040.h",
    "RP2350.h",
    "bsp_",
    "r_",
    "fsp/",
];

const AMBIGUOUS_INCLUDE_PREFIXES: &[&str] = &["avr/", "arm/"];

const FASTLED_PLATFORM_SUBDIRS: &[&str] = &[
    "adafruit/",
    "apollo3/",
    "arm/",
    "avr/",
    "esp/",
    "shared/",
    "stub/",
    "wasm/",
    "posix/",
];

const BANNED_INTERNAL_SUBPATHS: &[(&str, &str)] = &[("fl/math/lib8tion/", "fl/math/")];

const TYPO_INCLUDE_PREFIXES: &[(&str, &str)] = &[
    ("fL/", "fl/"),
    ("FL/", "fl/"),
    ("Fl/", "fl/"),
    ("platform/", "platforms/"),
    ("Platform/", "platforms/"),
    ("PLATFORMS/", "platforms/"),
];

#[derive(Debug, Clone)]
pub struct FileContent {
    pub path: String,
    pub content: String,
    pub lines: Vec<String>,
}

impl FileContent {
    fn read(path: &Path) -> Result<Self, DynError> {
        let content = fs::read_to_string(path)?;
        let lines = content.lines().map(str::to_string).collect();
        Ok(Self {
            path: normalize_path(&path_to_string(path)),
            content,
            lines,
        })
    }
}

#[derive(Debug, Clone, Eq, PartialEq, Ord, PartialOrd, Serialize, Deserialize)]
pub struct LintViolation {
    pub checker: String,
    pub path: String,
    pub line: usize,
    pub message: String,
}

pub trait FileContentChecker: Sync {
    fn name(&self) -> &'static str;
    fn should_process_file(&self, file_path: &str, project_root: &Path) -> bool;
    fn check_file_content(&self, file_content: &FileContent) -> Vec<(usize, String)>;
}

pub struct MultiCheckerFileProcessor {
    file_cache: HashMap<PathBuf, FileContent>,
}

impl MultiCheckerFileProcessor {
    pub fn new() -> Self {
        Self {
            file_cache: HashMap::new(),
        }
    }

    pub fn process_files_with_checkers(
        &mut self,
        file_paths: &[PathBuf],
        checkers: &[Box<dyn FileContentChecker>],
        project_root: &Path,
    ) -> Result<Vec<LintViolation>, DynError> {
        let mut unique_paths = BTreeSet::new();
        for path in file_paths {
            unique_paths.insert(path.clone());
        }

        let pending_paths: Vec<PathBuf> = unique_paths
            .iter()
            .filter(|path| !self.file_cache.contains_key(*path))
            .cloned()
            .collect();

        let loaded_files: Vec<Result<(PathBuf, FileContent), DynError>> = pending_paths
            .par_iter()
            .map(|path| FileContent::read(path).map(|content| (path.clone(), content)))
            .collect();

        for loaded in loaded_files {
            let (path, content) = loaded?;
            self.file_cache.insert(path, content);
        }

        let files: Vec<FileContent> = unique_paths
            .iter()
            .filter_map(|path| self.file_cache.get(path).cloned())
            .collect();

        let mut violations: Vec<LintViolation> = files
            .par_iter()
            .flat_map_iter(|file_content| {
                let mut file_violations = Vec::new();
                for checker in checkers {
                    if !checker.should_process_file(&file_content.path, project_root) {
                        continue;
                    }
                    for (line, message) in checker.check_file_content(file_content) {
                        file_violations.push(LintViolation {
                            checker: checker.name().to_string(),
                            path: file_content.path.clone(),
                            line,
                            message,
                        });
                    }
                }
                file_violations
            })
            .collect();

        violations.sort();
        Ok(violations)
    }
}

impl Default for MultiCheckerFileProcessor {
    fn default() -> Self {
        Self::new()
    }
}

pub fn supported_checker_names() -> &'static [&'static str] {
    &[
        "asm_js_location",
        "bare_allocation",
        "banned_define",
        "banned_macros",
        "banned_namespace",
        "builtin_memcpy",
        "cpp_include",
        "include_paths",
        "impl_hpp_includes",
        "pragma_once",
        "reinterpret_cast",
        "serial_printf",
        "static_in_headers",
        "using_namespace_fl_in_examples",
        "weak_attribute",
    ]
}

pub fn supported_python_checker_names() -> &'static [&'static str] {
    &[
        "AsmJsLocationChecker",
        "BareAllocationChecker",
        "BannedDefineChecker",
        "BannedMacrosChecker",
        "BannedNamespaceChecker",
        "BuiltinMemcpyChecker",
        "CppIncludeChecker",
        "IncludePathsChecker",
        "ImplHppIncludesChecker",
        "PragmaOnceChecker",
        "ReinterpretCastChecker",
        "SerialPrintfChecker",
        "StaticInHeaderChecker",
        "UsingNamespaceFlInExamplesChecker",
        "WeakAttributeChecker",
    ]
}

pub fn create_checkers(
    selected: Option<&HashSet<String>>,
) -> Result<Vec<Box<dyn FileContentChecker>>, DynError> {
    let mut checkers: Vec<(&'static str, Box<dyn FileContentChecker>)> = vec![
        ("asm_js_location", Box::new(AsmJsLocationChecker)),
        ("bare_allocation", Box::new(BareAllocationChecker)),
        ("banned_define", Box::new(BannedDefineChecker)),
        ("banned_macros", Box::new(BannedMacrosChecker)),
        ("banned_namespace", Box::new(BannedNamespaceChecker)),
        ("builtin_memcpy", Box::new(BuiltinMemcpyChecker)),
        ("cpp_include", Box::new(CppIncludeChecker)),
        ("include_paths", Box::new(IncludePathsChecker)),
        ("impl_hpp_includes", Box::new(ImplHppIncludesChecker)),
        ("pragma_once", Box::new(PragmaOnceChecker)),
        ("reinterpret_cast", Box::new(ReinterpretCastChecker)),
        ("serial_printf", Box::new(SerialPrintfChecker)),
        ("static_in_headers", Box::new(StaticInHeaderChecker)),
        (
            "using_namespace_fl_in_examples",
            Box::new(UsingNamespaceFlInExamplesChecker),
        ),
        ("weak_attribute", Box::new(WeakAttributeChecker)),
    ];

    if let Some(selected) = selected {
        let known: HashSet<&str> = checkers.iter().map(|(name, _)| *name).collect();
        let unknown: Vec<&String> = selected
            .iter()
            .filter(|name| !known.contains(name.as_str()))
            .collect();
        if !unknown.is_empty() {
            return Err(format!("unknown Rust C++ checker(s): {unknown:?}").into());
        }
        checkers.retain(|(name, _)| selected.contains(*name));
    }

    Ok(checkers.into_iter().map(|(_, checker)| checker).collect())
}

pub fn run_cli<I>(args: I) -> Result<u8, DynError>
where
    I: IntoIterator<Item = String>,
{
    let config = CliConfig::parse(args)?;

    if config.list_checkers {
        for checker in supported_checker_names() {
            println!("{checker}");
        }
        return Ok(0);
    }

    let selected = config.selected_checkers.as_ref();
    let checkers = create_checkers(selected)?;
    let files = collect_input_files(&config.project_root, &config.paths)?;
    let mut processor = MultiCheckerFileProcessor::new();
    let violations =
        processor.process_files_with_checkers(&files, &checkers, &config.project_root)?;

    match config.output_format {
        OutputFormat::Json => {
            println!("{}", serde_json::to_string_pretty(&violations)?);
        }
        OutputFormat::Text => print_text_results(&violations, &config.project_root),
    }

    Ok(if violations.is_empty() { 0 } else { 1 })
}

#[derive(Debug)]
struct CliConfig {
    output_format: OutputFormat,
    project_root: PathBuf,
    selected_checkers: Option<HashSet<String>>,
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
                        list_checkers: true,
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

fn print_text_results(violations: &[LintViolation], project_root: &Path) {
    if violations.is_empty() {
        println!("All Rust C++ linting checks passed!");
        return;
    }

    let mut current_checker = "";
    for violation in violations {
        if violation.checker != current_checker {
            current_checker = &violation.checker;
            println!("\n[{current_checker}]");
        }
        let display_path = relative_display_path(&violation.path, project_root);
        println!("  {display_path}:{}: {}", violation.line, violation.message);
    }
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
            if input.contains('*') || input.contains('?') || input.contains('[') {
                for entry in glob(input)? {
                    let path = entry?;
                    collect_path(&path, &mut files, false);
                }
            } else {
                collect_path(&PathBuf::from(input), &mut files, true);
            }
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

fn regex_has_include() -> &'static Regex {
    static VALUE: OnceLock<Regex> = OnceLock::new();
    VALUE.get_or_init(|| Regex::new(r"\b__has_include\s*\(").unwrap())
}

fn regex_static_assert() -> &'static Regex {
    static VALUE: OnceLock<Regex> = OnceLock::new();
    VALUE.get_or_init(|| Regex::new(r"\bstatic_assert\s*\(").unwrap())
}

fn regex_using_namespace_fl() -> &'static Regex {
    static VALUE: OnceLock<Regex> = OnceLock::new();
    VALUE.get_or_init(|| Regex::new(r"\busing\s+namespace\s+fl\s*;").unwrap())
}

fn regex_string_literal() -> &'static Regex {
    static VALUE: OnceLock<Regex> = OnceLock::new();
    VALUE.get_or_init(|| Regex::new(r#""(?:[^"\\]|\\.)*"|'(?:[^'\\]|\\.)*'"#).unwrap())
}

fn regex_new_alloc() -> &'static Regex {
    static VALUE: OnceLock<Regex> = OnceLock::new();
    VALUE.get_or_init(|| Regex::new(r"\bnew\s+[A-Za-z_:]").unwrap())
}

fn regex_delete() -> &'static Regex {
    static VALUE: OnceLock<Regex> = OnceLock::new();
    VALUE.get_or_init(|| Regex::new(r"\bdelete\b").unwrap())
}

fn regex_malloc_family() -> &'static Regex {
    static VALUE: OnceLock<Regex> = OnceLock::new();
    VALUE.get_or_init(|| Regex::new(r"\b(malloc|calloc|realloc)\s*\(").unwrap())
}

fn regex_free() -> &'static Regex {
    static VALUE: OnceLock<Regex> = OnceLock::new();
    VALUE.get_or_init(|| Regex::new(r"\bfree\s*\(").unwrap())
}

fn regex_deleted_func() -> &'static Regex {
    static VALUE: OnceLock<Regex> = OnceLock::new();
    VALUE.get_or_init(|| Regex::new(r"=\s*delete\s*[;{]").unwrap())
}

fn regex_deleted_func_eol() -> &'static Regex {
    static VALUE: OnceLock<Regex> = OnceLock::new();
    VALUE.get_or_init(|| Regex::new(r"=\s*delete\s*$").unwrap())
}

fn regex_inline_func() -> &'static Regex {
    static VALUE: OnceLock<Regex> = OnceLock::new();
    VALUE.get_or_init(|| Regex::new(r"\w+\s*\([^)]*\)\s*\{").unwrap())
}

fn regex_template() -> &'static Regex {
    static VALUE: OnceLock<Regex> = OnceLock::new();
    VALUE.get_or_init(|| Regex::new(r"^\s*template\s*<").unwrap())
}

fn regex_static_var() -> &'static Regex {
    static VALUE: OnceLock<Regex> = OnceLock::new();
    VALUE.get_or_init(|| {
        Regex::new(r"\bstatic\s+(?:const\s+)?[\w:]+(?:<[^>]+>)?\s+\w+\s*[=({]").unwrap()
    })
}

fn regex_static_func() -> &'static Regex {
    static VALUE: OnceLock<Regex> = OnceLock::new();
    VALUE.get_or_init(|| Regex::new(r"static\s+[\w:]+(?:<[^>]+>)?\s+\w+\s*\([^)]*\)\s*\{").unwrap())
}

fn regex_include_path() -> &'static Regex {
    static VALUE: OnceLock<Regex> = OnceLock::new();
    VALUE.get_or_init(|| Regex::new(r#"#include\s+"([^"]+)""#).unwrap())
}

fn regex_builtin_memcpy() -> &'static Regex {
    static VALUE: OnceLock<Regex> = OnceLock::new();
    VALUE.get_or_init(|| Regex::new(r"\b__builtin_memcpy\s*\(").unwrap())
}

fn regex_weak_attribute() -> &'static Regex {
    static VALUE: OnceLock<Regex> = OnceLock::new();
    VALUE.get_or_init(|| Regex::new(r"__attribute__\s*\(\s*\(\s*weak\s*\)\s*\)").unwrap())
}

fn regex_banned_namespace_fl_fl() -> &'static Regex {
    static VALUE: OnceLock<Regex> = OnceLock::new();
    VALUE.get_or_init(|| Regex::new(r"\bfl::fl\b").unwrap())
}

fn regex_cpp_include() -> &'static Regex {
    static VALUE: OnceLock<Regex> = OnceLock::new();
    VALUE.get_or_init(|| Regex::new(r#"#include\s+[<"]([^>"]+\.cpp)[>"]"#).unwrap())
}

fn regex_impl_hpp_include() -> &'static Regex {
    static VALUE: OnceLock<Regex> = OnceLock::new();
    VALUE.get_or_init(|| Regex::new(r#"#\s*include\s+[<"]([^>"]+\.impl\.hpp)[>"]"#).unwrap())
}

fn regex_asm_js_macro() -> &'static Regex {
    static VALUE: OnceLock<Regex> = OnceLock::new();
    VALUE.get_or_init(|| Regex::new(r"\b(?:EM_JS|EM_ASYNC_JS|EM_ASM)\s*\(").unwrap())
}

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
    } else if normalized_path.starts_with("src/") {
        PathBuf::from(".")
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

struct SerialPrintfChecker;

impl FileContentChecker for SerialPrintfChecker {
    fn name(&self) -> &'static str {
        "SerialPrintfChecker"
    }

    fn should_process_file(&self, file_path: &str, _project_root: &Path) -> bool {
        is_under_dir(file_path, "examples")
            && ends_with_any(file_path, &[".cpp", ".h", ".hpp", ".ino"])
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
            if code_part.contains("Serial.printf") {
                violations.push((index + 1, stripped.to_string()));
            }
        }

        violations
    }
}

struct UsingNamespaceFlInExamplesChecker;

impl FileContentChecker for UsingNamespaceFlInExamplesChecker {
    fn name(&self) -> &'static str {
        "UsingNamespaceFlInExamplesChecker"
    }

    fn should_process_file(&self, file_path: &str, _project_root: &Path) -> bool {
        is_under_dir(file_path, "examples")
            && ends_with_any(
                file_path,
                &[".cpp", ".h", ".hpp", ".ino", ".c", ".cc", ".cxx"],
            )
    }

    fn check_file_content(&self, file_content: &FileContent) -> Vec<(usize, String)> {
        file_content
            .lines
            .iter()
            .enumerate()
            .filter_map(|(index, line)| {
                if regex_using_namespace_fl().is_match(line) {
                    Some((index + 1, line.trim_end().to_string()))
                } else {
                    None
                }
            })
            .collect()
    }
}

struct BannedMacrosChecker;

impl FileContentChecker for BannedMacrosChecker {
    fn name(&self) -> &'static str {
        "BannedMacrosChecker"
    }

    fn should_process_file(&self, file_path: &str, _project_root: &Path) -> bool {
        if !ends_with_any(file_path, &[".cpp", ".h", ".hpp", ".ino", ".cpp.hpp"]) {
            return false;
        }
        if file_path.ends_with("has_include.h")
            || file_path.ends_with("compiler_control.h")
            || file_path.ends_with("cpp_compat.h")
            || file_path.ends_with("static_assert.h")
        {
            return false;
        }
        if is_under_dir(file_path, "third_party") {
            return false;
        }
        if ends_with_any(file_path, &["doctest.h", "catch.hpp", "gtest.h"]) {
            return false;
        }
        if ends_with_any(file_path, &[".md", ".txt"]) {
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

            let code_part = split_line_comment(line);
            if code_part.contains("#define") && code_part.contains("FL_HAS_INCLUDE") {
                continue;
            }

            let code_no_strings = strip_string_literals(code_part);
            if !code_no_strings.contains("__has_include")
                && !code_no_strings.contains("static_assert")
            {
                continue;
            }

            if regex_has_include().is_match(&code_no_strings) {
                if code_no_strings.contains("#ifndef") || code_no_strings.contains("#ifdef") {
                    continue;
                }
                violations.push((
                    index + 1,
                    format!(
                        "Use FL_HAS_INCLUDE instead of __has_include: {stripped}\n      \
Rationale: __has_include is not universally supported. FL_HAS_INCLUDE provides a portable wrapper with fallback for older compilers.\n      \
Include 'fl/stl/has_include.h' and replace __has_include(...) with FL_HAS_INCLUDE(...)"
                    ),
                ));
            }

            if regex_static_assert().is_match(&code_no_strings) {
                violations.push((
                    index + 1,
                    format!(
                        "Use FL_STATIC_ASSERT instead of raw static_assert: {stripped}\n      \
Rationale: FL_STATIC_ASSERT keeps compile-time assertions portable on old embedded toolchains.\n      \
Include 'fl/stl/static_assert.h' when needed and replace static_assert(...) with FL_STATIC_ASSERT(...)"
                    ),
                ));
            }
        }

        violations
    }
}

struct BareAllocationChecker;

impl FileContentChecker for BareAllocationChecker {
    fn name(&self) -> &'static str {
        "BareAllocationChecker"
    }

    fn should_process_file(&self, file_path: &str, _project_root: &Path) -> bool {
        if !ends_with_any(file_path, &[".cpp", ".h", ".hpp", ".ino", ".cpp.hpp"]) {
            return false;
        }
        if is_excluded_file(file_path) {
            return false;
        }
        let lower = file_path.to_ascii_lowercase();
        if lower.contains("third_party") || lower.contains("thirdparty") {
            return false;
        }
        !BARE_ALLOCATION_WHITELISTED_SUFFIXES
            .iter()
            .any(|suffix| file_path.ends_with(suffix))
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
            if line.contains("// ok bare allocation") || line.contains("// okay bare allocation") {
                continue;
            }
            if !line.contains("new")
                && !line.contains("delete")
                && !line.contains("malloc")
                && !line.contains("calloc")
                && !line.contains("realloc")
                && !line.contains("free")
            {
                continue;
            }

            let code_part = split_line_comment(line);
            let code_part = regex_string_literal().replace_all(code_part, "\"\"");

            if code_part.contains("operator new") || code_part.contains("operator delete") {
                continue;
            }
            if regex_deleted_func().is_match(&code_part)
                || regex_deleted_func_eol().is_match(code_part.trim_end())
            {
                continue;
            }

            if regex_new_alloc().is_match(&code_part) {
                violations.push((
                    index + 1,
                    format!("bare 'new': {stripped} — {}", suggestion_new()),
                ));
                continue;
            }

            if regex_delete().is_match(&code_part) {
                violations.push((
                    index + 1,
                    format!("bare 'delete': {stripped} — {}", suggestion_delete()),
                ));
                continue;
            }

            if let Some(mat) = regex_malloc_family()
                .find_iter(&code_part)
                .find(|mat| !has_forbidden_prefix(&code_part, mat.start()))
            {
                let func = regex_malloc_family()
                    .captures(mat.as_str())
                    .and_then(|captures| captures.get(1))
                    .map(|capture| capture.as_str())
                    .unwrap_or("malloc");
                violations.push((
                    index + 1,
                    format!("bare '{func}': {stripped} — {}", suggestion_malloc()),
                ));
                continue;
            }

            if regex_free()
                .find_iter(&code_part)
                .any(|mat| !has_forbidden_prefix(&code_part, mat.start()))
            {
                violations.push((
                    index + 1,
                    format!("bare 'free': {stripped} — {}", suggestion_free()),
                ));
            }
        }

        violations
    }
}

fn has_forbidden_prefix(code: &str, start: usize) -> bool {
    if start == 0 {
        return false;
    }
    let prefix = &code[..start];
    prefix.ends_with("::") || prefix.ends_with('.')
}

fn suggestion_new() -> &'static str {
    "Use fl::make_unique<T>() or fl::make_shared<T>() instead of bare 'new', or add '// ok bare allocation' to suppress"
}

fn suggestion_delete() -> &'static str {
    "Use fl::unique_ptr or fl::shared_ptr (automatic cleanup) instead of bare 'delete', or add '// ok bare allocation' to suppress"
}

fn suggestion_malloc() -> &'static str {
    "Use fl::make_unique<T>() or fl::make_shared<T>() instead, or fl::malloc() if raw memory is required, or add '// ok bare allocation' to suppress"
}

fn suggestion_free() -> &'static str {
    "Use fl::unique_ptr or fl::shared_ptr (automatic cleanup) instead, or fl::free() if raw memory is required, or add '// ok bare allocation' to suppress"
}

struct StaticInHeaderChecker;

impl FileContentChecker for StaticInHeaderChecker {
    fn name(&self) -> &'static str {
        "StaticInHeaderChecker"
    }

    fn should_process_file(&self, file_path: &str, _project_root: &Path) -> bool {
        if !ends_with_any(file_path, &[".h", ".hpp"]) {
            return false;
        }
        if file_path.ends_with(".cpp.hpp") {
            return false;
        }
        !is_excluded_file(file_path)
    }

    fn check_file_content(&self, file_content: &FileContent) -> Vec<(usize, String)> {
        if !file_content.content.contains("static") {
            return Vec::new();
        }

        let mut violations = Vec::new();
        let mut in_multiline_comment = false;
        let mut brace_depth: isize = 0;
        let mut in_function = false;
        let mut in_template_function = false;

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
            let open_braces = code_part.matches('{').count() as isize;
            let close_braces = code_part.matches('}').count() as isize;

            if code_part.contains("template") && regex_template().is_match(code_part) {
                in_template_function = true;
            }

            if code_part.contains('(')
                && code_part.contains('{')
                && regex_inline_func().is_match(code_part)
            {
                in_function = true;
                brace_depth = open_braces - close_braces;
            } else if in_function {
                brace_depth += open_braces - close_braces;
            }

            if in_function && brace_depth <= 0 {
                in_function = false;
                in_template_function = false;
                brace_depth = 0;
            }

            if in_function
                && brace_depth > 0
                && !in_template_function
                && code_part.contains("static")
                && regex_static_var().is_match(code_part)
                && !regex_static_func().is_match(code_part)
                && !line.contains("// okay static in header")
            {
                violations.push((index + 1, stripped.to_string()));
            }
        }

        violations
    }
}

struct IncludePathsChecker;

impl FileContentChecker for IncludePathsChecker {
    fn name(&self) -> &'static str {
        "IncludePathsChecker"
    }

    fn should_process_file(&self, file_path: &str, project_root: &Path) -> bool {
        if !ends_with_any(file_path, &[".cpp", ".h", ".hpp", ".c"]) {
            return false;
        }
        if is_excluded_file(file_path) {
            return false;
        }

        is_under_project_subpath(file_path, project_root, "src/fl")
            || is_under_project_subpath(file_path, project_root, "src/platforms")
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
            if comment_part
                .to_ascii_lowercase()
                .contains("ok include path")
            {
                continue;
            }

            let Some(captures) = regex_include_path().captures(code_part) else {
                continue;
            };
            let include_path = captures.get(1).map(|match_| match_.as_str()).unwrap_or("");

            if include_path.starts_with('<') {
                continue;
            }

            if is_valid_include_path(include_path)
                && !header_exists_for_file(&file_content.path, include_path)
            {
                violations.push((
                    index + 1,
                    format!(
                        "Header not found: #include \"{include_path}\"\n  Expected: src/{include_path}\n"
                    ),
                ));
                continue;
            }

            if is_valid_include_path(include_path) {
                continue;
            }

            if let Some(banned_replacement) = banned_subpath_replacement(include_path) {
                violations.push((
                    index + 1,
                    format!(
                        "Invalid include path: #include \"{include_path}\" - Use #include \"{banned_replacement}\" instead. Add '// ok include path' comment to suppress."
                    ),
                ));
            } else if is_relative_include_path(include_path) {
                violations.push((
                    index + 1,
                    format!(
                        "Invalid include path: #include \"{include_path}\" - Relative paths (../ or ./) are not allowed. Use paths relative to src/ instead (e.g., \"fl/foo.h\" or \"platforms/bar.h\"). Add '// ok include path' comment to suppress."
                    ),
                ));
            } else if is_fastled_platform_relative(include_path) {
                violations.push((
                    index + 1,
                    format!(
                        "Invalid include path: #include \"{include_path}\" - FastLED platform includes must use \"platforms/\" prefix. Use #include \"platforms/{include_path}\" instead. Add '// ok include path' comment to suppress."
                    ),
                ));
            } else if let Some(typo_correction) = typo_include_suggestion(include_path) {
                let typo_prefix = include_path
                    .split_once('/')
                    .map_or(include_path, |(prefix, _)| prefix);
                let typo_prefix = format!("{typo_prefix}/");
                let rest_of_path = include_path.strip_prefix(&typo_prefix).unwrap_or("");
                let corrected_path = format!("{typo_correction}{rest_of_path}");
                violations.push((
                    index + 1,
                    format!(
                        "Invalid include path: #include \"{include_path}\" - \"{typo_prefix}\" looks like a typo. Use #include \"{corrected_path}\" instead. Add '// ok include path' comment to suppress."
                    ),
                ));
            }
        }

        violations
    }
}

struct BuiltinMemcpyChecker;

impl FileContentChecker for BuiltinMemcpyChecker {
    fn name(&self) -> &'static str {
        "BuiltinMemcpyChecker"
    }

    fn should_process_file(&self, file_path: &str, _project_root: &Path) -> bool {
        if !ends_with_any(file_path, &[".cpp", ".h", ".hpp", ".ino", ".cpp.hpp"]) {
            return false;
        }
        if file_path.ends_with("compiler_control.h") {
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

            let code_part = split_line_comment(line);
            if code_part.contains("#define") && code_part.contains("FL_BUILTIN_MEMCPY") {
                continue;
            }
            if !code_part.contains("__builtin_memcpy") {
                continue;
            }
            if regex_builtin_memcpy().is_match(code_part) {
                violations.push((
                    index + 1,
                    format!(
                        "Use FL_BUILTIN_MEMCPY instead of __builtin_memcpy: {stripped}\n      \
Rationale: FL_BUILTIN_MEMCPY wraps __builtin_memcpy for portability.\n      \
Include 'fl/stl/compiler_control.h' and replace __builtin_memcpy(...) with FL_BUILTIN_MEMCPY(...)"
                    ),
                ));
            }
        }

        violations
    }
}

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
        if !is_under_project_subpath(&normalized, project_root, "src") {
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
        if !is_under_project_subpath(&normalized, project_root, "src") {
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

#[cfg(test)]
mod tests {
    use super::*;

    fn file(path: &str, content: &str) -> FileContent {
        FileContent {
            path: normalize_path(path),
            content: content.to_string(),
            lines: content.lines().map(str::to_string).collect(),
        }
    }

    #[test]
    fn banned_macros_ignores_string_literals() {
        let checker = BannedMacrosChecker;
        let result = checker.check_file_content(&file(
            "src/fl/example.h",
            "FL_WARN(\"use static_assert elsewhere\");",
        ));
        assert!(result.is_empty());
    }

    #[test]
    fn bare_allocation_rejects_malloc_but_not_fl_malloc() {
        let checker = BareAllocationChecker;
        assert_eq!(
            checker
                .check_file_content(&file("src/fl/example.h", "void* p = malloc(4);"))
                .len(),
            1
        );
        assert!(checker
            .check_file_content(&file("src/fl/example.h", "void* p = fl::malloc(4);"))
            .is_empty());
    }

    #[test]
    fn static_in_header_allows_template_static() {
        let checker = StaticInHeaderChecker;
        let content =
            "template<typename T>\nT& get() {\n    static T instance;\n    return instance;\n}";
        assert!(checker
            .check_file_content(&file("src/fl/example.h", content))
            .is_empty());
    }
}
