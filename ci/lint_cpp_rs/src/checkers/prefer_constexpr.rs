// PreferConstexprChecker — flag namespace-scope integer constants that are
// spelled as plain (or `const`) variables and should simply be `constexpr`.
//
// Split out of `SingletonElisionChecker` (FastLED#3483). That checker was
// reporting two unrelated populations under one message:
//
//     int PRIORITY_PARLIO = 3;              // wants `constexpr`
//     QueueHandle_t s_esp_request_queue;    // wants `fl::Singleton<T>`
//
// The advice differs, so the rules have to. Telling an author to wrap
// `PRIORITY_PARLIO` in `fl::Singleton<T>` is strictly worse than `constexpr`:
// the singleton hides the storage behind a function-local static that the
// linker *may* drop, while `constexpr` means the compiler inlines the value
// and emits no storage to drop in the first place. Conflating them also
// inflated the Singleton backlog with ~30 entries whose real fix is one
// keyword, which is why FastLED#3488's sweep is gated on this split.
//
// Scope is deliberately narrow: a non-`constexpr` declaration, of a plain
// integral type, bound to a compile-time integer literal. Anything else —
// pointers, floats, class types, macro initializers, function calls — stays
// with `SingletonElisionChecker`, where "move the storage" is the right
// advice. The two rules consume one shared scanner
// (`scan_namespace_scope_decls` in checkers/singleton_elision.rs) and
// partition its output, so no declaration can be reported by both or by
// neither.

/// True when `type_pretty` names a plain integral type.
///
/// Deliberately conservative — only spellings whose storage `constexpr`
/// reliably elides. Anything with `*`, `&` or `<` is a pointer, reference or
/// template and belongs to `SingletonElisionChecker` instead.
fn is_integral_type_name(type_pretty: &str) -> bool {
    let normalized = type_pretty
        .trim_start_matches("static ")
        .trim_start_matches("inline ")
        .trim_start_matches("const ")
        .trim_start_matches("volatile ")
        .trim();
    if normalized.contains('*') || normalized.contains('&') || normalized.contains('<') {
        return false;
    }
    matches!(
        normalized,
        "int"
            | "unsigned"
            | "unsigned int"
            | "signed"
            | "signed int"
            | "short"
            | "short int"
            | "unsigned short"
            | "long"
            | "unsigned long"
            | "long long"
            | "unsigned long long"
            | "char"
            | "signed char"
            | "unsigned char"
            | "bool"
            | "size_t"
            | "fl::size"
            | "u8"
            | "u16"
            | "u32"
            | "u64"
            | "i8"
            | "i16"
            | "i32"
            | "i64"
            | "fl::u8"
            | "fl::u16"
            | "fl::u32"
            | "fl::u64"
            | "fl::i8"
            | "fl::i16"
            | "fl::i32"
            | "fl::i64"
            | "uint8_t"
            | "uint16_t"
            | "uint32_t"
            | "uint64_t"
            | "int8_t"
            | "int16_t"
            | "int32_t"
            | "int64_t"
    )
}

/// True when `name` is assigned somewhere other than its own declaration.
///
/// This is the difference between a constant and mutable state that merely
/// *starts* at a literal. Driver files are full of the latter:
///
///     bool sInitialized = false;      // assigned in init() — NOT a constant
///     int  PRIORITY_PARLIO = 3;       // never reassigned — a constant
///
/// Both match "integral type bound to an integer literal", so without this
/// check the rule tells an author to write `constexpr bool sInitialized =
/// false;` — which does not compile the moment anything assigns to it, and
/// simultaneously steals the declaration from `SingletonElisionChecker`,
/// where mutable state genuinely belongs.
///
/// Line-based and deliberately over-eager: any `name =` (not `==`), `name++`,
/// `name--` or compound assignment anywhere else in the file disqualifies it.
/// Over-eager is the safe direction — a missed constant stays reported by the
/// Singleton rule, whereas a false "make it constexpr" is actively wrong
/// advice.
fn is_assigned_elsewhere(name: &str, decl_line_number: Option<usize>, lines: &[String]) -> bool {
    for (index, line) in lines.iter().enumerate() {
        if Some(index + 1) == decl_line_number {
            continue; // the declaration's own initializer
        }
        let code = split_line_comment(line);
        let mut search_from = 0usize;
        while let Some(found) = code[search_from..].find(name) {
            let start = search_from + found;
            let end = start + name.len();
            // Whole-identifier match only: `sInit` must not match `sInitDone`.
            let before_ok = start == 0
                || !{
                    let c = code[..start].chars().next_back().unwrap_or(' ');
                    c.is_ascii_alphanumeric() || c == '_'
                };
            let after = code[end..].trim_start();
            let after_ok = {
                let c = code[end..].chars().next().unwrap_or(' ');
                !(c.is_ascii_alphanumeric() || c == '_')
            };
            if before_ok && after_ok {
                // `=` but not `==` (comparison), and not `>=`/`<=`/`!=`
                // (those end with `=` but start elsewhere, so checking the
                // char after `=` is enough here).
                let is_plain_assign = after.starts_with('=')
                    && !after.starts_with("==")
                    && !after.starts_with("=>");
                let is_compound = ["+=", "-=", "*=", "/=", "%=", "|=", "&=", "^=", "<<=", ">>="]
                    .iter()
                    .any(|op| after.starts_with(op));
                let is_incdec = after.starts_with("++") || after.starts_with("--");
                if is_plain_assign || is_compound || is_incdec {
                    return true;
                }
            }
            search_from = end;
            if search_from >= code.len() {
                break;
            }
        }
    }
    false
}

/// Sibling headers for a TU source: `foo.cpp.hpp` → `foo.h`, `foo.hpp`.
///
/// A namespace-scope variable that another TU mutates is almost always
/// `extern`-declared in exactly this header, so it is the one extra file
/// worth reading. See [`is_shared_across_translation_units`].
fn sibling_header_paths(source_path: &str) -> Vec<String> {
    let stem = source_path
        .strip_suffix(".cpp.hpp")
        .or_else(|| source_path.strip_suffix(".cpp"))
        .or_else(|| source_path.strip_suffix(".cc"))
        .or_else(|| source_path.strip_suffix(".cxx"));
    match stem {
        Some(stem) => vec![format!("{stem}.h"), format!("{stem}.hpp")],
        None => Vec::new(),
    }
}

/// True when `name` is exported to, or mutated from, another translation unit.
///
/// A per-file checker cannot prove a non-`const` global is never assigned:
/// the definition can sit alone in its `.cpp.hpp` while every write happens
/// in a header included elsewhere. That is not hypothetical — it is exactly
/// `gTimeErrorAccum256ths`, whose only in-file appearance is
/// `u8 gTimeErrorAccum256ths = 0;` while `clockless_blocking.h` declares it
/// `extern` and assigns it. Calling that a compile-time constant would be
/// build-breaking advice.
///
/// So treat an `extern` declaration in the sibling header — or an assignment
/// there — as proof of shared mutable state and hand the declaration back to
/// `SingletonElisionChecker`.
///
/// Residual limitation, deliberately accepted: a variable `extern`-declared
/// from some *non-sibling* header is still missed. The rule is warn-only and
/// its message asks the author to confirm, so an over-eager report costs a
/// read rather than a broken build; narrowing further would drop the
/// priority-table case this rule exists for.
fn is_shared_across_translation_units(name: &str, source_path: &str) -> bool {
    for header in sibling_header_paths(source_path) {
        let Ok(content) = std::fs::read_to_string(&header) else {
            continue;
        };
        let lines: Vec<String> = content.lines().map(str::to_string).collect();
        for line in &lines {
            let code = split_line_comment(line);
            let trimmed = code.trim();
            if trimmed.starts_with("extern ") && trimmed.contains(name) {
                return true;
            }
        }
        // Assignment from the header counts too, even without `extern` on the
        // line we happened to match.
        if is_assigned_elsewhere(name, None, &lines) {
            return true;
        }
    }
    false
}

/// True when this declaration is an integer constant that should simply be
/// `constexpr` — the population `PreferConstexprChecker` owns.
///
/// `SingletonElisionChecker` calls this too, to exclude the same set. Keeping
/// one predicate is what guarantees the partition holds (FastLED#3483).
fn is_prefer_constexpr_candidate(
    decl: &NamespaceScopeDecl,
    lines: &[String],
    source_path: &str,
) -> bool {
    if decl.is_constexpr {
        return false;
    }
    if !is_integral_type_name(&decl.type_pretty) {
        return false;
    }
    // `volatile` is a hardware/ISR contract, not a constant — a SysTick
    // counter is spelled `volatile u32` and must keep its storage.
    if decl.type_pretty.contains("volatile") {
        return false;
    }
    match decl.rhs.as_deref() {
        Some(rhs) => {
            if !is_integer_literal_rhs(rhs) {
                return false;
            }
            // Mutable state that merely starts at a literal is not a
            // constant; leave it to SingletonElisionChecker.
            if is_assigned_elsewhere(&decl.name, Some(decl.line_number), lines) {
                return false;
            }
            // ...nor is state whose writes live in another translation unit.
            !is_shared_across_translation_units(&decl.name, source_path)
        }
        None => false,
    }
}

struct PreferConstexprChecker;

impl FileContentChecker for PreferConstexprChecker {
    fn name(&self) -> &'static str {
        "PreferConstexprChecker"
    }

    fn should_process_file(&self, file_path: &str, project_root: &Path) -> bool {
        // Same file set as SingletonElisionChecker — the two rules split one
        // population, so they must see the same files or the split leaks.
        if !ends_with_any(file_path, &[".cpp.hpp", ".cpp", ".cc", ".cxx"]) {
            return false;
        }
        if !is_under_project_subpath(file_path, project_root, "src") {
            return false;
        }
        if is_under_project_subpath(file_path, project_root, "src/third_party") {
            return false;
        }
        true
    }

    fn check_file_content(&self, file_content: &FileContent) -> Vec<(usize, String)> {
        let mut violations = Vec::new();

        for decl in scan_namespace_scope_decls(file_content) {
            if !is_prefer_constexpr_candidate(&decl, &file_content.lines, &file_content.path) {
                continue;
            }

            violations.push((
                decl.line_number,
                format!(
                    "namespace-scope `{name}` (type `{ty}`) is a compile-time \
                     integer constant; use `constexpr` so the compiler \
                     inlines it and no storage is emitted. `constexpr {ty} \
                     {name} = ...;` is the whole fix — this does NOT need \
                     `fl::Singleton<T>`. If the address is genuinely taken, \
                     or the value must live in memory for an ISR or a \
                     linker-invisible reference, annotate with \
                     `[[gnu::used]]` / `FL_KEEP` or a preceding \
                     `// FL_LINT_ALLOW_GLOBAL(<reason>)` comment line.",
                    name = decl.name,
                    ty = if decl.type_pretty.is_empty() {
                        "?"
                    } else {
                        &decl.type_pretty
                    }
                ),
            ));
        }

        violations
    }
}
