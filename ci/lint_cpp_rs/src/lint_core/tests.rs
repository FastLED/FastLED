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
