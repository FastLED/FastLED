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
            "FL_WARN(\"boot chatter\");\nFL_WARN_F_ONCE(\"boot chatter\");\nFL_WARN_LIT(\"boot chatter\");\nFL_PRINT_EVERY(1000, \"tick\");\nFL_PRINT_F(\"tick\");\nFL_PRINTLN(\"tick\");\nFL_ERROR(\"boot failure\");\nSerial.println(\"not rpc\");\nSerial.printf(\"not rpc\");\nSerial.printHex(\"not rpc\");\nfl::println(\"nope\");\nfl::write_bytes(bytes, len);\nfl::Serial.println(\"nope\");\nfl::Serial.printHex(\"nope\");\nfl::serial_print(\"nope\");\nfl::serial_println(\"nope\");\nfl::serial_printf(\"nope\");\nfl::serial_write(bytes, len);\nfl::serialPrintln(\"nope\");\nfl::serialWrite(bytes, len);\n",
        ));
        assert_eq!(result.len(), 20);
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
    fn autoresearch_runtime_output_marker_scope_can_protect_driver_cpp_hpp() {
        let checker = AutoResearchRuntimeOutputChecker;
        let path = "src/platforms/arm/teensy/teensy4_common/example.cpp.hpp";
        assert!(checker.should_process_file(path, Path::new(".")));
        let result = checker.check_file_content(&file(
            path,
            "// autoresearch-runtime-output-lint: begin\n\
FL_WARN(\"driver chatter\");\n\
FL_PRINT(\"driver chatter\");\n\
Serial.printX(\"driver chatter\");\n\
fl::serialPrintln(\"driver chatter\");\n\
fl::serial_write(bytes, len);\n\
// autoresearch-runtime-output-lint: end\n",
        ));
        assert_eq!(result.len(), 5);
    }

    #[test]
    fn autoresearch_runtime_output_ignores_unmarked_non_autoresearch_files() {
        let checker = AutoResearchRuntimeOutputChecker;
        let result = checker.check_file_content(&file(
            "src/platforms/arm/teensy/teensy4_common/example.cpp.hpp",
            "FL_WARN(\"allowed outside an explicit protected span\");\n",
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
        assert!(checker.should_process_file(
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
    fn autoresearch_runtime_output_marker_scope_flags_test_time_sections() {
        let checker = AutoResearchRuntimeOutputChecker;
        let result = checker.check_file_content(&file(
            "examples/AutoResearch/AutoResearchTest.cpp",
            "FL_WARN(\"legacy diagnostic outside guarded section\");\n\
// autoresearch-runtime-output-lint: begin\n\
FL_WARN(\"test-time chatter\");\n\
Serial.println(\"test-time chatter\");\n\
fl::serial_println(\"test-time chatter\");\n\
// autoresearch-runtime-output-lint: end\n\
FL_WARN(\"legacy diagnostic outside guarded section\");\n",
        ));
        assert_eq!(result.len(), 3);
    }

    #[test]
    fn autoresearch_runtime_output_marker_scope_restores_always_checked_files() {
        let checker = AutoResearchRuntimeOutputChecker;
        let result = checker.check_file_content(&file(
            "examples/AutoResearch/AutoResearchRemoteRunSingleTest.cpp",
            "// autoresearch-runtime-output-lint: end\n\
FL_WARN(\"still checked because remote files are always guarded\");\n",
        ));
        assert_eq!(result.len(), 1);
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

    // ---- SingletonElisionChecker (FastLED#3486) ----

    fn singleton_elision_violations(src: &str) -> Vec<(usize, String)> {
        SingletonElisionChecker.check_file_content(&file("src/fl/probe.cpp.hpp", src))
    }

    #[test]
    fn singleton_elision_accepts_suffixed_and_prefixed_literals() {
        // Rust's numeric parsers reject BOTH the `0x`/`0b` prefixes and the
        // C++ `u`/`l`/`f` suffixes, so a parse-based triviality test
        // misreports register masks as non-literal. Cases mirror
        // TRIVIAL_RHS_RE in ci/scan_singleton_elision.py.
        for rhs in [
            "100.0f", "10000.0f", "64u", "1uLL", "64", "-3",
            "0x80000000u", "0xFF", "0b1010", "0x8000uL",
            "nullptr", "NULL", "true", "false",
            "\"text\"", "'c'", "MAX_LEDS", "(1u << 3)", "(0x1u << 12)",
            // Exponent / trailing-dot / long-double float spellings. FastLED
            // timing and gamma tables use these freely; missing them keeps
            // the warn-only stream noisy and #3492 (hard-fail at zero)
            // unreachable.
            "1e-6f", "2.5e3f", "10.f", "1.5L", "1E9",
        ] {
            let src = format!("namespace fl {{\nconstexpr auto kX = {rhs};\n}}\n");
            assert!(
                singleton_elision_violations(&src).is_empty(),
                "expected `{rhs}` to be treated as a trivial literal",
            );
        }
    }

    #[test]
    fn singleton_elision_still_flags_non_literal_constexpr() {
        // Suffix/prefix tolerance must not swallow real initializers.
        // Only expression RHS forms are pinned here. `compute()` and
        // `Table{1, 2, 3}` are deliberately absent: the checker skips lines
        // containing `(` as probable function declarations, and `{` opens a
        // brace depth so the decl no longer reads as namespace-scope. Both
        // predate this change and are not what these cases pin down.
        for rhs in ["kMin * 2.0f", "kOther + 1", "0xFF + 1"] {
            let src = format!("namespace fl {{\nconstexpr auto kX = {rhs};\n}}\n");
            assert_eq!(
                singleton_elision_violations(&src).len(),
                1,
                "expected `{rhs}` to stay flagged",
            );
        }
    }

    #[test]
    fn singleton_elision_honors_allow_global_marker() {
        let src = "namespace fl {\n\
                   // FL_LINT_ALLOW_GLOBAL(deliberate)\n\
                   static int sCounter = 0;\n\
                   }\n";
        assert!(singleton_elision_violations(src).is_empty());
        assert!(prefer_constexpr_violations(src).is_empty());

        // Unannotated, this sample is an integral type bound to a literal, so
        // after FastLED#3483 it belongs to PreferConstexprChecker rather than
        // SingletonElisionChecker. Assert the partition directly: exactly one
        // rule owns it, never both and never neither.
        let unannotated = "namespace fl {\nstatic int sCounter = 0;\n}\n";
        let singleton_hits = singleton_elision_violations(unannotated).len();
        let constexpr_hits = prefer_constexpr_violations(unannotated).len();
        assert_eq!(
            singleton_hits + constexpr_hits,
            1,
            "expected exactly one rule to claim the declaration \
             (singleton={singleton_hits}, constexpr={constexpr_hits})"
        );
        assert_eq!(
            constexpr_hits, 1,
            "an integer constant belongs to PreferConstexpr"
        );
    }

    // ---- PreferConstexprChecker (FastLED#3483) ----
    //
    // The tree currently has ZERO instances of this pattern -- every case the
    // issue cited (`int PRIORITY_*`, `const int kBassStart`) had already been
    // converted to `constexpr` by the time the rule was written. So these
    // tests are the whole evidence that the rule fires at all, and that it
    // partitions cleanly against SingletonElisionChecker rather than
    // shadowing it.

    fn prefer_constexpr_violations(src: &str) -> Vec<(usize, String)> {
        PreferConstexprChecker.check_file_content(&file("src/fl/probe.cpp.hpp", src))
    }

    #[test]
    fn prefer_constexpr_flags_plain_integer_constant() {
        // The motivating shape from FastLED#3483.
        let src = "namespace fl {
int PRIORITY_PARLIO = 3;
}
";
        let hits = prefer_constexpr_violations(src);
        assert_eq!(hits.len(), 1, "expected the integer constant to be flagged");
        assert!(
            hits[0].1.contains("constexpr"),
            "message must name the fix, got: {}",
            hits[0].1
        );
        // ...and the Singleton rule must NOT also claim it, or one
        // declaration carries two contradictory fixes.
        assert!(
            singleton_elision_violations(src).is_empty(),
            "PreferConstexpr candidates must be excluded from SingletonElision"
        );
    }

    #[test]
    fn prefer_constexpr_flags_const_int() {
        let src = "namespace fl {
const int kBassStart = 20;
}
";
        assert_eq!(prefer_constexpr_violations(src).len(), 1);
        assert!(singleton_elision_violations(src).is_empty());
    }

    #[test]
    fn prefer_constexpr_accepts_hex_and_shift_literals() {
        // No parenthesised forms here: the shared scanner skips any line with
        // `(` before `;` as a probable function declaration, so `(1u << 3)`
        // never reaches either rule. Pre-existing caveat, same one noted in
        // singleton_elision_still_flags_non_literal_constexpr.
        for rhs in ["0xFF", "0b1010", "64u", "-3", "true"] {
            let src = format!("namespace fl {{
int kX = {rhs};
}}
");
            assert_eq!(
                prefer_constexpr_violations(&src).len(),
                1,
                "expected `{rhs}` to read as an integer literal",
            );
        }
    }

    #[test]
    fn prefer_constexpr_ignores_mutable_state_that_merely_starts_at_a_literal() {
        // `bool sInitialized = false;` matches "integral type bound to an
        // integer literal" exactly like a real constant does. Advising
        // `constexpr` here does not compile -- and it would also steal the
        // declaration from SingletonElisionChecker, where mutable driver
        // state belongs. This case is why the rule scans for assignments.
        let src = "namespace fl {
                   bool sInitialized = false;
                   void init() { sInitialized = true; }
                   }
";
        assert!(
            prefer_constexpr_violations(src).is_empty(),
            "assigned-later state must not be called a constant"
        );
        // It stays with the Singleton rule -- not dropped by both.
        assert_eq!(singleton_elision_violations(src).len(), 1);
    }

    #[test]
    fn prefer_constexpr_ignores_compound_assignment_and_increment() {
        for mutation in ["sCount += 1;", "sCount++;", "sCount |= 2;"] {
            let src = format!(
                "namespace fl {{
int sCount = 0;
void bump() {{ {mutation} }}
}}
"
            );
            assert!(
                prefer_constexpr_violations(&src).is_empty(),
                "`{mutation}` marks sCount mutable",
            );
        }
    }

    #[test]
    fn prefer_constexpr_does_not_confuse_comparison_with_assignment() {
        // `==` is a read, not a write; the constant must stay flagged.
        let src = "namespace fl {
                   int kMode = 2;
                   bool isMode() { return kMode == 2; }
                   }
";
        assert_eq!(prefer_constexpr_violations(src).len(), 1);
    }

    #[test]
    fn prefer_constexpr_matches_whole_identifiers_only() {
        // Assigning `kModeExtra` must not mark `kMode` mutable.
        let src = "namespace fl {
                   int kMode = 2;
                   void f() { kModeExtra = 5; }
                   }
";
        assert_eq!(prefer_constexpr_violations(src).len(), 1);
    }

    #[test]
    fn prefer_constexpr_leaves_non_integral_types_to_singleton() {
        // Pointers, handles and volatile hardware state are not constants.
        for decl in [
            "QueueHandle_t s_queue = 0;",
            "void *pMatrix = nullptr;",
            "volatile u32 g_millis = 0;",
            "float kGain = 1.5f;",
        ] {
            let src = format!("namespace fl {{
{decl}
}}
");
            assert!(
                prefer_constexpr_violations(&src).is_empty(),
                "`{decl}` is not an integer constant",
            );
            assert_eq!(
                singleton_elision_violations(&src).len(),
                1,
                "`{decl}` must still be reported by SingletonElision",
            );
        }
    }

    #[test]
    fn prefer_constexpr_skips_already_constexpr() {
        let src = "namespace fl {
constexpr int kX = 5;
}
";
        assert!(prefer_constexpr_violations(src).is_empty());
        // Fixing a hit silences BOTH rules -- the issue's acceptance criterion.
        assert!(singleton_elision_violations(src).is_empty());
    }

    #[test]
    fn prefer_constexpr_honors_allow_global_marker() {
        let src = "namespace fl {
                   // FL_LINT_ALLOW_GLOBAL(deliberate)
                   int kX = 5;
                   }
";
        assert!(prefer_constexpr_violations(src).is_empty());
    }

    #[test]
    fn singleton_elision_ignores_third_party() {
        let checker = SingletonElisionChecker;
        let project_root = std::env::current_dir().unwrap();
        let project_root = project_root
            .ancestors()
            .find(|candidate| candidate.join("src").join("fl").exists())
            .map(|path| path.to_path_buf())
            .unwrap_or(project_root);

        let third_party = normalize_path(
            &project_root
                .join("src")
                .join("third_party")
                .join("vendor.cpp")
                .to_string_lossy(),
        );
        assert!(!checker.should_process_file(&third_party, &project_root));

        let ours = normalize_path(
            &project_root
                .join("src")
                .join("fl")
                .join("probe.cpp.hpp")
                .to_string_lossy(),
        );
        assert!(checker.should_process_file(&ours, &project_root));

        // Headers are StaticInHeaderChecker's job, not this checker's.
        let header = normalize_path(
            &project_root.join("src").join("fl").join("probe.h").to_string_lossy(),
        );
        assert!(!checker.should_process_file(&header, &project_root));
    }

    // ---- FastLED#3287: container raw-pointer checkers ----

    #[test]
    fn container_non_contiguous_flags_data_call() {
        let checker = ContainerNonContiguousPtrChecker;
        let hits = checker.check_file_content(&file(
            "src/fl/example.cpp",
            "fl::deque<u8> chunks;\nconst u8* raw = chunks.data();\n",
        ));
        assert_eq!(hits.len(), 1);
        assert_eq!(hits[0].0, 2);
        assert!(hits[0].1.contains("non-contiguous"));
    }

    #[test]
    fn container_non_contiguous_flags_element_addresses() {
        let checker = ContainerNonContiguousPtrChecker;
        let hits = checker.check_file_content(&file(
            "src/fl/example.cpp",
            concat!(
                "fl::deque<u8> chunks;\n",
                "auto* a = &chunks[0];\n",
                "auto* b = &chunks.at(1);\n",
                "auto* c = &chunks.front();\n",
                "auto* d = &chunks.back();\n",
                "fl::circular_buffer<int> ring;\n",
                "auto* e = &ring[2];\n",
            ),
        ));
        assert_eq!(hits.len(), 5);
        assert_eq!(
            hits.iter().map(|hit| hit.0).collect::<Vec<_>>(),
            vec![2, 3, 4, 5, 7]
        );
    }

    #[test]
    fn container_non_contiguous_ignores_contiguous_containers() {
        let checker = ContainerNonContiguousPtrChecker;
        let hits = checker.check_file_content(&file(
            "src/fl/example.cpp",
            "fl::vector<u8> bytes;\nconst u8* raw = bytes.data();\nauto* first = &bytes[0];\n",
        ));
        assert!(hits.is_empty());
    }

    #[test]
    fn container_element_address_allows_data_on_contiguous() {
        let checker = ContainerElementAddressChecker;
        let hits = checker.check_file_content(&file(
            "src/fl/example.cpp",
            "fl::vector<u8> bytes;\nconst u8* raw = bytes.data();\n",
        ));
        assert!(hits.is_empty());
    }

    #[test]
    fn container_element_address_flags_contiguous_element_address() {
        let checker = ContainerElementAddressChecker;
        let hits = checker.check_file_content(&file(
            "src/fl/example.cpp",
            "fl::vector<Group> groups;\nGroup* found = &groups[i];\n",
        ));
        assert_eq!(hits.len(), 1);
        assert_eq!(hits[0].0, 2);
        assert!(hits[0].1.contains("fl::span"));
    }

    #[test]
    fn container_element_address_flags_member_declared_below_use() {
        // Class members are declared at the bottom of the class, below the
        // accessors that use them -- the declaration scan must be whole-file.
        let checker = ContainerElementAddressChecker;
        let hits = checker.check_file_content(&file(
            "src/platforms/shared/mock/peripheral_mock.h",
            concat!(
                "class Mock {\n",
                "  public:\n",
                "    const Record* last() const {\n",
                "        return &mRecords[mRecords.size() - 1];\n",
                "    }\n",
                "  private:\n",
                "    fl::vector<Record> mRecords;\n",
                "};\n",
            ),
        ));
        assert_eq!(hits.len(), 1);
        assert_eq!(hits[0].0, 4);
    }

    #[test]
    fn container_ptr_follows_member_qualified_receivers() {
        // Regression guard (CodeRabbit on #3759). The scanner originally
        // parsed one identifier after `&` and bailed when the next token was
        // a `.`/`->` whose member was not at/front/back -- so `&this->mQueue[0]`
        // and `&obj.mQueue[0]` were silently skipped. Members are the common
        // shape for fl::deque FIELDS, which is exactly what the whole-file
        // declaration scan exists to catch, so this was a hole in the
        // HARD-FAIL tier, not just the warn tier.
        let non_contiguous = ContainerNonContiguousPtrChecker;
        let contiguous = ContainerElementAddressChecker;
        let source = concat!(
            "fl::deque<int> mQueue;
",
            "fl::vector<int> mVec;
",
            "int* a = &this->mQueue[0];
",
            "int* b = &obj.mQueue[0];
",
            "int* c = &this->mQueue.front();
",
            "int* d = &state.inner.mQueue[2];
",
            "int* e = &this->mVec[0];
",
            "int* f = this->mVec.data();
",
        );
        let hard = non_contiguous.check_file_content(&file("src/fl/example.cpp", source));
        assert_eq!(hard.len(), 4, "expected 4 hard-fail hits, got {hard:?}");

        let warn = contiguous.check_file_content(&file("src/fl/example.cpp", source));
        // Only `&this->mVec[0]`. `.data()` on a contiguous container is fine.
        assert_eq!(warn.len(), 1, "expected 1 warn hit, got {warn:?}");
    }

    #[test]
    fn container_ptr_does_not_match_logical_and_operator() {
        // A naive `&\s*(\w+)\s*\[` pattern matches the second `&` of `&&`
        // and produced 7 false positives out of 11 during development.
        let non_contiguous = ContainerNonContiguousPtrChecker;
        let contiguous = ContainerElementAddressChecker;
        let source = concat!(
            "fl::deque<u8> annexB;\n",
            "fl::vector<u8> a;\n",
            "fl::vector<u8> b;\n",
            "if (annexB[i] == 0 && annexB[i + 1] == 0) { return; }\n",
            "if (ok && b[i]) { return; }\n",
            "if (a[i] && b[j]) { return; }\n",
            "u8 masked = flags & a[i];\n",
        );
        assert!(non_contiguous
            .check_file_content(&file("src/fl/example.cpp", source))
            .is_empty());
        assert!(contiguous
            .check_file_content(&file("src/fl/example.cpp", source))
            .is_empty());
    }

    #[test]
    fn container_ptr_honors_inline_suppression() {
        let checker = ContainerNonContiguousPtrChecker;
        let hits = checker.check_file_content(&file(
            "src/fl/example.cpp",
            "fl::deque<u8> chunks;\nauto* p = chunks.data();  // fl-lint: container-data-ptr-ok legacy C API\n",
        ));
        assert!(hits.is_empty());
    }

    #[test]
    fn container_ptr_honors_preceding_line_suppression() {
        let checker = ContainerElementAddressChecker;
        let hits = checker.check_file_content(&file(
            "src/fl/example.cpp",
            "fl::vector<u8> bytes;\n// fl-lint: container-data-ptr-ok legacy C API\nauto* p = &bytes[0];\n",
        ));
        assert!(hits.is_empty());
    }

    #[test]
    fn container_ptr_ignores_undeclared_and_raw_arrays() {
        let non_contiguous = ContainerNonContiguousPtrChecker;
        let contiguous = ContainerElementAddressChecker;
        // `lane_waves` is a raw C array with no fl:: declaration in view;
        // issue #3287 scopes raw arrays out explicitly.
        let source = concat!(
            "fl::vector<u8> anchor;\n",
            "u8 lane_waves[8];\n",
            "memcpy(dst, &lane_waves[0], 8);\n",
            "auto* p = unknown.data();\n",
        );
        assert!(non_contiguous
            .check_file_content(&file("src/fl/example.cpp", source))
            .is_empty());
        assert!(contiguous
            .check_file_content(&file("src/fl/example.cpp", source))
            .is_empty());
    }

    #[test]
    fn container_ptr_ignores_comments_and_string_literals() {
        let checker = ContainerNonContiguousPtrChecker;
        let hits = checker.check_file_content(&file(
            "src/fl/example.cpp",
            concat!(
                "fl::deque<u8> chunks;\n",
                "// auto* p = chunks.data();\n",
                "/* auto* q = &chunks[0]; */\n",
                "const char* msg = \"chunks.data()\";\n",
            ),
        ));
        assert!(hits.is_empty());
    }

    #[test]
    fn container_ptr_drops_names_shadowed_by_raw_arrays() {
        // tests/fl/slice.cpp declares `fl::array<int, 4> arr` and
        // `int arr[] = {5, 6, 7}` in sibling FL_SUBCASE blocks. Name matching
        // is per-file, not scope-aware, so an ambiguous name must be dropped.
        let checker = ContainerElementAddressChecker;
        let hits = checker.check_file_content(&file(
            "tests/fl/slice.cpp",
            concat!(
                "fl::array<int, 4> arr = {7, 8, 9, 10};\n",
                "int arr[] = {5, 6, 7};\n",
                "span<int, 3> s(&arr[0], 3);\n",
            ),
        ));
        assert!(hits.is_empty());
    }

    #[test]
    fn container_ptr_skips_third_party() {
        assert!(!ContainerNonContiguousPtrChecker
            .should_process_file("src/third_party/foo.h", Path::new(".")));
        assert!(!ContainerElementAddressChecker
            .should_process_file("src/third_party/foo.h", Path::new(".")));
        assert!(ContainerNonContiguousPtrChecker
            .should_process_file("src/fl/deque.h", Path::new(".")));
    }

    #[test]
    fn r1_cleanup_flags_dirty_early_return() {
        let hits = R1CleanupChecker.check_file_content(&file(
            "src/example.cpp",
            concat!(
                "u8 convert(bool stop) {\n",
                "  u8 value = scale8_LEAVING_R1_DIRTY(1, 2);\n",
                "  if (stop) { return value; }\n",
                "  cleanup_R1();\n",
                "  return value;\n",
                "}\n",
            ),
        ));
        assert_eq!(hits.len(), 1);
        assert_eq!(hits[0].0, 3);
    }

    #[test]
    fn r1_cleanup_tracks_branch_that_skips_cleanup() {
        let hits = R1CleanupChecker.check_file_content(&file(
            "src/example.cpp",
            concat!(
                "void convert(bool scale, bool clean) {\n",
                "  if (scale) value = scale8_video_LEAVING_R1_DIRTY(value, value);\n",
                "  if (clean) { cleanup_R1(); }\n",
                "}\n",
            ),
        ));
        assert_eq!(hits.len(), 1);
        assert_eq!(hits[0].0, 4);
    }

    #[test]
    fn r1_cleanup_accepts_cleanup_on_every_branch() {
        let hits = R1CleanupChecker.check_file_content(&file(
            "src/example.cpp",
            concat!(
                "u8 convert(bool zero) {\n",
                "  u8 value = scale8_LEAVING_R1_DIRTY(1, 2);\n",
                "  if (zero) { cleanup_R1(); return 0; }\n",
                "  else { cleanup_R1(); return value; }\n",
                "}\n",
            ),
        ));
        assert!(hits.is_empty());
    }

    #[test]
    fn r1_cleanup_flags_dirty_loop_iteration() {
        let hits = R1CleanupChecker.check_file_content(&file(
            "src/example.cpp",
            concat!(
                "void convert(int count) {\n",
                "  for (int i = 0; i < count; ++i) {\n",
                "    value = scale8_LEAVING_R1_DIRTY(value, 2);\n",
                "  }\n",
                "}\n",
            ),
        ));
        assert_eq!(hits.len(), 1);
        assert_eq!(hits[0].0, 5);
    }

    #[test]
    fn r1_cleanup_ignores_comments_and_strings() {
        let hits = R1CleanupChecker.check_file_content(&file(
            "src/example.cpp",
            concat!(
                "void convert() {\n",
                "  // scale8_LEAVING_R1_DIRTY(value, 2);\n",
                "  const char* text = \"scale8_LEAVING_R1_DIRTY\";\n",
                "}\n",
            ),
        ));
        assert!(hits.is_empty());
    }

    #[test]
    fn r1_cleanup_tracks_calls_in_conditions() {
        let hits = R1CleanupChecker.check_file_content(&file(
            "src/example.cpp",
            concat!(
                "void convert() {\n",
                "  if (scale8_LEAVING_R1_DIRTY(value, 2)) { return; }\n",
                "  cleanup_R1();\n",
                "}\n",
            ),
        ));
        assert_eq!(hits.len(), 1);
        assert_eq!(hits[0].0, 2);
    }

    #[test]
    fn r1_cleanup_flags_dirty_control_transfer() {
        let hits = R1CleanupChecker.check_file_content(&file(
            "src/example.cpp",
            concat!(
                "void convert(int count) {\n",
                "  for (int i = 0; i < count; ++i) {\n",
                "    value = scale8_LEAVING_R1_DIRTY(value, 2);\n",
                "    if (value == 0) continue;\n",
                "    cleanup_R1();\n",
                "  }\n",
                "}\n",
            ),
        ));
        assert_eq!(hits.len(), 1);
        assert_eq!(hits[0].0, 7);
    }

}
