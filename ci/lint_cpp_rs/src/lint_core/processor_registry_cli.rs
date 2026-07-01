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
    // Arc-wrapped so the clone-from-cache phase is a refcount bump
    // (per-file ~tens of nanos) instead of a deep clone of the content
    // String + Vec<String> lines (which dominated ~140ms wall time
    // before this change, ~50us per file).
    file_cache: HashMap<PathBuf, std::sync::Arc<FileContent>>,
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
        self.process_files_with_checkers_profiled(file_paths, checkers, project_root, false)
    }

    pub fn process_files_with_checkers_profiled(
        &mut self,
        file_paths: &[PathBuf],
        checkers: &[Box<dyn FileContentChecker>],
        project_root: &Path,
        profile: bool,
    ) -> Result<Vec<LintViolation>, DynError> {
        let t_unique = std::time::Instant::now();
        let mut unique_paths = BTreeSet::new();
        for path in file_paths {
            unique_paths.insert(path.clone());
        }
        if profile {
            eprintln!(
                "[PROFILE]   unique_paths build: {:?} ({} unique)",
                t_unique.elapsed(),
                unique_paths.len()
            );
        }

        let pending_paths: Vec<PathBuf> = unique_paths
            .iter()
            .filter(|path| !self.file_cache.contains_key(*path))
            .cloned()
            .collect();

        let t_read = std::time::Instant::now();
        let loaded_files: Vec<(PathBuf, Result<FileContent, DynError>)> = pending_paths
            .par_iter()
            .map(|path| (path.clone(), FileContent::read(path)))
            .collect();

        for (path, loaded) in loaded_files {
            match loaded {
                Ok(content) => {
                    self.file_cache.insert(path, std::sync::Arc::new(content));
                }
                Err(error) => {
                    eprintln!(
                        "warning: skipped unreadable file {}: {error}",
                        path.display()
                    );
                }
            }
        }
        if profile {
            eprintln!(
                "[PROFILE]   file read+parse (par): {:?} ({} files)",
                t_read.elapsed(),
                pending_paths.len()
            );
        }

        let t_collect = std::time::Instant::now();
        // Arc::clone is a refcount bump; the heavy String + Vec<String>
        // payload is shared, not duplicated.
        let files: Vec<std::sync::Arc<FileContent>> = unique_paths
            .iter()
            .filter_map(|path| self.file_cache.get(path).map(std::sync::Arc::clone))
            .collect();
        if profile {
            eprintln!(
                "[PROFILE]   clone-from-cache: {:?} ({} files)",
                t_collect.elapsed(),
                files.len()
            );
        }

        let t_dispatch = std::time::Instant::now();
        // Per-checker timing aggregates (only allocated when profiling is on).
        // Returned alongside the per-file violations via flat_map_iter so we
        // can roll them up after the parallel pass without an atomic-counter
        // dance per checker. Each per-file work item produces a Vec<u128>
        // (nanos of check_file_content time per checker) and a Vec<bool>
        // (whether check_file_content ran for that checker on this file).
        let mut violations: Vec<LintViolation>;
        if profile {
            let per_file: Vec<(Vec<LintViolation>, Vec<u128>, Vec<bool>)> = files
                .par_iter()
                .map(|file_content| {
                    let mut file_violations = Vec::new();
                    let mut checker_ns = vec![0u128; checkers.len()];
                    let mut checker_ran = vec![false; checkers.len()];
                    for (idx, checker) in checkers.iter().enumerate() {
                        if !checker.should_process_file(&file_content.path, project_root) {
                            continue;
                        }
                        checker_ran[idx] = true;
                        let t = std::time::Instant::now();
                        let results = checker.check_file_content(file_content);
                        checker_ns[idx] = t.elapsed().as_nanos();
                        for (line, message) in results {
                            file_violations.push(LintViolation {
                                checker: checker.name().to_string(),
                                path: file_content.path.clone(),
                                line,
                                message,
                            });
                        }
                    }
                    (file_violations, checker_ns, checker_ran)
                })
                .collect();

            let mut totals_ns = vec![0u128; checkers.len()];
            let mut ran_counts = vec![0usize; checkers.len()];
            violations = Vec::new();
            for (v, ns, ran) in per_file {
                violations.extend(v);
                for (i, n) in ns.iter().enumerate() {
                    totals_ns[i] += n;
                }
                for (i, r) in ran.iter().enumerate() {
                    if *r {
                        ran_counts[i] += 1;
                    }
                }
            }
            eprintln!(
                "[PROFILE]   checker dispatch (par): {:?} ({} files x {} checkers)",
                t_dispatch.elapsed(),
                files.len(),
                checkers.len()
            );
            // Per-checker top spenders. Sort by total nanos desc; print top 20.
            let mut idx_by_ns: Vec<usize> = (0..checkers.len()).collect();
            idx_by_ns.sort_by(|a, b| totals_ns[*b].cmp(&totals_ns[*a]));
            eprintln!("[PROFILE]   per-checker top 20 by total time:");
            for &idx in idx_by_ns.iter().take(20) {
                if totals_ns[idx] == 0 {
                    break;
                }
                let ms = totals_ns[idx] as f64 / 1_000_000.0;
                eprintln!(
                    "[PROFILE]     {:>8.1}ms  {:>5} files  {}",
                    ms,
                    ran_counts[idx],
                    checkers[idx].name()
                );
            }
        } else {
            violations = files
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
        }

        let t_sort = std::time::Instant::now();
        violations.sort();
        if profile {
            eprintln!(
                "[PROFILE]   sort violations: {:?} ({} violations)",
                t_sort.elapsed(),
                violations.len()
            );
        }
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
        "bare_digit_separator",
        "bare_libm",
        "bare_noinline",
        "bare_snprintf",
        "bare_using",
        "banned_headers",
        "banned_define",
        "banned_macros",
        "banned_namespace",
        "builtin_memcpy",
        "autoresearch_runtime_output",
        "fl_no_underscore",
        "legacy_log_macro",
        "cpp_hpp_includes",
        "cpp_hpp_header_pair",
        "cpp_include",
        "ctype_global",
        "em_asm_clang_format",
        "enum_class",
        "esp_rom_printf",
        "fastled_header_usage",
        "fl_is_defined",
        "headers_exist",
        "include_after_namespace",
        "include_paths",
        "impl_hpp_includes",
        "logging_in_iram",
        "is_header_include",
        "iwyu_pragma_block",
        "iwyu_pragma_private",
        "member_style",
        "namespace_fl_declaration",
        "namespace_includes",
        "namespace_platforms",
        "native_platform_defines",
        "noexcept_special_members",
        "numeric_limit_macros",
        "platform_includes",
        "platforms_fl_namespace",
        "pragma_once",
        "pch_file",
        "platform_pragma",
        "platform_trampoline",
        "public_settings_pattern",
        "unity_build",
        "raw_noexcept",
        "raw_pragma",
        "reinterpret_cast",
        "relative_include",
        "serial_printf",
        "simd_intrinsics",
        "singleton_elision",
        "sleep_for",
        "span_from_pointer",
        "static_in_headers",
        "std_namespace",
        "singleton_in_headers",
        "stdint_type",
        "subdir_namespace",
        "test_aggregation",
        "test_include_paths",
        "test_path_structure",
        "thread_local_keyword",
        "example_serial",
        "unit_test",
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
        "BareDigitSeparatorChecker",
        "BareLibmChecker",
        "BareNoInlineChecker",
        "BareSnprintfChecker",
        "BareUsingChecker",
        "BannedDefineChecker",
        "BannedHeadersChecker",
        "BannedMacrosChecker",
        "BannedNamespaceChecker",
        "BuiltinMemcpyChecker",
        "AutoResearchRuntimeOutputChecker",
        "FlNoUnderscoreChecker",
        "LegacyLogMacroChecker",
        "CppHppIncludesChecker",
        "CppHppHeaderPairChecker",
        "CppIncludeChecker",
        "CtypeGlobalChecker",
        "EmAsmClangFormatChecker",
        "EnumClassChecker",
        "EspRomPrintfChecker",
        "ExampleSerialChecker",
        "FastLEDHeaderUsageChecker",
        "FlIsDefinedChecker",
        "HeadersExistChecker",
        "IncludeAfterNamespaceChecker",
        "IncludePathsChecker",
        "ImplHppIncludesChecker",
        "IsHeaderIncludeChecker",
        "IwyuPragmaBlockChecker",
        "IwyuPragmaPrivateChecker",
        "LoggingInIramChecker",
        "MemberStyleChecker",
        "NamespaceFlDeclarationChecker",
        "NamespaceIncludesChecker",
        "NamespacePlatformsChecker",
        "NativePlatformDefinesChecker",
        "NoexceptSpecialMembersChecker",
        "NumericLimitMacroChecker",
        "PlatformIncludesChecker",
        "PlatformsFlNamespaceChecker",
        "PragmaOnceChecker",
        "PchFileChecker",
        "PlatformPragmaChecker",
        "PlatformTrampolineChecker",
        "PublicSettingsPatternChecker",
        "UnityBuildChecker",
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
        "StdintTypeChecker",
        "SubdirNamespaceChecker",
        "TestAggregationChecker",
        "TestIncludePathsChecker",
        "TestPathStructureChecker",
        "ThreadLocalKeywordChecker",
        "UnitTestChecker",
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
        ("bare_digit_separator", Box::new(BareDigitSeparatorChecker)),
        ("bare_libm", Box::new(BareLibmChecker)),
        ("bare_noinline", Box::new(BareNoInlineChecker)),
        ("bare_snprintf", Box::new(BareSnprintfChecker)),
        ("bare_using", Box::new(BareUsingChecker)),
        (
            "banned_headers",
            Box::new(BannedHeadersChecker {
                banned_headers: BANNED_HEADERS_CORE_RS,
                strict_mode: true,
                scope: BannedHeadersScope::Fl,
            }),
        ),
        (
            "banned_headers",
            Box::new(BannedHeadersChecker {
                banned_headers: BANNED_HEADERS_CORE_RS,
                strict_mode: true,
                scope: BannedHeadersScope::Lib8tion,
            }),
        ),
        (
            "banned_headers",
            Box::new(BannedHeadersChecker {
                banned_headers: BANNED_HEADERS_CORE_RS,
                strict_mode: false,
                scope: BannedHeadersScope::FxSensorsPlatformsShared,
            }),
        ),
        (
            "banned_headers",
            Box::new(BannedHeadersChecker {
                banned_headers: BANNED_HEADERS_COMMON_RS,
                strict_mode: false,
                scope: BannedHeadersScope::Platforms,
            }),
        ),
        (
            "banned_headers",
            Box::new(BannedHeadersChecker {
                banned_headers: BANNED_HEADERS_COMMON_RS,
                strict_mode: false,
                scope: BannedHeadersScope::Examples,
            }),
        ),
        (
            "banned_headers",
            Box::new(BannedHeadersChecker {
                banned_headers: BANNED_HEADERS_COMMON_RS,
                strict_mode: true,
                scope: BannedHeadersScope::ThirdParty,
            }),
        ),
        (
            "banned_headers",
            Box::new(BannedHeadersChecker {
                banned_headers: BANNED_HEADERS_CORE_RS,
                strict_mode: false,
                scope: BannedHeadersScope::Tests,
            }),
        ),
        ("banned_define", Box::new(BannedDefineChecker)),
        ("banned_macros", Box::new(BannedMacrosChecker)),
        ("banned_namespace", Box::new(BannedNamespaceChecker)),
        ("builtin_memcpy", Box::new(BuiltinMemcpyChecker)),
        (
            "autoresearch_runtime_output",
            Box::new(AutoResearchRuntimeOutputChecker),
        ),
        ("fl_no_underscore", Box::new(FlNoUnderscoreChecker)),
        ("legacy_log_macro", Box::new(LegacyLogMacroChecker)),
        ("cpp_hpp_includes", Box::new(CppHppIncludesChecker)),
        ("cpp_hpp_header_pair", Box::new(CppHppHeaderPairChecker)),
        ("cpp_include", Box::new(CppIncludeChecker)),
        ("ctype_global", Box::new(CtypeGlobalChecker)),
        ("em_asm_clang_format", Box::new(EmAsmClangFormatChecker)),
        ("enum_class", Box::new(EnumClassChecker)),
        ("esp_rom_printf", Box::new(EspRomPrintfChecker)),
        ("example_serial", Box::new(ExampleSerialChecker)),
        ("fastled_header_usage", Box::new(FastLEDHeaderUsageChecker)),
        ("fl_is_defined", Box::new(FlIsDefinedChecker)),
        ("headers_exist", Box::new(HeadersExistChecker)),
        (
            "include_after_namespace",
            Box::new(IncludeAfterNamespaceChecker),
        ),
        ("include_paths", Box::new(IncludePathsChecker)),
        ("impl_hpp_includes", Box::new(ImplHppIncludesChecker)),
        ("is_header_include", Box::new(IsHeaderIncludeChecker)),
        ("iwyu_pragma_block", Box::new(IwyuPragmaBlockChecker)),
        ("iwyu_pragma_private", Box::new(IwyuPragmaPrivateChecker)),
        ("logging_in_iram", Box::new(LoggingInIramChecker)),
        ("member_style", Box::new(MemberStyleChecker)),
        (
            "namespace_fl_declaration",
            Box::new(NamespaceFlDeclarationChecker),
        ),
        ("namespace_includes", Box::new(NamespaceIncludesChecker)),
        ("namespace_platforms", Box::new(NamespacePlatformsChecker)),
        (
            "native_platform_defines",
            Box::new(NativePlatformDefinesChecker),
        ),
        (
            "noexcept_special_members",
            Box::new(NoexceptSpecialMembersChecker),
        ),
        ("numeric_limit_macros", Box::new(NumericLimitMacroChecker)),
        ("platform_includes", Box::new(PlatformIncludesChecker)),
        (
            "platforms_fl_namespace",
            Box::new(PlatformsFlNamespaceChecker),
        ),
        ("pragma_once", Box::new(PragmaOnceChecker)),
        ("platform_pragma", Box::new(PlatformPragmaChecker)),
        ("platform_trampoline", Box::new(PlatformTrampolineChecker)),
        ("public_settings_pattern", Box::new(PublicSettingsPatternChecker)),
        ("unity_build", Box::new(UnityBuildChecker)),
        ("raw_noexcept", Box::new(RawNoexceptChecker)),
        ("raw_pragma", Box::new(RawPragmaChecker)),
        ("reinterpret_cast", Box::new(ReinterpretCastChecker)),
        ("relative_include", Box::new(RelativeIncludeChecker)),
        ("serial_printf", Box::new(SerialPrintfChecker)),
        ("simd_intrinsics", Box::new(SimdIntrinsicsChecker)),
        ("singleton_elision", Box::new(SingletonElisionChecker)),
        ("sleep_for", Box::new(SleepForChecker)),
        ("singleton_in_headers", Box::new(SingletonInHeadersChecker)),
        ("span_from_pointer", Box::new(SpanFromPointerChecker)),
        ("static_in_headers", Box::new(StaticInHeaderChecker)),
        ("std_namespace", Box::new(StdNamespaceChecker)),
        ("stdint_type", Box::new(StdintTypeChecker)),
        (
            "subdir_namespace",
            Box::new(SubdirNamespaceChecker { subdir: "net" }),
        ),
        (
            "subdir_namespace",
            Box::new(SubdirNamespaceChecker { subdir: "video" }),
        ),
        (
            "subdir_namespace",
            Box::new(SubdirNamespaceChecker { subdir: "task" }),
        ),
        ("test_aggregation", Box::new(TestAggregationChecker)),
        ("test_include_paths", Box::new(TestIncludePathsChecker)),
        ("test_path_structure", Box::new(TestPathStructureChecker)),
        ("thread_local_keyword", Box::new(ThreadLocalKeywordChecker)),
        ("unit_test", Box::new(UnitTestChecker)),
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
    let t_total = std::time::Instant::now();
    let profile = matches!(
        std::env::var("FASTLED_LINT_PROFILE").as_deref(),
        Ok("1") | Ok("true") | Ok("yes") | Ok("on")
    );

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

    let t = std::time::Instant::now();
    let checkers = create_checkers(selected)?;
    if profile {
        eprintln!(
            "[PROFILE] create_checkers: {:?} ({} checkers)",
            t.elapsed(),
            checkers.len()
        );
    }

    let t = std::time::Instant::now();
    let files = collect_input_files(&config.project_root, &config.paths)?;
    if profile {
        eprintln!(
            "[PROFILE] collect_input_files: {:?} ({} files)",
            t.elapsed(),
            files.len()
        );
    }

    let mut processor = MultiCheckerFileProcessor::new();
    let mut violations = processor.process_files_with_checkers_profiled(
        &files,
        &checkers,
        &config.project_root,
        profile,
    )?;

    // Structural passes only fire in whole-project mode (no explicit paths) —
    // single-file mode shouldn't see cross-file walks, mirroring the Python
    // orchestrator's behaviour.
    if config.paths.is_empty() {
        let t = std::time::Instant::now();
        let mut structural = run_structural_passes(&config.project_root);
        if profile {
            eprintln!(
                "[PROFILE] run_structural_passes: {:?} ({} violations)",
                t.elapsed(),
                structural.len()
            );
        }
        if let Some(selected) = selected {
            structural.retain(|violation| selected.contains(violation.checker.as_str())
                || selected_contains_structural_alias(selected, &violation.checker));
        }
        violations.extend(structural);
        violations.sort();
    }

    if profile {
        eprintln!("[PROFILE] TOTAL run_cli: {:?}", t_total.elapsed());
    }

    match config.output_format {
        OutputFormat::Json => {
            println!("{}", serde_json::to_string_pretty(&violations)?);
        }
        OutputFormat::Text => print_text_results(&violations, &config.project_root),
    }

    Ok(if violations.is_empty() { 0 } else { 1 })
}

fn selected_contains_structural_alias(selected: &HashSet<String>, checker_name: &str) -> bool {
    // Structural passes are referenced by their snake_case alias in the
    // --checker selector (e.g. `pch_file`) just like per-file checkers; both
    // the alias and the class name should activate them.
    match checker_name {
        "PchFileChecker" => selected.contains("pch_file"),
        _ => false,
    }
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
