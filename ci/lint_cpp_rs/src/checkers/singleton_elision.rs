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
        // File-level opt-out: if any of the first 15 lines contains
        // `FL_LINT_ALLOW_GLOBAL_FILE(<reason>)`, skip the entire file.
        // Used for Tier 2 driver files pending migration to Singleton<T>
        // via their own sub-issues (see FastLED#3481) — annotating 10+
        // sites individually is noise; a single file-header marker
        // captures the intent cleanly.
        for line in file_content.lines.iter().take(15) {
            if line.contains("FL_LINT_ALLOW_GLOBAL_FILE") {
                return Vec::new();
            }
        }

        let mut violations = Vec::new();

        // Brace-depth tracker. Depth 0 = file scope. Inside `namespace X {`
        // the depth counter goes to 1 but that's *still* namespace-scope
        // storage (variables there still get emitted unconditionally), so
        // we also track a "namespace stack" of open braces that came from a
        // `namespace X {` line and treat those depths as file-scope-
        // equivalent for the purposes of this check.
        let mut depth: i32 = 0;
        let mut namespace_stack: Vec<i32> = Vec::new(); // depths that are inside `namespace X {`
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

            // Snapshot the previous line's carry-over marker BEFORE we
            // update it for the next iteration. The current line uses
            // `effective_marker` below; the flag we set for next iteration
            // carries only when THIS line is comment-only (so the marker
            // "belongs" to the following declaration).
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

            // Track `namespace X {` opens. Regex-lite: any line that starts
            // with `namespace` and ends with `{` (possibly with an ID) opens
            // a namespace at the current depth+1.
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

            // Now: is this line a candidate variable-definition at
            // namespace-scope (or file-scope, depth 0)?
            //
            // Namespace-scope means depth ∈ namespace_stack ∪ {0}. Depth 0
            // covers TU scope; each entry in namespace_stack covers one
            // `namespace X { ... }` block. Anything deeper is either inside
            // a function body or a class body — both are fine.
            //
            // BUT we must evaluate the depth BEFORE consuming this line's
            // opening brace (so a line like `int gCounter = 0;` at TU scope
            // is checked against depth 0, not depth 1 that we'd see if the
            // line ended in `{` — which it doesn't for a var-def).
            //
            // We compute an "effective" line-scope depth: depth after
            // processing this line's braces. Then we allow the check only
            // if this line has no `{` or `}` (a pure declaration line).
            if code.contains('{') || code.contains('}') {
                continue;
            }

            let namespace_scope = depth == 0 || namespace_stack.contains(&depth);
            if !namespace_scope {
                continue;
            }

            // Inside `extern "C" { ... }` blocks the storage-emission
            // behaviour is identical to namespace scope, so still catch.
            // (We track the block but don't treat it as an escape.)

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
            if trimmed.starts_with("namespace ") || trimmed.starts_with("class ")
                || trimmed.starts_with("struct ") || trimmed.starts_with("enum ")
                || trimmed.starts_with("union ")
                || trimmed.starts_with("template<") || trimmed.starts_with("template ")
                || trimmed.starts_with("template ")
                || trimmed.starts_with("friend ")
                || trimmed.starts_with("public:") || trimmed.starts_with("private:")
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

            // Must end with `;` (variable definitions are single-line for
            // this pass — multi-line brace-initializer defs get caught on
            // the closing-brace/semicolon line, which will be `}` or `};`
            // and skipped by the brace filter above; that's acceptable
            // false-negative territory for the first pass).
            if !trimmed.ends_with(';') {
                continue;
            }

            // Must NOT look like a function declaration / call.
            // Heuristic: presence of `(` before the first `;` and the
            // parens contain more than a single value (which could be an
            // initializer like `int x(5);`). The safe cut: any `(` before
            // `;` → skip.
            if let Some(paren_pos) = code.find('(') {
                if let Some(semi_pos) = code.find(';') {
                    if paren_pos < semi_pos {
                        continue;
                    }
                }
            }

            // Opt-outs on the declaration line itself.
            //
            // - `[[gnu::used]]` / `__attribute__((used))`: compile-time
            //   directive telling the linker to keep the symbol regardless
            //   of use. Author has thought about this.
            // - `FL_KEEP_ALIVE` / `FL_KEEP`: FastLED's export macro; same
            //   semantics as above.
            // - `FL_LINT_ALLOW_GLOBAL(<reason>)`: linter-only comment marker
            //   with no compile-time effect. Use this when the storage
            //   genuinely cannot move to `Singleton<T>` (public API surface,
            //   ISR cache pointer, singleton-registry backing, host-only
            //   entry point). The `<reason>` argument is mandatory — the
            //   reviewer must see justification for each escape.
            //
            // Opt-out honored either inline on the declaration line or on
            // the immediately preceding comment line.
            if this_line_marker || carry_marker {
                continue;
            }

            // Skip `constexpr` when the value is a pure literal (compiler
            // will inline; no storage). Keep the check when it's not
            // literal-shaped since address-taken constexpr still emits.
            // Simple heuristic: if the RHS after `=` is a simple integer
            // literal, string literal, or `nullptr`, skip.
            // (We deliberately keep other constexpr in the violation set —
            // author can inspect and move to Singleton<T> or annotate.)
            let is_constexpr = trimmed.starts_with("constexpr ")
                || trimmed.starts_with("static constexpr ")
                || trimmed.starts_with("inline constexpr ");
            if is_constexpr {
                if let Some(eq_pos) = code.find('=') {
                    let rhs = code[eq_pos + 1..].trim_end_matches(';').trim();
                    let is_trivial = rhs.parse::<i64>().is_ok()
                        || rhs.parse::<f64>().is_ok()
                        || rhs == "nullptr"
                        || rhs == "true"
                        || rhs == "false"
                        || (rhs.starts_with('"') && rhs.ends_with('"'))
                        || (rhs.starts_with('\'') && rhs.ends_with('\''));
                    if is_trivial {
                        continue;
                    }
                }
            }

            // At this point the line looks like a namespace-scope variable
            // definition. Emit it.
            //
            // Try to extract the identifier name for the report. Take the
            // token immediately before `=`, `;`, or `[` — whichever comes
            // first — walking back to the last non-identifier boundary.
            let end_marker = ['=', ';', '['];
            let end_pos = code
                .find(|c: char| end_marker.contains(&c))
                .unwrap_or(code.len());
            let head = &code[..end_pos];

            // Class-static-member out-of-class definitions like
            // `constexpr bool numeric_limits<char>::is_signed = true;` or
            // `const float SpectralEqualizer::A_WEIGHTING_16BAND[16] = ...;`
            // — the `::` in the name path is the giveaway. C++11 requires
            // these; they don't emit storage unless ODR-used. Skip.
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
            // Try to grab the type — everything before the name.
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

            violations.push((
                index + 1,
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
                    name = name,
                    ty = if type_pretty.is_empty() {
                        "?"
                    } else {
                        &type_pretty
                    }
                ),
            ));
        }

        violations
    }
}
