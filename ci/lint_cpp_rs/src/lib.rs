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

const SLEEP_FOR_WHITELISTED_SUFFIXES: &[&str] = &[
    "fl/stl/thread.h",
    "platforms/stub/thread_stub_stl.h",
    "platforms/stub/thread_stub_noop.h",
    "platforms/stub/platform_time.cpp.hpp",
    "fl/async.cpp.hpp",
    "platforms/shared/spi_bitbang/host_timer.cpp.hpp",
    "platforms/stub/coroutine_stub.impl.hpp",
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

const ARDUINO_BANNED_MACROS: &[&str] = &["INPUT", "OUTPUT", "DEFAULT"];

const ATTRIBUTE_MAPPINGS: &[(&str, &str)] = &[
    ("maybe_unused", "FL_MAYBE_UNUSED"),
    ("nodiscard", "FL_NODISCARD"),
    ("fallthrough", "FL_FALLTHROUGH"),
    ("deprecated", "FL_DEPRECATED"),
    ("noreturn", "FL_NORETURN"),
    ("likely", "FL_LIKELY"),
    ("unlikely", "FL_UNLIKELY"),
    ("no_unique_address", "FL_NO_UNIQUE_ADDRESS"),
];

const NUMERIC_LIMIT_MACROS: &[&str] = &[
    "UINT8_MAX",
    "UINT16_MAX",
    "UINT32_MAX",
    "UINT64_MAX",
    "UINTMAX_MAX",
    "UINTPTR_MAX",
    "SIZE_MAX",
    "INT8_MAX",
    "INT16_MAX",
    "INT32_MAX",
    "INT64_MAX",
    "INTMAX_MAX",
    "INTPTR_MAX",
    "INT8_MIN",
    "INT16_MIN",
    "INT32_MIN",
    "INT64_MIN",
    "INTMAX_MIN",
    "INTPTR_MIN",
    "UCHAR_MAX",
    "USHRT_MAX",
    "UINT_MAX",
    "ULONG_MAX",
    "ULLONG_MAX",
    "CHAR_MAX",
    "SHRT_MAX",
    "INT_MAX",
    "LONG_MAX",
    "LLONG_MAX",
    "CHAR_MIN",
    "SHRT_MIN",
    "INT_MIN",
    "LONG_MIN",
    "LLONG_MIN",
];

const NUMERIC_LIMIT_EXCLUDED_SUFFIXES: &[&str] =
    &["tests/fl/stl/cstdint.cpp", "tests/fl/stl/stdint.cpp"];

const STD_BRIDGE_FILE_WHITELIST: &[&str] = &[
    "platforms/stub/mutex_stub_stl.h",
    "platforms/stub/mutex_stub_noop.h",
    "platforms/esp/32/mutex_esp32.h",
    "platforms/arm/rp/mutex_rp.h",
    "platforms/arm/stm32/mutex_stm32.h",
    "platforms/arm/stm32/mutex_stm32_rtos.h",
    "platforms/arm/d21/mutex_samd.h",
    "platforms/arm/nrf52/mutex_nrf52.h",
    "platforms/stub/condition_variable_stub.h",
    "platforms/esp/32/condition_variable_esp32.h",
    "platforms/esp/32/condition_variable_esp32.cpp.hpp",
    "platforms/stub/thread_stub_stl.h",
    "platforms/stub/thread_stub_noop.h",
    "platforms/stub/semaphore_stub_stl.h",
    "platforms/stub/semaphore_stub_noop.h",
    "platforms/esp/32/semaphore_esp32.h",
    "platforms/arm/d21/semaphore_samd.h",
    "platforms/arm/d21/semaphore_samd.cpp.hpp",
    "platforms/arm/rp/semaphore_rp.h",
    "platforms/arm/rp/semaphore_rp.cpp.hpp",
    "platforms/arm/stm32/semaphore_stm32.h",
    "platforms/arm/stm32/semaphore_stm32.cpp.hpp",
    "platforms/stub/platform_time.cpp.hpp",
    "platforms/apple/run_example.hpp",
    "platforms/apple/run_unit_test.hpp",
    "platforms/posix/run_example.hpp",
    "platforms/posix/run_unit_test.hpp",
    "platforms/win/run_example.hpp",
    "platforms/win/run_unit_test.hpp",
];

const ALLOWED_STD_SYMBOLS: &[&str] = &[
    "std::atomic_thread_fence",
    "std::memory_order_acquire",
    "std::memory_order_release",
    "std::memory_order_seq_cst",
    "std::memory_order_relaxed",
    "std::memory_order_acq_rel",
    "std::memory_order_consume",
];

const FORBIDDEN_SERIAL_METHODS: &[&str] = &[
    "print",
    "println",
    "printf",
    "write",
    "read",
    "available",
    "peek",
    "readStringUntil",
    "flush",
    "begin",
];

const ALLOWED_SERIAL_METHODS: &[&str] = &["setTxBufferSize", "setTxTimeoutMs"];

const INCLUDE_AFTER_NAMESPACE_SKIP_PATTERNS: &[&str] = &[
    ".venv",
    "node_modules",
    "build",
    ".build",
    "third_party",
    "ziglang",
    "greenlet",
    ".git",
];

const PLATFORM_INCLUDE_LOCATIONS: &[&str] = &[
    "/src/fl/",
    "/src/fx/",
    "/src/lib8tion/",
    "/src/sensors/",
    "/tests/fl/",
    "/tests/fx/",
    "/examples/",
];

const DEEP_PLATFORM_REPLACEMENTS: &[(&str, &str)] = &[
    ("platforms/shared/int_windows.h", "platforms/int.h"),
    ("platforms/shared/int.h", "platforms/int.h"),
    ("platforms/esp/", "platforms/init.h"),
    ("platforms/arm/", "platforms/init.h"),
    ("platforms/stub/", "platforms/init.h"),
    ("platforms/avr/", "platforms/init.h"),
    ("platforms/wasm/", "platforms/init.h"),
];

const SIMD_PATTERNS: &[(&str, &str)] = &[
    (
        "__m128i",
        "x86 SSE type — use fl::simd::simd_u8x16 / simd_u16x8 / simd_u32x4",
    ),
    ("__m128", "x86 SSE type — use fl::simd types"),
    ("__m256i", "x86 AVX type — use fl::simd types"),
    ("__m256", "x86 AVX type — use fl::simd types"),
    ("_mm_", "x86 SSE intrinsic — use fl::simd operations"),
    ("_mm256_", "x86 AVX2 intrinsic — use fl::simd operations"),
    ("_mm512_", "x86 AVX-512 intrinsic — use fl::simd operations"),
    ("_MM_HINT", "x86 prefetch hint — use __builtin_prefetch"),
    ("<emmintrin.h>", "SSE2 header — use fl/math/simd.h"),
    ("<immintrin.h>", "AVX header — use fl/math/simd.h"),
    ("<smmintrin.h>", "SSE4.1 header — use fl/math/simd.h"),
    ("<xmmintrin.h>", "SSE header — use fl/math/simd.h"),
    ("<tmmintrin.h>", "SSSE3 header — use fl/math/simd.h"),
    ("<arm_neon.h>", "ARM NEON header — use fl/math/simd.h"),
    ("vld1q_", "ARM NEON intrinsic — use fl::simd operations"),
    ("vst1q_", "ARM NEON intrinsic — use fl::simd operations"),
    ("vaddq_", "ARM NEON intrinsic — use fl::simd operations"),
    ("vmulq_", "ARM NEON intrinsic — use fl::simd operations"),
    ("vmovl_", "ARM NEON intrinsic — use fl::simd operations"),
    ("vqmovn_", "ARM NEON intrinsic — use fl::simd operations"),
    ("vcombine_", "ARM NEON intrinsic — use fl::simd operations"),
    ("vdupq_", "ARM NEON intrinsic — use fl::simd operations"),
    ("ee.vadds", "Xtensa PIE intrinsic — use fl::simd operations"),
    ("ee.vld", "Xtensa PIE intrinsic — use fl::simd operations"),
    ("ee.vst", "Xtensa PIE intrinsic — use fl::simd operations"),
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

        let loaded_files: Vec<(PathBuf, Result<FileContent, DynError>)> = pending_paths
            .par_iter()
            .map(|path| (path.clone(), FileContent::read(path)))
            .collect();

        for (path, loaded) in loaded_files {
            match loaded {
                Ok(content) => {
                    self.file_cache.insert(path, content);
                }
                Err(error) => {
                    eprintln!(
                        "warning: skipped unreadable file {}: {error}",
                        path.display()
                    );
                }
            }
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
        "arduino_macro_usage",
        "asm_js_location",
        "attribute",
        "bare_allocation",
        "banned_define",
        "banned_macros",
        "banned_namespace",
        "builtin_memcpy",
        "cpp_hpp_includes",
        "cpp_include",
        "esp_rom_printf",
        "fastled_header_usage",
        "fl_is_defined",
        "include_after_namespace",
        "include_paths",
        "impl_hpp_includes",
        "namespace_fl_declaration",
        "numeric_limit_macros",
        "platform_includes",
        "pragma_once",
        "platform_pragma",
        "platform_trampoline",
        "raw_noexcept",
        "raw_pragma",
        "reinterpret_cast",
        "relative_include",
        "serial_printf",
        "simd_intrinsics",
        "sleep_for",
        "span_from_pointer",
        "static_in_headers",
        "std_namespace",
        "singleton_in_headers",
        "thread_local_keyword",
        "example_serial",
        "using_namespace",
        "using_namespace_fl",
        "using_namespace_fl_in_examples",
        "weak_attribute",
    ]
}

pub fn supported_python_checker_names() -> &'static [&'static str] {
    &[
        "ArduinoMacroUsageChecker",
        "AsmJsLocationChecker",
        "AttributeChecker",
        "BareAllocationChecker",
        "BannedDefineChecker",
        "BannedMacrosChecker",
        "BannedNamespaceChecker",
        "BuiltinMemcpyChecker",
        "CppHppIncludesChecker",
        "CppIncludeChecker",
        "EspRomPrintfChecker",
        "ExampleSerialChecker",
        "FastLEDHeaderUsageChecker",
        "FlIsDefinedChecker",
        "IncludeAfterNamespaceChecker",
        "IncludePathsChecker",
        "ImplHppIncludesChecker",
        "NamespaceFlDeclarationChecker",
        "NumericLimitMacroChecker",
        "PlatformIncludesChecker",
        "PragmaOnceChecker",
        "PlatformPragmaChecker",
        "PlatformTrampolineChecker",
        "RawNoexceptChecker",
        "RawPragmaChecker",
        "ReinterpretCastChecker",
        "RelativeIncludeChecker",
        "SerialPrintfChecker",
        "SimdIntrinsicsChecker",
        "SleepForChecker",
        "SingletonInHeadersChecker",
        "SpanFromPointerChecker",
        "StaticInHeaderChecker",
        "StdNamespaceChecker",
        "ThreadLocalKeywordChecker",
        "UsingNamespaceChecker",
        "UsingNamespaceFlChecker",
        "UsingNamespaceFlInExamplesChecker",
        "WeakAttributeChecker",
    ]
}

pub fn create_checkers(
    selected: Option<&HashSet<String>>,
) -> Result<Vec<Box<dyn FileContentChecker>>, DynError> {
    let mut checkers: Vec<(&'static str, Box<dyn FileContentChecker>)> = vec![
        ("arduino_macro_usage", Box::new(ArduinoMacroUsageChecker)),
        ("asm_js_location", Box::new(AsmJsLocationChecker)),
        ("attribute", Box::new(AttributeChecker)),
        ("bare_allocation", Box::new(BareAllocationChecker)),
        ("banned_define", Box::new(BannedDefineChecker)),
        ("banned_macros", Box::new(BannedMacrosChecker)),
        ("banned_namespace", Box::new(BannedNamespaceChecker)),
        ("builtin_memcpy", Box::new(BuiltinMemcpyChecker)),
        ("cpp_hpp_includes", Box::new(CppHppIncludesChecker)),
        ("cpp_include", Box::new(CppIncludeChecker)),
        ("esp_rom_printf", Box::new(EspRomPrintfChecker)),
        ("example_serial", Box::new(ExampleSerialChecker)),
        ("fastled_header_usage", Box::new(FastLEDHeaderUsageChecker)),
        ("fl_is_defined", Box::new(FlIsDefinedChecker)),
        (
            "include_after_namespace",
            Box::new(IncludeAfterNamespaceChecker),
        ),
        ("include_paths", Box::new(IncludePathsChecker)),
        ("impl_hpp_includes", Box::new(ImplHppIncludesChecker)),
        (
            "namespace_fl_declaration",
            Box::new(NamespaceFlDeclarationChecker),
        ),
        ("numeric_limit_macros", Box::new(NumericLimitMacroChecker)),
        ("platform_includes", Box::new(PlatformIncludesChecker)),
        ("pragma_once", Box::new(PragmaOnceChecker)),
        ("platform_pragma", Box::new(PlatformPragmaChecker)),
        ("platform_trampoline", Box::new(PlatformTrampolineChecker)),
        ("raw_noexcept", Box::new(RawNoexceptChecker)),
        ("raw_pragma", Box::new(RawPragmaChecker)),
        ("reinterpret_cast", Box::new(ReinterpretCastChecker)),
        ("relative_include", Box::new(RelativeIncludeChecker)),
        ("serial_printf", Box::new(SerialPrintfChecker)),
        ("simd_intrinsics", Box::new(SimdIntrinsicsChecker)),
        ("sleep_for", Box::new(SleepForChecker)),
        ("singleton_in_headers", Box::new(SingletonInHeadersChecker)),
        ("span_from_pointer", Box::new(SpanFromPointerChecker)),
        ("static_in_headers", Box::new(StaticInHeaderChecker)),
        ("std_namespace", Box::new(StdNamespaceChecker)),
        ("thread_local_keyword", Box::new(ThreadLocalKeywordChecker)),
        ("using_namespace", Box::new(UsingNamespaceChecker)),
        ("using_namespace_fl", Box::new(UsingNamespaceFlChecker)),
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

    if config.show_help {
        return Ok(0);
    }

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
    let mut visible = String::new();
    let mut rest = line;

    loop {
        if *in_block_comment {
            let Some(end) = rest.find("*/") else {
                return visible;
            };
            rest = &rest[end + 2..];
            *in_block_comment = false;
            continue;
        }

        let Some(start) = rest.find("/*") else {
            visible.push_str(rest);
            return visible;
        };
        visible.push_str(&rest[..start]);
        rest = &rest[start + 2..];
        *in_block_comment = true;
    }
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

fn regex_cpp_hpp_include() -> &'static Regex {
    static VALUE: OnceLock<Regex> = OnceLock::new();
    VALUE.get_or_init(|| Regex::new(r#"#\s*include\s+[<"]([^>"]+\.cpp\.hpp)[>"]"#).unwrap())
}

fn regex_impl_hpp_include() -> &'static Regex {
    static VALUE: OnceLock<Regex> = OnceLock::new();
    VALUE.get_or_init(|| Regex::new(r#"#\s*include\s+[<"]([^>"]+\.impl\.hpp)[>"]"#).unwrap())
}

fn regex_asm_js_macro() -> &'static Regex {
    static VALUE: OnceLock<Regex> = OnceLock::new();
    VALUE.get_or_init(|| Regex::new(r"\b(?:EM_JS|EM_ASYNC_JS|EM_ASM)\s*\(").unwrap())
}

fn regex_esp_rom_printf() -> &'static Regex {
    static VALUE: OnceLock<Regex> = OnceLock::new();
    VALUE.get_or_init(|| Regex::new(r"\besp_rom_printf\s*\(").unwrap())
}

fn regex_sleep_for() -> &'static Regex {
    static VALUE: OnceLock<Regex> = OnceLock::new();
    VALUE.get_or_init(|| Regex::new(r"\bsleep_for\s*\(").unwrap())
}

fn regex_thread_local() -> &'static Regex {
    static VALUE: OnceLock<Regex> = OnceLock::new();
    VALUE.get_or_init(|| Regex::new(r"\bthread_local\b").unwrap())
}

fn regex_span_data_size() -> &'static Regex {
    static VALUE: OnceLock<Regex> = OnceLock::new();
    VALUE.get_or_init(|| {
        Regex::new(r"span<[^>]+>\s*\([^)]*\.data\(\)[^)]*\.size\(\)[^)]*\)").unwrap()
    })
}

fn regex_relative_include() -> &'static Regex {
    static VALUE: OnceLock<Regex> = OnceLock::new();
    VALUE.get_or_init(|| Regex::new(r#"^\s*#\s*include\s+[<"]([^>"]*\.\..*)[>"]"#).unwrap())
}

fn regex_fastled_h_include() -> &'static Regex {
    static VALUE: OnceLock<Regex> = OnceLock::new();
    VALUE.get_or_init(|| Regex::new(r#"^\s*#\s*include\s+[<"]FastLED\.h[>"]"#).unwrap())
}

fn regex_fastled_internal_define() -> &'static Regex {
    static VALUE: OnceLock<Regex> = OnceLock::new();
    VALUE.get_or_init(|| Regex::new(r"^\s*#\s*define\s+FASTLED_INTERNAL").unwrap())
}

fn regex_arduino_macro() -> &'static Regex {
    static VALUE: OnceLock<Regex> = OnceLock::new();
    VALUE.get_or_init(|| Regex::new(r"\b(INPUT|OUTPUT|DEFAULT)\b").unwrap())
}

fn regex_arduino_scoped_enum() -> &'static Regex {
    static VALUE: OnceLock<Regex> = OnceLock::new();
    VALUE.get_or_init(|| Regex::new(r"::\s*(?:INPUT|OUTPUT|DEFAULT)\b").unwrap())
}

fn regex_arduino_enum_member() -> &'static Regex {
    static VALUE: OnceLock<Regex> = OnceLock::new();
    VALUE.get_or_init(|| Regex::new(r"^\s*(?:INPUT|OUTPUT|DEFAULT)\s*(?:=\s*\w+)?\s*[,}]").unwrap())
}

fn regex_arduino_preprocessor() -> &'static Regex {
    static VALUE: OnceLock<Regex> = OnceLock::new();
    VALUE.get_or_init(|| {
        Regex::new(r"^\s*#\s*(?:define|undef|ifdef|ifndef|if|elif|pragma|error|include)\b").unwrap()
    })
}

fn regex_standard_attribute() -> &'static Regex {
    static VALUE: OnceLock<Regex> = OnceLock::new();
    VALUE.get_or_init(|| Regex::new(r"\[\[\s*([a-z_]+)\s*\]\]").unwrap())
}

fn regex_alignas() -> &'static Regex {
    static VALUE: OnceLock<Regex> = OnceLock::new();
    VALUE.get_or_init(|| Regex::new(r"\balignas\s*\(").unwrap())
}

fn regex_preprocessor_if_elif() -> &'static Regex {
    static VALUE: OnceLock<Regex> = OnceLock::new();
    VALUE.get_or_init(|| Regex::new(r"^\s*#\s*(?:if|elif)\b").unwrap())
}

fn regex_preprocessor_include() -> &'static Regex {
    static VALUE: OnceLock<Regex> = OnceLock::new();
    VALUE.get_or_init(|| Regex::new(r"^#\s*include\b").unwrap())
}

fn regex_namespace_declaration() -> &'static Regex {
    static VALUE: OnceLock<Regex> = OnceLock::new();
    VALUE.get_or_init(|| Regex::new(r"^\s*(?:namespace\s+\w+|namespace\s*\{)").unwrap())
}

fn regex_any_include() -> &'static Regex {
    static VALUE: OnceLock<Regex> = OnceLock::new();
    VALUE.get_or_init(|| Regex::new(r#"^\s*#\s*include\s*[<"].*[>"]"#).unwrap())
}

fn regex_allow_include_after_namespace() -> &'static Regex {
    static VALUE: OnceLock<Regex> = OnceLock::new();
    VALUE.get_or_init(|| Regex::new(r"//\s*allow-include-after-namespace").unwrap())
}

fn regex_nolint() -> &'static Regex {
    static VALUE: OnceLock<Regex> = OnceLock::new();
    VALUE.get_or_init(|| Regex::new(r"//\s*nolint").unwrap())
}

fn regex_namespace_fl_declaration() -> &'static Regex {
    static VALUE: OnceLock<Regex> = OnceLock::new();
    VALUE.get_or_init(|| Regex::new(r"\bnamespace\s+fl\s*\{").unwrap())
}

fn regex_using_namespace() -> &'static Regex {
    static VALUE: OnceLock<Regex> = OnceLock::new();
    VALUE.get_or_init(|| Regex::new(r"\busing\s+namespace\s+\w+(?:::\w+)*\s*;").unwrap())
}

fn regex_define_using_namespace() -> &'static Regex {
    static VALUE: OnceLock<Regex> = OnceLock::new();
    VALUE.get_or_init(|| Regex::new(r"#define\s+\w+.*using\s+namespace").unwrap())
}

fn regex_force_use_namespace() -> &'static Regex {
    static VALUE: OnceLock<Regex> = OnceLock::new();
    VALUE.get_or_init(|| Regex::new(r"#if\s+.*FORCE_USE_NAMESPACE").unwrap())
}

fn regex_fl_is_token() -> &'static Regex {
    static VALUE: OnceLock<Regex> = OnceLock::new();
    VALUE.get_or_init(|| Regex::new(r"\bFL_IS_\w+\b").unwrap())
}

fn regex_defined_fl_is() -> &'static Regex {
    static VALUE: OnceLock<Regex> = OnceLock::new();
    VALUE.get_or_init(|| Regex::new(r"!?\s*defined\s*\(\s*FL_IS_\w+\s*\)").unwrap())
}

fn regex_singleton() -> &'static Regex {
    static VALUE: OnceLock<Regex> = OnceLock::new();
    VALUE.get_or_init(|| Regex::new(r"\bSingleton\s*<").unwrap())
}

fn regex_singleton_shared() -> &'static Regex {
    static VALUE: OnceLock<Regex> = OnceLock::new();
    VALUE.get_or_init(|| Regex::new(r"\bSingletonShared\b").unwrap())
}

fn regex_friend_class() -> &'static Regex {
    static VALUE: OnceLock<Regex> = OnceLock::new();
    VALUE.get_or_init(|| Regex::new(r"\bfriend\s+class\b").unwrap())
}

fn regex_numeric_limit_macro() -> &'static Regex {
    static VALUE: OnceLock<Regex> = OnceLock::new();
    VALUE.get_or_init(|| Regex::new(&format!(r"\b({})\b", NUMERIC_LIMIT_MACROS.join("|"))).unwrap())
}

fn regex_platform_pragma() -> &'static Regex {
    static VALUE: OnceLock<Regex> = OnceLock::new();
    VALUE.get_or_init(|| {
        Regex::new(r"^\s*#\s*pragma\s+(?:GCC\s+diagnostic|clang\s+diagnostic|warning\s*\()")
            .unwrap()
    })
}

fn regex_raw_pragma() -> &'static Regex {
    static VALUE: OnceLock<Regex> = OnceLock::new();
    VALUE.get_or_init(|| Regex::new(r"\b_Pragma\s*\(").unwrap())
}

fn regex_raw_noexcept() -> &'static Regex {
    static VALUE: OnceLock<Regex> = OnceLock::new();
    VALUE.get_or_init(|| Regex::new(r"\bnoexcept\b").unwrap())
}

fn regex_define_fl_noexcept() -> &'static Regex {
    static VALUE: OnceLock<Regex> = OnceLock::new();
    VALUE.get_or_init(|| Regex::new(r"#\s*define\s+FL_NOEXCEPT\b").unwrap())
}

fn regex_noexcept_suppression() -> &'static Regex {
    static VALUE: OnceLock<Regex> = OnceLock::new();
    VALUE.get_or_init(|| Regex::new(r"(?i)//\s*(?:ok\s+noexcept|nolint)\b").unwrap())
}

fn regex_std_usage() -> &'static Regex {
    static VALUE: OnceLock<Regex> = OnceLock::new();
    VALUE.get_or_init(|| Regex::new(r"std::\w+").unwrap())
}

fn regex_serial_method() -> &'static Regex {
    static VALUE: OnceLock<Regex> = OnceLock::new();
    VALUE.get_or_init(|| Regex::new(r"\bSerial\.(\w+)\s*\(").unwrap())
}

fn regex_deep_platform_header() -> &'static Regex {
    static VALUE: OnceLock<Regex> = OnceLock::new();
    VALUE.get_or_init(|| Regex::new(r#"(#include\s+"(platforms/[^/]+/[^"]+\.h[^"]*)")"#).unwrap())
}

fn regex_deep_platform_include() -> &'static Regex {
    static VALUE: OnceLock<Regex> = OnceLock::new();
    VALUE.get_or_init(|| {
        Regex::new(r#"^\s*#\s*include\s+[<"]platforms/([^/"]+)/([^"]+)[">]"#).unwrap()
    })
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

fn platform_trampoline_suggestion(include_path: &str) -> &'static str {
    DEEP_PLATFORM_REPLACEMENTS
        .iter()
        .find_map(|(pattern, replacement)| include_path.contains(pattern).then_some(*replacement))
        .unwrap_or("platforms/*.h (appropriate trampoline)")
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

struct CppHppIncludesChecker;

impl FileContentChecker for CppHppIncludesChecker {
    fn name(&self) -> &'static str {
        "CppHppIncludesChecker"
    }

    fn should_process_file(&self, file_path: &str, project_root: &Path) -> bool {
        let is_src = is_under_project_subpath(file_path, project_root, "src");
        let is_tests = is_under_project_subpath(file_path, project_root, "tests");

        if !is_src && !is_tests {
            return false;
        }

        if is_src && !ends_with_any(file_path, &[".cpp", ".h", ".hpp", ".cpp.hpp"]) {
            return false;
        }
        if is_tests && !ends_with_any(file_path, &[".cpp", ".h", ".hpp"]) {
            return false;
        }

        if is_src {
            if file_path.ends_with("_build.hpp")
                || file_path.ends_with("_build.cpp")
                || file_path.ends_with("_build.cpp.hpp")
            {
                return false;
            }
            if is_under_project_subpath(file_path, project_root, "src/fl/build") {
                return false;
            }
        }

        true
    }

    fn check_file_content(&self, file_content: &FileContent) -> Vec<(usize, String)> {
        let mut violations = Vec::new();
        let mut in_multiline_comment = false;
        let normalized = normalize_path(&file_content.path);
        let is_test = normalized.contains("/tests/") || normalized.starts_with("tests/");

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
            let Some(captures) = regex_cpp_hpp_include().captures(code_part) else {
                continue;
            };
            let included_file = captures.get(1).map(|m| m.as_str()).unwrap_or("");
            if is_test {
                let h_header = included_file
                    .strip_suffix(".cpp.hpp")
                    .map(|prefix| format!("{prefix}.h"))
                    .unwrap_or_else(|| included_file.to_string());
                let h_header = h_header
                    .strip_suffix(".impl.h")
                    .map(|prefix| format!("{prefix}.h"))
                    .unwrap_or(h_header);
                violations.push((
                    index + 1,
                    format!(
                        "{stripped} - Including *.cpp.hpp files in tests is banned (hard ban, no opt-out). Include the public header instead: #include \"{h_header}\""
                    ),
                ));
            } else {
                violations.push((
                    index + 1,
                    format!(
                        "{stripped} - *.cpp.hpp files should ONLY be included by _build.* files (hard ban, no opt-out). Found include of '{included_file}' in non-build file. Move this include to the appropriate _build.hpp file."
                    ),
                ));
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

struct EspRomPrintfChecker;

impl FileContentChecker for EspRomPrintfChecker {
    fn name(&self) -> &'static str {
        "EspRomPrintfChecker"
    }

    fn should_process_file(&self, file_path: &str, _project_root: &Path) -> bool {
        if !ends_with_any(file_path, &[".cpp", ".h", ".hpp", ".ino", ".cpp.hpp"]) {
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

            let (code_part, comment_part) = line
                .split_once("//")
                .map_or((line.as_str(), ""), |(code, comment)| (code, comment));
            if !code_part.contains("esp_rom_printf") {
                continue;
            }
            if !regex_esp_rom_printf().is_match(code_part) {
                continue;
            }
            if comment_part
                .to_ascii_lowercase()
                .contains("ok esp_rom_printf")
            {
                continue;
            }

            violations.push((
                index + 1,
                format!(
                    "Avoid `esp_rom_printf` — use FastLED logging (FL_LOG_INFO, FL_WARN, FL_PRINT) or fl::io instead: {stripped}\n      Rationale: esp_rom_printf bypasses FastLED's logging facilities, blocks on a slow peripheral, and proliferates platform-specific I/O into general code.\n      If this call is genuinely required (e.g. early bootstrap before logging is up), suppress with `// ok esp_rom_printf - <reason>` on the same line."
                ),
            ));
        }

        violations
    }
}

struct SleepForChecker;

impl FileContentChecker for SleepForChecker {
    fn name(&self) -> &'static str {
        "SleepForChecker"
    }

    fn should_process_file(&self, file_path: &str, _project_root: &Path) -> bool {
        if !ends_with_any(file_path, &[".cpp", ".h", ".hpp", ".ino", ".cpp.hpp"]) {
            return false;
        }
        if is_excluded_file(file_path) {
            return false;
        }
        if file_path.contains("third_party") || file_path.contains("thirdparty") {
            return false;
        }
        let normalized = normalize_path(file_path);
        !SLEEP_FOR_WHITELISTED_SUFFIXES
            .iter()
            .any(|suffix| normalized.ends_with(suffix))
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
            if line.contains("// ok sleep for") || line.contains("// okay sleep for") {
                continue;
            }

            let code_part = split_line_comment(line);
            if !code_part.contains("sleep_for") {
                continue;
            }
            if regex_sleep_for().is_match(code_part) {
                violations.push((
                    index + 1,
                    format!(
                        "⚠️  CRITICAL: sleep_for() BLOCKS async/scheduler pumping! Async operations HANG, tasks FREEZE, UI becomes UNRESPONSIVE. USE fl::delay(ms) INSTEAD! Only suppress with '// ok sleep for' in core infrastructure: {stripped}"
                    ),
                ));
            }
        }

        violations
    }
}

struct ThreadLocalKeywordChecker;

impl FileContentChecker for ThreadLocalKeywordChecker {
    fn name(&self) -> &'static str {
        "ThreadLocalKeywordChecker"
    }

    fn should_process_file(&self, file_path: &str, _project_root: &Path) -> bool {
        ends_with_any(file_path, &[".cpp", ".h", ".hpp", ".ino"])
    }

    fn check_file_content(&self, file_content: &FileContent) -> Vec<(usize, String)> {
        if !file_content.content.contains("thread_local") {
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
            let code_without_strings = strip_string_literals(code_part);
            if regex_thread_local().is_match(&code_without_strings)
                && !line.contains("// ok thread_local")
            {
                violations.push((
                    index + 1,
                    format!(
                        "❌ Raw 'thread_local' keyword is banned — use fl::SingletonThreadLocal<T>::instance() instead (portable, never-destroyed, LSAN-safe): {stripped}"
                    ),
                ));
            }
        }

        violations
    }
}

struct SpanFromPointerChecker;

impl FileContentChecker for SpanFromPointerChecker {
    fn name(&self) -> &'static str {
        "SpanFromPointerChecker"
    }

    fn should_process_file(&self, file_path: &str, _project_root: &Path) -> bool {
        if !ends_with_any(file_path, &[".cpp", ".h", ".hpp", ".ino", ".cpp.hpp"]) {
            return false;
        }
        !file_path.contains("third_party") && !file_path.contains("thirdparty")
    }

    fn check_file_content(&self, file_content: &FileContent) -> Vec<(usize, String)> {
        if !file_content.content.contains("span<") && !file_content.content.contains("span <") {
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
            if line.contains("// ok span from pointer") {
                continue;
            }

            let code_part = split_line_comment(line);
            if !code_part.contains(".data()") || !code_part.contains(".size()") {
                continue;
            }
            if !code_part.contains("span<") && !code_part.contains("span <") {
                continue;
            }
            if regex_span_data_size().is_match(code_part) {
                violations.push((
                    index + 1,
                    format!(
                        "{stripped}  →  Use span<T>(container) instead of span<T>(container.data(), container.size()). Suppress with '// ok span from pointer'"
                    ),
                ));
            }
        }

        violations
    }
}

struct RelativeIncludeChecker;

impl FileContentChecker for RelativeIncludeChecker {
    fn name(&self) -> &'static str {
        "RelativeIncludeChecker"
    }

    fn should_process_file(&self, file_path: &str, project_root: &Path) -> bool {
        is_under_project_subpath(file_path, project_root, "src")
            && ends_with_any(file_path, &[".cpp", ".h", ".hpp", ".cc", ".cxx", ".ino"])
    }

    fn check_file_content(&self, file_content: &FileContent) -> Vec<(usize, String)> {
        const ALLOWED_RELATIVE_INCLUDE_FILES: &[&str] = &[
            "src/platforms/win/run_example.hpp",
            "src/platforms/posix/run_example.hpp",
            "src/platforms/apple/run_example.hpp",
        ];

        let normalized_path = normalize_path(&file_content.path);
        if ALLOWED_RELATIVE_INCLUDE_FILES
            .iter()
            .any(|allowed_file| normalized_path.ends_with(allowed_file))
        {
            return Vec::new();
        }

        let mut violations = Vec::new();
        let mut in_block_comment = false;
        for (index, line) in file_content.lines.iter().enumerate() {
            let visible_line = strip_block_comments_from_line(line, &mut in_block_comment);
            if regex_relative_include().is_match(&visible_line)
                && !line.contains("// ok relative include")
            {
                violations.push((index + 1, format!("Relative include: {}", line.trim())));
            }
        }

        violations
    }
}

struct FastLEDHeaderUsageChecker;

impl FileContentChecker for FastLEDHeaderUsageChecker {
    fn name(&self) -> &'static str {
        "FastLEDHeaderUsageChecker"
    }

    fn should_process_file(&self, file_path: &str, project_root: &Path) -> bool {
        if !is_under_project_subpath(file_path, project_root, "src") {
            return false;
        }
        if !ends_with_any(file_path, &[".h", ".hpp"]) {
            return false;
        }
        let normalized = normalize_path(file_path);
        let basename = normalized.rsplit('/').next().unwrap_or(&normalized);
        !matches!(basename, "FastLED.h" | "fastspi.h")
    }

    fn check_file_content(&self, file_content: &FileContent) -> Vec<(usize, String)> {
        let mut violations = Vec::new();
        let mut in_block_comment = false;
        let visible_lines: Vec<String> = file_content
            .lines
            .iter()
            .map(|line| strip_block_comments_from_line(line, &mut in_block_comment))
            .collect();

        for (index, visible_line) in visible_lines.iter().enumerate() {
            if !regex_fastled_h_include().is_match(visible_line) {
                continue;
            }
            let line = &file_content.lines[index];
            let lower = line.to_ascii_lowercase();
            if lower.contains("// ok include") {
                continue;
            }

            let lookback = (index).min(5);
            let has_internal_define = (1..=lookback)
                .map(|offset| &visible_lines[index - offset])
                .any(|prev_line| regex_fastled_internal_define().is_match(prev_line));
            if !has_internal_define {
                violations.push((
                    index + 1,
                    format!(
                        "Use 'fl/system/fastled.h' instead of 'FastLED.h': {}",
                        line.trim()
                    ),
                ));
            }
        }

        violations
    }
}

struct ArduinoMacroUsageChecker;

impl FileContentChecker for ArduinoMacroUsageChecker {
    fn name(&self) -> &'static str {
        "ArduinoMacroUsageChecker"
    }

    fn should_process_file(&self, file_path: &str, project_root: &Path) -> bool {
        if !ends_with_any(file_path, &[".cpp", ".h", ".hpp", ".ino", ".cpp.hpp"]) {
            return false;
        }
        if !is_under_project_subpath(file_path, project_root, "src") {
            return false;
        }
        if is_under_dir(file_path, "platforms") || is_under_dir(file_path, "third_party") {
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
            if regex_arduino_preprocessor().is_match(code_part)
                || regex_arduino_scoped_enum().is_match(code_part)
                || regex_arduino_enum_member().is_match(code_part)
            {
                continue;
            }

            for name in ARDUINO_BANNED_MACROS {
                let Some(found) = regex_arduino_macro()
                    .captures_iter(code_part)
                    .find(|capture| capture.get(1).is_some_and(|m| m.as_str() == *name))
                else {
                    continue;
                };
                let macro_name = found.get(1).unwrap().as_str();
                violations.push((
                    index + 1,
                    format!(
                        "Banned Arduino macro '{macro_name}' used: {stripped}\n      These macros pollute the global namespace and conflict with Windows headers.\n      Use platform-specific APIs or define local constants instead."
                    ),
                ));
            }
        }

        violations
    }
}

struct AttributeChecker;

impl FileContentChecker for AttributeChecker {
    fn name(&self) -> &'static str {
        "AttributeChecker"
    }

    fn should_process_file(&self, file_path: &str, _project_root: &Path) -> bool {
        if !ends_with_any(file_path, &[".cpp", ".h", ".hpp", ".ino", ".cpp.hpp"]) {
            return false;
        }
        let normalized = normalize_path(file_path);
        if normalized.ends_with("compiler_control.h") {
            return false;
        }
        if is_under_dir(&normalized, "third_party") {
            return false;
        }
        !ends_with_any(&normalized, &["doctest.h", "catch.hpp", "gtest.h"])
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
            if code_part.contains("#define") && code_part.contains("FL_") {
                continue;
            }
            if !code_part.contains("[[") && !code_part.contains("alignas") {
                continue;
            }

            if code_part.contains("[[") {
                for capture in regex_standard_attribute().captures_iter(code_part) {
                    let Some(attribute) = capture.get(1).map(|m| m.as_str()) else {
                        continue;
                    };
                    if let Some(fl_macro) = standard_attribute_replacement(attribute) {
                        violations.push((
                            index + 1,
                            format!("Use {fl_macro} instead of [[{attribute}]]: {stripped}"),
                        ));
                    }
                }
            }

            if code_part.contains("alignas")
                && regex_alignas().is_match(code_part)
                && !code_part.contains("FL_ALIGNAS")
            {
                violations.push((
                    index + 1,
                    format!("Use FL_ALIGNAS instead of alignas: {stripped}"),
                ));
            }
        }

        violations
    }
}

struct FlIsDefinedChecker;

impl FileContentChecker for FlIsDefinedChecker {
    fn name(&self) -> &'static str {
        "FlIsDefinedChecker"
    }

    fn should_process_file(&self, file_path: &str, _project_root: &Path) -> bool {
        ends_with_any(file_path, &[".cpp", ".h", ".hpp", ".ino"])
    }

    fn check_file_content(&self, file_content: &FileContent) -> Vec<(usize, String)> {
        if !file_content.content.contains("FL_IS_") {
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
            if stripped.starts_with("//") || !regex_preprocessor_if_elif().is_match(stripped) {
                continue;
            }

            let code_part = split_line_comment(line);
            if !regex_fl_is_token().is_match(code_part) {
                continue;
            }
            let stripped_code = regex_defined_fl_is().replace_all(code_part, "");
            for token in regex_fl_is_token().find_iter(&stripped_code) {
                violations.push((
                    index + 1,
                    format!(
                        "Bare '{}' in preprocessor conditional. FL_IS_* macros are defined/undefined (no value). Use '#ifdef {}' or '#if defined({})' instead.",
                        token.as_str(),
                        token.as_str(),
                        token.as_str()
                    ),
                ));
            }
        }

        violations
    }
}

struct NumericLimitMacroChecker;

impl FileContentChecker for NumericLimitMacroChecker {
    fn name(&self) -> &'static str {
        "NumericLimitMacroChecker"
    }

    fn should_process_file(&self, file_path: &str, _project_root: &Path) -> bool {
        if !ends_with_any(file_path, &[".cpp", ".h", ".hpp", ".ino"]) {
            return false;
        }
        if is_excluded_file(file_path) {
            return false;
        }
        let normalized = normalize_path(file_path);
        !NUMERIC_LIMIT_EXCLUDED_SUFFIXES
            .iter()
            .any(|suffix| normalized.ends_with(suffix))
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

            if line.contains("// okay numeric limit macro") {
                continue;
            }
            let code_part = split_line_comment(line);
            if !code_part.contains("_MAX") && !code_part.contains("_MIN") {
                continue;
            }

            let Some(capture) = regex_numeric_limit_macro().captures(code_part) else {
                continue;
            };
            let macro_name = capture.get(1).unwrap().as_str();
            let suggestion = numeric_limit_suggestion(macro_name);
            violations.push((index + 1, format!("{stripped} (use {suggestion} instead)")));
        }

        violations
    }
}

struct PlatformPragmaChecker;

impl FileContentChecker for PlatformPragmaChecker {
    fn name(&self) -> &'static str {
        "PlatformPragmaChecker"
    }

    fn should_process_file(&self, file_path: &str, _project_root: &Path) -> bool {
        if !ends_with_any(file_path, &[".cpp", ".h", ".hpp", ".ino", ".cpp.hpp"]) {
            return false;
        }
        let normalized = normalize_path(file_path);
        !is_under_dir(&normalized, "third_party")
            && !is_under_dir(&normalized, "platforms")
            && !normalized.ends_with("compiler_control.h")
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

            if index >= 1 && file_content.lines[index - 1].contains("FL_ALLOW_PLATFORM_PRAGMA") {
                continue;
            }
            if line.contains("FL_ALLOW_PLATFORM_PRAGMA") {
                continue;
            }
            if regex_platform_pragma().is_match(line) {
                violations.push((
                    index + 1,
                    format!(
                        "Raw platform-specific pragma: {stripped}\n      Use FL_DISABLE_WARNING_PUSH / FL_DISABLE_WARNING(<name>) / FL_DISABLE_WARNING_POP from fl/stl/compiler_control.h.\n      If the FL macros cannot express this pragma, place FL_ALLOW_PLATFORM_PRAGMA on the preceding line as an escape hatch."
                    ),
                ));
            }
        }

        violations
    }
}

struct RawPragmaChecker;

impl FileContentChecker for RawPragmaChecker {
    fn name(&self) -> &'static str {
        "RawPragmaChecker"
    }

    fn should_process_file(&self, file_path: &str, _project_root: &Path) -> bool {
        if !ends_with_any(file_path, &[".cpp", ".h", ".hpp", ".ino", ".cpp.hpp"]) {
            return false;
        }
        let normalized = normalize_path(file_path);
        !normalized.ends_with("compiler_control.h") && !is_under_dir(&normalized, "third_party")
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

            if regex_raw_pragma().is_match(line) {
                violations.push((
                    index + 1,
                    format!(
                        "Raw _Pragma() usage: {stripped}\n      Use FL_DISABLE_WARNING_PUSH / FL_DISABLE_WARNING_* / FL_DISABLE_WARNING_POP from fl/stl/compiler_control.h instead.\n      _Pragma() is not portable across all target compilers."
                    ),
                ));
            }
        }

        violations
    }
}

struct RawNoexceptChecker;

impl FileContentChecker for RawNoexceptChecker {
    fn name(&self) -> &'static str {
        "RawNoexceptChecker"
    }

    fn should_process_file(&self, file_path: &str, project_root: &Path) -> bool {
        if !is_under_project_subpath(file_path, project_root, "src") {
            return false;
        }
        if !ends_with_any(file_path, &[".h", ".hpp", ".cpp", ".cpp.hpp"]) {
            return false;
        }
        let normalized = normalize_path(file_path);
        !normalized.ends_with("fl/stl/noexcept.h") && !is_under_dir(&normalized, "third_party")
    }

    fn check_file_content(&self, file_content: &FileContent) -> Vec<(usize, String)> {
        if !file_content.content.contains("noexcept") {
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

            let code = split_line_comment(stripped).trim();
            if code.is_empty()
                || regex_preprocessor_include().is_match(code)
                || regex_define_fl_noexcept().is_match(code)
                || !regex_raw_noexcept().is_match(code)
            {
                continue;
            }

            let temp = code.replace("FL_NOEXCEPT", "");
            let remaining: Vec<_> = regex_raw_noexcept().find_iter(&temp).collect();
            let all_operator_form = remaining
                .iter()
                .all(|m| temp[m.end()..].trim_start().starts_with('('));
            if !remaining.is_empty() && all_operator_form {
                continue;
            }
            if regex_noexcept_suppression().is_match(line) {
                continue;
            }

            violations.push((
                index + 1,
                format!(
                    "Raw 'noexcept' keyword — use FL_NOEXCEPT macro instead (defined in fl/stl/noexcept.h, currently a noop everywhere for cross-platform compatibility): {stripped}"
                ),
            ));
        }

        violations
    }
}

struct SingletonInHeadersChecker;

impl FileContentChecker for SingletonInHeadersChecker {
    fn name(&self) -> &'static str {
        "SingletonInHeadersChecker"
    }

    fn should_process_file(&self, file_path: &str, _project_root: &Path) -> bool {
        if !ends_with_any(file_path, &[".h", ".hpp"]) {
            return false;
        }
        let normalized = normalize_path(file_path);
        if normalized.ends_with("fl/stl/singleton.h") {
            return false;
        }
        if normalized.ends_with(".hpp") {
            return false;
        }
        !is_excluded_file(&normalized)
    }

    fn check_file_content(&self, file_content: &FileContent) -> Vec<(usize, String)> {
        let is_cpp_hpp = file_content.path.ends_with(".cpp.hpp");
        let is_private_header = file_content
            .lines
            .iter()
            .take(50)
            .any(|line| line.contains("IWYU pragma: private"));
        if is_cpp_hpp || is_private_header {
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
            if !code_part.contains("Singleton") {
                continue;
            }
            if is_cpp_hpp {
                if regex_singleton_shared().is_match(code_part) {
                    violations.push((
                        index + 1,
                        format!("Use Singleton<T> instead of SingletonShared<T> in .cpp.hpp: {stripped}"),
                    ));
                }
            } else if regex_singleton().is_match(code_part) {
                if regex_friend_class().is_match(code_part) {
                    continue;
                }
                violations.push((
                    index + 1,
                    format!(
                        "Use SingletonShared<T> instead of Singleton<T> in headers: {stripped}"
                    ),
                ));
            }
        }

        violations
    }
}

struct StdNamespaceChecker;

impl FileContentChecker for StdNamespaceChecker {
    fn name(&self) -> &'static str {
        "StdNamespaceChecker"
    }

    fn should_process_file(&self, file_path: &str, project_root: &Path) -> bool {
        if !ends_with_any(file_path, &[".cpp", ".h", ".hpp", ".ino"]) {
            return false;
        }
        if !is_under_project_subpath(file_path, project_root, "src")
            && !is_under_project_subpath(file_path, project_root, "tests")
        {
            return false;
        }
        if is_excluded_file(file_path) {
            return false;
        }
        let normalized = normalize_path(file_path);
        if normalized.contains("third_party") || normalized.contains("thirdparty") {
            return false;
        }
        !is_std_bridge_file(&normalized)
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
            if !code_part.contains("std::")
                || line.contains("// okay std namespace")
                || line_has_only_allowed_std_symbols(line)
            {
                continue;
            }
            violations.push((index + 1, stripped.to_string()));
        }

        violations
    }
}

struct ExampleSerialChecker;

impl FileContentChecker for ExampleSerialChecker {
    fn name(&self) -> &'static str {
        "ExampleSerialChecker"
    }

    fn should_process_file(&self, file_path: &str, _project_root: &Path) -> bool {
        if !ends_with_any(file_path, &[".cpp", ".h", ".hpp", ".ino"]) {
            return false;
        }
        normalize_path(file_path).contains("examples/AutoResearch/")
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
            if !code_part.contains("Serial.") {
                continue;
            }
            let Some(capture) = regex_serial_method().captures(code_part) else {
                continue;
            };
            let method = capture.get(1).unwrap().as_str();
            if ALLOWED_SERIAL_METHODS.contains(&method)
                || !FORBIDDEN_SERIAL_METHODS.contains(&method)
            {
                continue;
            }
            if comment_part.to_ascii_lowercase().contains("ok serial") {
                continue;
            }

            let replacement = serial_replacement(method);
            violations.push((
                index + 1,
                format!(
                    "Avoid `Serial.{method}(...)` in enforced examples — use `{replacement}` instead.\n      Rationale: fl:: wrappers carry the non-blocking HWCDC fixes from FastLED #2669 (setTxTimeoutMs=0, guarded flush, host-presence skip). Raw `Serial.{method}` bypasses them.\n      Line: {stripped}\n      If this call is genuinely required (platform-specific config with no fl:: equivalent), suppress with `// ok serial - <reason>` on the same line."
                ),
            ));
        }

        violations
    }
}

struct IncludeAfterNamespaceChecker;

impl FileContentChecker for IncludeAfterNamespaceChecker {
    fn name(&self) -> &'static str {
        "IncludeAfterNamespaceChecker"
    }

    fn should_process_file(&self, file_path: &str, _project_root: &Path) -> bool {
        if INCLUDE_AFTER_NAMESPACE_SKIP_PATTERNS
            .iter()
            .any(|pattern| file_path.contains(pattern))
        {
            return false;
        }
        ends_with_any(file_path, &[".cpp", ".h", ".hpp", ".cc", ".ino"])
    }

    fn check_file_content(&self, file_content: &FileContent) -> Vec<(usize, String)> {
        if file_content
            .lines
            .iter()
            .any(|line| regex_allow_include_after_namespace().is_match(line))
        {
            return Vec::new();
        }

        let mut namespace_started = false;
        let mut violations = Vec::new();

        for (index, line) in file_content.lines.iter().enumerate() {
            if regex_namespace_declaration().is_match(line) {
                namespace_started = true;
                continue;
            }
            if namespace_started && regex_any_include().is_match(line) {
                if regex_nolint().is_match(line) {
                    continue;
                }
                violations.push((index + 1, line.trim_end_matches('\n').to_string()));
            }
        }

        violations
    }
}

struct NamespaceFlDeclarationChecker;

impl FileContentChecker for NamespaceFlDeclarationChecker {
    fn name(&self) -> &'static str {
        "NamespaceFlDeclarationChecker"
    }

    fn should_process_file(&self, file_path: &str, project_root: &Path) -> bool {
        let path = Path::new(file_path);
        if path
            .parent()
            .and_then(Path::file_name)
            .and_then(|name| name.to_str())
            != Some("src")
        {
            return false;
        }
        let Ok(src_root) = project_root.join("src").canonicalize() else {
            return false;
        };
        let Ok(parent) = path
            .parent()
            .unwrap_or_else(|| Path::new(""))
            .canonicalize()
        else {
            return false;
        };
        if parent != src_root {
            return false;
        }
        matches!(
            path.extension().and_then(|value| value.to_str()),
            Some("h" | "cpp")
        )
    }

    fn check_file_content(&self, file_content: &FileContent) -> Vec<(usize, String)> {
        let mut violations = Vec::new();
        for (index, line) in file_content.lines.iter().enumerate() {
            if line.trim().starts_with("//") {
                continue;
            }
            if regex_namespace_fl_declaration().is_match(line) {
                violations.push((index + 1, line.trim().to_string()));
            }
        }
        violations
    }
}

struct UsingNamespaceFlChecker;

impl FileContentChecker for UsingNamespaceFlChecker {
    fn name(&self) -> &'static str {
        "UsingNamespaceFlChecker"
    }

    fn should_process_file(&self, file_path: &str, project_root: &Path) -> bool {
        let normalized = normalize_path(file_path);
        if !is_under_project_subpath(&normalized, project_root, "src") {
            return false;
        }
        if normalized.contains("/examples/") || normalized.contains("/tests/") {
            return false;
        }
        if normalized.contains("FastLED.h") {
            return false;
        }
        ends_with_any(&normalized, &[".h", ".hpp", ".cpp", ".cpp.hpp"])
    }

    fn check_file_content(&self, file_content: &FileContent) -> Vec<(usize, String)> {
        let mut violations = Vec::new();
        for (index, line) in file_content.lines.iter().enumerate() {
            if line.starts_with("//") {
                continue;
            }
            if line.contains("using namespace fl;") {
                violations.push((index + 1, line.trim().to_string()));
            }
        }
        violations
    }
}

struct UsingNamespaceChecker;

impl FileContentChecker for UsingNamespaceChecker {
    fn name(&self) -> &'static str {
        "UsingNamespaceChecker"
    }

    fn should_process_file(&self, file_path: &str, project_root: &Path) -> bool {
        let normalized = normalize_path(file_path);
        if !is_under_project_subpath(&normalized, project_root, "src/fl") {
            return false;
        }
        if normalized.contains("/examples/") || normalized.contains("/tests/") {
            return false;
        }
        ends_with_any(&normalized, &[".h", ".hpp", ".hxx", ".hh"])
    }

    fn check_file_content(&self, file_content: &FileContent) -> Vec<(usize, String)> {
        let mut violations = Vec::new();
        for (index, line) in file_content.lines.iter().enumerate() {
            let without_line_comment = split_line_comment(line);
            let line_clean = strip_inline_block_comments(without_line_comment);
            if !regex_using_namespace().is_match(&line_clean) {
                continue;
            }
            if regex_define_using_namespace().is_match(&line_clean) {
                continue;
            }

            let mut is_conditional = false;
            for previous_index in index.saturating_sub(5)..index {
                if regex_force_use_namespace().is_match(file_content.lines[previous_index].trim()) {
                    is_conditional = true;
                    break;
                }
            }
            if !is_conditional {
                violations.push((index + 1, line.trim().to_string()));
            }
        }
        violations
    }
}

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
    fn help_parse_exits_without_listing_checkers() {
        let config = CliConfig::parse(vec!["--help".to_string()]).unwrap();
        assert!(config.show_help);
        assert!(!config.list_checkers);
    }

    #[test]
    fn explicit_missing_input_is_an_error() {
        let input =
            std::env::temp_dir().join(format!("fastled_lint_missing_input_{}", std::process::id()));
        let error = collect_input_files(Path::new("."), &[path_to_string(&input)])
            .unwrap_err()
            .to_string();
        assert!(error.contains("input path not found"));
    }

    #[test]
    fn unmatched_input_pattern_is_an_error() {
        let temp_dir = normalize_path(&path_to_string(&std::env::temp_dir()));
        let input = format!(
            "{temp_dir}/fastled_lint_missing_pattern_{}/*.h",
            std::process::id()
        );
        let error = collect_input_files(Path::new("."), &[input])
            .unwrap_err()
            .to_string();
        assert!(error.contains("input pattern matched no files"));
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
