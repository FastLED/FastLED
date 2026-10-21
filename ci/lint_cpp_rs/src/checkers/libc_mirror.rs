// --- LibcMirrorChecker --------------------------------------------------------
//
// FastLED #4834 (follow-up to #4827 / #4830 / #4833). Every function declared at
// namespace scope anywhere inside `namespace fl` whose name is a libc name must
// be a force-inlined template wrapper:
//
//     template <typename FlLibcTag = void>
//     FASTLED_FORCE_INLINE void* memcpy(void* d, const void* s, size_t n) FL_NO_EXCEPT {
//         return libc_impl::memcpy(d, s, n);
//     }
//
// Why: an unqualified `memcpy(leds, ...)` on a `CRGB*` (or any other fl type)
// finds `fl::memcpy` through ADL as well as `::memcpy`. A plain function ties
// with libc and the call is ambiguous; a template loses the tie, so libc wins.
// `FASTLED_FORCE_INLINE` is required because GCC otherwise emits out-of-line
// clones of the forwarders (+98 B on ESP32-S3, #4833).
//
// The implementations live in `fl::libc_impl`, which is exempt. Class members
// are not checked (ADL never finds them). The name list
// (`LIBC_RESERVED_NAMES`, lint_core/libc_reserved_names.rs) is the full C11
// <string.h>/<stdlib.h> plus <strings.h> and the POSIX / newlib / glibc
// extensions, not the names fl happens to mirror today, so a new mirror is
// caught the day it is added.


const LIBC_MIRROR_TEMPLATE_HEAD: &str = "template < typename FlLibcTag = void >";

struct LibcMirrorChecker;

impl FileContentChecker for LibcMirrorChecker {
    fn name(&self) -> &'static str {
        "LibcMirrorChecker"
    }

    fn should_process_file(&self, file_path: &str, _project_root: &Path) -> bool {
        is_under_dir(file_path, "src") && ends_with_any(file_path, &[".cpp", ".h", ".hpp"])
    }

    fn check_file_content(&self, file_content: &FileContent) -> Vec<(usize, String)> {
        libc_mirror_violations(&file_content.content)
    }
}

#[derive(Clone, Copy, PartialEq)]
enum LibcTokKind {
    Ident,
    Punct,
}

struct LibcTok {
    kind: LibcTokKind,
    text: String,
    line: usize,
}

enum LibcScope {
    Namespace(Vec<String>),
    /// `extern "C" { ... }`: still namespace scope.
    Transparent,
    /// Class / struct / union body: only `friend` functions are checked
    /// (hidden friends are found by ADL).
    Class,
    /// Unscoped `enum` body: its enumerators land in the enclosing namespace.
    UnscopedEnum,
    /// Function body, initializer, scoped enum, ...
    Other,
}

/// Replace comments, string/char literals and preprocessor lines with spaces,
/// keeping newlines so line numbers survive.
fn libc_mirror_mask(src: &str) -> Vec<char> {
    let chars: Vec<char> = src.chars().collect();
    let mut out = chars.clone();
    let n = chars.len();
    let is_ident = |c: char| c.is_ascii_alphanumeric() || c == '_';
    let blank = |out: &mut Vec<char>, from: usize, to: usize| {
        for c in out.iter_mut().take(to.min(n)).skip(from) {
            if *c != '\n' {
                *c = ' ';
            }
        }
    };
    let mut i = 0;
    let mut at_line_start = true;
    while i < n {
        let c = chars[i];
        if c == '\n' {
            at_line_start = true;
            i += 1;
            continue;
        }
        if at_line_start && c.is_whitespace() {
            i += 1;
            continue;
        }
        if at_line_start && c == '#' {
            // Preprocessor directive, including backslash continuations.
            let start = i;
            while i < n && !(chars[i] == '\n' && (i == 0 || chars[i - 1] != '\\')) {
                i += 1;
            }
            blank(&mut out, start, i);
            continue;
        }
        at_line_start = false;
        if c == '/' && i + 1 < n && chars[i + 1] == '/' {
            let start = i;
            while i < n && chars[i] != '\n' {
                i += 1;
            }
            blank(&mut out, start, i);
            continue;
        }
        if c == '/' && i + 1 < n && chars[i + 1] == '*' {
            let start = i;
            i += 2;
            while i + 1 < n && !(chars[i] == '*' && chars[i + 1] == '/') {
                i += 1;
            }
            i = (i + 2).min(n);
            blank(&mut out, start, i);
            continue;
        }
        if c == '"' {
            let start = i;
            // Raw string: R"delim( ... )delim"
            if i > 0 && chars[i - 1] == 'R' {
                let mut j = i + 1;
                let mut delim = String::new();
                while j < n && chars[j] != '(' {
                    delim.push(chars[j]);
                    j += 1;
                }
                let close: Vec<char> = format!("){delim}\"").chars().collect();
                while j < n && !chars[j..].starts_with(&close) {
                    j += 1;
                }
                i = (j + close.len()).min(n);
            } else {
                i += 1;
                while i < n && chars[i] != '"' && chars[i] != '\n' {
                    if chars[i] == '\\' {
                        i += 1;
                    }
                    i += 1;
                }
                i = (i + 1).min(n);
            }
            blank(&mut out, start, i);
            continue;
        }
        if c == '\'' {
            // A quote after an identifier char is a digit separator (1'000),
            // unless the identifier is a character-literal prefix.
            let mut k = i;
            while k > 0 && is_ident(chars[k - 1]) {
                k -= 1;
            }
            let prefix: String = chars[k..i].iter().collect();
            let is_literal = prefix.is_empty() || matches!(prefix.as_str(), "L" | "u" | "U" | "u8");
            if is_literal {
                let start = i;
                i += 1;
                while i < n && chars[i] != '\'' && chars[i] != '\n' {
                    if chars[i] == '\\' {
                        i += 1;
                    }
                    i += 1;
                }
                i = (i + 1).min(n);
                blank(&mut out, start, i);
                continue;
            }
        }
        i += 1;
    }
    out
}

fn libc_mirror_tokens(masked: &[char]) -> Vec<LibcTok> {
    let mut toks = Vec::new();
    let mut line = 1;
    let mut i = 0;
    let n = masked.len();
    while i < n {
        let c = masked[i];
        if c == '\n' {
            line += 1;
            i += 1;
        } else if c.is_whitespace() {
            i += 1;
        } else if c.is_ascii_alphabetic() || c == '_' {
            let start = i;
            while i < n && (masked[i].is_ascii_alphanumeric() || masked[i] == '_') {
                i += 1;
            }
            toks.push(LibcTok { kind: LibcTokKind::Ident, text: masked[start..i].iter().collect(), line });
        } else if c.is_ascii_digit() {
            while i < n && (masked[i].is_ascii_alphanumeric() || masked[i] == '_' || masked[i] == '.' || masked[i] == '\'') {
                i += 1;
            }
        } else if c == ':' && i + 1 < n && masked[i + 1] == ':' {
            toks.push(LibcTok { kind: LibcTokKind::Punct, text: "::".to_string(), line });
            i += 2;
        } else if (c == '-' && i + 1 < n && masked[i + 1] == '>') || (c == '=' && i + 1 < n && masked[i + 1] == '=') {
            toks.push(LibcTok { kind: LibcTokKind::Punct, text: masked[i..i + 2].iter().collect(), line });
            i += 2;
        } else {
            toks.push(LibcTok { kind: LibcTokKind::Punct, text: c.to_string(), line });
            i += 1;
        }
    }
    toks
}

/// The statement prefix with any leading `template <...>` headers removed.
fn libc_mirror_after_templates(stmt: &[&LibcTok]) -> usize {
    let mut i = 0;
    while i < stmt.len() && stmt[i].text == "template" {
        i += 1;
        if i < stmt.len() && stmt[i].text == "<" {
            let mut depth = 0;
            while i < stmt.len() {
                match stmt[i].text.as_str() {
                    "<" => depth += 1,
                    ">" => {
                        depth -= 1;
                        if depth == 0 {
                            i += 1;
                            break;
                        }
                    }
                    _ => {}
                }
                i += 1;
            }
        }
    }
    i
}

fn libc_mirror_message(name: &str, problem: &str) -> String {
    format!(
        "fl::{name} has the libc name `{name}` but {problem}. An unqualified `{name}(...)` on an fl \
         type (ADL) or under `using namespace fl;` then becomes ambiguous with ::{name}. Declare it as \
         `template <typename FlLibcTag = void> FASTLED_FORCE_INLINE` forwarding to \
         fl::libc_impl::{name}, or rename it (FastLED #4834)."
    )
}

/// True while `stmt` is still inside an unclosed `template <...>` parameter list.
fn libc_mirror_in_template_head(stmt: &[&LibcTok]) -> bool {
    let mut depth = 0i32;
    let mut in_head = false;
    for (k, t) in stmt.iter().enumerate() {
        match t.text.as_str() {
            "<" if k > 0 && stmt[k - 1].text == "template" => {
                in_head = true;
                depth += 1;
            }
            "<" if in_head => depth += 1,
            ">" if in_head => {
                depth -= 1;
                if depth == 0 {
                    in_head = false;
                }
            }
            _ => {}
        }
    }
    in_head
}

/// True when `stmt` holds nothing but declaration prefixes: template heads,
/// specifiers, `[[attributes]]` and all-caps macros (`FASTLED_FORCE_INLINE`).
fn libc_mirror_is_decl_prefix(stmt: &[&LibcTok]) -> bool {
    let mut attr_depth = 0usize;
    stmt[libc_mirror_after_templates(stmt)..].iter().all(|t| match t.text.as_str() {
        "[" => {
            attr_depth += 1;
            true
        }
        "]" => {
            attr_depth = attr_depth.saturating_sub(1);
            true
        }
        _ if attr_depth > 0 => true,
        "static" | "inline" | "constexpr" | "extern" | "friend" | "virtual" | "explicit" => true,
        text => t.kind == LibcTokKind::Ident && text.chars().all(|c| c.is_ascii_uppercase() || c.is_ascii_digit() || c == '_'),
    })
}

/// Scope facts that only change when the scope stack changes.
struct LibcScopeFacts {
    ns_level: bool,
    member_level: bool,
    in_fl: bool,
    in_impl: bool,
    /// Names here are exposed by `using namespace fl;` (anonymous and inline
    /// namespaces included). Non-functions only matter here, because ADL never
    /// finds variables or types.
    directly_in_fl: bool,
    /// The innermost scope is an unscoped enum whose enclosing scopes are all
    /// namespaces, so its enumerators are namespace members.
    enum_at_ns_level: bool,
}

fn libc_scope_facts(scopes: &[LibcScope]) -> LibcScopeFacts {
    let is_ns = |s: &LibcScope| matches!(s, LibcScope::Namespace(_) | LibcScope::Transparent);
    let namespaces: Vec<&str> = scopes
        .iter()
        .filter_map(|s| match s {
            LibcScope::Namespace(names) => Some(names.iter().map(String::as_str)),
            _ => None,
        })
        .flatten()
        .collect();
    let in_fl = namespaces.first() == Some(&"fl");
    let enum_at_ns_level = matches!(scopes.last(), Some(LibcScope::UnscopedEnum))
        && scopes[..scopes.len() - 1].iter().all(is_ns);
    LibcScopeFacts {
        ns_level: scopes.iter().all(is_ns),
        member_level: matches!(scopes.last(), Some(LibcScope::Class)),
        in_fl,
        // Only the implementation namespace itself, `fl::libc_impl`, is exempt.
        in_impl: in_fl && namespaces.get(1) == Some(&"libc_impl"),
        directly_in_fl: in_fl && namespaces[1..].iter().all(|n| n.is_empty()),
        enum_at_ns_level,
    }
}

fn libc_mirror_violations(src: &str) -> Vec<(usize, String)> {
    let masked = libc_mirror_mask(src);
    let toks = libc_mirror_tokens(&masked);
    let mut violations = Vec::new();
    let mut scopes: Vec<LibcScope> = Vec::new();
    let mut facts = libc_scope_facts(&scopes);
    let mut stmt: Vec<&LibcTok> = Vec::new();
    let mut paren_depth = 0usize;
    let is_libc_name = |t: &str| LIBC_RESERVED_NAMES.contains(&t) || LIBC_ARITHMETIC_NAMES.contains(&t);
    let mut i = 0;
    while i < toks.len() {
        let tok = &toks[i];
        let prev = if i > 0 { toks[i - 1].text.as_str() } else { "" };
        let next = toks.get(i + 1).map_or("", |t| t.text.as_str());
        match tok.text.as_str() {
            "namespace" if facts.ns_level => {
                // namespace a::b {  |  namespace {  |  inline namespace v1 {  |  namespace x = y;
                // Inline and anonymous namespaces are recorded as "": their
                // members are members of the enclosing namespace.
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
                    scopes.push(LibcScope::Namespace(names));
                    facts = libc_scope_facts(&scopes);
                    stmt.clear();
                    i = j + 1;
                    continue;
                }
            }
            "{" => {
                let decl_level = (facts.ns_level || facts.member_level) && paren_depth == 0;
                let body = &stmt[libc_mirror_after_templates(&stmt)..];
                let has = |kw: &str| body.iter().any(|t| t.text == kw);
                let no_call = !has("(") && !has("=");
                let scope = if facts.ns_level && paren_depth == 0 && stmt.last().is_some_and(|t| t.text == "extern") {
                    LibcScope::Transparent
                } else if decl_level && no_call && has("enum") {
                    if has("class") || has("struct") {
                        LibcScope::Other
                    } else {
                        LibcScope::UnscopedEnum
                    }
                } else if decl_level && no_call && (has("class") || has("struct") || has("union")) {
                    LibcScope::Class
                } else {
                    LibcScope::Other
                };
                if facts.in_impl && facts.ns_level && matches!(scope, LibcScope::Class | LibcScope::UnscopedEnum) {
                    violations.push((
                        tok.line,
                        "fl::libc_impl must not define types: a type there makes ADL reach the \
                         exempt plain libc_impl functions (FastLED #4838)."
                            .to_string(),
                    ));
                }
                scopes.push(scope);
                facts = libc_scope_facts(&scopes);
                stmt.clear();
                i += 1;
                continue;
            }
            "}" => {
                scopes.pop();
                facts = libc_scope_facts(&scopes);
                stmt.clear();
                paren_depth = 0;
                i += 1;
                continue;
            }
            _ => {}
        }
        // Enumerators of an unscoped enum at namespace scope are namespace members.
        if facts.enum_at_ns_level && facts.directly_in_fl && matches!(prev, "{" | ",") && is_libc_name(&tok.text) {
            violations.push((tok.line, libc_mirror_message(&tok.text, "is an enumerator, not a function")));
        }
        if !(facts.ns_level || facts.member_level) {
            i += 1;
            continue;
        }
        match tok.text.as_str() {
            ";" if paren_depth == 0 => {
                stmt.clear();
                i += 1;
                continue;
            }
            ":" if facts.member_level
                && stmt.len() == 1
                && matches!(stmt[0].text.as_str(), "public" | "private" | "protected") =>
            {
                stmt.clear();
                i += 1;
                continue;
            }
            "(" => paren_depth += 1,
            ")" => paren_depth = paren_depth.saturating_sub(1),
            _ => {}
        }
        // Cheap token test first; the statement scans below run only for libc names.
        let named = tok.kind == LibcTokKind::Ident
            && paren_depth == 0
            && is_libc_name(&tok.text)
            && facts.in_fl
            && !facts.in_impl
            && !matches!(prev, "::" | "." | "->" | "~" | "operator")
            // `using ::strlen;` re-declares libc's own function; `using strdup = T;` is an alias.
            && !(prev == "using" && next != "=");
        if named && !libc_mirror_in_template_head(&stmt) {
            let name = tok.text.as_str();
            let body_start = libc_mirror_after_templates(&stmt);
            let is_initializer = stmt[body_start..].iter().any(|t| t.text == "=");
            let is_friend = stmt.iter().any(|t| t.text == "friend");
            if !is_initializer && (facts.ns_level || is_friend) {
                if next == "(" {
                    let has_template = stmt[..body_start]
                        .iter()
                        .map(|t| t.text.as_str())
                        .collect::<Vec<_>>()
                        .join(" ")
                        .contains(LIBC_MIRROR_TEMPLATE_HEAD);
                    let has_force_inline = stmt.iter().any(|t| t.text == "FASTLED_FORCE_INLINE");
                    let missing = if LIBC_ARITHMETIC_NAMES.contains(&name) {
                        // Arithmetic-only names: any template loses the tie with libc.
                        (body_start == 0).then_some("is not a template")
                    } else {
                        match (has_template, has_force_inline) {
                            (true, true) => None,
                            (false, false) => Some(
                                "is missing `template <typename FlLibcTag = void>` and `FASTLED_FORCE_INLINE`",
                            ),
                            (false, true) => Some("is missing `template <typename FlLibcTag = void>`"),
                            (true, false) => Some("is missing `FASTLED_FORCE_INLINE`"),
                        }
                    };
                    if let Some(problem) = missing {
                        violations.push((tok.line, libc_mirror_message(name, problem)));
                    }
                } else if facts.ns_level
                    && facts.directly_in_fl
                    && (matches!(next, ";" | "=" | "{" | "[" | ",")
                        || matches!(prev, "struct" | "class" | "union" | "enum" | "using"))
                {
                    violations.push((
                        tok.line,
                        libc_mirror_message(name, "is a variable or type, not a template function"),
                    ));
                }
            }
        }
        // A function-like macro invoked at declaration level with a libc name
        // as an argument (`FL_DECLARE(memcpy)`) can expand to a libc-named
        // function this token scan never sees, so it is rejected outright
        // (FastLED #4850).
        let macro_like = tok.kind == LibcTokKind::Ident
            && tok.text.chars().all(|c| c.is_ascii_uppercase() || c.is_ascii_digit() || c == '_')
            && tok.text.chars().any(|c| c.is_ascii_uppercase());
        // Only at the start of a declaration (after template heads, specifiers,
        // attributes or other macros): `using F = fl::function<CRGB(u8 index)>;`
        // names a function type, not a macro.
        if macro_like
            && next == "("
            && libc_mirror_is_decl_prefix(&stmt)
            && paren_depth == 0
            && facts.in_fl
            && !facts.in_impl
        {
            let mut depth = 0usize;
            let mut k = i + 1;
            while k < toks.len() {
                let t = &toks[k];
                match t.text.as_str() {
                    "(" => depth += 1,
                    ")" => {
                        depth -= 1;
                        if depth == 0 {
                            break;
                        }
                    }
                    _ => {}
                }
                let qualified = matches!(toks[k - 1].text.as_str(), "::" | "." | "->");
                if t.kind == LibcTokKind::Ident && !qualified && is_libc_name(&t.text) {
                    violations.push((
                        t.line,
                        format!(
                            "macro `{}` is invoked in namespace fl with the libc name `{}` as an argument. \
                             A macro can expand to a libc-named function that LibcMirrorChecker cannot see; \
                             write the `template <typename FlLibcTag = void> FASTLED_FORCE_INLINE` wrapper \
                             out by hand, or pass a non-libc name (FastLED #4850).",
                            tok.text, t.text
                        ),
                    ));
                }
                k += 1;
            }
            // The invocation is skipped as a unit and not pushed onto `stmt`:
            // declaration macros often end without `;`, so the next one must
            // still look like a declaration start.
            i = k + 1;
            continue;
        }
        stmt.push(tok);
        i += 1;
    }
    violations
}

#[cfg(test)]
mod libc_mirror_tests {
    use super::*;

    fn lines(src: &str) -> Vec<usize> {
        libc_mirror_violations(src).into_iter().map(|v| v.0).collect()
    }

    #[test]
    fn compliant_wrappers_pass() {
        let src = "namespace fl {\n\
            namespace libc_impl { void* memcpy(void* d, const void* s, size_t n); }\n\
            template <typename FlLibcTag = void>\n\
            FASTLED_FORCE_INLINE void* memcpy(void* d, const void* s, size_t n) FL_NO_EXCEPT {\n\
                return libc_impl::memcpy(d, s, n);\n\
            }\n\
            }\n";
        assert!(lines(src).is_empty());
    }

    #[test]
    fn plain_inline_mirror_fails() {
        let src = "namespace fl {\ninline void* memset(void* p, int v, size_t n) { return p; }\n}\n";
        assert_eq!(lines(src), vec![2]);
    }

    #[test]
    fn template_without_force_inline_fails() {
        let src = "namespace fl {\ntemplate <typename FlLibcTag = void>\ninline size_t strlen(const char* s) { return 0; }\n}\n";
        let v = libc_mirror_violations(src);
        assert_eq!(v.len(), 1);
        assert!(v[0].1.contains("missing `FASTLED_FORCE_INLINE`"));
    }

    #[test]
    fn force_inline_without_template_fails() {
        let src = "namespace fl {\nFASTLED_FORCE_INLINE void free(void* p) {}\n}\n";
        let v = libc_mirror_violations(src);
        assert_eq!(v.len(), 1);
        assert!(v[0].1.contains("missing `template <typename FlLibcTag = void>`"));
    }

    #[test]
    fn other_template_head_fails() {
        let src = "namespace fl {\ntemplate <typename T>\nFASTLED_FORCE_INLINE T* strdup(const T* s) { return 0; }\n}\n";
        assert_eq!(lines(src), vec![3]);
    }

    #[test]
    fn sub_namespace_and_nested_namespace_fail() {
        let src = "namespace fl {\nnamespace detail {\nvoid* malloc(size_t n);\n}\n}\n\
                   namespace fl::adl_barrier {\nchar* strdup(const char* s);\n}\n";
        assert_eq!(lines(src), vec![3, 7]);
    }

    #[test]
    fn multi_line_declaration_reports_name_line() {
        let src = "namespace fl {\nint\nmemcmp(\n    const void* a,\n    const void* b,\n    size_t n);\n}\n";
        assert_eq!(lines(src), vec![3]);
    }

    #[test]
    fn extern_c_block_inside_fl_is_namespace_scope() {
        let src = "namespace fl {\nextern \"C\" {\nvoid qsort(void* b, size_t n, size_t s, int (*cmp)(const void*, const void*));\n}\n}\n";
        assert_eq!(lines(src), vec![3]);
    }

    #[test]
    fn libc_impl_class_members_calls_and_outside_fl_are_ignored() {
        let src = "void* memcpy(void* d, const void* s, size_t n);\n\
            namespace other { void free(void* p); }\n\
            namespace fl {\n\
            namespace libc_impl { size_t strlen(const char* s); }\n\
            size_t libc_impl::strlen(const char* s) { return ::strlen(s); }\n\
            struct Buffer { void free(); void* malloc(size_t n); };\n\
            inline void use(char* p) { memset(p, 0, 4); fl::free(p); }\n\
            static const size_t kLen = strlen(\"abc\");\n\
            void takes_default(size_t n = strlen(\"x\"));\n\
            }\n";
        assert!(lines(src).is_empty(), "{:?}", libc_mirror_violations(src));
    }

    #[test]
    fn comments_strings_and_preprocessor_are_ignored() {
        let src = "namespace fl {\n\
            // void* memcpy(void* d, const void* s, size_t n);\n\
            /* inline void free(void* p) {} */\n\
            #define FL_FREE(p) free(p)\n\
            const char* kDoc = \"void* malloc(size_t n);\";\n\
            const char kBrace = '{';\n\
            const int kBig = 1'000;\n\
            }\n";
        assert!(lines(src).is_empty(), "{:?}", libc_mirror_violations(src));
    }

    #[test]
    fn non_reserved_names_are_ignored() {
        let src = "namespace fl {\ninline void* memcopy(void* d, const void* s, size_t n) { return d; }\n\
                   void aligned_free(void* p);\n}\n";
        assert!(lines(src).is_empty());
    }

    #[test]
    fn hidden_friend_mirror_fails_and_template_friend_passes() {
        let src = "namespace fl {\nstruct Buf {\npublic:\n    friend void* memcpy(Buf* d, const Buf* s, size_t n) { return d; }\n\
                   template <typename FlLibcTag = void>\n    friend FASTLED_FORCE_INLINE void free(Buf* p) {}\n\
                   void* malloc(size_t n);\n};\n}\n";
        assert_eq!(lines(src), vec![4]);
    }

    #[test]
    fn arithmetic_names_must_be_templates() {
        let src = "namespace fl {\nint abs(int x) { return x; }\ntemplate <typename T> constexpr inline T abs(T v) { return v; }\n\
                   div_t div(int a, int b);\n}\n";
        assert_eq!(lines(src), vec![2, 4]);
    }

    #[test]
    fn variables_types_and_enumerators_with_libc_names_fail() {
        let src = "namespace fl {\nextern int rand;\nstruct strdup { int x; };\nenum Mode { free, keep = 2, malloc };\n\
                   enum class Scoped { memcpy };\nusing ::strlen;\nint (*memset_fn)(void*) = nullptr;\n\
                   namespace math { class random { random(); }; }\n}\n";
        assert_eq!(lines(src), vec![2, 3, 4, 4]);
    }

    #[test]
    fn only_fl_libc_impl_is_exempt_and_it_defines_no_types() {
        let src = "namespace fl {\nnamespace detail { namespace libc_impl { void* memcpy(void* d, const void* s, size_t n); } }\n\
                   namespace libc_impl {\nstruct Arena { int x; };\nvoid free(void* p);\n}\n}\n";
        assert_eq!(lines(src), vec![2, 4]);
    }

    /// The real tree passes, and is not vacuously passing: breaking one real
    /// wrapper in cstring.h is caught.
    #[test]
    fn real_tree_passes_and_mutation_is_caught() {
        let root = Path::new(env!("CARGO_MANIFEST_DIR")).join("../../src");
        let mut checked = 0;
        for entry in WalkDir::new(&root).into_iter().filter_map(Result::ok) {
            let path = path_to_string(entry.path());
            if entry.file_type().is_file() && ends_with_any(&path, &[".cpp", ".h", ".hpp"]) {
                let content = std::fs::read_to_string(entry.path()).unwrap_or_default();
                let v = libc_mirror_violations(&content);
                assert!(v.is_empty(), "{path}: {v:?}");
                checked += 1;
            }
        }
        assert!(checked > 1000, "only {checked} files checked");
        let cstring = std::fs::read_to_string(root.join("fl/stl/cstring.h")).unwrap();
        assert!(libc_mirror_violations(&cstring).is_empty());
        let mutated = cstring.replacen("FASTLED_FORCE_INLINE", "inline", 1);
        assert_eq!(libc_mirror_violations(&mutated).len(), 1);
        let malloc_impl = std::fs::read_to_string(root.join("fl/stl/malloc.cpp.hpp")).unwrap();
        let mutated = malloc_impl.replacen("libc_impl::realloc(", "realloc(", 1);
        assert_eq!(libc_mirror_violations(&mutated).len(), 1);
    }

    #[test]
    fn class_nested_enum_and_template_parameters_are_not_namespace_names() {
        let src = "namespace fl {\nstruct Pool { enum Mode { free, keep }; };\n\
                   template <int index, typename T> struct Get;\ntemplate <size_t random = 0> void f();\n}\n";
        assert!(lines(src).is_empty(), "{:?}", libc_mirror_violations(src));
    }

    #[test]
    fn using_alias_and_inline_namespace_members_fail() {
        let src = "namespace fl {\nusing strdup = int;\ninline namespace v1 {\nextern int rand;\n}\n}\n\
                   namespace fl::inline v2 {\nenum E { free };\n}\n";
        assert_eq!(lines(src), vec![2, 4, 8]);
    }

    #[test]
    fn macro_invocation_with_libc_name_argument_fails() {
        let src = "namespace fl {\n#define FL_DECL(n) void* n(void* p) { return p; }\nFL_DECL(memcpy)\n\
                   FL_DECL(\n    strdup)\nFL_DECL(memcopy)\ntemplate <typename T> static FL_DECL(free)\n[[nodiscard]] inline FL_DECL(malloc)\nFL_STATIC_ASSERT(sizeof(::memcpy) > 0, \"\");\n\
                   inline void f() { FL_CHECK(memcpy(0, 0, 0)); }\n\
                   struct G { using F = fl::function<CRGB(u8 index)>; };\nusing H = fl::function<CRGB(u8 index)>;\n\
                   namespace libc_impl { FL_DECL(free) }\n}\nFL_DECL(malloc)\n";
        assert_eq!(lines(src), vec![3, 5, 7, 8], "{:?}", libc_mirror_violations(src));
    }
}
