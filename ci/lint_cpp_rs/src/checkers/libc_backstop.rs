// --- LibcBackstopChecker (structural pass) -----------------------------------
//
// FastLED #4838. `tests/fl/stl/adl_barrier.cpp` is the compile back-stop for
// LibcMirrorChecker: it calls every fl libc wrapper unqualified, so a wrapper
// that ties with libc fails to compile. That only works if the test calls
// every wrapper, so this pass keeps the two in lock-step:
//
//   * every `template <typename FlLibcTag = void>` wrapper under src/ is called
//     in the test's `libc_every_using_fl` block (under `using namespace fl`);
//   * every wrapper with a `void*` parameter is also called in the
//     `libc_every_adl` block (CRGB* arguments, no using-directive). Only a
//     `void*` parameter accepts an fl-typed pointer, so only those wrappers
//     are reachable through ADL;
//   * the `libc_every_adl` block must not contain a using-directive;
//   * neither block calls a libc name that is no longer an fl wrapper.

const LIBC_BACKSTOP_TEST: &str = "tests/fl/stl/adl_barrier.cpp";
const LIBC_BACKSTOP_ADL_BLOCK: &str = "libc_every_adl";
const LIBC_BACKSTOP_USING_BLOCK: &str = "libc_every_using_fl";

/// `(name, has_void_pointer_parameter)` for every FlLibcTag wrapper in `src`.
fn libc_backstop_wrappers(src: &str) -> Vec<(String, bool)> {
    let toks = libc_mirror_tokens(&libc_mirror_mask(src));
    let head: Vec<&str> = LIBC_MIRROR_TEMPLATE_HEAD.split(' ').collect();
    let mut out = Vec::new();
    let mut i = 0;
    while i + head.len() <= toks.len() {
        let matches_head = toks[i..i + head.len()].iter().zip(&head).all(|(t, h)| t.text == *h);
        if !matches_head {
            i += 1;
            continue;
        }
        // The wrapper name is the first non-macro identifier followed by `(`;
        // function-like macros and `decltype(...)` in the prefix are skipped.
        let mut j = i + head.len();
        while j < toks.len() && toks[j].text != ";" && toks[j].text != "{" {
            if toks[j].text == "(" {
                let callee = &toks[j - 1].text;
                let is_macro = callee == "decltype"
                    || callee.chars().all(|c| c.is_ascii_uppercase() || c.is_ascii_digit() || c == '_');
                if !is_macro {
                    break;
                }
                let mut depth = 0usize;
                while j < toks.len() {
                    match toks[j].text.as_str() {
                        "(" => depth += 1,
                        ")" => {
                            depth -= 1;
                            if depth == 0 {
                                break;
                            }
                        }
                        _ => {}
                    }
                    j += 1;
                }
            }
            j += 1;
        }
        if j < toks.len() && toks[j].text == "(" && j > 0 && toks[j - 1].kind == LibcTokKind::Ident {
            let name = toks[j - 1].text.clone();
            let mut depth = 0usize;
            let mut has_pointer = false;
            let mut k = j;
            while k < toks.len() {
                match toks[k].text.as_str() {
                    "(" => depth += 1,
                    ")" => {
                        depth -= 1;
                        if depth == 0 {
                            break;
                        }
                    }
                    // Exactly `void*` at the top parameter level: `void**` and
                    // function-pointer parameters do not accept a CRGB*.
                    "*" if depth == 1
                        && toks[k - 1].text == "void"
                        && toks.get(k + 1).is_some_and(|t| t.text != "*") =>
                    {
                        has_pointer = true
                    }
                    _ => {}
                }
                k += 1;
            }
            out.push((name, has_pointer));
        }
        i = j.max(i + 1);
    }
    out
}

/// Tokens of `namespace <block> { ... }` in the masked test source, or None.
fn libc_backstop_block(toks: &[LibcTok], block: &str) -> Option<Vec<(String, String)>> {
    let start = toks
        .windows(3)
        .position(|w| w[0].text == "namespace" && w[1].text == block && w[2].text == "{")?;
    let mut depth = 0usize;
    let mut out = Vec::new();
    for k in start + 2..toks.len() {
        match toks[k].text.as_str() {
            "{" => depth += 1,
            "}" => {
                depth -= 1;
                if depth == 0 {
                    return Some(out);
                }
            }
            _ => {}
        }
        let next = toks.get(k + 1).map_or("", |t| t.text.as_str());
        out.push((toks[k].text.clone(), next.to_string()));
    }
    None
}

/// Violation messages for the given wrappers against the back-stop test source.
fn libc_backstop_violations(wrappers: &[(String, bool)], test_src: &str) -> Vec<String> {
    let toks = libc_mirror_tokens(&libc_mirror_mask(test_src));
    let mut out = Vec::new();
    let mut blocks = Vec::new();
    for block in [LIBC_BACKSTOP_ADL_BLOCK, LIBC_BACKSTOP_USING_BLOCK] {
        match libc_backstop_block(&toks, block) {
            Some(b) => blocks.push(b),
            None => {
                out.push(format!("{LIBC_BACKSTOP_TEST}: missing `namespace {block} {{ ... }}` back-stop block"));
                return out;
            }
        }
    }
    let called = |b: &[(String, String)], name: &str| b.iter().any(|(t, n)| t == name && n == "(");
    let (adl, using_fl) = (&blocks[0], &blocks[1]);
    if adl.iter().any(|(t, _)| t == "using") {
        out.push(format!(
            "{LIBC_BACKSTOP_TEST}: `{LIBC_BACKSTOP_ADL_BLOCK}` must not contain `using`: it tests the ADL path"
        ));
    }
    let mut names: Vec<&(String, bool)> = wrappers.iter().collect();
    names.sort();
    names.dedup();
    for (name, has_pointer) in names {
        if !called(using_fl, name) {
            out.push(format!(
                "{LIBC_BACKSTOP_TEST}: fl::{name} wrapper is not called unqualified in `{LIBC_BACKSTOP_USING_BLOCK}`"
            ));
        }
        if *has_pointer && !called(adl, name) {
            out.push(format!(
                "{LIBC_BACKSTOP_TEST}: fl::{name} takes a void* but is not called unqualified with fl-typed \
                 arguments in `{LIBC_BACKSTOP_ADL_BLOCK}`"
            ));
        }
    }
    for (block_name, block) in [(LIBC_BACKSTOP_ADL_BLOCK, adl), (LIBC_BACKSTOP_USING_BLOCK, using_fl)] {
        let mut stale: Vec<&str> = block
            .iter()
            .filter(|(t, n)| {
                n == "("
                    && (LIBC_RESERVED_NAMES.contains(&t.as_str()) || LIBC_ARITHMETIC_NAMES.contains(&t.as_str()))
            })
            .map(|(t, _)| t.as_str())
            .filter(|t| !wrappers.iter().any(|(w, _)| w == t))
            .collect();
        stale.sort();
        stale.dedup();
        for name in stale {
            out.push(format!(
                "{LIBC_BACKSTOP_TEST}: `{block_name}` calls `{name}`, which is no longer an fl libc wrapper"
            ));
        }
    }
    out
}

fn libc_backstop_collect_wrappers(project_root: &Path) -> Vec<(String, bool)> {
    let mut wrappers = Vec::new();
    for entry in WalkDir::new(project_root.join("src")).into_iter().filter_map(Result::ok) {
        let path = path_to_string(entry.path());
        if entry.file_type().is_file() && ends_with_any(&path, &[".h", ".hpp", ".cpp"]) {
            if let Ok(content) = std::fs::read_to_string(entry.path()) {
                if content.contains("FlLibcTag") {
                    wrappers.extend(libc_backstop_wrappers(&content));
                }
            }
        }
    }
    wrappers
}

fn libc_backstop_pass(project_root: &Path, violations: &mut Vec<LintViolation>) {
    let wrappers = libc_backstop_collect_wrappers(project_root);
    let messages = match std::fs::read_to_string(project_root.join(LIBC_BACKSTOP_TEST)) {
        Ok(test_src) => libc_backstop_violations(&wrappers, &test_src),
        Err(err) => vec![format!("{LIBC_BACKSTOP_TEST}: cannot read the libc back-stop test: {err}")],
    };
    for message in messages {
        violations.push(LintViolation {
            checker: "LibcBackstopChecker".to_string(),
            path: LIBC_BACKSTOP_TEST.to_string(),
            line: 0,
            message,
        });
    }
}

#[cfg(test)]
mod libc_backstop_tests {
    use super::*;

    const TEST_SRC: &str = "namespace libc_every_adl {\nvoid t() { CRGB* p; memcpy(p, p, 1); }\n}\n\
        namespace libc_every_using_fl {\nvoid t() { using namespace fl; memcpy(v, v, 1); atoi(cs); }\n}\n";

    fn wrappers() -> Vec<(String, bool)> {
        vec![("memcpy".to_string(), true), ("atoi".to_string(), false)]
    }

    #[test]
    fn extracts_wrappers_and_pointer_params() {
        let src = "namespace fl {\ntemplate <typename FlLibcTag = void>\nFASTLED_FORCE_INLINE int\natoi(const char* s) { return 0; }\n\
                   template <typename FlLibcTag = void>\nFASTLED_FORCE_INLINE void free(void* p) {}\n\
                   template <typename FlLibcTag = void>\nFASTLED_FORCE_INLINE int rand() { return 0; }\n}\n";
        assert_eq!(
            libc_backstop_wrappers(src),
            vec![("atoi".to_string(), false), ("free".to_string(), true), ("rand".to_string(), false)]
        );
    }

    #[test]
    fn complete_backstop_passes() {
        assert!(libc_backstop_violations(&wrappers(), TEST_SRC).is_empty());
    }

    #[test]
    fn new_wrapper_without_test_line_fails() {
        let mut w = wrappers();
        w.push(("strdup".to_string(), true));
        let v = libc_backstop_violations(&w, TEST_SRC);
        assert_eq!(v.len(), 2, "{v:?}");
        assert!(v.iter().all(|m| m.contains("fl::strdup")));
    }

    #[test]
    fn removed_wrapper_is_stale() {
        let v = libc_backstop_violations(&[("memcpy".to_string(), true)], TEST_SRC);
        assert_eq!(v.len(), 1, "{v:?}");
        assert!(v[0].contains("`atoi`, which is no longer"));
    }

    #[test]
    fn removed_arithmetic_wrapper_is_stale() {
        let src = TEST_SRC.replace("atoi(cs);", "atoi(cs); div(1, 2);");
        let v = libc_backstop_violations(&wrappers(), &src);
        assert_eq!(v.len(), 1, "{v:?}");
        assert!(v[0].contains("`div`, which is no longer"));
    }

    #[test]
    fn using_in_adl_block_and_missing_block_fail() {
        let src = TEST_SRC.replace("CRGB* p;", "using namespace fl; CRGB* p;");
        assert_eq!(libc_backstop_violations(&wrappers(), &src).len(), 1);
        let v = libc_backstop_violations(&wrappers(), "namespace libc_every_adl {}\n");
        assert!(v[0].contains("missing `namespace libc_every_using_fl"));
    }

    /// The real tree: every wrapper under src/ is covered by the real back-stop.
    #[test]
    fn real_tree_backstop_is_complete() {
        let root = Path::new(env!("CARGO_MANIFEST_DIR")).join("../..");
        let wrappers = libc_backstop_collect_wrappers(&root);
        assert!(wrappers.len() >= 30, "found only {} wrappers", wrappers.len());
        let test_src = std::fs::read_to_string(root.join(LIBC_BACKSTOP_TEST)).unwrap();
        let v = libc_backstop_violations(&wrappers, &test_src);
        assert!(v.is_empty(), "{v:#?}");
    }

    #[test]
    fn macro_prefix_and_double_pointer_are_handled() {
        let src = "namespace fl {\ntemplate <typename FlLibcTag = void>\nFL_NODISCARD_MSG(\"x\") FASTLED_FORCE_INLINE void* malloc(size_t n) { return 0; }\n\
                   template <typename FlLibcTag = void>\nFASTLED_FORCE_INLINE int posix_memalign(void** p, size_t a, size_t n) { return 0; }\n\
                   template <typename FlLibcTag = void>\nFASTLED_FORCE_INLINE void qsort(void* b, size_t n, size_t s, int (*cmp)(const void*, const void*)) {}\n}\n";
        assert_eq!(
            libc_backstop_wrappers(src),
            vec![
                ("malloc".to_string(), false),
                ("posix_memalign".to_string(), false),
                ("qsort".to_string(), true)
            ]
        );
    }
}
