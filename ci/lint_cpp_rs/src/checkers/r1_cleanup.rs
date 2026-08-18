const R1_CLEAN: u8 = 1;
const R1_DIRTY: u8 = 2;

#[derive(Clone, Debug)]
struct R1Token {
    text: String,
    line: usize,
}

#[derive(Default)]
struct R1Flow {
    normal: u8,
    breaks: u8,
    continues: u8,
    violations: Vec<(usize, String)>,
}

impl R1Flow {
    fn normal(state: u8) -> Self {
        Self {
            normal: state,
            ..Self::default()
        }
    }

    fn merge(mut self, other: Self) -> Self {
        self.normal |= other.normal;
        self.breaks |= other.breaks;
        self.continues |= other.continues;
        self.violations.extend(other.violations);
        self
    }
}

struct R1Parser<'a> {
    tokens: &'a [R1Token],
    pos: usize,
    end: usize,
    nested_callables: &'a HashMap<usize, usize>,
}

impl<'a> R1Parser<'a> {
    fn parse_block(&mut self, mut state: u8) -> R1Flow {
        let mut result = R1Flow::default();
        while self.pos < self.end && self.tokens[self.pos].text != "}" {
            let flow = self.parse_statement(state);
            state = flow.normal;
            result.breaks |= flow.breaks;
            result.continues |= flow.continues;
            result.violations.extend(flow.violations);
        }
        if self.pos < self.end && self.tokens[self.pos].text == "}" {
            self.pos += 1;
        }
        result.normal = state;
        result
    }

    fn parse_statement(&mut self, state: u8) -> R1Flow {
        if self.pos >= self.end {
            return R1Flow::normal(state);
        }
        if self.pos + 1 < self.end && self.tokens[self.pos + 1].text == ":" {
            self.pos += 2;
            return self.parse_statement(state);
        }
        match self.tokens[self.pos].text.as_str() {
            "{" => {
                if let Some(close) = self.nested_callables.get(&self.pos) {
                    self.pos = close + 1;
                    R1Flow::normal(state)
                } else {
                    self.pos += 1;
                    self.parse_block(state)
                }
            }
            "if" => self.parse_if(state),
            "for" => self.parse_for_loop(state),
            "while" => self.parse_while_loop(state),
            "do" => self.parse_do_loop(state),
            "switch" => self.parse_switch(state),
            "return" => self.parse_return(state),
            "break" => self.parse_loop_transfer(state, true),
            "continue" => self.parse_loop_transfer(state, false),
            "goto" | "throw" => self.parse_function_transfer(state),
            _ => self.parse_simple(state),
        }
    }

    fn parse_if(&mut self, state: u8) -> R1Flow {
        self.pos += 1;
        let state = self.scan_parenthesized(state);
        let then_flow = self.parse_statement(state);
        if self.pos < self.end && self.tokens[self.pos].text == "else" {
            self.pos += 1;
            then_flow.merge(self.parse_statement(state))
        } else {
            then_flow.merge(R1Flow::normal(state))
        }
    }

    fn parse_while_loop(&mut self, state: u8) -> R1Flow {
        self.pos += 1;
        let condition_start = self.pos;
        let first_condition = self.scan_parenthesized(state);
        let condition_end = self.pos;
        let body_start = self.pos;
        let mut body_entry = first_condition;
        let mut body = self.parse_statement(body_entry);
        loop {
            let repeated_input = body.normal | body.continues;
            let repeated_condition = r1_apply_range(
                self.tokens,
                condition_start,
                condition_end,
                repeated_input,
                false,
            );
            let next_entry = body_entry | repeated_condition;
            if next_entry == body_entry {
                break;
            }
            body_entry = next_entry;
            body = self.reparse_statement(body_start, body_entry);
        }
        R1Flow {
            normal: body_entry | body.breaks,
            violations: body.violations,
            ..R1Flow::default()
        }
    }

    fn parse_for_loop(&mut self, state: u8) -> R1Flow {
        self.pos += 1;
        let Some((init, condition, increment, after_header)) =
            r1_for_clause_ranges(self.tokens, self.pos, self.end)
        else {
            if let Some((header, after_header)) =
                r1_range_for_header(self.tokens, self.pos, self.end)
            {
                return self.parse_range_for_loop(state, header, after_header);
            }
            let header_state = self.scan_parenthesized(state);
            let body = self.parse_statement(header_state);
            return R1Flow {
                normal: header_state | body.normal | body.breaks | body.continues,
                violations: body.violations,
                ..R1Flow::default()
            };
        };
        self.pos = after_header;

        let after_init = r1_apply_range(self.tokens, init.0, init.1, state, true);
        let first_condition =
            r1_apply_range(self.tokens, condition.0, condition.1, after_init, false);
        let body_start = self.pos;
        let mut body_entry = first_condition;
        let mut body = self.parse_statement(body_entry);
        loop {
            let backedge = body.normal | body.continues;
            let after_increment = r1_apply_range(
                self.tokens,
                increment.0,
                increment.1,
                backedge,
                true,
            );
            let repeated_condition = r1_apply_range(
                self.tokens,
                condition.0,
                condition.1,
                after_increment,
                false,
            );
            let next_entry = body_entry | repeated_condition;
            if next_entry == body_entry {
                break;
            }
            body_entry = next_entry;
            body = self.reparse_statement(body_start, body_entry);
        }
        R1Flow {
            normal: body_entry | body.breaks,
            violations: body.violations,
            ..R1Flow::default()
        }
    }

    fn parse_range_for_loop(
        &mut self,
        state: u8,
        header: R1Range,
        after_header: usize,
    ) -> R1Flow {
        self.pos = after_header;
        let first_entry = r1_apply_range(self.tokens, header.0, header.1, state, false);
        let body_start = self.pos;
        let mut body_entry = first_entry;
        let mut body = self.parse_statement(body_entry);
        loop {
            let next_entry = body_entry | body.normal | body.continues;
            if next_entry == body_entry {
                break;
            }
            body_entry = next_entry;
            body = self.reparse_statement(body_start, body_entry);
        }
        R1Flow {
            normal: body_entry | body.breaks,
            violations: body.violations,
            ..R1Flow::default()
        }
    }

    fn parse_do_loop(&mut self, state: u8) -> R1Flow {
        self.pos += 1;
        let body_start = self.pos;
        let mut body_entry = state;
        let mut body = self.parse_statement(body_entry);
        let mut exit_state = body.breaks;
        if self.pos < self.end && self.tokens[self.pos].text == "while" {
            self.pos += 1;
            let condition_start = self.pos;
            let first_condition = self.scan_parenthesized(body.normal | body.continues);
            let condition_end = self.pos;
            body_entry |= first_condition;
            body = self.reparse_statement(body_start, body_entry);
            loop {
                let repeated_condition = r1_apply_range(
                    self.tokens,
                    condition_start,
                    condition_end,
                    body.normal | body.continues,
                    false,
                );
                let next_entry = body_entry | repeated_condition;
                if next_entry == body_entry {
                    exit_state = body.breaks | repeated_condition;
                    break;
                }
                body_entry = next_entry;
                body = self.reparse_statement(body_start, body_entry);
            }
            if self.pos < self.end && self.tokens[self.pos].text == ";" {
                self.pos += 1;
            }
        } else {
            exit_state |= body.normal | body.continues;
        }
        R1Flow {
            normal: exit_state,
            violations: body.violations,
            ..R1Flow::default()
        }
    }

    fn reparse_statement(&self, start: usize, state: u8) -> R1Flow {
        let mut parser = R1Parser {
            tokens: self.tokens,
            pos: start,
            end: self.end,
            nested_callables: self.nested_callables,
        };
        parser.parse_statement(state)
    }

    fn parse_switch(&mut self, state: u8) -> R1Flow {
        self.pos += 1;
        let entry = self.scan_parenthesized(state);
        if self.pos >= self.end || self.tokens[self.pos].text != "{" {
            return self.parse_statement(entry);
        }
        self.pos += 1;
        let mut result = R1Flow::default();
        let mut fallthrough = 0;
        let mut has_default = false;

        while self.pos < self.end && self.tokens[self.pos].text != "}" {
            let label = self.tokens[self.pos].text.as_str();
            if label != "case" && label != "default" {
                let ignored = self.parse_statement(0);
                result.violations.extend(ignored.violations);
                continue;
            }
            has_default |= label == "default";
            self.skip_case_label();
            let mut branch_state = entry | fallthrough;
            fallthrough = 0;
            while self.pos < self.end
                && self.tokens[self.pos].text != "}"
                && self.tokens[self.pos].text != "case"
                && self.tokens[self.pos].text != "default"
            {
                let flow = self.parse_statement(branch_state);
                branch_state = flow.normal;
                result.normal |= flow.breaks;
                result.continues |= flow.continues;
                result.violations.extend(flow.violations);
            }
            fallthrough |= branch_state;
        }
        if self.pos < self.end && self.tokens[self.pos].text == "}" {
            self.pos += 1;
        }
        result.normal |= fallthrough;
        if !has_default {
            result.normal |= entry;
        }
        result
    }

    fn parse_return(&mut self, state: u8) -> R1Flow {
        let return_line = self.tokens[self.pos].line;
        self.pos += 1;
        let state_at_exit = self.scan_simple_expression(state, false);
        let mut result = R1Flow::default();
        if state_at_exit & R1_DIRTY != 0 {
            result.violations.push((
                return_line,
                "cleanup_R1() is missing on a reachable return after a *_LEAVING_R1_DIRTY call"
                    .to_string(),
            ));
        }
        result
    }

    fn parse_loop_transfer(&mut self, state: u8, is_break: bool) -> R1Flow {
        self.skip_to_semicolon();
        if is_break {
            R1Flow {
                breaks: state,
                ..R1Flow::default()
            }
        } else {
            R1Flow {
                continues: state,
                ..R1Flow::default()
            }
        }
    }

    fn parse_function_transfer(&mut self, state: u8) -> R1Flow {
        let line = self.tokens[self.pos].line;
        let keyword = self.tokens[self.pos].text.clone();
        self.pos += 1;
        let state_at_exit = self.scan_simple_expression(state, false);
        let mut result = R1Flow::default();
        if state_at_exit & R1_DIRTY != 0 {
            result.violations.push((
                line,
                format!(
                    "cleanup_R1() is missing before reachable {keyword} after a *_LEAVING_R1_DIRTY call"
                ),
            ));
        }
        result
    }

    fn parse_simple(&mut self, state: u8) -> R1Flow {
        R1Flow::normal(self.scan_simple_expression(state, true))
    }

    fn scan_simple_expression(&mut self, state: u8, allow_standalone_cleanup: bool) -> u8 {
        let statement_start = self.pos;
        let mut state = state;
        let mut paren_depth = 0_i32;
        while self.pos < self.end {
            let token = &self.tokens[self.pos].text;
            if token == ";" && paren_depth == 0 {
                self.pos += 1;
                break;
            }
            if token == "{" && paren_depth == 0 {
                if let Some(close) = self.nested_callables.get(&self.pos) {
                    self.pos = close + 1;
                    continue;
                }
                self.pos += 1;
                let nested = self.parse_block(state);
                state |= nested.normal | nested.breaks | nested.continues;
                continue;
            }
            if token == "}" && paren_depth == 0 {
                break;
            }
            state = r1_apply_token(token, state);
            if token == "(" {
                paren_depth += 1;
            } else if token == ")" {
                paren_depth -= 1;
            }
            self.pos += 1;
        }
        let statement_end = self.pos;
        if allow_standalone_cleanup
            && r1_is_standalone_cleanup(self.tokens, statement_start, statement_end)
            && state != 0
        {
            R1_CLEAN
        } else {
            state
        }
    }

    fn scan_parenthesized(&mut self, mut state: u8) -> u8 {
        if self.pos >= self.end || self.tokens[self.pos].text != "(" {
            return state;
        }
        let mut depth = 0_i32;
        while self.pos < self.end {
            let token = self.tokens[self.pos].text.as_str();
            state = r1_apply_token(token, state);
            match token {
                "(" => depth += 1,
                ")" => {
                    depth -= 1;
                    self.pos += 1;
                    if depth == 0 {
                        return state;
                    }
                    continue;
                }
                _ => {}
            }
            self.pos += 1;
        }
        state
    }

    fn skip_case_label(&mut self) {
        while self.pos < self.end {
            let done = self.tokens[self.pos].text == ":";
            self.pos += 1;
            if done {
                break;
            }
        }
    }

    fn skip_to_semicolon(&mut self) {
        while self.pos < self.end {
            let done = self.tokens[self.pos].text == ";";
            self.pos += 1;
            if done {
                break;
            }
        }
    }
}

struct R1CleanupChecker;

impl FileContentChecker for R1CleanupChecker {
    fn name(&self) -> &'static str {
        "R1CleanupChecker"
    }

    fn should_process_file(&self, file_path: &str, project_root: &Path) -> bool {
        ends_with_any(file_path, &[".h", ".hpp", ".cpp", ".cpp.hpp"])
            && is_under_project_subpath(file_path, project_root, "src")
            && !is_under_project_subpath(file_path, project_root, "src/third_party")
    }

    fn check_file_content(&self, file_content: &FileContent) -> Vec<(usize, String)> {
        if !file_content.content.contains("_LEAVING_R1_DIRTY") {
            return Vec::new();
        }
        let tokens = r1_tokens(&file_content.content);
        let callable_pairs: Vec<(usize, usize)> = r1_brace_pairs(&tokens)
            .into_iter()
            .filter(|(open, _)| r1_is_callable_body(&tokens, *open))
            .collect();
        let mut violations = Vec::new();

        for (open, close) in &callable_pairs {
            let nested_callables: HashMap<usize, usize> = callable_pairs
                .iter()
                .filter(|(nested_open, nested_close)| {
                    nested_open > open && nested_close < close
                })
                .copied()
                .collect();
            let mut parser = R1Parser {
                tokens: &tokens,
                pos: open + 1,
                end: *close,
                nested_callables: &nested_callables,
            };
            let flow = parser.parse_block(R1_CLEAN);
            violations.extend(flow.violations);
            let exit_state = flow.normal | flow.breaks | flow.continues;
            if exit_state & R1_DIRTY != 0 {
                violations.push((
                    tokens[*close].line,
                    "cleanup_R1() is missing on a reachable function exit after a *_LEAVING_R1_DIRTY call"
                        .to_string(),
                ));
            }
        }
        violations.sort();
        violations.dedup();
        violations
    }
}

fn r1_apply_token(token: &str, state: u8) -> u8 {
    if token.ends_with("_LEAVING_R1_DIRTY") {
        if state == 0 { 0 } else { R1_DIRTY }
    } else {
        state
    }
}

fn r1_apply_range(
    tokens: &[R1Token],
    start: usize,
    end: usize,
    mut state: u8,
    allow_standalone_cleanup: bool,
) -> u8 {
    for token in &tokens[start..end] {
        state = r1_apply_token(&token.text, state);
    }
    if allow_standalone_cleanup && r1_is_standalone_cleanup(tokens, start, end) && state != 0 {
        R1_CLEAN
    } else {
        state
    }
}

fn r1_is_standalone_cleanup(tokens: &[R1Token], start: usize, end: usize) -> bool {
    let significant: Vec<&str> = tokens[start..end]
        .iter()
        .map(|token| token.text.as_str())
        .filter(|token| !matches!(*token, "(" | ")" | ";"))
        .collect();
    significant == ["cleanup_R1"]
}

type R1Range = (usize, usize);

fn r1_for_clause_ranges(
    tokens: &[R1Token],
    open_paren: usize,
    end: usize,
) -> Option<(R1Range, R1Range, R1Range, usize)> {
    if open_paren >= end || tokens[open_paren].text != "(" {
        return None;
    }
    let mut depth = 1_i32;
    let mut separators = Vec::new();
    let mut index = open_paren + 1;
    while index < end {
        match tokens[index].text.as_str() {
            "(" => depth += 1,
            ")" => {
                depth -= 1;
                if depth == 0 {
                    if separators.len() != 2 {
                        return None;
                    }
                    return Some((
                        (open_paren + 1, separators[0]),
                        (separators[0] + 1, separators[1]),
                        (separators[1] + 1, index),
                        index + 1,
                    ));
                }
            }
            ";" if depth == 1 => separators.push(index),
            _ => {}
        }
        index += 1;
    }
    None
}

fn r1_range_for_header(
    tokens: &[R1Token],
    open_paren: usize,
    end: usize,
) -> Option<(R1Range, usize)> {
    if open_paren >= end || tokens[open_paren].text != "(" {
        return None;
    }
    let mut depth = 1_i32;
    let mut has_top_level_colon = false;
    let mut index = open_paren + 1;
    while index < end {
        match tokens[index].text.as_str() {
            "(" => depth += 1,
            ")" => {
                depth -= 1;
                if depth == 0 {
                    return has_top_level_colon
                        .then_some(((open_paren + 1, index), index + 1));
                }
            }
            ":" if depth == 1 => has_top_level_colon = true,
            ";" if depth == 1 => return None,
            _ => {}
        }
        index += 1;
    }
    None
}

fn r1_push_token(tokens: &mut Vec<R1Token>, text: &str, line: usize) {
    tokens.push(R1Token {
        text: text.to_string(),
        line,
    });
}

fn r1_push_preprocessor(tokens: &mut Vec<R1Token>, directive: &str, line: usize) {
    let keyword = directive
        .trim_start_matches('#')
        .split_whitespace()
        .next()
        .unwrap_or("");
    match keyword {
        "if" | "ifdef" | "ifndef" => {
            for token in ["if", "(", "R1_PP_CONDITION", ")", "{"] {
                r1_push_token(tokens, token, line);
            }
        }
        "elif" => {
            for token in ["}", "else", "if", "(", "R1_PP_CONDITION", ")", "{"] {
                r1_push_token(tokens, token, line);
            }
        }
        "else" => {
            for token in ["}", "else", "{"] {
                r1_push_token(tokens, token, line);
            }
        }
        "endif" => r1_push_token(tokens, "}", line),
        _ => {}
    }
}

fn r1_tokens(content: &str) -> Vec<R1Token> {
    let bytes = content.as_bytes();
    let mut tokens = Vec::new();
    let mut index = 0;
    let mut line = 1;
    while index < bytes.len() {
        if bytes[index] == b'\n' {
            line += 1;
            index += 1;
            continue;
        }
        if bytes[index].is_ascii_whitespace() {
            index += 1;
            continue;
        }
        if bytes[index] == b'#' {
            let start = index;
            while index < bytes.len() && bytes[index] != b'\n' {
                index += 1;
            }
            r1_push_preprocessor(&mut tokens, &content[start..index], line);
            continue;
        }
        if index + 1 < bytes.len() && bytes[index] == b'/' && bytes[index + 1] == b'/' {
            while index < bytes.len() && bytes[index] != b'\n' {
                index += 1;
            }
            continue;
        }
        if index + 1 < bytes.len() && bytes[index] == b'/' && bytes[index + 1] == b'*' {
            index += 2;
            while index + 1 < bytes.len() {
                if bytes[index] == b'\n' {
                    line += 1;
                }
                if bytes[index] == b'*' && bytes[index + 1] == b'/' {
                    index += 2;
                    break;
                }
                index += 1;
            }
            continue;
        }
        if bytes[index] == b'"' || bytes[index] == b'\'' {
            let quote = bytes[index];
            index += 1;
            while index < bytes.len() {
                if bytes[index] == b'\n' {
                    line += 1;
                }
                if bytes[index] == b'\\' {
                    index += 2;
                    continue;
                }
                if bytes[index] == quote {
                    index += 1;
                    break;
                }
                index += 1;
            }
            continue;
        }
        if bytes[index].is_ascii_alphabetic() || bytes[index] == b'_' {
            let start = index;
            index += 1;
            while index < bytes.len()
                && (bytes[index].is_ascii_alphanumeric() || bytes[index] == b'_')
            {
                index += 1;
            }
            r1_push_token(&mut tokens, &content[start..index], line);
            continue;
        }
        let ch = bytes[index] as char;
        if "{}();:[]".contains(ch) {
            r1_push_token(&mut tokens, &ch.to_string(), line);
        }
        index += 1;
    }
    tokens
}

fn r1_brace_pairs(tokens: &[R1Token]) -> Vec<(usize, usize)> {
    let mut stack = Vec::new();
    let mut pairs = Vec::new();
    for (index, token) in tokens.iter().enumerate() {
        if token.text == "{" {
            stack.push(index);
        } else if token.text == "}" {
            if let Some(open) = stack.pop() {
                pairs.push((open, index));
            }
        }
    }
    pairs.sort();
    pairs
}

fn r1_matching_open_paren(tokens: &[R1Token], close: usize) -> Option<usize> {
    let mut depth = 0_i32;
    for index in (0..=close).rev() {
        match tokens[index].text.as_str() {
            ")" => depth += 1,
            "(" => {
                depth -= 1;
                if depth == 0 {
                    return Some(index);
                }
            }
            _ => {}
        }
    }
    None
}

fn r1_is_callable_body(tokens: &[R1Token], open_brace: usize) -> bool {
    let mut index = open_brace;
    while index > 0 {
        index -= 1;
        match tokens[index].text.as_str() {
            ";" | "}" | "{" => return false,
            "]" => return true,
            ")" => {
                let Some(open_paren) = r1_matching_open_paren(tokens, index) else {
                    return false;
                };
                let Some(name_index) = open_paren.checked_sub(1) else {
                    return false;
                };
                let name = tokens[name_index].text.as_str();
                if matches!(name, "if" | "for" | "while" | "switch" | "catch") {
                    return false;
                }
                if name == "]" {
                    return true;
                }
                if !matches!(name, "noexcept" | "requires" | "decltype") {
                    return true;
                }
                index = open_paren;
            }
            _ => {}
        }
    }
    false
}

#[cfg(test)]
mod r1_cleanup_structural_tests {
    use super::*;

    fn source(content: &str) -> FileContent {
        FileContent {
            path: "src/example.cpp".to_string(),
            content: content.to_string(),
            lines: content.lines().map(str::to_string).collect(),
        }
    }

    #[test]
    fn processes_relative_and_absolute_src_paths() {
        let checker = R1CleanupChecker;
        assert!(checker.should_process_file("src/example.cpp", Path::new(".")));
        let root = std::env::current_dir().unwrap();
        assert!(checker.should_process_file(
            &path_to_string(&root.join("src/example.cpp")),
            &root,
        ));
        assert!(!checker.should_process_file(
            &path_to_string(&root.join("src/third_party/example.cpp")),
            &root,
        ));
    }

    #[test]
    fn keeps_preprocessor_arms_exclusive() {
        let hits = R1CleanupChecker.check_file_content(&source(concat!(
            "void convert() {\n",
            "#if USE_DIRTY\n",
            "  value = scale8_LEAVING_R1_DIRTY(value, 2);\n",
            "#else\n",
            "  cleanup_R1();\n",
            "#endif\n",
            "  return;\n",
            "}\n",
        )));
        assert_eq!(hits.len(), 1);
        assert_eq!(hits[0].0, 7);
    }

    #[test]
    fn tracks_nested_preprocessor_arms() {
        let hits = R1CleanupChecker.check_file_content(&source(concat!(
            "void convert() {\n",
            "#if OUTER\n",
            "#if INNER\n",
            "  value = scale8_LEAVING_R1_DIRTY(value, 2);\n",
            "#else\n",
            "  cleanup_R1();\n",
            "#endif\n",
            "#endif\n",
            "  return;\n",
            "}\n",
        )));
        assert_eq!(hits.len(), 1);
        assert_eq!(hits[0].0, 9);
    }

    #[test]
    fn tracks_switch_case_returns() {
        let hits = R1CleanupChecker.check_file_content(&source(concat!(
            "u8 convert(int mode) {\n",
            "  switch (mode) {\n",
            "    case 0: return scale8_LEAVING_R1_DIRTY(value, 2);\n",
            "    default: break;\n",
            "  }\n",
            "  cleanup_R1();\n",
            "  return 0;\n",
            "}\n",
        )));
        assert_eq!(hits.len(), 1);
        assert_eq!(hits[0].0, 3);
    }

    #[test]
    fn keeps_code_after_do_while_break_reachable() {
        let hits = R1CleanupChecker.check_file_content(&source(concat!(
            "u8 convert() {\n",
            "  do { break; } while (false);\n",
            "  value = scale8_LEAVING_R1_DIRTY(value, 2);\n",
            "  return value;\n",
            "}\n",
        )));
        assert_eq!(hits.len(), 1);
        assert_eq!(hits[0].0, 4);
    }

    #[test]
    fn accepts_cleanup_after_dirty_continue_loop() {
        let hits = R1CleanupChecker.check_file_content(&source(concat!(
            "void convert(int count) {\n",
            "  for (int i = 0; i < count; ++i) {\n",
            "    value = scale8_LEAVING_R1_DIRTY(value, 2);\n",
            "    if (value == 0) continue;\n",
            "    cleanup_R1();\n",
            "  }\n",
            "  cleanup_R1();\n",
            "}\n",
        )));
        assert!(hits.is_empty());
    }

    #[test]
    fn recognizes_no_parameter_lambda() {
        let hits = R1CleanupChecker.check_file_content(&source(concat!(
            "auto convert = [] {\n",
            "  value = scale8_LEAVING_R1_DIRTY(value, 2);\n",
            "  return;\n",
            "};\n",
        )));
        assert_eq!(hits.len(), 1);
        assert_eq!(hits[0].0, 3);
    }

    #[test]
    fn for_increment_cleanup_does_not_clean_dirty_initializer_on_entry() {
        let hits = R1CleanupChecker.check_file_content(&source(concat!(
            "void convert(bool stop) {\n",
            "  for (u8 x = scale8_LEAVING_R1_DIRTY(value, 2); ; cleanup_R1()) {\n",
            "    if (stop) return;\n",
            "    break;\n",
            "  }\n",
            "  cleanup_R1();\n",
            "}\n",
        )));
        assert_eq!(hits.len(), 1);
        assert_eq!(hits[0].0, 3);
    }

    #[test]
    fn for_continue_runs_increment_cleanup_before_condition() {
        let hits = R1CleanupChecker.check_file_content(&source(concat!(
            "void convert(bool running, bool skip) {\n",
            "  for (; running; cleanup_R1()) {\n",
            "    value = scale8_LEAVING_R1_DIRTY(value, 2);\n",
            "    if (skip) continue;\n",
            "    cleanup_R1();\n",
            "  }\n",
            "}\n",
        )));
        assert!(hits.is_empty());
    }

    #[test]
    fn conditional_ternary_cleanup_does_not_clear_all_paths() {
        let hits = R1CleanupChecker.check_file_content(&source(concat!(
            "u8 convert(bool clean) {\n",
            "  value = scale8_LEAVING_R1_DIRTY(value, 2);\n",
            "  return clean ? (cleanup_R1(), value) : value;\n",
            "}\n",
        )));
        assert_eq!(hits.len(), 1);
        assert_eq!(hits[0].0, 3);
    }

    #[test]
    fn short_circuit_cleanup_does_not_clear_all_paths() {
        let hits = R1CleanupChecker.check_file_content(&source(concat!(
            "void convert(bool clean) {\n",
            "  value = scale8_LEAVING_R1_DIRTY(value, 2);\n",
            "  if (clean || (cleanup_R1(), true)) return;\n",
            "  cleanup_R1();\n",
            "}\n",
        )));
        assert_eq!(hits.len(), 1);
        assert_eq!(hits[0].0, 3);
    }

    #[test]
    fn while_reanalyzes_early_return_with_dirty_backedge() {
        let hits = R1CleanupChecker.check_file_content(&source(concat!(
            "void convert(bool running, bool stop) {\n",
            "  while (running) {\n",
            "    if (stop) return;\n",
            "    value = scale8_LEAVING_R1_DIRTY(value, 2);\n",
            "  }\n",
            "  cleanup_R1();\n",
            "}\n",
        )));
        assert_eq!(hits.len(), 1);
        assert_eq!(hits[0].0, 3);
    }

    #[test]
    fn for_reanalyzes_early_return_after_dirty_increment() {
        let hits = R1CleanupChecker.check_file_content(&source(concat!(
            "void convert(bool running, bool stop) {\n",
            "  for (; running; value = scale8_LEAVING_R1_DIRTY(value, 2)) {\n",
            "    if (stop) return;\n",
            "  }\n",
            "  cleanup_R1();\n",
            "}\n",
        )));
        assert_eq!(hits.len(), 1);
        assert_eq!(hits[0].0, 3);
    }

    #[test]
    fn range_for_preserves_dirty_early_return_violation() {
        let hits = R1CleanupChecker.check_file_content(&source(
            "void convert(const Values& values) {\n  for (auto value : values) {\n    value = scale8_LEAVING_R1_DIRTY(value, 2);\n    return;\n  }\n  cleanup_R1();\n}\n",
        ));
        assert_eq!(hits.len(), 1);
        assert_eq!(hits[0].0, 4);
    }
}
