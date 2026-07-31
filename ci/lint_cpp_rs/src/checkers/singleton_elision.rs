// SingletonElisionChecker — flag file-scope / namespace-scope variables
// that should be wrapped in `fl::Singleton<T>` so `--gc-sections` can
// drop them when unused.
//
// Motivating example: `src/platforms/esp/32/drivers/i2s/i2s_esp32dev.cpp.hpp`
// carries ~15 file-scope statics (`gCallback`, `gCntBuffer`, `gDoneFilling`,
// `dmaBuffers[]`, `gTX_semaphore`, `gI2S_intr_handle`, `i2s`, timing
// tables, ...). Each one lands in `.data`/`.bss`/`.rodata` even when the
// user program never calls into the I2S driver, because the linker sees a
// definition at namespace scope with external/internal linkage and must
// emit storage.
//
// The `fl::Singleton<T>` pattern (see `src/fl/stl/singleton.h`) hides the
// storage inside a function-local static — `--gc-sections` can drop the
// whole `Singleton<T>::instance()` symbol AND its storage together when no
// code path references it. Same elision benefit for mutable state, `const`
// tables, and `FL_PROGMEM` tables: put the data inside a class as a member,
// route access through `Singleton<T>::instance().field`.
//
// Only opt-outs: `[[gnu::used]]` / `FL_KEEP` annotation on the declaration
// (author-visible, code-review visible, per-decl explicit), plus a short
// allowlist for the handful of variables that genuinely cannot move (the
// `fl::detail::singleton_registry` backing itself, ISR-only IRAM handles
// where the ISR path is invisible to the linker, etc.).
//
// This file also hosts the shared namespace-scope declaration scanner used
// by `PreferConstexprChecker` (checkers/prefer_constexpr.rs). The two rules
// must partition one candidate set exactly — see `scan_namespace_scope_decls`.

// Recognizes a right-hand side that is a pure compile-time literal, so a
// `constexpr` bound to it emits no storage and must not be flagged.
//
// This is a direct port of `TRIVIAL_RHS_RE` in `ci/scan_singleton_elision.py`
// (whose docstring declares that scanner mirrors this checker). Do NOT
// reimplement with `str::parse` — Rust's numeric parsers reject the `0x`/`0b`
// prefixes AND the C++ `u`/`l`/`f` suffixes, so hex register masks like
// `constexpr u32 kMask = 0x8000u;` get misreported as non-literal.
//
// Keep this grammar and TRIVIAL_RHS_RE in sync by hand — there is no
// cross-language parity test. The Rust side is pinned by
// `singleton_elision_accepts_suffixed_and_prefixed_literals` and
// `singleton_elision_still_flags_non_literal_constexpr` in
// lint_core/tests.rs.
fn trivial_rhs_regex() -> &'static regex::Regex {
    static RE: std::sync::OnceLock<regex::Regex> = std::sync::OnceLock::new();
    RE.get_or_init(|| {
        regex::Regex::new(concat!(
            r"^\s*(",
            r"[-+]?0[xX][0-9a-fA-F]+[uUlL]*",             // hex literal
            r"|[-+]?0[bB][01]+[uUlL]*",                   // binary literal
            r"|[-+]?\d+[uUlL]*",                          // int literal
            // float literal, incl. `10.f`, `1e-6f`, `2.5e3f`, `1.5L`
            r"|[-+]?(?:\d*\.\d+|\d+\.\d*|\d+)(?:[eE][-+]?\d+)?[fFlL]?",
            r"|nullptr|NULL|true|false",                  // symbolic constants
            r#"|"[^"]*""#,                                // string literal
            r"|'.'",                                      // char literal
            r"|[A-Z_][A-Z0-9_]*",                         // single all-caps macro
            r"|\(\s*[-+]?0[xX][0-9a-fA-F]+[uUlL]*\s*<<\s*\d+\s*\)", // (0x1u << N)
            r"|\(\s*[-+]?\d+[uUlL]*\s*<<\s*\d+\s*\)",     // (1u << N)
            r")\s*$",
        ))
        .expect("trivial-RHS regex must compile")
    })
}

fn is_trivial_rhs(rhs: &str) -> bool {
    trivial_rhs_regex().is_match(rhs)
}

/// Integer-literal subset of [`is_trivial_rhs`].
///
/// `is_trivial_rhs` also accepts strings, floats, `nullptr` and bare all-caps
/// macros. None of those belong to the `constexpr` rule: a macro RHS is not
/// necessarily a constant expression at all, and a float or string constant
/// is a different conversation from an integer priority table.
fn is_integer_literal_rhs(rhs: &str) -> bool {
    static RE: std::sync::OnceLock<regex::Regex> = std::sync::OnceLock::new();
    let re = RE.get_or_init(|| {
        regex::Regex::new(concat!(
            r"^\s*(",
            r"[-+]?0[xX][0-9a-fA-F]+[uUlL]*",             // hex literal
            r"|[-+]?0[bB][01]+[uUlL]*",                   // binary literal
            r"|[-+]?\d+[uUlL]*",                          // int literal
            r"|true|false",                               // bool literal
            r"|\(\s*[-+]?0[xX][0-9a-fA-F]+[uUlL]*\s*<<\s*\d+\s*\)", // (0x1u << N)
            r"|\(\s*[-+]?\d+[uUlL]*\s*<<\s*\d+\s*\)",     // (1u << N)
            r")\s*$",
        ))
        .expect("integer-literal RHS regex must compile")
    });
    re.is_match(rhs)
}

/// One namespace-scope variable definition, as found by
/// [`scan_namespace_scope_decls`].
///
/// Two checkers consume these: `SingletonElisionChecker` (storage that should
/// hide inside `fl::Singleton<T>`) and `PreferConstexprChecker` (integer
/// constants that should simply be `constexpr`). They share this scanner
/// rather than each re-deriving namespace scope, comment stripping and the
/// opt-out markers — that tracking is subtle enough that a second copy would
/// drift, and the two rules must partition the same candidate set exactly or
/// a declaration gets reported twice, or by neither (FastLED#3483).
struct NamespaceScopeDecl {
    /// 1-based line number, ready to report.
    line_number: usize,
    /// Declared identifier.
    name: String,
    /// Type text with storage/constexpr keywords stripped, for the message.
    type_pretty: String,
    /// True when the declaration itself says `constexpr`.
    is_constexpr: bool,
    /// Initializer text, when the line has one.
    rhs: Option<String>,
}

/// Walk `file_content` and return every namespace-scope variable definition,
/// with opt-outs already applied.
///
/// Skips the file entirely when one of the first 15 lines carries
/// `FL_LINT_ALLOW_GLOBAL_FILE(<reason>)`, and drops declarations annotated
/// with `[[gnu::used]]` / `FL_KEEP` / `FL_LINT_ALLOW_GLOBAL`, inline or on
/// the immediately preceding comment line.
fn scan_namespace_scope_decls(file_content: &FileContent) -> Vec<NamespaceScopeDecl> {
    // File-level opt-out: used for Tier 2 driver files pending migration to
    // Singleton<T> via their own sub-issues (see FastLED#3481) — annotating
    // 10+ sites individually is noise; a single file-header marker captures
    // the intent cleanly.
    for line in file_content.lines.iter().take(15) {
        if line.contains("FL_LINT_ALLOW_GLOBAL_FILE") {
            return Vec::new();
        }
    }

    let mut decls = Vec::new();

    // Brace-depth tracker. Depth 0 = file scope. Inside `namespace X {`
    // the depth counter goes to 1 but that's *still* namespace-scope
    // storage (variables there still get emitted unconditionally), so
    // we also track a "namespace stack" of open braces that came from a
    // `namespace X {` line and treat those depths as file-scope-
    // equivalent for the purposes of this check.
    let mut depth: i32 = 0;
    let mut namespace_stack: Vec<i32> = Vec::new(); // depths inside `namespace X {`
    let mut in_multiline_comment = false;
    let mut in_extern_c_block: Vec<i32> = Vec::new(); // depths inside `extern "C" {`

    // Track whether the previous non-blank line contained a
    // FL_LINT_ALLOW_GLOBAL / FL_KEEP / [[gnu::used]] marker. This lets
    // authors put the annotation on the line ABOVE the declaration
    // (preferred style — keeps the declaration clean):
    //
    //     // FL_LINT_ALLOW_GLOBAL(reason)
    //     MyType global_thing;
    //
    // as well as inline on the declaration line itself.
    let mut prev_line_had_marker = false;

    for (index, line) in file_content.lines.iter().enumerate() {
        let raw = line;
        let trimmed = raw.trim();

        // Snapshot the previous line's carry-over marker BEFORE we update it
        // for the next iteration. The flag we set carries only when THIS line
        // is comment-only (so the marker "belongs" to the next declaration).
        let carry_marker = prev_line_had_marker;
        let this_line_marker = raw.contains("[[gnu::used]]")
            || raw.contains("__attribute__((used))")
            || raw.contains("FL_KEEP")
            || raw.contains("FL_LINT_ALLOW_GLOBAL");
        let this_is_comment_only = trimmed.starts_with("//") || trimmed.starts_with("/*");
        prev_line_had_marker = this_line_marker && this_is_comment_only;

        // Strip multi-line comments (handle nested cases with a flag).
        let mut work = raw.to_string();
        if in_multiline_comment {
            if let Some(end) = work.find("*/") {
                work = work[end + 2..].to_string();
                in_multiline_comment = false;
            } else {
                continue;
            }
        }
        // Also strip any inline `/* ... */` on this line.
        while let (Some(s), Some(e)) = (work.find("/*"), work.find("*/")) {
            if s < e {
                let rest = work[e + 2..].to_string();
                work.truncate(s);
                work.push_str(&rest);
            } else {
                break;
            }
        }
        if let Some(s) = work.find("/*") {
            work.truncate(s);
            in_multiline_comment = true;
        }
        // Strip line comment.
        let code = split_line_comment(&work).to_string();

        // Preprocessor lines: never variable definitions.
        if trimmed.starts_with('#') {
            continue;
        }

        // Track `namespace X {` opens. Regex-lite: any line that starts with
        // `namespace` and has a `{` opens a namespace at the current depth+1.
        let opens_namespace = trimmed.starts_with("namespace ")
            && (trimmed.ends_with('{') || trimmed.contains("{"));
        let opens_extern_c = trimmed.contains("extern \"C\"") && trimmed.contains('{');

        // Now walk braces in the code-part-of-line to update depth.
        for ch in code.chars() {
            match ch {
                '{' => {
                    depth += 1;
                    if opens_namespace && !namespace_stack.contains(&depth) {
                        namespace_stack.push(depth);
                    }
                    if opens_extern_c {
                        in_extern_c_block.push(depth);
                    }
                }
                '}' => {
                    namespace_stack.retain(|&d| d != depth);
                    in_extern_c_block.retain(|&d| d != depth);
                    depth -= 1;
                    if depth < 0 {
                        depth = 0;
                    }
                }
                _ => {}
            }
        }

        // Only pure declaration lines are considered. Multi-line brace
        // initializers get skipped by this filter — accepted false-negative
        // territory for this pass.
        if code.contains('{') || code.contains('}') {
            continue;
        }

        // Namespace-scope means depth ∈ namespace_stack ∪ {0}. Depth 0 covers
        // TU scope; each namespace_stack entry covers one `namespace X { ... }`
        // block. Anything deeper is a function or class body — both fine.
        let namespace_scope = depth == 0 || namespace_stack.contains(&depth);
        if !namespace_scope {
            continue;
        }

        // Inside `extern "C" { ... }` blocks the storage-emission behaviour is
        // identical to namespace scope, so still catch. (Tracked, not escaped.)

        // Skip obvious non-variable lines.
        if trimmed.is_empty() {
            continue;
        }
        if trimmed.starts_with("//") {
            continue;
        }
        if trimmed.starts_with("using ") || trimmed.starts_with("typedef ") {
            continue;
        }
        if trimmed.starts_with("namespace ")
            || trimmed.starts_with("class ")
            || trimmed.starts_with("struct ")
            || trimmed.starts_with("enum ")
            || trimmed.starts_with("union ")
            || trimmed.starts_with("template<")
            || trimmed.starts_with("template ")
            || trimmed.starts_with("friend ")
            || trimmed.starts_with("public:")
            || trimmed.starts_with("private:")
            || trimmed.starts_with("protected:")
        {
            continue;
        }
        if trimmed.starts_with("extern ") {
            // `extern int x;` — declaration, not definition. Skip.
            continue;
        }
        if trimmed.starts_with("return ") || trimmed.starts_with("return;") {
            continue;
        }

        // Must end with `;` (variable definitions are single-line for this
        // pass).
        if !trimmed.ends_with(';') {
            continue;
        }

        // Must NOT look like a function declaration / call: any `(` before
        // the `;` is the safe cut.
        if let Some(paren_pos) = code.find('(') {
            if let Some(semi_pos) = code.find(';') {
                if paren_pos < semi_pos {
                    continue;
                }
            }
        }

        // Opt-outs, inline on the declaration or on the preceding comment:
        //
        // - `[[gnu::used]]` / `__attribute__((used))`: tells the linker to
        //   keep the symbol regardless of use. Author has thought about it.
        // - `FL_KEEP_ALIVE` / `FL_KEEP`: FastLED's export macro, same idea.
        // - `FL_LINT_ALLOW_GLOBAL(<reason>)`: linter-only marker with no
        //   compile-time effect, for storage that genuinely cannot move
        //   (public API surface, ISR cache pointer, singleton-registry
        //   backing, host-only entry point). The `<reason>` is mandatory —
        //   the reviewer must see a justification for each escape.
        if this_line_marker || carry_marker {
            continue;
        }

        // Extract the identifier: the token immediately before `=`, `;` or
        // `[`, whichever comes first.
        let end_marker = ['=', ';', '['];
        let end_pos = code
            .find(|c: char| end_marker.contains(&c))
            .unwrap_or(code.len());
        // Trim before the split below. `end_pos` stops just *before* the
        // `=` / `;` / `[`, so for the ordinary spelling `int x = 0;` the head
        // is "int x " — with a trailing space. The `rsplit` on non-identifier
        // characters would then split on that trailing space and hand back the
        // empty tail, the name would come out as "" and the `name.is_empty()`
        // guard below would skip the line.
        //
        // Since virtually every real declaration puts a space before the
        // initializer, that made the whole checker a silent no-op: it reported
        // zero violations tree-wide while the Python scanner found 101. See
        // FastLED#3482.
        let head = code[..end_pos].trim();

        // Class-static-member out-of-class definitions like
        // `constexpr bool numeric_limits<char>::is_signed = true;` or
        // `const float SpectralEqualizer::A_WEIGHTING_16BAND[16] = ...;` —
        // the `::` in the name path is the giveaway. C++11 requires these;
        // they don't emit storage unless ODR-used. Skip.
        if head.contains("::") {
            continue;
        }
        let name = head
            .rsplit(|c: char| !c.is_ascii_alphanumeric() && c != '_')
            .next()
            .unwrap_or("")
            .to_string();
        if name.is_empty() || name.chars().all(|c| c.is_ascii_digit()) {
            continue;
        }

        // Type is everything before the name.
        let head_trim = head.trim();
        let type_part = if head_trim.ends_with(&name) {
            head_trim[..head_trim.len() - name.len()].trim()
        } else {
            head_trim
        };
        let type_pretty = type_part
            .trim_start_matches("static ")
            .trim_start_matches("inline ")
            .trim_start_matches("constexpr ")
            .trim_start_matches("static constexpr ")
            .trim()
            .to_string();

        let is_constexpr = trimmed.starts_with("constexpr ")
            || trimmed.starts_with("static constexpr ")
            || trimmed.starts_with("inline constexpr ");

        let rhs = code
            .find('=')
            .map(|eq_pos| code[eq_pos + 1..].trim_end_matches(';').trim().to_string());

        decls.push(NamespaceScopeDecl {
            line_number: index + 1,
            name,
            type_pretty,
            is_constexpr,
            rhs,
        });
    }

    decls
}

struct SingletonElisionChecker;

impl FileContentChecker for SingletonElisionChecker {
    fn name(&self) -> &'static str {
        "SingletonElisionChecker"
    }

    fn should_process_file(&self, file_path: &str, project_root: &Path) -> bool {
        // TU sources under the project's `src/` directory ONLY. Anchor to
        // project root so scratch paths like `tests/fbuild_qemu_smoke/src/`
        // are NOT mistakenly caught (they contain `/src/` as a substring
        // but are user-sketch scaffolding, not FastLED library sources).
        //
        // Excludes:
        // - `src/third_party/` — upstream code we don't rewrite
        // - `src/fl/stl/singleton.h` itself — it IS the implementation of
        //   the pattern we're pushing everyone toward
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
            // Skip `constexpr` when the value is a pure literal (compiler
            // will inline; no storage). Keep the check when it's not
            // literal-shaped since address-taken constexpr still emits.
            if decl.is_constexpr {
                if let Some(rhs) = decl.rhs.as_deref() {
                    if is_trivial_rhs(rhs) {
                        continue;
                    }
                }
            }

            // Integer constants belong to PreferConstexprChecker. Reporting
            // them here as well would hand one declaration two different
            // fixes, which is exactly the conflation FastLED#3483 removes.
            if is_prefer_constexpr_candidate(&decl, &file_content.lines, &file_content.path) {
                continue;
            }

            violations.push((
                decl.line_number,
                format!(
                    "namespace-scope `{name}` (type `{ty}`) is emitted \
                     unconditionally; wrap in `fl::Singleton<T>` for \
                     linker elision. Applies to mutable state, `const` \
                     tables, and `FL_PROGMEM` data alike — put the value \
                     inside a class as a member, access via \
                     `fl::Singleton<T>::instance().field`. If the storage \
                     genuinely cannot move (linker-invisible reference, \
                     ISR-only IRAM placement, singleton-registry backing), \
                     annotate with `[[gnu::used]]` / `FL_KEEP` or a \
                     preceding `// FL_LINT_ALLOW_GLOBAL(<reason>)` \
                     comment line.",
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
