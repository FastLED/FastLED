// Architecture policy, not a C preprocessor: feature switches and even #if 0
// remain unknown so dormant native dependencies are inspected too.
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
enum EspArchitectureTruth {
    True,
    False,
    Unknown,
}

impl EspArchitectureTruth {
    fn not(self) -> Self {
        match self {
            Self::True => Self::False,
            Self::False => Self::True,
            Self::Unknown => Self::Unknown,
        }
    }
    fn and(self, other: Self) -> Self {
        match (self, other) {
            (Self::False, _) | (_, Self::False) => Self::False,
            (Self::True, Self::True) => Self::True,
            _ => Self::Unknown,
        }
    }
    fn or(self, other: Self) -> Self {
        match (self, other) {
            (Self::True, _) | (_, Self::True) => Self::True,
            (Self::False, Self::False) => Self::False,
            _ => Self::Unknown,
        }
    }
}

fn esp_architecture_macro(name: &str) -> EspArchitectureTruth {
    use EspArchitectureTruth::*;
    if matches!(name, "ESP32" | "ARDUINO_ARCH_ESP32" | "FL_IS_ESP32") {
        return True;
    }
    const FOREIGN_COMPILER_PREFIXES: &[&str] = &[
        "__AVR",
        "__MK",
        "__IMXRT",
        "__SAM",
        "__STM32",
        "STM32",
        "NRF51",
        "NRF52",
        "NRF528",
        "ARDUINO_ARCH_AVR",
        "ARDUINO_ARCH_RP",
        "ARDUINO_ARCH_RENESAS",
        "ARDUINO_ARCH_APOLLO",
        "ARDUINO_ARCH_SILABS",
        "ARDUINO_ARCH_CI13",
        "PICO_RP",
        "CONFIG_SOC_STM32",
    ];
    if matches!(
        name,
        "FASTLED_STUB_IMPL"
            | "ESP8266"
            | "ARDUINO_ARCH_ESP8266"
            | "__EMSCRIPTEN__"
            | "__linux__"
            | "__APPLE__"
            | "__unix__"
            | "__x86_64__"
            | "_WIN32"
            | "_WIN64"
            | "__RFduino__"
            | "__Simblee__"
            | "ARDUINO_UNO_Q"
            | "CONFIG_BOARD_ARDUINO_UNO_Q"
            | "ARDUINO_GIGA"
            | "ARDUINO_GIGA_M7"
    ) || FOREIGN_COMPILER_PREFIXES
        .iter()
        .any(|prefix| name.starts_with(prefix))
    {
        return False;
    }
    // Enumerate platform families, never classify arbitrary FL_IS_* feature or
    // compiler macros as foreign. ESP32 chip variants deliberately stay unknown.
    const FOREIGN: &[&str] = &[
        "FL_IS_AVR",
        "FL_IS_ARM",
        "FL_IS_STUB",
        "FL_IS_WASM",
        "FL_IS_ESP8266",
        "FL_IS_STM32",
        "FL_IS_TEENSY",
        "FL_IS_SAMD",
        "FL_IS_SAM",
        "FL_IS_SAME53",
        "FL_IS_RP",
        "FL_IS_SILABS",
        "FL_IS_NRF52",
        "FL_IS_RENESAS",
        "FL_IS_APOLLO3",
        "FL_IS_CI13XX",
        "FL_IS_LINUX",
        "FL_IS_APPLE",
        "FL_IS_BSD",
        "FL_IS_POSIX",
        "FL_IS_WIN",
        "FL_IS_WIN_MSVC",
        "FL_IS_WIN_MINGW",
    ];
    if FOREIGN.iter().any(|family| {
        name == *family
            || name
                .strip_prefix(*family)
                .is_some_and(|suffix| suffix.starts_with('_'))
    }) {
        False
    } else {
        Unknown
    }
}

struct EspArchitectureExpression<'a> {
    text: &'a str,
    cursor: usize,
    valid: bool,
}

impl<'a> EspArchitectureExpression<'a> {
    fn take(&mut self, token: &str) -> bool {
        self.space();
        if self.text[self.cursor..].starts_with(token) {
            self.cursor += token.len();
            true
        } else {
            false
        }
    }
    fn space(&mut self) {
        while self
            .text
            .as_bytes()
            .get(self.cursor)
            .is_some_and(u8::is_ascii_whitespace)
        {
            self.cursor += 1;
        }
    }
    fn identifier(&mut self) -> &'a str {
        self.space();
        let start = self.cursor;
        while self
            .text
            .as_bytes()
            .get(self.cursor)
            .is_some_and(|byte| byte.is_ascii_alphanumeric() || *byte == b'_')
        {
            self.cursor += 1;
        }
        &self.text[start..self.cursor]
    }
    fn expression(&mut self) -> EspArchitectureTruth {
        let mut value = self.conjunction();
        while self.take("||") {
            value = value.or(self.conjunction());
        }
        value
    }
    fn conjunction(&mut self) -> EspArchitectureTruth {
        let mut value = self.unary();
        while self.take("&&") {
            value = value.and(self.unary());
        }
        value
    }
    fn unary(&mut self) -> EspArchitectureTruth {
        if self.take("!") {
            return self.unary().not();
        }
        let mut value = if self.take("(") {
            let value = self.expression();
            if !self.take(")") {
                self.valid = false;
            }
            value
        } else {
            let name = self.identifier();
            if name == "defined" {
                let parenthesized = self.take("(");
                let name = self.identifier();
                if name.is_empty() {
                    self.valid = false;
                }
                let value = esp_architecture_macro(name);
                if parenthesized && !self.take(")") {
                    self.valid = false;
                }
                value
            } else if name.is_empty() {
                self.valid = false;
                EspArchitectureTruth::Unknown
            } else {
                esp_architecture_macro(name)
            }
        };
        // Comparisons, arithmetic and function-like feature probes are opaque
        // atoms. Consume their balanced tail, preserving surrounding && / ||:
        // foreign_arch && SDK_VERSION >= FOO(1) is still provably foreign.
        self.space();
        let rest = &self.text[self.cursor..];
        if !rest.is_empty()
            && !rest.starts_with(')')
            && !rest.starts_with("&&")
            && !rest.starts_with("||")
        {
            value = EspArchitectureTruth::Unknown;
            let mut depth = 0usize;
            while self.cursor < self.text.len() {
                let tail = &self.text[self.cursor..];
                if depth == 0
                    && (tail.starts_with(')') || tail.starts_with("&&") || tail.starts_with("||"))
                {
                    break;
                }
                match self.text.as_bytes()[self.cursor] {
                    b'(' => depth += 1,
                    b')' => depth = depth.saturating_sub(1),
                    _ => (),
                }
                self.cursor += tail.chars().next().map_or(1, char::len_utf8);
            }
            if depth != 0 {
                self.valid = false;
            }
        }
        value
    }
}

fn esp_architecture_condition(text: &str) -> EspArchitectureTruth {
    let mut parser = EspArchitectureExpression {
        text,
        cursor: 0,
        valid: true,
    };
    let result = parser.expression();
    parser.space();
    if parser.valid && parser.cursor == text.len() {
        result
    } else {
        EspArchitectureTruth::Unknown
    }
}

fn esp_native_directives(content: &FileContent) -> Vec<(usize, String)> {
    esp_native_directives_with_conditions(content, false)
}

fn esp_native_directives_with_conditions(
    content: &FileContent,
    include_conditions: bool,
) -> Vec<(usize, String)> {
    use EspArchitectureTruth::*;
    // Each frame retains its parent's reachability and the disjunction of
    // preceding conditions, so elif/else cannot resurrect a foreign parent.
    let mut stack: Vec<(EspArchitectureTruth, EspArchitectureTruth)> = Vec::new();
    let mut active = True;
    let mut output = Vec::new();
    for (line, directive) in esp_preprocessor_directives(content) {
        let Some(rest) = directive.trim_start().strip_prefix('#') else {
            if active != False {
                output.push((line, directive));
            }
            continue;
        };
        let rest = rest.trim_start();
        let end = rest
            .find(|ch: char| !ch.is_ascii_alphanumeric() && ch != '_')
            .unwrap_or(rest.len());
        let (command, expression) = rest.split_at(end);
        let expression = expression.trim();
        match command {
            "if" | "ifdef" | "ifndef" => {
                let condition = match command {
                    "ifdef" => esp_architecture_macro(expression),
                    "ifndef" => esp_architecture_macro(expression).not(),
                    _ => esp_architecture_condition(expression),
                };
                if include_conditions && active.and(condition) != False {
                    output.push((line, directive.clone()));
                }
                stack.push((active, condition));
                active = active.and(condition);
            }
            "elif" => {
                if let Some((parent, taken)) = stack.last_mut() {
                    let condition = esp_architecture_condition(expression);
                    active = parent.and(taken.not()).and(condition);
                    if include_conditions && active != False {
                        output.push((line, directive.clone()));
                    }
                    *taken = taken.or(condition);
                }
            }
            "else" => {
                if let Some((parent, taken)) = stack.last_mut() {
                    active = parent.and(taken.not());
                    *taken = True;
                }
            }
            "endif" => {
                if let Some((parent, _)) = stack.pop() {
                    active = parent;
                }
            }
            _ => {
                if active != False {
                    output.push((line, directive));
                }
            }
        }
    }
    output
}

#[cfg(test)]
mod esp_architecture_tests {
    use super::*;
    fn visible(source: &str) -> Vec<String> {
        let content = FileContent {
            path: "fixture.h".into(),
            content: source.into(),
            lines: source.lines().map(str::to_string).collect(),
        };
        esp_native_directives(&content)
            .into_iter()
            .map(|(_, directive)| directive)
            .collect()
    }
    #[test]
    fn foreign_architecture_is_excluded_but_disabled_features_remain() {
        assert_eq!(visible("#ifdef FL_IS_AVR\n#include <foreign.h>\n#endif\n#if 0\n#include <dormant.h>\n#endif"), vec!["#include <dormant.h>"]);
    }
    #[test]
    fn unknown_features_keep_both_arms_and_line_numbers() {
        assert_eq!(
            visible("#if FL_ESP32_SPI_ARDUINO\n#include <one.h>\n#else\n#include <two.h>\n#endif"),
            vec!["#include <one.h>", "#include <two.h>"]
        );
        let content = FileContent {
            path: "fixture.h".into(),
            content: String::new(),
            lines: vec!["#if 0".into(), "#include <keep.h>".into(), "#endif".into()],
        };
        assert_eq!(esp_native_directives(&content)[0].0, 2);
    }
    #[test]
    fn nested_elif_and_else_preserve_parent_constraints() {
        assert_eq!(visible("#if FEATURE\n#if FL_IS_ARM\n#include <bad.h>\n#elif defined(FL_IS_ESP32)\n#include <yes.h>\n#else\n#include <bad2.h>\n#endif\n#elif ESP32\n#include <maybe.h>\n#else\n#include <bad3.h>\n#endif"), vec!["#include <yes.h>", "#include <maybe.h>"]);
    }
    #[test]
    fn boolean_expressions_preserve_unknowns_conservatively() {
        assert_eq!(
            esp_architecture_condition("defined(FL_IS_AVR) && SDK_VERSION >= FOO(1)"),
            EspArchitectureTruth::False
        );
        assert_eq!(
            esp_architecture_condition("!defined(FL_IS_ARM) && (ESP32 || FEATURE)"),
            EspArchitectureTruth::True
        );
        assert_eq!(
            esp_architecture_condition("0"),
            EspArchitectureTruth::Unknown
        );
        assert_eq!(
            esp_architecture_condition("FL_IS_ESP_32S3"),
            EspArchitectureTruth::Unknown
        );
        assert_eq!(
            esp_architecture_condition("FL_IS_GCC"),
            EspArchitectureTruth::Unknown
        );
    }
}
