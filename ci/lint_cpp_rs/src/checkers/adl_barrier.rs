// --- AdlBarrierChecker (structural pass) -------------------------------------
//
// FastLED #4851 (follow-up to #4827 / #4830). fl's generic templates that
// collide with std (`reverse`, `sort`, `move`, `begin`, `make_shared`, ...)
// live in `fl::adl_barrier`, which fl imports with a using-directive. ADL
// ignores the directive, so an unqualified `sort(leds, ...)` on a CRGB* does not
// find fl's overload and tie with std's (see src/fl/stl/adl_barrier.h).
//
// Two ways to break it, both whole-tree properties:
//   * declare an overload of a barrier name directly in `fl`. ADL sees it again,
//     and qualified `fl::name` lookup stops at fl and never reaches the
//     barrier's other overloads;
//   * add a barrier function that tests/fl/stl/adl_barrier.cpp never calls as
//     `fl::name`, the compile check that qualified lookup still reaches it.

const ADL_BARRIER_TEST: &str = "tests/fl/stl/adl_barrier.cpp";
const ADL_BARRIER_REACHABLE_CASE: &str = "every adl_barrier name is reachable as fl::name";

/// Identifiers followed by `(` that are never a declared function name.
const ADL_NOT_FUNCTION_NAMES: &[&str] = &[
    "static_assert", "decltype", "sizeof", "alignof", "alignas", "noexcept", "requires",
    "operator", "return", "if", "while", "for", "switch",
];

/// `(namespace path, name, line)` for every function declared at namespace
/// scope. Inline and anonymous namespaces are recorded as "".
fn adl_namespace_functions(src: &str) -> Vec<(Vec<String>, String, usize)> {
    let toks = libc_mirror_tokens(&libc_mirror_mask(src));
    let mut out = Vec::new();
    // None = non-namespace scope (class, function body, ...).
    let mut scopes: Vec<Option<Vec<String>>> = Vec::new();
    let mut stmt: Vec<&LibcTok> = Vec::new();
    let mut paren_depth = 0usize;
    let mut i = 0;
    while i < toks.len() {
        let tok = &toks[i];
        let prev = if i > 0 { toks[i - 1].text.as_str() } else { "" };
        let ns_level = scopes.iter().all(Option::is_some);
        match tok.text.as_str() {
            "namespace" if ns_level => {
                let mut names = Vec::new();
                let mut inline_next = prev == "inline";
                let mut j = i + 1;
                while j < toks.len() && (toks[j].kind == LibcTokKind::Ident || toks[j].text == "::") {
                    if toks[j].text == "inline" {
                        inline_next = true;
                    } else if toks[j].kind == LibcTokKind::Ident {
                        names.push(if inline_next { String::new() } else { toks[j].text.clone() });
                        inline_next = false;
                    }
                    j += 1;
                }
                if j < toks.len() && toks[j].text == "{" {
                    if names.is_empty() {
                        names.push(String::new());
                    }
                    scopes.push(Some(names));
                    stmt.clear();
                    i = j + 1;
                    continue;
                }
            }
            "{" => {
                // `extern "C" {` keeps namespace scope.
                let transparent = ns_level && paren_depth == 0 && stmt.last().is_some_and(|t| t.text == "extern");
                scopes.push(if transparent { Some(Vec::new()) } else { None });
                stmt.clear();
                i += 1;
                continue;
            }
            "}" => {
                scopes.pop();
                stmt.clear();
                paren_depth = 0;
                i += 1;
                continue;
            }
            _ => {}
        }
        if !ns_level {
            i += 1;
            continue;
        }
        match tok.text.as_str() {
            ";" if paren_depth == 0 => {
                stmt.clear();
                i += 1;
                continue;
            }
            "(" => paren_depth += 1,
            ")" => paren_depth = paren_depth.saturating_sub(1),
            _ => {}
        }
        let next = toks.get(i + 1).map_or("", |t| t.text.as_str());
        let is_macro = tok.text.chars().all(|c| c.is_ascii_uppercase() || c.is_ascii_digit() || c == '_');
        if tok.kind == LibcTokKind::Ident
            && next == "("
            && paren_depth == 0
            && !is_macro
            && !ADL_NOT_FUNCTION_NAMES.contains(&tok.text.as_str())
            && !matches!(prev, "::" | "." | "->" | "~" | "operator")
            && libc_mirror_in_template_head(&stmt) == false
        {
            let body = &stmt[libc_mirror_after_templates(&stmt)..];
            let declares = !body.is_empty()
                && !body.iter().any(|t| matches!(t.text.as_str(), "=" | "using" | "typedef" | "return"));
            if declares {
                let path: Vec<String> = scopes.iter().flatten().flatten().cloned().collect();
                out.push((path, tok.text.clone(), tok.line));
            }
        }
        stmt.push(tok);
        i += 1;
    }
    out
}

/// True for a path directly in `fl` (anonymous / inline namespaces included).
fn adl_directly_in(path: &[String], ns: &[&str]) -> bool {
    let named: Vec<&str> = path.iter().map(String::as_str).filter(|n| !n.is_empty()).collect();
    named == ns
}

/// Names called as `fl::name` inside the reachability test case.
fn adl_reachable_names(test_src: &str) -> Option<BTreeSet<String>> {
    let start = test_src.find(ADL_BARRIER_REACHABLE_CASE)?;
    let masked = libc_mirror_mask(test_src);
    let open = masked.iter().skip(start).position(|&c| c == '{')? + start;
    let mut depth = 0usize;
    let mut end = masked.len();
    for (k, &c) in masked.iter().enumerate().skip(open) {
        match c {
            '{' => depth += 1,
            '}' => {
                depth -= 1;
                if depth == 0 {
                    end = k;
                    break;
                }
            }
            _ => {}
        }
    }
    let toks = libc_mirror_tokens(&masked[open..end]);
    Some(
        toks.windows(3)
            .filter(|w| w[0].text == "fl" && w[1].text == "::" && w[2].kind == LibcTokKind::Ident)
            .map(|w| w[2].text.clone())
            .collect(),
    )
}

/// `(path, line, message)` violations for the given source files and test.
fn adl_barrier_violations(files: &[(String, String)], test_src: Option<&str>) -> Vec<(String, usize, String)> {
    let mut functions = Vec::new();
    for (path, content) in files {
        for (ns, name, line) in adl_namespace_functions(content) {
            functions.push((path.clone(), ns, name, line));
        }
    }
    let barrier: BTreeSet<String> = functions
        .iter()
        .filter(|(_, ns, _, _)| adl_directly_in(ns, &["fl", "adl_barrier"]))
        .map(|(_, _, name, _)| name.clone())
        .collect();
    let mut out = Vec::new();
    for (path, ns, name, line) in &functions {
        if barrier.contains(name) && adl_directly_in(ns, &["fl"]) {
            out.push((
                path.clone(),
                *line,
                format!(
                    "fl::{name} is declared directly in fl, but `{name}` is an fl::adl_barrier name. \
                     ADL on fl types finds this overload again and it can tie with std::{name}, and \
                     qualified fl::{name} lookup stops here and misses the barrier's overloads. \
                     Declare it inside `namespace adl_barrier {{ ... }}` (FastLED #4851)."
                ),
            ));
        }
    }
    match test_src.and_then(adl_reachable_names) {
        None => out.push((
            ADL_BARRIER_TEST.to_string(),
            0,
            format!("missing the FL_TEST_CASE \"{ADL_BARRIER_REACHABLE_CASE}\" (FastLED #4851)"),
        )),
        Some(reached) => {
            for name in barrier.difference(&reached) {
                out.push((
                    ADL_BARRIER_TEST.to_string(),
                    0,
                    format!(
                        "fl::adl_barrier::{name} is never called as `fl::{name}` in \
                         \"{ADL_BARRIER_REACHABLE_CASE}\" (FastLED #4851)"
                    ),
                ));
            }
        }
    }
    out
}

fn adl_barrier_pass(project_root: &Path, violations: &mut Vec<LintViolation>) {
    let mut files = Vec::new();
    for entry in WalkDir::new(project_root.join("src")).into_iter().filter_map(Result::ok) {
        let path = path_to_string(entry.path());
        if entry.file_type().is_file() && ends_with_any(&path, &[".h", ".hpp", ".cpp"]) {
            if let Ok(content) = std::fs::read_to_string(entry.path()) {
                let rel = project_relative_path(&normalize_path(&path)).unwrap_or(path);
                files.push((rel, content));
            }
        }
    }
    let test_src = std::fs::read_to_string(project_root.join(ADL_BARRIER_TEST)).ok();
    for (path, line, message) in adl_barrier_violations(&files, test_src.as_deref()) {
        violations.push(LintViolation { checker: "AdlBarrierChecker".to_string(), path, line, message });
    }
}

#[cfg(test)]
mod adl_barrier_tests {
    use super::*;

    const HEADER: &str = "namespace fl {\nnamespace adl_barrier {\ntemplate <typename It> void reverse(It a, It b);\n\
        template <typename T> T&& move(T& t);\n}\nusing namespace adl_barrier;\n}\n";
    const TEST: &str = "FL_TEST_CASE(\"every adl_barrier name is reachable as fl::name\") {\n\
        fl::reverse(v, v + 4);\n int m = fl::move(v[0]);\n}\n";

    fn run(extra: &str, test: &str) -> Vec<String> {
        let files = vec![
            ("src/fl/stl/a.h".to_string(), HEADER.to_string()),
            ("src/fl/stl/b.h".to_string(), extra.to_string()),
        ];
        adl_barrier_violations(&files, Some(test)).into_iter().map(|v| format!("{}:{} {}", v.0, v.1, v.2)).collect()
    }

    #[test]
    fn barrier_only_passes() {
        assert!(run("", TEST).is_empty(), "{:?}", run("", TEST));
    }

    #[test]
    fn overload_directly_in_fl_fails_anywhere_in_the_tree() {
        let extra = "namespace fl {\ninline void\nreverse(int* a, int* b) {}\nnamespace detail { void move(); }\n}\n";
        let v = run(extra, TEST);
        assert_eq!(v.len(), 1, "{v:?}");
        assert!(v[0].starts_with("src/fl/stl/b.h:3 fl::reverse"), "{v:?}");
    }

    #[test]
    fn calls_members_and_other_namespaces_are_ignored() {
        let extra = "namespace fl {\nstruct S { void reverse(); };\ninline void f() { reverse(a, b); }\n\
                     static const int k = move(x);\nusing R = decltype(reverse(a, b));\n}\nvoid reverse(int);\n";
        assert!(run(extra, TEST).is_empty(), "{:?}", run(extra, TEST));
    }

    #[test]
    fn inline_namespace_in_fl_counts_as_fl() {
        let extra = "namespace fl {\ninline namespace v1 {\ntemplate <typename T> void move(T&);\n}\n}\n";
        assert_eq!(run(extra, TEST).len(), 1);
    }

    #[test]
    fn unreached_barrier_name_and_missing_case_fail() {
        let test = TEST.replace("int m = fl::move(v[0]);", "");
        let v = run("", &test);
        assert_eq!(v.len(), 1, "{v:?}");
        assert!(v[0].contains("fl::adl_barrier::move is never called"));
        assert!(run("", "")[0].contains("missing the FL_TEST_CASE"));
    }

    #[test]
    fn real_tree_passes() {
        let root = Path::new(env!("CARGO_MANIFEST_DIR")).join("../..");
        let mut violations = Vec::new();
        adl_barrier_pass(&root, &mut violations);
        let msgs: Vec<String> = violations.iter().map(|v| format!("{}:{} {}", v.path, v.line, v.message)).collect();
        assert!(msgs.is_empty(), "{msgs:#?}");
    }
}
