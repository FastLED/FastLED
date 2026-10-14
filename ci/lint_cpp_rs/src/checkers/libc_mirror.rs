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
// are not checked (ADL never finds them). The name list is the full C11
// <string.h>/<stdlib.h> plus <strings.h> and the POSIX / newlib / glibc
// extensions, not the names fl happens to mirror today, so a new mirror is
// caught the day it is added.

/// libc functions that take a pointer argument (or none), so an fl-typed
/// argument can drag `fl` into ADL. Purely arithmetic functions (`abs`, `labs`,
/// `llabs`, `div`, `ldiv`, `lldiv`, `ffs`) are left out on purpose: ADL never
/// reaches `fl` for them, and `fl::abs` is a generic math overload set.
const LIBC_RESERVED_NAMES: &[&str] = &[
    // C11 <string.h>
    "memchr", "memcmp", "memcpy", "memmove", "memset", "strcat", "strchr",
    "strcmp", "strcoll", "strcpy", "strcspn", "strerror", "strlen", "strncat",
    "strncmp", "strncpy", "strpbrk", "strrchr", "strspn", "strstr", "strtok",
    "strxfrm",
    // C11 <stdlib.h>
    "atof", "atoi", "atol", "atoll", "strtod", "strtof", "strtold", "strtol",
    "strtoll", "strtoul", "strtoull", "rand", "srand", "aligned_alloc",
    "calloc", "free", "malloc", "realloc", "abort", "atexit", "at_quick_exit",
    "exit", "_Exit", "quick_exit", "getenv", "system", "bsearch", "qsort",
    "mblen", "mbtowc", "wctomb", "mbstowcs", "wcstombs",
    // <strings.h>
    "bcmp", "bcopy", "bzero", "index", "rindex", "strcasecmp", "strncasecmp",
    // POSIX / newlib / glibc / BSD extensions
    "strdup", "strndup", "strnlen", "memccpy", "mempcpy", "memmem", "memrchr",
    "strlcpy", "strlcat", "strtok_r", "strerror_r", "strsep", "stpcpy",
    "stpncpy", "strsignal", "strcasestr", "strchrnul", "posix_memalign",
    "memalign", "valloc", "pvalloc", "reallocarray", "setenv", "unsetenv",
    "putenv", "random", "srandom", "rand_r", "mkstemp", "mkdtemp", "realpath",
    "qsort_r", "explicit_bzero",
];

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
    /// Class, enum, function body, initializer, ...
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

fn libc_mirror_violations(src: &str) -> Vec<(usize, String)> {
    let masked = libc_mirror_mask(src);
    let toks = libc_mirror_tokens(&masked);
    let mut violations = Vec::new();
    let mut scopes: Vec<LibcScope> = Vec::new();
    let mut stmt: Vec<&LibcTok> = Vec::new();
    let mut paren_depth = 0usize;
    let mut i = 0;
    while i < toks.len() {
        let tok = &toks[i];
        let at_namespace_scope = scopes.iter().all(|s| !matches!(s, LibcScope::Other));
        match tok.text.as_str() {
            "namespace" if at_namespace_scope => {
                // namespace a::b {   |   namespace {   |   namespace x = y;
                let mut names = Vec::new();
                let mut j = i + 1;
                while j < toks.len() && (toks[j].kind == LibcTokKind::Ident || toks[j].text == "::") {
                    if toks[j].kind == LibcTokKind::Ident && toks[j].text != "inline" {
                        names.push(toks[j].text.clone());
                    }
                    j += 1;
                }
                if j < toks.len() && toks[j].text == "{" {
                    if names.is_empty() {
                        names.push(String::new());
                    }
                    scopes.push(LibcScope::Namespace(names));
                    stmt.clear();
                    i = j + 1;
                    continue;
                }
            }
            "{" => {
                let transparent = at_namespace_scope
                    && paren_depth == 0
                    && stmt.last().is_some_and(|t| t.text == "extern");
                scopes.push(if transparent { LibcScope::Transparent } else { LibcScope::Other });
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
        if !at_namespace_scope {
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
        let namespaces: Vec<&str> = scopes
            .iter()
            .filter_map(|s| match s {
                LibcScope::Namespace(names) => Some(names.iter().map(String::as_str)),
                _ => None,
            })
            .flatten()
            .collect();
        let in_fl = namespaces.first() == Some(&"fl");
        let in_impl = namespaces.contains(&"libc_impl");
        let is_candidate = in_fl
            && !in_impl
            && paren_depth == 0
            && tok.kind == LibcTokKind::Ident
            && LIBC_RESERVED_NAMES.contains(&tok.text.as_str())
            && toks.get(i + 1).is_some_and(|t| t.text == "(")
            && !stmt.last().is_some_and(|t| matches!(t.text.as_str(), "::" | "." | "->"));
        if is_candidate {
            let body_start = libc_mirror_after_templates(&stmt);
            let is_initializer = stmt[body_start..].iter().any(|t| t.text == "=");
            if !is_initializer {
                let head: Vec<&str> = stmt[..body_start].iter().map(|t| t.text.as_str()).collect();
                let has_template = head.join(" ").contains(LIBC_MIRROR_TEMPLATE_HEAD);
                let has_force_inline = stmt.iter().any(|t| t.text == "FASTLED_FORCE_INLINE");
                if !(has_template && has_force_inline) {
                    let missing = match (has_template, has_force_inline) {
                        (false, false) => "`template <typename FlLibcTag = void>` and `FASTLED_FORCE_INLINE`",
                        (false, true) => "`template <typename FlLibcTag = void>`",
                        _ => "`FASTLED_FORCE_INLINE`",
                    };
                    violations.push((
                        tok.line,
                        format!(
                            "fl::{name} has the libc name `{name}` but is missing {missing}. An unqualified \
                             `{name}(...)` on an fl type finds it through ADL and becomes ambiguous with \
                             ::{name}. Declare it as `template <typename FlLibcTag = void> FASTLED_FORCE_INLINE` \
                             forwarding to fl::libc_impl::{name}, or rename it (FastLED #4834).",
                            name = tok.text
                        ),
                    ));
                }
            }
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
                   inline int abs(int v) { return v; }\nvoid aligned_free(void* p);\n}\n";
        assert!(lines(src).is_empty());
    }
}
