// Container raw-pointer checkers (FastLED#3287).
//
// PR #3282 re-laid `fl::deque<T>` out as a chunked map of fixed-size blocks.
// Pointer stability across push is now guaranteed, but the elements are NO
// LONGER contiguous, so every `&dq[0]` / `dq.data()`-style "treat the container
// as a flat buffer" idiom became an immediate buffer overrun (#3286 was one
// such crash in the audio detector; `json::to_string_native` was another).
//
// Two checkers live here, deliberately split because the warn-only mechanism
// is per-checker rather than per-violation:
//
// - `ContainerNonContiguousPtrChecker` (hard fail) -- `.data()` and
//   address-of-element on containers whose storage is NOT contiguous
//   (`fl::deque`, `fl::circular_buffer`). This is the actual UB class. The
//   tree is already clean, so the checker gates regressions from day one.
// - `ContainerElementAddressChecker` (warn only) -- address-of-element on
//   containers that ARE contiguous today (`fl::vector`, `fl::string`, ...).
//   Not UB, but brittle to layout changes and bounds-check-bypassing;
//   `fl::span<T>` or an iterator pair says the same thing safely. `.data()`
//   is deliberately NOT flagged here: it is the idiomatic contiguous accessor.
//
// Both are same-file type-aware rather than blanket regex. A blanket
// `\.data\(\)` scan matches ~460 sites tree-wide and nearly all of them are
// legitimate. Instead we pre-scan the file for `fl::<container><...> name`
// declarations, build a name -> kind map, and only then look at uses of those
// exact names. A variable with no visible `fl::` declaration in the same file
// is never flagged.
//
// Suppression: `// fl-lint: container-data-ptr-ok`, either as a line-suffix
// comment or on the immediately preceding comment-only line (same shape as
// `FL_LINT_ALLOW_GLOBAL` in checkers/singleton_elision.rs).

const CONTAINER_PTR_SUPPRESSION: &str = "fl-lint: container-data-ptr-ok";

/// `fl::` containers whose elements are NOT guaranteed contiguous.
const NON_CONTIGUOUS_CONTAINERS: &[&str] = &["deque", "circular_buffer"];

/// `fl::` containers whose elements are contiguous today.
const CONTIGUOUS_CONTAINERS: &[&str] = &[
    "vector",
    "fixed_vector",
    "inlined_vector",
    "string",
    "array",
];

/// Keywords that may legally precede a unary `&`. Anything else that looks
/// like an identifier before `&` means the `&` is the binary bitwise-AND
/// operator (`mask & table[i]`), not an address-of.
const ADDRESS_OF_PRECEDING_KEYWORDS: &[&str] = &[
    "return",
    "co_return",
    "co_yield",
    "case",
    "throw",
    "new",
    "delete",
    "sizeof",
    "and",
    "or",
    "not",
];

#[derive(Clone, Copy, PartialEq, Eq, Debug)]
enum ContainerKind {
    NonContiguous,
    Contiguous,
}

fn classify_container(type_name: &str) -> Option<ContainerKind> {
    if NON_CONTIGUOUS_CONTAINERS.contains(&type_name) {
        return Some(ContainerKind::NonContiguous);
    }
    if CONTIGUOUS_CONTAINERS.contains(&type_name) {
        return Some(ContainerKind::Contiguous);
    }
    None
}

/// One file, pre-processed: comment/string-free code per line, per-line
/// suppression flags, and the `fl::` container declarations found anywhere in
/// the file (declarations frequently appear BELOW their uses -- private
/// members at the bottom of a class -- so this must be a whole-file pre-pass).
struct ContainerScan {
    code: Vec<String>,
    suppressed: Vec<bool>,
    declarations: HashMap<String, ContainerKind>,
}

fn scan_container_file(file_content: &FileContent) -> ContainerScan {
    let mut code = Vec::with_capacity(file_content.lines.len());
    let mut suppressed = Vec::with_capacity(file_content.lines.len());
    let mut declarations: HashMap<String, ContainerKind> = HashMap::new();
    let mut shadowed: HashSet<String> = HashSet::new();

    let mut in_block_comment = CommentScanState::default();
    let mut previous_line_suppresses = false;

    for line in &file_content.lines {
        let trimmed = line.trim();
        let has_marker = line.contains(CONTAINER_PTR_SUPPRESSION);
        let comment_only = trimmed.starts_with("//") || trimmed.starts_with("/*");

        suppressed.push(has_marker || previous_line_suppresses);
        previous_line_suppresses = has_marker && comment_only;

        let visible = strip_block_comments_from_line(line, &mut in_block_comment);
        let without_comment = split_line_comment(&visible);
        let cleaned = if trimmed.starts_with('#') {
            // Preprocessor lines are never declarations or expressions we can
            // reason about; blank them out rather than special-casing later.
            String::new()
        } else {
            strip_string_literals(without_comment)
        };

        collect_container_declarations(&cleaned, &mut declarations);
        if let Some(name) = raw_array_declaration_name(&cleaned) {
            shadowed.insert(name);
        }
        code.push(cleaned);
    }

    // Names are matched textually within one file, so a name that is ALSO a
    // raw C array somewhere in the file is ambiguous. Raw arrays are out of
    // scope for FastLED#3287, so drop the name rather than risk a false
    // positive. Real case: tests/fl/slice.cpp declares both
    // `fl::array<int, 4> arr` and `int arr[] = {5, 6, 7}` in sibling
    // FL_SUBCASE blocks.
    declarations.retain(|name, _| !shadowed.contains(name));

    ContainerScan {
        code,
        suppressed,
        declarations,
    }
}

/// Record `fl::<container>[<args>] [const|&|*] name` declarations found in one
/// cleaned line. Handles locals, members, references, and constructor-call
/// initialization (`fl::deque<T> d(cap);`).
fn collect_container_declarations(code: &str, declarations: &mut HashMap<String, ContainerKind>) {
    let mut rest = code;
    while let Some(position) = rest.find("fl::") {
        let after = &rest[position + 4..];
        let name_length = after
            .bytes()
            .take_while(|byte| byte.is_ascii_alphanumeric() || *byte == b'_')
            .count();
        let type_name = &after[..name_length];
        let mut tail = &after[name_length..];
        rest = tail;

        let Some(kind) = classify_container(type_name) else {
            continue;
        };

        tail = tail.trim_start();
        if tail.starts_with('<') {
            let Some(end) = matching_angle_bracket(tail) else {
                // Template argument list continues on the next line; the
                // declaration is not analyzable from this line alone.
                continue;
            };
            tail = &tail[end..];
        }

        // Skip qualifiers and reference/pointer decorations between the type
        // and the declared name.
        loop {
            tail = tail.trim_start();
            if let Some(next) = tail.strip_prefix('&').or_else(|| tail.strip_prefix('*')) {
                tail = next;
                continue;
            }
            if let Some(next) = strip_keyword(tail, "const").or_else(|| strip_keyword(tail, "volatile")) {
                tail = next;
                continue;
            }
            break;
        }

        let identifier_length = tail
            .bytes()
            .take_while(|byte| byte.is_ascii_alphanumeric() || *byte == b'_')
            .count();
        if identifier_length == 0 {
            continue;
        }
        let name = &tail[..identifier_length];
        let remainder = tail[identifier_length..].trim_start();
        // A declaration is followed by `;`, an initializer, a parameter
        // separator, or the end of the line. Anything else (`.`, `->`, `::`)
        // means this was a use or a qualified name, not a declaration.
        let looks_like_declaration = remainder.is_empty()
            || remainder.starts_with(';')
            || remainder.starts_with('=')
            || remainder.starts_with('(')
            || remainder.starts_with('{')
            || remainder.starts_with(',')
            || remainder.starts_with(')');
        if looks_like_declaration {
            declarations.insert(name.to_string(), kind);
        }
        rest = &tail[identifier_length..];
    }
}

/// Name of a raw C array declared on this line (`int arr[] = {...};`,
/// `u8 lane_waves[8];`), if any.
fn raw_array_declaration_name(code: &str) -> Option<String> {
    let captures = regex_raw_array_declaration().captures(code)?;
    // `return arr[0];` / `case tbl[i]:` shape the same way; reject when the
    // leading token is a statement keyword rather than a type.
    const STATEMENT_KEYWORDS: &[&str] = &[
        "return", "case", "throw", "delete", "else", "do", "goto", "co_return",
    ];
    if STATEMENT_KEYWORDS.contains(&&captures[1]) {
        return None;
    }
    Some(captures[2].to_string())
}

fn strip_keyword<'a>(text: &'a str, keyword: &str) -> Option<&'a str> {
    let rest = text.strip_prefix(keyword)?;
    match rest.bytes().next() {
        Some(byte) if byte.is_ascii_alphanumeric() || byte == b'_' => None,
        _ => Some(rest),
    }
}

/// Byte offset just past the `>` that closes the `<` at the start of `text`.
fn matching_angle_bracket(text: &str) -> Option<usize> {
    let mut depth = 0usize;
    for (index, character) in text.char_indices() {
        match character {
            '<' => depth += 1,
            '>' => {
                depth -= 1;
                if depth == 0 {
                    return Some(index + 1);
                }
            }
            ';' | '{' => return None,
            _ => {}
        }
    }
    None
}

/// `name.data()` / `name->data()` receivers on one cleaned line.
fn find_data_call_receivers(code: &str) -> Vec<String> {
    regex_container_data_call()
        .captures_iter(code)
        .map(|capture| capture[1].to_string())
        .collect()
}

/// `(receiver, human-readable form)` for every address-of-element expression
/// on one cleaned line: `&name[i]`, `&name.at(i)`, `&name.front()`,
/// `&name.back()`.
///
/// Hand-rolled rather than regex because the naive `&\s*(\w+)\s*\[` pattern
/// happily matches the SECOND `&` of a logical-AND: `annexB[i] == 0 &&
/// annexB[i + 1] == 0` produced 7 false positives out of 11 hits during
/// development. The `regex` crate has no lookaround, so the `&&` rejection and
/// the unary-vs-bitwise disambiguation are done by inspecting neighbours.
fn find_element_address_uses(code: &str) -> Vec<(String, String)> {
    let bytes = code.as_bytes();
    let mut uses = Vec::new();
    let mut index = 0usize;

    while index < bytes.len() {
        if bytes[index] != b'&' {
            index += 1;
            continue;
        }
        // `&&` in either direction is the logical-AND operator (or an rvalue
        // reference declarator), never an address-of.
        if bytes.get(index + 1) == Some(&b'&') {
            index += 2;
            continue;
        }
        if index > 0 && bytes[index - 1] == b'&' {
            index += 1;
            continue;
        }
        if !is_unary_address_of(code, index) {
            index += 1;
            continue;
        }

        // Walk the whole receiver chain: `&a[0]`, `&this->a[0]`,
        // `&obj.a[0]`, `&mState.mQueue[i]`. The container is named by the
        // LAST identifier before the subscript or accessor, so re-anchor on
        // each member as the chain is consumed.
        //
        // Without this, a member-qualified receiver was silently skipped --
        // and members are the common shape for fl::deque FIELDS, which is
        // precisely what the whole-file declaration scan exists to catch. It
        // was a hole in the hard-fail tier, not just the warn tier.
        let mut cursor = index + 1;
        loop {
            while cursor < bytes.len() && (bytes[cursor] == b' ' || bytes[cursor] == b'\t') {
                cursor += 1;
            }
            let name_start = cursor;
            while cursor < bytes.len()
                && (bytes[cursor].is_ascii_alphanumeric() || bytes[cursor] == b'_')
            {
                cursor += 1;
            }
            if cursor == name_start {
                break;
            }
            let name = code[name_start..cursor].to_string();

            let mut after = cursor;
            while after < bytes.len() && (bytes[after] == b' ' || bytes[after] == b'\t') {
                after += 1;
            }

            // `name[` -- subscript on the thing we just named.
            if after < bytes.len() && bytes[after] == b'[' {
                let form = format!("&{name}[...]");
                uses.push((name, form));
                cursor = after;
                break;
            }

            // `name->member` / `name.member`
            let separator = if after + 1 < bytes.len()
                && bytes[after] == b'-'
                && bytes[after + 1] == b'>'
            {
                2
            } else if after < bytes.len() && bytes[after] == b'.' {
                1
            } else {
                cursor = after;
                break;
            };

            let mut member_start = after + separator;
            while member_start < bytes.len()
                && (bytes[member_start] == b' ' || bytes[member_start] == b'\t')
            {
                member_start += 1;
            }
            let mut member_end = member_start;
            while member_end < bytes.len()
                && (bytes[member_end].is_ascii_alphanumeric() || bytes[member_end] == b'_')
            {
                member_end += 1;
            }
            if member_end == member_start {
                cursor = member_end;
                break;
            }
            let member = &code[member_start..member_end];

            let mut paren = member_end;
            while paren < bytes.len() && (bytes[paren] == b' ' || bytes[paren] == b'\t') {
                paren += 1;
            }

            // An accessor CALL terminates the chain: `&x.front()`.
            if paren < bytes.len() && bytes[paren] == b'(' {
                match member {
                    "at" => uses.push((name.clone(), format!("&{name}.at(...)"))),
                    "front" => uses.push((name.clone(), format!("&{name}.front()"))),
                    "back" => uses.push((name.clone(), format!("&{name}.back()"))),
                    _ => {}
                }
                cursor = paren;
                break;
            }

            // Plain member access: re-anchor on the member and keep walking.
            cursor = member_start;
        }
        index = index + 1;
    }

    uses
}

/// True when the `&` at `position` is the unary address-of operator rather
/// than binary bitwise-AND.
fn is_unary_address_of(code: &str, position: usize) -> bool {
    let bytes = code.as_bytes();
    let mut back = position;
    while back > 0 && (bytes[back - 1] == b' ' || bytes[back - 1] == b'\t') {
        back -= 1;
    }
    if back == 0 {
        return true;
    }
    let previous = bytes[back - 1];
    if previous.is_ascii_alphanumeric() || previous == b'_' {
        // `return &v[0]` is unary; `mask & table[0]` is binary. Tell them
        // apart by the identifier that precedes the operator.
        let mut word_start = back - 1;
        while word_start > 0
            && (bytes[word_start - 1].is_ascii_alphanumeric() || bytes[word_start - 1] == b'_')
        {
            word_start -= 1;
        }
        let word = &code[word_start..back];
        return ADDRESS_OF_PRECEDING_KEYWORDS.contains(&word);
    }
    // A closing bracket or a literal means the previous token produced a
    // value, so the `&` binds as binary AND.
    !matches!(previous, b')' | b']' | b'"' | b'\'')
}

fn container_ptr_should_process(file_path: &str) -> bool {
    if !ends_with_any(file_path, &[".cpp.hpp", ".cpp", ".h", ".hpp", ".ino", ".cc", ".cxx"]) {
        return false;
    }
    let normalized = normalize_path(file_path);
    !is_under_dir(&normalized, "third_party")
}

struct ContainerNonContiguousPtrChecker;

impl FileContentChecker for ContainerNonContiguousPtrChecker {
    fn name(&self) -> &'static str {
        "ContainerNonContiguousPtrChecker"
    }

    fn should_process_file(&self, file_path: &str, _project_root: &Path) -> bool {
        container_ptr_should_process(file_path)
    }

    fn check_file_content(&self, file_content: &FileContent) -> Vec<(usize, String)> {
        let scan = scan_container_file(file_content);
        if scan.declarations.is_empty() {
            return Vec::new();
        }

        let mut violations = Vec::new();
        for (index, code) in scan.code.iter().enumerate() {
            if scan.suppressed[index] || code.trim().is_empty() {
                continue;
            }

            for receiver in find_data_call_receivers(code) {
                if scan.declarations.get(&receiver) == Some(&ContainerKind::NonContiguous) {
                    violations.push((
                        index + 1,
                        format!(
                            "`{receiver}.data()` on a non-contiguous fl:: container: its \
                             elements are stored in chunks, so the returned pointer does not \
                             address the whole sequence (FastLED#3287). Use an iterator pair \
                             `[begin, end)`, or copy into a contiguous buffer first: \
                             `fl::vector<T> flat({receiver}.begin(), {receiver}.end());`. \
                             Genuine C-API interop can add a \
                             `// {CONTAINER_PTR_SUPPRESSION}` comment with a reason."
                        ),
                    ));
                }
            }

            for (receiver, form) in find_element_address_uses(code) {
                if scan.declarations.get(&receiver) == Some(&ContainerKind::NonContiguous) {
                    violations.push((
                        index + 1,
                        format!(
                            "`{form}` takes the address of an element of non-contiguous \
                             fl:: container `{receiver}`; treating it as a flat buffer is \
                             undefined behaviour (FastLED#3287). Use an iterator pair \
                             `[begin, end)`, or copy into a contiguous buffer first: \
                             `fl::vector<T> flat({receiver}.begin(), {receiver}.end());`. \
                             Genuine C-API interop can add a \
                             `// {CONTAINER_PTR_SUPPRESSION}` comment with a reason."
                        ),
                    ));
                }
            }
        }

        violations
    }
}

struct ContainerElementAddressChecker;

impl FileContentChecker for ContainerElementAddressChecker {
    fn name(&self) -> &'static str {
        "ContainerElementAddressChecker"
    }

    fn should_process_file(&self, file_path: &str, _project_root: &Path) -> bool {
        container_ptr_should_process(file_path)
    }

    fn check_file_content(&self, file_content: &FileContent) -> Vec<(usize, String)> {
        let scan = scan_container_file(file_content);
        if scan.declarations.is_empty() {
            return Vec::new();
        }

        let mut violations = Vec::new();
        for (index, code) in scan.code.iter().enumerate() {
            if scan.suppressed[index] || code.trim().is_empty() {
                continue;
            }
            for (receiver, form) in find_element_address_uses(code) {
                if scan.declarations.get(&receiver) == Some(&ContainerKind::Contiguous) {
                    violations.push((
                        index + 1,
                        format!(
                            "`{form}` takes a raw pointer into fl:: container `{receiver}`. \
                             Contiguity holds today, but the pattern is brittle to layout \
                             changes and bypasses bounds checking (FastLED#3287) -- the same \
                             idiom became undefined behaviour on `fl::deque` after the \
                             chunked-storage rewrite. Prefer `fl::span<T>` for a bounded \
                             view, or an iterator / reference. Genuine C-API interop can add \
                             a `// {CONTAINER_PTR_SUPPRESSION}` comment with a reason."
                        ),
                    ));
                }
            }
        }

        violations
    }
}
