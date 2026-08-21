// Second half of the `mod tests` body from tests.rs.
//
// Textually included into that module, so it shares its `use super::*;`
// and helper fns and must NOT declare a module of its own. Split purely
// to satisfy the 1,000-line-per-file ceiling the linter enforces on
// itself.

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

    // ---- FastLED#3946: visible code around block comments ----

    #[test]
    fn bare_noinline_handles_block_comments() {
        let checker = BareNoInlineChecker;
        let hits = checker.check_file_content(&file(
            "src/fl/example.cpp",
            concat!(
                "__attribute__((noinline)) void before(); /* comment starts\n",
                "still commented __attribute__((noinline))\n",
                "*/ __attribute__((noinline)) void after();\n",
                "/* __attribute__((noinline)) void hidden(); */\n",
                "__attribute__((noinline)) void suppressed(); // ok noinline\n",
                "// documentation mentions /* without opening a comment\n",
                "const char *marker = \"/*\";\n",
                "__attribute__((noinline)) void after_literals();\n",
            ),
        ));
        assert_eq!(
            hits.iter().map(|hit| hit.0).collect::<Vec<_>>(),
            vec![1, 3, 8]
        );
    }

    #[test]
    fn bare_snprintf_handles_block_comments() {
        let checker = BareSnprintfChecker;
        let hits = checker.check_file_content(&file(
            "src/fl/example.cpp",
            concat!(
                "snprintf(buf, sizeof(buf), \"%d\", value); /* comment starts\n",
                "still commented snprintf(buf, 1, \"x\");\n",
                "*/ snprintf(buf, sizeof(buf), \"%d\", value);\n",
                "/* snprintf(buf, sizeof(buf), \"%d\", value); */\n",
                "snprintf(buf, sizeof(buf), \"%d\", value); // ok snprintf\n",
            ),
        ));
        assert_eq!(
            hits.iter().map(|hit| hit.0).collect::<Vec<_>>(),
            vec![1, 3]
        );
    }

    #[test]
    fn legacy_log_macro_handles_block_comments() {
        let checker = LegacyLogMacroChecker;
        let hits = checker.check_file_content(&file(
            "src/fl/example.cpp",
            concat!(
                "FL_WARN(\"before\"); /* comment starts\n",
                "still commented FL_WARN(\"hidden\");\n",
                "*/ FL_ERROR(\"after\");\n",
                "/* FL_DBG(\"hidden\"); */\n",
                "#define FL_WARN(...)\n",
            ),
        ));
        assert_eq!(
            hits.iter().map(|hit| hit.0).collect::<Vec<_>>(),
            vec![1, 3]
        );
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

    // ---- comment scanner literal state across physical lines (FastLED#3960) ----

    /// Run the scanner over a whole file the way every caller does, carrying
    /// state between physical lines.
    fn scan_visible(src: &str) -> Vec<String> {
        let mut state = CommentScanState::default();
        src.lines()
            .map(|line| strip_block_comments_from_line(line, &mut state))
            .collect()
    }

    #[test]
    fn scanner_keeps_code_after_a_raw_string_holding_comment_text() {
        // The regression: `/*` inside a multiline raw string used to set
        // in_block_comment, so every later line was stripped from checker
        // input -- a silent false negative.
        let src = concat!(
            "const char* kDoc = R@@(
",
            "  /* not a comment, just text
",
            ")@@;
",
            "int visible_after = 1;
",
        )
        .replace("@@", "\"");
        let visible = scan_visible(&src);
        assert!(
            visible.last().unwrap().contains("visible_after"),
            "code after a raw string must stay visible, got {visible:?}"
        );
    }

    #[test]
    fn scanner_handles_a_delimited_raw_string() {
        let src = concat!(
            "auto s = R@@delim(
",
            "  /* looks like a block comment but is raw-string text
",
            ")delim@@;
",
            "int after_delim = 2;
",
        )
        .replace("@@", "\"");
        let visible = scan_visible(&src);
        assert!(visible.last().unwrap().contains("after_delim"), "{visible:?}");
    }

    #[test]
    fn scanner_ignores_a_closing_delimiter_that_does_not_match() {
        // `)other"` must NOT close an `R"delim(` raw string.
        let src = concat!(
            "auto s = R@@delim(
",
            ")other@@
",
            "  /* only a comment if the raw string wrongly closed above
",
            ")delim@@;
",
            "int after_mismatch = 3;
",
        )
        .replace("@@", "\"");
        let visible = scan_visible(&src);
        assert!(visible.last().unwrap().contains("after_mismatch"), "{visible:?}");
    }

    #[test]
    fn scanner_carries_a_backslash_continued_string() {
        // A string continued with a trailing backslash keeps running; `/*`
        // inside it is data.
        let src = "const char* k = @@abc \\
  /* still string@@;
int after_cont = 4;
"
            .replace("@@", "\"")
            .replace("\\", &"\\".to_string());
        let visible = scan_visible(&src);
        assert!(visible.last().unwrap().contains("after_cont"), "{visible:?}");
    }

    #[test]
    fn scanner_still_strips_real_block_comments_across_lines() {
        let src = "int a = 1; /* open
hidden line
*/ int b = 2;
";
        let visible = scan_visible(src);
        assert!(visible[0].contains("int a = 1;"));
        assert!(!visible[1].contains("hidden"), "{visible:?}");
        assert!(visible[2].contains("int b = 2;"), "{visible:?}");
    }

    #[test]
    fn scanner_does_not_open_a_block_comment_inside_a_line_comment() {
        // This helper intentionally leaves `//` text in its output -- callers
        // use split_line_comment for that. What it must do is stop *scanning*
        // at `//`, so a `/*` there cannot swallow the following lines.
        let visible = scan_visible("int a = 1; // /* not a real open
int b = 2;
");
        assert!(visible[1].contains("int b = 2;"), "{visible:?}");
    }

    #[test]
    fn scanner_keeps_char_literals_intact() {
        let visible = scan_visible("char c = 'x'; /* g */ int d = 2;
");
        assert!(visible[0].contains("char c = 'x';"), "{visible:?}");
        assert!(visible[0].contains("int d = 2;"), "{visible:?}");
        assert!(!visible[0].contains(" g "), "{visible:?}");
    }

    #[test]
    fn scanner_does_not_treat_identifier_r_as_a_raw_string() {
        // `MYR"..."` is an ordinary string, not a raw string.
        let src = "auto s = MYR@@plain@@; /* c */ int after_ident = 5;
".replace("@@", "\"");
        let visible = scan_visible(&src);
        assert!(visible[0].contains("after_ident"), "{visible:?}");
    }

