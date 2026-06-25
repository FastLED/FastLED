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
    fn autoresearch_runtime_output_flags_direct_logging_and_serial_prints() {
        let checker = AutoResearchRuntimeOutputChecker;
        let result = checker.check_file_content(&file(
            "examples/AutoResearch/AutoResearch.ino",
            "FL_WARN(\"boot chatter\");\nFL_WARN_F_ONCE(\"boot chatter\");\nFL_PRINT_EVERY(1000, \"tick\");\nFL_ERROR(\"boot failure\");\nSerial.println(\"not rpc\");\nSerial.printf(\"not rpc\");\nfl::println(\"nope\");\nfl::serial_println(\"nope\");\nfl::serial_printf(\"nope\");\n",
        ));
        assert_eq!(result.len(), 9);
    }

    #[test]
    fn autoresearch_runtime_output_allows_serial_setup_helpers() {
        let checker = AutoResearchRuntimeOutputChecker;
        let result = checker.check_file_content(&file(
            "examples/AutoResearch/AutoResearch.ino",
            "fl::serial_begin(115200);\nwhile (!fl::serial_ready()) {}\n",
        ));
        assert!(result.is_empty());
    }

    #[test]
    fn autoresearch_runtime_output_allows_rpc_serial_boundary_only() {
        let checker = AutoResearchRuntimeOutputChecker;
        let result = checker.check_file_content(&file(
            "examples/AutoResearch/AutoResearchRemote.cpp",
            "Serial.println(formatted.c_str());  // ok autoresearch rpc serial - RPC response boundary\n",
        ));
        assert!(result.is_empty());
    }

    #[test]
    fn autoresearch_runtime_output_scope_is_limited() {
        let checker = AutoResearchRuntimeOutputChecker;
        assert!(checker.should_process_file(
            "examples/AutoResearch/AutoResearchRemotePinMethods.cpp",
            Path::new(".")
        ));
        assert!(!checker.should_process_file(
            "examples/AutoResearch/AutoResearchTest.cpp",
            Path::new(".")
        ));
        assert!(checker
            .check_file_content(&file(
                "examples/AutoResearch/AutoResearchTest.cpp",
                "FL_WARN(\"diagnostic-only path\");\n",
            ))
            .is_empty());
    }

    #[test]
    fn autoresearch_runtime_output_ignores_strings_and_comments() {
        let checker = AutoResearchRuntimeOutputChecker;
        let result = checker.check_file_content(&file(
            "examples/AutoResearch/AutoResearch.ino",
            "// FL_WARN(\"comment\")\nconst char* s = \"Serial.println\";\n",
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

    #[test]
    fn rust_lint_source_files_stay_under_one_thousand_lines() {
        fn visit(path: &Path, oversized: &mut Vec<(PathBuf, usize)>) {
            for entry in std::fs::read_dir(path).unwrap() {
                let entry = entry.unwrap();
                let path = entry.path();
                if path.is_dir() {
                    visit(&path, oversized);
                    continue;
                }
                if path.extension().and_then(|ext| ext.to_str()) != Some("rs") {
                    continue;
                }
                let line_count = std::fs::read_to_string(&path).unwrap().lines().count();
                if line_count >= 1_000 {
                    oversized.push((path, line_count));
                }
            }
        }

        let src = Path::new(env!("CARGO_MANIFEST_DIR")).join("src");
        let mut oversized = Vec::new();
        visit(&src, &mut oversized);
        assert!(
            oversized.is_empty(),
            "Rust C++ linter source files must stay under 1,000 lines: {oversized:?}"
        );
    }

    #[test]
    fn selected_checker_filter_keeps_registry_targeted() {
        let selected = HashSet::from(["bare_allocation".to_string()]);
        let checkers = create_checkers(Some(&selected)).unwrap();
        let names: Vec<&str> = checkers.iter().map(|checker| checker.name()).collect();
        assert_eq!(names, vec!["BareAllocationChecker"]);
    }

    #[test]
    fn em_asm_without_clang_format_off_in_wasm_triggers_violation() {
        let checker = EmAsmClangFormatChecker;
        let project_root = std::env::current_dir().unwrap();
        let project_root = project_root
            .ancestors()
            .find(|candidate| candidate.join("src").join("platforms").join("wasm").exists())
            .map(|path| path.to_path_buf())
            .unwrap_or(project_root);
        let path = project_root
            .join("src")
            .join("platforms")
            .join("wasm")
            .join("synthetic_em_asm_test.cpp");
        let path_str = normalize_path(&path.to_string_lossy());
        let content = "#include <emscripten.h>\nvoid foo() {\n    EM_ASM_({ console.log($0); }, 42);\n}\n";

        assert!(checker.should_process_file(&path_str, &project_root));
        let violations = checker.check_file_content(&file(&path_str, content));
        assert_eq!(violations.len(), 1);
        assert_eq!(violations[0].0, 1);
        assert!(violations[0].1.contains("Missing clang-format off"));
    }

    #[test]
    fn em_asm_with_clang_format_off_in_wasm_passes() {
        let checker = EmAsmClangFormatChecker;
        let project_root = std::env::current_dir().unwrap();
        let project_root = project_root
            .ancestors()
            .find(|candidate| candidate.join("src").join("platforms").join("wasm").exists())
            .map(|path| path.to_path_buf())
            .unwrap_or(project_root);
        let path = project_root
            .join("src")
            .join("platforms")
            .join("wasm")
            .join("synthetic_em_asm_test.cpp");
        let path_str = normalize_path(&path.to_string_lossy());
        let content = "// clang-format off\n#include <emscripten.h>\nvoid foo() {\n    EM_ASM_({ console.log($0); }, 42);\n}\n";

        assert!(checker.should_process_file(&path_str, &project_root));
        let violations = checker.check_file_content(&file(&path_str, content));
        assert!(violations.is_empty());
    }

    #[test]
    fn em_asm_outside_wasm_scope_is_ignored() {
        let checker = EmAsmClangFormatChecker;
        let project_root = std::env::current_dir().unwrap();
        let project_root = project_root
            .ancestors()
            .find(|candidate| candidate.join("src").join("fl").exists())
            .map(|path| path.to_path_buf())
            .unwrap_or(project_root);
        let path = project_root
            .join("src")
            .join("fl")
            .join("synthetic_em_asm_test.cpp");
        let path_str = normalize_path(&path.to_string_lossy());
        let content = "void foo() {\n    EM_ASM_({ console.log($0); }, 42);\n}\n";

        // Out of scope: should_process_file must reject src/fl/* paths even
        // though the file contains EM_ASM_ without a clang-format-off marker.
        assert!(!checker.should_process_file(&path_str, &project_root));
        // Defensive: even if a caller invoked check_file_content directly, the
        // checker would still report the violation — guard scoping is enforced
        // by should_process_file alone, which is the contract we just verified.
        assert_eq!(
            checker
                .check_file_content(&file(&path_str, content))
                .len(),
            1
        );
    }

    #[test]
    fn bare_digit_separator_flags_cpp14_literal() {
        let checker = BareDigitSeparatorChecker;
        let project_root = std::env::current_dir().unwrap();
        let path_str = normalize_path("src/fl/synthetic.cpp");
        let content = "u32 x = 999'999'999ULL;\n";
        let violations = checker.check_file_content(&file(&path_str, content));
        assert_eq!(violations.len(), 1);
        assert_eq!(violations[0].0, 1);
        assert!(violations[0].1.contains("digit separator"));
        let _ = project_root; // touch to keep the binding live
    }

    #[test]
    fn bare_digit_separator_skips_comments() {
        let checker = BareDigitSeparatorChecker;
        let path_str = normalize_path("src/fl/synthetic.cpp");
        // Single-line `//` comment + doxygen `///<` comment + block comment all
        // mention `999'999'999` as documentation — none should fire.
        let content = "// Using: (ns * hz + 999'999'999) / 1'000'000'000\n\
                       u32 y = 0; ///< base 80'000'000 Hz\n\
                       /* documenting 9'9 */ u32 z = 1;\n";
        let violations = checker.check_file_content(&file(&path_str, content));
        assert!(violations.is_empty(), "comments must not trigger: {violations:?}");
    }

    #[test]
    fn bare_digit_separator_skips_string_literals_and_suppression() {
        let checker = BareDigitSeparatorChecker;
        let path_str = normalize_path("src/fl/synthetic.cpp");
        // 1) `"don't 9'9"` is inside a string literal — must not fire.
        // 2) Trailing `// ok digit-separator` opts a line out explicitly.
        let content = "const char* s = \"don't 9'9\";\n\
                       u32 q = 1'234ULL; // ok digit-separator\n";
        let violations = checker.check_file_content(&file(&path_str, content));
        assert!(violations.is_empty(), "string + suppression must pass: {violations:?}");
    }

    #[test]
    fn bare_digit_separator_scope_excludes_third_party() {
        let checker = BareDigitSeparatorChecker;
        let project_root = std::env::current_dir().unwrap();
        let third_party_path = normalize_path("src/third_party/foo/bar.cpp");
        assert!(!checker.should_process_file(&third_party_path, &project_root));
        let in_scope = normalize_path("src/fl/example.cpp");
        assert!(checker.should_process_file(&in_scope, &project_root));
    }

    #[test]
    fn lint_violation_json_shape_stays_stable() {
        let violation = LintViolation {
            checker: "ExampleChecker".to_string(),
            path: "src/example.h".to_string(),
            line: 7,
            message: "example message".to_string(),
        };
        let json = serde_json::to_value(&violation).unwrap();
        assert_eq!(json["checker"], "ExampleChecker");
        assert_eq!(json["path"], "src/example.h");
        assert_eq!(json["line"], 7);
        assert_eq!(json["message"], "example message");
    }
}
