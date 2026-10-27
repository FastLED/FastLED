// Source syntax is deliberately parsed without a preprocessor: inactive ESP
// branches must be checked too, without downloading any vendor SDK.
struct EspNoexceptChecker;

fn esp_noexcept_visible(content: &str) -> String {
    let mut state = CommentScanState::default();
    content
        .split_inclusive('\n')
        .map(|line| {
            let mut masked = mask_macro_prefix_literals_and_comments(line, &mut state);
            if line.ends_with('\n') {
                masked.pop();
                masked.push('\n');
            }
            masked
        })
        .collect()
}

impl FileContentChecker for EspNoexceptChecker {
    fn name(&self) -> &'static str {
        "EspNoexceptChecker"
    }

    fn should_process_file(&self, path: &str, root: &Path) -> bool {
        let path = normalize_path(path);
        ends_with_any(&path, &[".h", ".hpp", ".cpp"])
            && (is_under_project_subpath(&path, root, "src/platforms/esp")
                || is_under_project_subpath(&path, root, "src/fl/fs/sd"))
            && !path.contains("/third_party/")
    }

    fn check_file_content(&self, file: &FileContent) -> Vec<(usize, String)> {
        let macros = esp_noexcept_macro_ranges(&file.content);
        let mut bytes = esp_noexcept_source(&file.content).into_bytes();
        // tree-sitter does not lex every C preprocessor continuation/comment
        // combination. Replacement lists are parsed separately below.
        for range in &macros {
            let start = file.content[..range.start]
                .rfind('\n')
                .map_or(0, |index| index + 1);
            for byte in &mut bytes[start..range.end] {
                if *byte != b'\n' {
                    *byte = b' ';
                }
            }
        }
        let source = String::from_utf8(bytes).unwrap();
        let mut parser = tree_sitter::Parser::new();
        if let Err(error) = parser.set_language(&tree_sitter_cpp::LANGUAGE.into()) {
            return vec![(1, format!("Cannot load ESP C++ grammar: {error}"))];
        }
        let Some(tree) = parser.parse(&source, None) else {
            return vec![(1, "Cannot parse ESP C++ source for FL_NO_EXCEPT".into())];
        };
        let mut hits = Vec::new();
        let mut pending = vec![tree.root_node()];
        while let Some(node) = pending.pop() {
            if node.is_error() || node.is_missing() {
                hits.push((node.start_position().row + 1,
                    format!("Cannot check FL_NO_EXCEPT: unparsed C++ {} (normalize the syntax instead of skipping it)", node.kind())));
                // Report one diagnostic per broken region, not every child.
                continue;
            }
            if matches!(node.kind(), "function_declarator" | "operator_cast")
                && !esp_noexcept_misparsed_assignment(node, &source)
            {
                if let Some(hit) = esp_noexcept_function(node, &source, &file.content) {
                    hits.push(hit);
                }
            }
            if let Some(hit) = esp_noexcept_scope_call(node, &source) {
                hits.push(hit);
            }
            let mut cursor = node.walk();
            pending.extend(node.children(&mut cursor));
        }
        for range in macros {
            let line = file.content[..range.start]
                .bytes()
                .filter(|byte| *byte == b'\n')
                .count();
            hits.extend(esp_noexcept_macro(&file.content[range], line));
        }
        hits.sort();
        hits.dedup();
        hits
    }
}

fn esp_noexcept_scope_call(node: tree_sitter::Node<'_>, source: &str) -> Option<(usize, String)> {
    // A bare member macro parses as a declaration with a type and no name,
    // including inside a local class nested in an annotated function body.
    if node.kind() == "field_declaration" && node.child_by_field_name("declarator").is_none() {
        if let Some(kind) = node
            .child_by_field_name("type")
            .filter(|kind| matches!(kind.kind(), "type_identifier" | "qualified_identifier"))
        {
            return Some((
                node.start_position().row + 1,
                format!(
                    "Cannot check FL_NO_EXCEPT: unknown member declaration macro {}",
                    &source[kind.byte_range()]
                ),
            ));
        }
    }
    if node.kind() != "expression_statement" {
        return None;
    }
    let mut ancestor = node.parent();
    while let Some(parent) = ancestor {
        if matches!(
            parent.kind(),
            "compound_statement"
                | "function_definition"
                | "declaration"
                | "field_declaration"
                | "init_declarator"
        ) {
            return None;
        }
        ancestor = parent.parent();
    }
    let expression = node.named_child(0)?;
    let expression = if expression.kind() == "assignment_expression" {
        expression.child_by_field_name("left")?
    } else {
        expression
    };
    let function = expression
        .child_by_field_name("function")
        .unwrap_or(expression);
    // The grammar ambiguously parses `Type (*callback)()` as two calls at
    // namespace scope. A pointer to a named object is still a type declaration;
    // `Type (*factory())()` is excluded because its argument is itself a call.
    if function.kind() == "call_expression" {
        if let Some(arguments) = function.child_by_field_name("arguments") {
            if arguments.named_child_count() == 1 {
                if let Some(pointer) = arguments
                    .named_child(0)
                    .filter(|arg| arg.kind() == "pointer_expression")
                {
                    if pointer.child_by_field_name("argument").is_some_and(|arg| {
                        matches!(
                            arg.kind(),
                            "identifier" | "qualified_identifier" | "subscript_expression"
                        )
                    }) {
                        return None;
                    }
                }
            }
        }
    }
    let name = &source[function.byte_range()];
    // _FL_DEFPIN creates an empty FastPin specialization; FASTLED_SHARED_PTR
    // creates only a forward declaration and a using alias (shared_ptr.h).
    // Their replacement lists remain checked for callable signatures.
    if matches!(name, "_FL_DEFPIN" | "FASTLED_SHARED_PTR") {
        return None;
    }
    Some((node.start_position().row + 1, format!("Cannot check FL_NO_EXCEPT: unknown declaration macro {name}; write the callable signature explicitly")))
}

// Preprocessor replacement lists are opaque to the C++ grammar. Parse those
// too, so a trampoline-generating macro cannot hide an unannotated function.
fn esp_noexcept_macro_ranges(content: &str) -> Vec<std::ops::Range<usize>> {
    static HEADER: std::sync::OnceLock<regex::Regex> = std::sync::OnceLock::new();
    let header = HEADER.get_or_init(|| {
        regex::Regex::new(r"(?m)^[ \t]*#[ \t]*define[ \t]+[A-Za-z_]\w*(?:\([^()\n]*\))?").unwrap()
    });
    let visible = esp_noexcept_visible(content);
    header
        .find_iter(&visible)
        .map(|found| {
            let mut end = found.end();
            while end < content.len() {
                let newline = content[end..]
                    .find('\n')
                    .map_or(content.len(), |offset| end + offset);
                let continuation = content[end..newline].trim_end_matches('\r').ends_with('\\');
                end = newline;
                if !continuation || end == content.len() {
                    break;
                }
                end += 1;
            }
            found.end()..end
        })
        .collect()
}

fn esp_noexcept_macro(body: &str, start_line: usize) -> Vec<(usize, String)> {
    if !body.contains('(') {
        return Vec::new();
    }
    let body = body.replace("\\\r\n", " \r\n").replace("\\\n", " \n");
    static PASTE: std::sync::OnceLock<regex::Regex> = std::sync::OnceLock::new();
    static STRINGIFY: std::sync::OnceLock<regex::Regex> = std::sync::OnceLock::new();
    let paste = PASTE.get_or_init(|| regex::Regex::new(r"\b\w+[ \t]*##[ \t]*\w+\b").unwrap());
    let stringify = STRINGIFY.get_or_init(|| regex::Regex::new(r"#[ \t]*[A-Za-z_]\w*").unwrap());
    let visible = esp_noexcept_visible(&body);
    let mut bytes = body.as_bytes().to_vec();
    for found in paste.find_iter(&visible) {
        bytes[found.range()].fill(b'_');
    }
    loop {
        let pasted = String::from_utf8(bytes.clone()).unwrap();
        let visible = esp_noexcept_visible(&pasted);
        let Some(found) = paste.find(&visible) else {
            break;
        };
        bytes[found.range()].fill(b'_');
    }
    let pasted = String::from_utf8(bytes.clone()).unwrap();
    let visible = esp_noexcept_visible(&pasted);
    for found in stringify.find_iter(&visible) {
        bytes[found.range()].fill(b' ');
        bytes[found.start()..found.start() + 2].copy_from_slice(b"\"\"");
    }
    let body = String::from_utf8(bytes).unwrap();
    let mut parser = tree_sitter::Parser::new();
    parser
        .set_language(&tree_sitter_cpp::LANGUAGE.into())
        .expect("grammar already checked");
    // A replacement list may be a namespace declaration or a statement/expression
    // intended for a function body. Require one complete, error-free parse.
    for (prefix, suffix) in [
        ("", "\n;"),
        ("void lint_macro_wrapper() noexcept {\n", "\n;\n}"),
        ("struct lint_macro_wrapper {\n", "\n;\n};"),
    ] {
        let expanded = format!("{prefix}{body}{suffix}");
        let source = esp_noexcept_source(&expanded);
        let Some(tree) = parser.parse(&source, None) else {
            continue;
        };
        if tree.root_node().has_error() {
            continue;
        }
        let line_offset = usize::from(!prefix.is_empty());
        let mut pending = vec![tree.root_node()];
        let mut hits = Vec::new();
        while let Some(child) = pending.pop() {
            if matches!(child.kind(), "function_declarator" | "operator_cast")
                && child.start_byte() >= prefix.len()
            {
                if let Some((line, message)) = esp_noexcept_function(child, &source, &expanded) {
                    hits.push((start_line + line - line_offset, message));
                }
            }
            let mut cursor = child.walk();
            pending.extend(child.children(&mut cursor));
        }
        return hits;
    }
    // Attributes and deliberately incomplete syntax macros have no callable
    // signature. A body that can generate a declaration must parse completely.
    static SIGNATURE: std::sync::OnceLock<regex::Regex> = std::sync::OnceLock::new();
    let callable = SIGNATURE.get_or_init(|| {
        regex::Regex::new(r"\b[A-Za-z_]\w*(?:::\w+)*[ \t*&]+[A-Za-z_]\w*\s*\(").unwrap()
    });
    if body.contains('{') || body.contains(';') || callable.is_match(&body) {
        vec![(
            start_line + 1,
            "Cannot check FL_NO_EXCEPT: unparsed macro replacement list".into(),
        )]
    } else {
        Vec::new()
    }
}

// Keep byte offsets and newlines unchanged so diagnostics and suppression
// comments always refer to the actual file. Only known syntactic annotation
// macros are normalized; unknown syntax is an error, never an exemption.
fn esp_noexcept_source(content: &str) -> String {
    static MACROS: std::sync::OnceLock<regex::Regex> = std::sync::OnceLock::new();
    let macros = MACROS.get_or_init(|| regex::Regex::new(concat!(
        r"\b(?:FL_OPTIMIZE_FUNCTION\s*\([^()]*\)|FL_ALIGNAS\s*\([^()]*\)|",
        r"(?:FL_HAS_INCLUDE|__has_include)\s*\([^()]*\)|",
        r"(?:FL_NO_EXCEPT|FL_NOEXCEPT|FASTLED_FORCE_INLINE|FL_NO_INLINE|FL_COLD|FL_NO_RETURN|FL_OPTIMIZE_FUNCTION|",
        r"FASTLED_REGISTER|FL_STATIC_ASSERT|_Static_assert|FL_DISABLE_WARNING_PUSH|FL_DISABLE_WARNING_POP|",
        r"FL_DISABLE_WARNING_DEPRECATED_REGISTER|FL_DISABLE_WARNING_VOLATILE|",
        r"DEFINE_ESP_INT16_64_TYPES|DEFINE_ESP_INT32_TYPES|DEFINE_ESP_POINTER_TYPES|",
        r"DEFINE_ESP8266_INT_TYPES|DEFINE_ESP8266_POINTER_TYPES|PLATFORM_INT32_CONDITIONAL_CHOOSE|",
        r"IRAM_ATTR|FL_IRAM|DRAM_ATTR|RTC_NOINIT_ATTR|FL_PROGMEM|FL_ALIGN_PROGMEM|FL_LINK_WEAK|",
        r"FL_EXTERN_C_BEGIN|FL_EXTERN_C_END|FL_EXTERN_C|",
        r"FASTLED_NAMESPACE_BEGIN|FASTLED_NAMESPACE_END|",
        r"FL_NAMESPACE_BEGIN|FL_NAMESPACE_END|",
        r"FL_OPTIMIZATION_LEVEL_O3_BEGIN|FL_OPTIMIZATION_LEVEL_O3_END)\b)"
    )).unwrap());
    let mut bytes = content.as_bytes().to_vec();
    let visible = esp_noexcept_visible(content);
    for found in macros.find_iter(&visible) {
        let line_start = content[..found.start()]
            .rfind('\n')
            .map_or(0, |index| index + 1);
        let prefix = content[line_start..found.start()].trim_start();
        // Macro definition/redefinition names are preprocessor syntax, not
        // annotations. Rewriting them can turn valid directives into errors.
        if prefix.starts_with('#')
            && !(found.as_str().starts_with("FL_HAS_INCLUDE")
                || found.as_str().starts_with("__has_include"))
        {
            continue;
        }
        let replacement = match found.as_str() {
            "FL_NO_EXCEPT" | "FL_NOEXCEPT" => "noexcept",
            "FL_STATIC_ASSERT" | "_Static_assert" => "static_assert",
            "FL_EXTERN_C_BEGIN" => "extern \"C\" {",
            "FL_EXTERN_C_END" | "FASTLED_NAMESPACE_END" | "FL_NAMESPACE_END" => "}",
            "FL_EXTERN_C" => "extern \"C\"",
            "FASTLED_NAMESPACE_BEGIN" | "FL_NAMESPACE_BEGIN" => "namespace fl {",
            value if value.starts_with("FL_HAS_INCLUDE") || value.starts_with("__has_include") => {
                "1"
            }
            _ => "",
        };
        bytes[found.range()].fill(b' ');
        bytes[found.start()..found.start() + replacement.len()]
            .copy_from_slice(replacement.as_bytes());
    }
    static REGISTER: std::sync::OnceLock<regex::Regex> = std::sync::OnceLock::new();
    let register = REGISTER.get_or_init(|| {
        regex::Regex::new(r#"\b(?:asm|__asm__)\s*\(\s*"[^"\n]*"\s*\)\s*="#).unwrap()
    });
    for found in register.find_iter(content) {
        if visible.as_bytes()[found.start()] != b' ' {
            bytes[found.start()..found.end() - 1].fill(b' ');
        }
    }
    static TEMPLATE: std::sync::OnceLock<regex::Regex> = std::sync::OnceLock::new();
    let template = TEMPLATE.get_or_init(|| regex::Regex::new(r"(?m)^[ \t]*(?:extern\s+)?template\s+(?:class|struct)\s+[A-Za-z_]\w*(?:::\w+)*(?:<[^;\n]+>)?\s*;").unwrap());
    for found in template.find_iter(&visible) {
        for byte in &mut bytes[found.range()] {
            if *byte != b'\n' {
                *byte = b' ';
            }
        }
    }
    static LINKAGE: std::sync::OnceLock<regex::Regex> = std::sync::OnceLock::new();
    let linkage = LINKAGE.get_or_init(|| regex::Regex::new(r#"(?m)^[ \t]*#\s*ifdef\s+__cplusplus[ \t]*\r?\n[ \t]*(?:extern[ \t]+"C"[ \t]*\{|\})[ \t]*\r?\n[ \t]*#\s*endif\b[^\n]*"#).unwrap());
    for found in linkage.find_iter(content) {
        let mut offset = found.start();
        for line in content[found.range()].split_inclusive('\n') {
            if line.trim_start().starts_with('#') {
                for byte in &mut bytes[offset..offset + line.len()] {
                    if *byte != b'\n' {
                        *byte = b' ';
                    }
                }
            }
            offset += line.len();
        }
    }
    static ATOMIC: std::sync::OnceLock<regex::Regex> = std::sync::OnceLock::new();
    for found in ATOMIC
        .get_or_init(|| regex::Regex::new(r"\bPERIPH_RCC_ATOMIC\s*\(\s*\)").unwrap())
        .find_iter(&visible)
    {
        bytes[found.range()].fill(b' ');
        bytes[found.start()..found.start() + 9].copy_from_slice(b"if (true)");
    }
    static MEMBER_POINTER: std::sync::OnceLock<regex::Regex> = std::sync::OnceLock::new();
    for found in MEMBER_POINTER
        .get_or_init(|| regex::Regex::new(r"\b[A-Za-z_]\w*(?:::\w+)*::\*").unwrap())
        .find_iter(&visible)
    {
        bytes[found.start()..found.end() - 1].fill(b' ');
    }
    for (index, _) in visible.match_indices("->*") {
        bytes[index + 2] = b' ';
    }
    // This exact macro wraps statement tokens in a four-lane loop. Preserve
    // its full body as a compound statement, including any nested callables.
    for (start, _) in visible.match_indices("FL_SIMD_LANE4") {
        let after = start + "FL_SIMD_LANE4".len();
        let Some(open_offset) = visible[after..].find(|ch: char| !ch.is_whitespace()) else {
            continue;
        };
        let open = after + open_offset;
        if visible.as_bytes()[open] != b'(' {
            continue;
        }
        let mut depth = 0;
        for (offset, byte) in visible.as_bytes()[open..].iter().enumerate() {
            if *byte == b'(' {
                depth += 1;
            }
            if *byte == b')' {
                depth -= 1;
            }
            if depth == 0 {
                bytes[start..=open].fill(b' ');
                bytes[start] = b'{';
                bytes[open + offset] = b'}';
                break;
            }
        }
    }
    String::from_utf8(bytes).expect("ASCII macro replacement preserves UTF-8")
}

/// Inside a function body, tree-sitter reads a register-macro store such as
/// `USIE(kUart) = 0;` as a function declarator with a pure-specifier. A
/// pure-specifier is only legal on a class member, so a `= ...` declarator
/// whose nearest scope is a block is an assignment, not a function.
fn esp_noexcept_misparsed_assignment(node: tree_sitter::Node<'_>, source: &str) -> bool {
    if !source[node.end_byte()..].trim_start().starts_with('=') {
        return false;
    }
    let mut parent = node.parent();
    while let Some(p) = parent {
        match p.kind() {
            "compound_statement" => return true,
            "field_declaration_list" | "translation_unit" => return false,
            _ => parent = p.parent(),
        }
    }
    false
}

fn esp_noexcept_function(
    mut node: tree_sitter::Node<'_>,
    source: &str,
    original: &str,
) -> Option<(usize, String)> {
    // `Type (name)(args)` with a non-primitive return type parses as a nested
    // declarator chain, so the inner `Type(name)` node is this declaration's
    // parenthesized name, not a callable of its own. The outer node owns it.
    if node.parent().is_some_and(|parent| {
        parent.kind() == "function_declarator"
            && parent
                .child_by_field_name("declarator")
                .is_some_and(|inner| inner.id() == node.id())
    }) {
        return None;
    }
    let declarator = if node.kind() == "operator_cast" {
        let name = node.child_by_field_name("type")?;
        node = node.child_by_field_name("declarator")?;
        while node.kind() != "abstract_function_declarator" {
            node = node.child_by_field_name("declarator")?;
        }
        name
    } else {
        node.child_by_field_name("declarator")?
    };
    // `void (*callback)()` describes a pointer's type, not a function owned by
    // this file. The inner function in `void (*factory())()` is still checked.
    let declarator = match declarator.kind() {
        // A shielded name, `Type (name)(args)`: parentheses are the only
        // spelling that keeps Arduino-ESP32's io_pin_remap.h function-like
        // macros from rewriting the declaration (see #4914). Unwrap to the
        // name; a parenthesized pointer/reference/array type is still skipped.
        "parenthesized_declarator" => {
            let inner = declarator.named_child(0)?;
            if matches!(
                inner.kind(),
                "identifier" | "qualified_identifier" | "field_identifier"
            ) {
                inner
            } else {
                return None;
            }
        }
        // `PinValue (name)(args)` parses as a nested declarator chain when the
        // return type is a non-primitive identifier: the grammar reads
        // `PinValue(name)` as an inner call-like declarator. Same shielded
        // declaration, so unwrap it the same way.
        "function_declarator" => {
            let inner = declarator.child_by_field_name("declarator")?;
            if !matches!(
                inner.kind(),
                "identifier" | "qualified_identifier" | "field_identifier"
            ) {
                return None;
            }
            inner
        }
        "pointer_declarator"
        | "reference_declarator"
        | "array_declarator"
        | "structured_binding_declarator" => return None,
        _ => declarator,
    };
    let mut parent = node.parent();
    let owner = loop {
        let ancestor = parent?;
        match ancestor.kind() {
            "parameter_declaration"
            | "optional_parameter_declaration"
            | "type_descriptor"
            | "type_definition"
            | "alias_declaration"
            | "lambda_expression" => return None,
            "function_definition" | "declaration" | "field_declaration" => break ancestor,
            _ => parent = ancestor.parent(),
        }
    };
    if owner.kind() != "function_definition" {
        let mut ancestor = owner.parent();
        while let Some(parent) = ancestor {
            if matches!(
                parent.kind(),
                "class_specifier" | "struct_specifier" | "union_specifier"
            ) {
                break;
            }
            if parent.kind() == "linkage_specification" {
                return None;
            }
            ancestor = parent.parent();
        }
    }
    let name = &source[declarator.byte_range()];
    // Destructors are implicitly noexcept. Deleted/defaulted functions have
    // compiler-derived exception specifications and no user body to fix.
    if name
        .rsplit("::")
        .next()
        .is_some_and(|name| name.trim_start().starts_with('~'))
    {
        return None;
    }
    let mut cursor = owner.walk();
    if owner.children(&mut cursor).any(|child| {
        matches!(
            child.kind(),
            "default_method_clause" | "delete_method_clause"
        )
    }) {
        return None;
    }
    let mut cursor = node.walk();
    if node.children(&mut cursor).any(|child| {
        child.kind() == "noexcept" && {
            let text = &source[child.byte_range()];
            let compact: String = text.chars().filter(|ch| !ch.is_whitespace()).collect();
            // Conditional contracts are not universally nothrow. They require a
            // documented exception just like a vendor virtual override.
            compact == "noexcept" || compact == "noexcept(true)"
        }
    }) {
        return None;
    }
    let signature_end = owner
        .child_by_field_name("body")
        .map_or(owner.end_byte(), |body| body.start_byte());
    let line_end = original[signature_end..]
        .find('\n')
        .map_or(original.len(), |offset| signature_end + offset);
    static SUPPRESSION: std::sync::OnceLock<regex::Regex> = std::sync::OnceLock::new();
    let suppression = SUPPRESSION.get_or_init(|| {
        regex::Regex::new(
            r"//\s*(?:ok no (?:noexcept|FL_NO_EXCEPT)|noexcept not required)\s*[:\-]\s*\S.*",
        )
        .unwrap()
    });
    // Use actual comment nodes, so a default argument containing the literal
    // string "// ok no noexcept: ..." cannot suppress a function.
    let mut root = owner;
    while let Some(parent) = root.parent() {
        root = parent;
    }
    let mut pending = vec![root];
    while let Some(comment) = pending.pop() {
        if comment.end_byte() < owner.start_byte() || comment.start_byte() > line_end {
            continue;
        }
        if comment.kind() == "comment" && comment.start_byte() >= owner.start_byte() {
            let in_signature = comment.start_byte() <= signature_end;
            let trailing_signature = comment.start_byte() >= signature_end
                && original[signature_end..comment.start_byte()]
                    .trim()
                    .chars()
                    .all(|ch| ch == '{');
            if (in_signature || trailing_signature)
                && suppression.is_match(&original[comment.byte_range()])
            {
                return None;
            }
        }
        let mut cursor = comment.walk();
        pending.extend(comment.children(&mut cursor));
    }
    Some((
        node.start_position().row + 1,
        format!("Missing FL_NO_EXCEPT on ESP function {name}"),
    ))
}

#[cfg(test)]
mod esp_noexcept_tests {
    use super::*;
    fn check(text: &str) -> Vec<(usize, String)> {
        EspNoexceptChecker.check_file_content(&FileContent {
            path: "src/platforms/esp/32/example.cpp.hpp".into(),
            content: text.into(),
            lines: text.lines().map(str::to_owned).collect(),
        })
    }

    #[test]
    fn esp_noexcept_checks_parenthesized_declarators() {
        // Parenthesized names keep Arduino-ESP32 pin-remap macros from
        // rewriting the declaration; they are still owned functions.
        assert!(check("inline void (pinMode)(int, PinMode) FL_NO_EXCEPT {}\n").is_empty());
        assert_eq!(check("inline void (pinMode)(int, PinMode) {}\n").len(), 1);
        assert!(
            check("inline PinValue (digitalRead)(int) FL_NO_EXCEPT { return 0; }\n").is_empty()
        );
        assert_eq!(check("inline PinValue (digitalRead)(int) { return 0; }\n").len(), 1);
        assert_eq!(check("inline u16 (analogRead)(int) { return 0; }\n").len(), 1);
        assert!(check("inline u16 (analogRead)(int) FL_NO_EXCEPT { return 0; }\n").is_empty());
        // Parenthesized pointer types stay exempt, as before.
        assert!(check("void (*callback)();\n").is_empty());
    }

    #[test]
    fn esp_noexcept_checks_both_preprocessor_arms_and_multiline_callbacks() {
        let hits = check("#if ESP32\nvoid foo(\nvoid (*handler)() noexcept\n) {}\n#else\nvoid foo() {}\n#endif\n");
        assert_eq!(hits.len(), 2, "{hits:?}");
        assert!(hits.iter().all(|hit| hit.1.starts_with("Missing")));
    }

    #[test]
    fn esp_noexcept_ignores_register_macro_stores_in_function_bodies() {
        assert!(check("void f() FL_NO_EXCEPT { USIE(kUart) = 0; USC0(kUart) = 1; }\n").is_empty());
        // Local class members are still checked.
        assert_eq!(check("void f() FL_NO_EXCEPT { struct L { virtual void go() = 0; }; }\n").len(), 1);
    }

    #[test]
    fn esp_noexcept_checks_members_operators_and_constructors() {
        let hits = check("struct C { C(); ~C(); void go() = 0; int operator()(); C(const C&) = default; C(int) = delete; };\n");
        assert_eq!(hits.len(), 3, "{hits:?}");
        assert_eq!(
            check("struct C { operator bool() const; operator int*() const; };\n").len(),
            2
        );
        assert!(check("struct C { operator bool() const FL_NO_EXCEPT; operator int*() const FL_NO_EXCEPT; };\n").is_empty());
        assert_eq!(check("struct C { C operator~(); ~C(); };\n").len(), 1);
    }

    #[test]
    fn esp_noexcept_accepts_annotations_without_using_body_or_parameter_annotations() {
        assert!(check(
            "IRAM_ATTR void go() FL_NO_EXCEPT {}\nvoid ok() noexcept(true);\nvoid (*callback)();\n"
        )
        .is_empty());
        let hits = check(
            "void outer(void (*callback)() noexcept) { const char *s = \"FL_NO_EXCEPT\"; }\n",
        );
        assert_eq!(hits.len(), 1, "{hits:?}");
        assert!(hits[0].1.contains("outer"));
        assert_eq!(check("void no() noexcept(false);\n").len(), 1);
    }

    #[test]
    fn esp_noexcept_requires_reason_for_vendor_contract_suppression() {
        assert!(check("void vendor(); // ok no noexcept: vendor C ABI contract\n").is_empty());
        assert_eq!(check("void missing(); // ok no noexcept\n").len(), 1);
        assert_eq!(
            check("void missing(const char *s = \"// ok no noexcept: fake\");\n").len(),
            1
        );
        assert_eq!(
            check("void missing() { call(); // ok no noexcept: fake\n}\n").len(),
            1
        );
    }

    #[test]
    fn esp_noexcept_fails_closed_on_malformed_source() {
        assert!(check("void broken( {\n")
            .iter()
            .any(|hit| hit.1.starts_with("Cannot")));
        assert!(
            check("#ifndef A\n#define A 1\n#endif\n").is_empty(),
            "{:?}",
            check("#ifndef A\n#define A 1\n#endif\n")
        );
    }

    #[test]
    fn esp_noexcept_checks_macro_generated_functions() {
        let hits =
            check("#define TRAMPOLINE(name) \\\nvoid name(void *arg) { \\\n(void)arg; \\\n}\n");
        assert_eq!(hits.len(), 1, "{hits:?}");
        assert!(hits[0].1.starts_with("Missing"));
        assert!(check("#define TRAMPOLINE(name) void name() FL_NO_EXCEPT {}\n").is_empty());
        assert_eq!(check("#define DECLARE(name) void name();\n").len(), 1);
        assert!(
            check("#define EXPR(x) ((x) + 2)\n#define STMT(x) do { call(x); } while (0)\n")
                .is_empty()
        );
        assert!(check("#define NUM 12 // Macro (configuration) comment\n#define POINTER(name) typedef void (*_##name##_t)(); _##name##_t name\n").is_empty());
        assert_eq!(
            check("#define BAD(name) void name() UNKNOWN_SUFFIX {}\n").len(),
            1
        );
        assert_eq!(
            check("#if 0\n#define BAD(name) void name() {}\n#endif\n").len(),
            1
        );
        assert_eq!(check("#define DECL void hidden\nDECL();\n").len(), 1);
        assert_eq!(
            check("#define DECL void hidden\n#define WRAP DECL()\nWRAP;\n").len(),
            1
        );
        assert!(
            !check("#define DECL void hidden\n#define WRAP DECL()\nstruct C { WRAP; };\n")
                .is_empty()
        );
        assert!(!check("#define DECL void hidden\n#define WRAP DECL()\nvoid outer() FL_NO_EXCEPT { struct C { WRAP; }; }\n").is_empty());
        assert_eq!(
            check("#define DECL void hidden\nstruct C { DECL(); };\n").len(),
            1
        );
        assert!(check("FASTLED_SHARED_PTR(C);\n_FL_DEFPIN(3);\n").is_empty());
    }

    #[test]
    fn esp_noexcept_normalizes_syntax_without_hiding_callable_bodies() {
        assert!(check("void ok() FL_NO_EXCEPT { register void *p asm(\"a0\") = nullptr; FL_SIMD_LANE4(call(i);); PERIPH_RCC_ATOMIC() { call(); } }\n").is_empty());
        assert_eq!(
            check(
                "void outer() FL_NO_EXCEPT { FL_SIMD_LANE4(struct Local { void missing(); };); }\n"
            )
            .len(),
            1
        );
        assert!(check("#ifdef __cplusplus\nextern \"C\" {\n#endif\nvoid vendor();\n#ifdef __cplusplus\n}\n#endif\n").is_empty());
        assert!(
            check("template class Thing<1>;\nvoid calls() FL_NO_EXCEPT { funcs[i](); }\n")
                .is_empty()
        );
        assert!(check("struct A { static void (A::*ptr)(); };\nvoid (A::*A::ptr)() = nullptr;\nvoid call() FL_NO_EXCEPT { (this->*ptr)(); }\n").is_empty());
    }

    #[test]
    fn esp_noexcept_checks_owned_c_definitions_but_not_vendor_prototypes() {
        assert!(check("extern \"C\" int esp_clk_cpu_freq(void);\n").is_empty());
        assert_eq!(check("extern \"C\" int owned() { return 1; }\n").len(), 1);
        assert_eq!(
            check("extern \"C\" { struct C { C(); void missing(); }; }\n").len(),
            2
        );
        assert!(check(
            "extern \"C\" { struct C { C() FL_NO_EXCEPT; void good() FL_NO_EXCEPT; }; }\n"
        )
        .is_empty());
        assert!(!check("void (*factory())();\n").is_empty());
        assert!(!check("void (*factory())() FL_NO_EXCEPT;\n").is_empty());
        assert!(check("void (*factory() FL_NO_EXCEPT)();\n").is_empty());
    }

    #[test]
    fn esp_noexcept_regression_checks_all_nine_riscv_definitions() {
        let source = include_str!("../../../../src/platforms/esp/32/interrupts/riscv.cpp.hpp");
        assert!(check(source).is_empty(), "{:?}", check(source));
        let broken = source.replace(" FL_NO_EXCEPT", "");
        let hits = check(&broken);
        assert_eq!(hits.len(), 9, "{hits:?}");
        assert!(hits.iter().all(|hit| hit.1.starts_with("Missing")));
    }

    #[test]
    fn esp_noexcept_checks_both_optional_sd_backend_branches() {
        let source = include_str!("../../../../src/fl/fs/sd/fs_sdcard_arduino.cpp.hpp");
        assert!(check(source).is_empty(), "{:?}", check(source));
        let mut broken = source.to_owned();
        for signature in [
            "has_error() const",
            "clear_error()",
            "error_code() const",
            "error_message() const",
        ] {
            broken = broken.replace(&format!("{signature} FL_NO_EXCEPT"), signature);
        }
        let hits = check(&broken);
        assert_eq!(hits.len(), 8, "{hits:?}");
        assert!(hits.iter().all(|hit| hit.1.starts_with("Missing")));
    }

    #[test]
    fn esp_noexcept_scopes_all_esp_source_extensions() {
        let checker = EspNoexceptChecker;
        for path in [
            "src/platforms/esp/32/foo.h",
            "src/platforms/esp/32/foo.hpp",
            "src/platforms/esp/32/foo.cpp.hpp",
            "src/platforms/esp/8266/foo.cpp",
            "src/fl/fs/sd/fs_sdcard_arduino.cpp.hpp",
        ] {
            assert!(checker.should_process_file(path, Path::new(".")));
        }
        assert!(!checker.should_process_file("src/platforms/arm/foo.hpp", Path::new(".")));
    }
}
