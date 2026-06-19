fn regex_has_include() -> &'static Regex {
    static VALUE: OnceLock<Regex> = OnceLock::new();
    VALUE.get_or_init(|| Regex::new(r"\b__has_include\s*\(").unwrap())
}

fn regex_static_assert() -> &'static Regex {
    static VALUE: OnceLock<Regex> = OnceLock::new();
    VALUE.get_or_init(|| Regex::new(r"\bstatic_assert\s*\(").unwrap())
}

fn regex_using_namespace_fl() -> &'static Regex {
    static VALUE: OnceLock<Regex> = OnceLock::new();
    VALUE.get_or_init(|| Regex::new(r"\busing\s+namespace\s+fl\s*;").unwrap())
}

fn regex_string_literal() -> &'static Regex {
    static VALUE: OnceLock<Regex> = OnceLock::new();
    VALUE.get_or_init(|| Regex::new(r#""(?:[^"\\]|\\.)*"|'(?:[^'\\]|\\.)*'"#).unwrap())
}

fn regex_new_alloc() -> &'static Regex {
    static VALUE: OnceLock<Regex> = OnceLock::new();
    VALUE.get_or_init(|| Regex::new(r"\bnew\s+[A-Za-z_:]").unwrap())
}

fn regex_delete() -> &'static Regex {
    static VALUE: OnceLock<Regex> = OnceLock::new();
    VALUE.get_or_init(|| Regex::new(r"\bdelete\b").unwrap())
}

fn regex_malloc_family() -> &'static Regex {
    static VALUE: OnceLock<Regex> = OnceLock::new();
    VALUE.get_or_init(|| Regex::new(r"\b(malloc|calloc|realloc)\s*\(").unwrap())
}

fn regex_free() -> &'static Regex {
    static VALUE: OnceLock<Regex> = OnceLock::new();
    VALUE.get_or_init(|| Regex::new(r"\bfree\s*\(").unwrap())
}

fn regex_deleted_func() -> &'static Regex {
    static VALUE: OnceLock<Regex> = OnceLock::new();
    VALUE.get_or_init(|| Regex::new(r"=\s*delete\s*[;{]").unwrap())
}

fn regex_deleted_func_eol() -> &'static Regex {
    static VALUE: OnceLock<Regex> = OnceLock::new();
    VALUE.get_or_init(|| Regex::new(r"=\s*delete\s*$").unwrap())
}

fn regex_inline_func() -> &'static Regex {
    static VALUE: OnceLock<Regex> = OnceLock::new();
    VALUE.get_or_init(|| Regex::new(r"\w+\s*\([^)]*\)\s*\{").unwrap())
}

fn regex_template() -> &'static Regex {
    static VALUE: OnceLock<Regex> = OnceLock::new();
    VALUE.get_or_init(|| Regex::new(r"^\s*template\s*<").unwrap())
}

fn regex_static_var() -> &'static Regex {
    static VALUE: OnceLock<Regex> = OnceLock::new();
    VALUE.get_or_init(|| {
        Regex::new(r"\bstatic\s+(?:const\s+)?[\w:]+(?:<[^>]+>)?\s+\w+\s*[=({]").unwrap()
    })
}

fn regex_static_func() -> &'static Regex {
    static VALUE: OnceLock<Regex> = OnceLock::new();
    VALUE.get_or_init(|| Regex::new(r"static\s+[\w:]+(?:<[^>]+>)?\s+\w+\s*\([^)]*\)\s*\{").unwrap())
}

fn regex_include_path() -> &'static Regex {
    static VALUE: OnceLock<Regex> = OnceLock::new();
    VALUE.get_or_init(|| Regex::new(r#"#include\s+"([^"]+)""#).unwrap())
}

fn regex_quoted_include_line() -> &'static Regex {
    static VALUE: OnceLock<Regex> = OnceLock::new();
    VALUE.get_or_init(|| Regex::new(r#"^\s*#\s*include\s+"([^"]+)""#).unwrap())
}

fn regex_builtin_memcpy() -> &'static Regex {
    static VALUE: OnceLock<Regex> = OnceLock::new();
    VALUE.get_or_init(|| Regex::new(r"\b__builtin_memcpy\s*\(").unwrap())
}

fn regex_weak_attribute() -> &'static Regex {
    static VALUE: OnceLock<Regex> = OnceLock::new();
    VALUE.get_or_init(|| Regex::new(r"__attribute__\s*\(\s*\(\s*weak\s*\)\s*\)").unwrap())
}

fn regex_banned_namespace_fl_fl() -> &'static Regex {
    static VALUE: OnceLock<Regex> = OnceLock::new();
    VALUE.get_or_init(|| Regex::new(r"\bfl::fl\b").unwrap())
}

fn regex_cpp_include() -> &'static Regex {
    static VALUE: OnceLock<Regex> = OnceLock::new();
    VALUE.get_or_init(|| Regex::new(r#"#include\s+[<"]([^>"]+\.cpp)[>"]"#).unwrap())
}

fn regex_cpp_hpp_include() -> &'static Regex {
    static VALUE: OnceLock<Regex> = OnceLock::new();
    VALUE.get_or_init(|| Regex::new(r#"#\s*include\s+[<"]([^>"]+\.cpp\.hpp)[>"]"#).unwrap())
}

fn regex_include_any_path() -> &'static Regex {
    static VALUE: OnceLock<Regex> = OnceLock::new();
    VALUE.get_or_init(|| Regex::new(r#"^\s*#\s*include\s*[<"](.+?)[>"]"#).unwrap())
}

fn regex_iwyu_include() -> &'static Regex {
    static VALUE: OnceLock<Regex> = OnceLock::new();
    VALUE.get_or_init(|| Regex::new(r#"^\s*#include\s+[<"]([^>"]+)[>"](?:\s*//\s*(.*))?"#).unwrap())
}

fn regex_iwyu_begin() -> &'static Regex {
    static VALUE: OnceLock<Regex> = OnceLock::new();
    VALUE.get_or_init(|| Regex::new(r"(?i)^\s*//\s*IWYU\s+pragma:\s*begin_keep").unwrap())
}

fn regex_iwyu_end() -> &'static Regex {
    static VALUE: OnceLock<Regex> = OnceLock::new();
    VALUE.get_or_init(|| Regex::new(r"(?i)^\s*//\s*IWYU\s+pragma:\s*end_keep").unwrap())
}

fn regex_iwyu_keep_inline() -> &'static Regex {
    static VALUE: OnceLock<Regex> = OnceLock::new();
    VALUE.get_or_init(|| Regex::new(r"(?i)IWYU\s+pragma:\s*keep").unwrap())
}

fn regex_impl_hpp_include() -> &'static Regex {
    static VALUE: OnceLock<Regex> = OnceLock::new();
    VALUE.get_or_init(|| Regex::new(r#"#\s*include\s+[<"]([^>"]+\.impl\.hpp)[>"]"#).unwrap())
}

fn regex_asm_js_macro() -> &'static Regex {
    static VALUE: OnceLock<Regex> = OnceLock::new();
    VALUE.get_or_init(|| Regex::new(r"\b(?:EM_JS|EM_ASYNC_JS|EM_ASM)\s*\(").unwrap())
}

fn regex_esp_rom_printf() -> &'static Regex {
    static VALUE: OnceLock<Regex> = OnceLock::new();
    VALUE.get_or_init(|| Regex::new(r"\besp_rom_printf\s*\(").unwrap())
}

fn regex_sleep_for() -> &'static Regex {
    static VALUE: OnceLock<Regex> = OnceLock::new();
    VALUE.get_or_init(|| Regex::new(r"\bsleep_for\s*\(").unwrap())
}

fn regex_thread_local() -> &'static Regex {
    static VALUE: OnceLock<Regex> = OnceLock::new();
    VALUE.get_or_init(|| Regex::new(r"\bthread_local\b").unwrap())
}

fn regex_span_data_size() -> &'static Regex {
    static VALUE: OnceLock<Regex> = OnceLock::new();
    VALUE.get_or_init(|| {
        Regex::new(r"span<[^>]+>\s*\([^)]*\.data\(\)[^)]*\.size\(\)[^)]*\)").unwrap()
    })
}

fn regex_relative_include() -> &'static Regex {
    static VALUE: OnceLock<Regex> = OnceLock::new();
    VALUE.get_or_init(|| Regex::new(r#"^\s*#\s*include\s+[<"]([^>"]*\.\..*)[>"]"#).unwrap())
}

fn regex_fastled_h_include() -> &'static Regex {
    static VALUE: OnceLock<Regex> = OnceLock::new();
    VALUE.get_or_init(|| Regex::new(r#"^\s*#\s*include\s+[<"]FastLED\.h[>"]"#).unwrap())
}

fn regex_fastled_internal_define() -> &'static Regex {
    static VALUE: OnceLock<Regex> = OnceLock::new();
    VALUE.get_or_init(|| Regex::new(r"^\s*#\s*define\s+FASTLED_INTERNAL").unwrap())
}

fn regex_arduino_macro() -> &'static Regex {
    static VALUE: OnceLock<Regex> = OnceLock::new();
    VALUE.get_or_init(|| Regex::new(r"\b(INPUT|OUTPUT|DEFAULT)\b").unwrap())
}

fn regex_arduino_scoped_enum() -> &'static Regex {
    static VALUE: OnceLock<Regex> = OnceLock::new();
    VALUE.get_or_init(|| Regex::new(r"::\s*(?:INPUT|OUTPUT|DEFAULT)\b").unwrap())
}

fn regex_arduino_enum_member() -> &'static Regex {
    static VALUE: OnceLock<Regex> = OnceLock::new();
    VALUE.get_or_init(|| Regex::new(r"^\s*(?:INPUT|OUTPUT|DEFAULT)\s*(?:=\s*\w+)?\s*[,}]").unwrap())
}

fn regex_arduino_preprocessor() -> &'static Regex {
    static VALUE: OnceLock<Regex> = OnceLock::new();
    VALUE.get_or_init(|| {
        Regex::new(r"^\s*#\s*(?:define|undef|ifdef|ifndef|if|elif|pragma|error|include)\b").unwrap()
    })
}

fn regex_standard_attribute() -> &'static Regex {
    static VALUE: OnceLock<Regex> = OnceLock::new();
    VALUE.get_or_init(|| Regex::new(r"\[\[\s*([a-z_]+)\s*\]\]").unwrap())
}

fn regex_alignas() -> &'static Regex {
    static VALUE: OnceLock<Regex> = OnceLock::new();
    VALUE.get_or_init(|| Regex::new(r"\balignas\s*\(").unwrap())
}

fn regex_preprocessor_if_elif() -> &'static Regex {
    static VALUE: OnceLock<Regex> = OnceLock::new();
    VALUE.get_or_init(|| Regex::new(r"^\s*#\s*(?:if|elif)\b").unwrap())
}

fn regex_preprocessor_conditional() -> &'static Regex {
    static VALUE: OnceLock<Regex> = OnceLock::new();
    VALUE.get_or_init(|| Regex::new(r"^\s*#\s*(?:if|ifdef|ifndef|elif)\b").unwrap())
}

fn regex_preprocessor_include() -> &'static Regex {
    static VALUE: OnceLock<Regex> = OnceLock::new();
    VALUE.get_or_init(|| Regex::new(r"^#\s*include\b").unwrap())
}

fn regex_namespace_declaration() -> &'static Regex {
    static VALUE: OnceLock<Regex> = OnceLock::new();
    VALUE.get_or_init(|| Regex::new(r"^\s*(?:namespace\s+\w+|namespace\s*\{)").unwrap())
}

fn regex_bare_using_decl() -> &'static Regex {
    static VALUE: OnceLock<Regex> = OnceLock::new();
    VALUE.get_or_init(|| {
        Regex::new(r"^\s*using\s+(?:namespace\s+\w+(?:::\w+)*|\w+(?:::\w+)+)\s*;").unwrap()
    })
}

fn regex_bare_using_suppress() -> &'static Regex {
    static VALUE: OnceLock<Regex> = OnceLock::new();
    VALUE.get_or_init(|| Regex::new(r"(?i)//\s*ok(?:ay)?\s+bare\s+using").unwrap())
}

fn regex_named_namespace_open() -> &'static Regex {
    static VALUE: OnceLock<Regex> = OnceLock::new();
    VALUE.get_or_init(|| Regex::new(r"^\s*namespace\s+\w+").unwrap())
}

fn regex_anon_namespace_open() -> &'static Regex {
    static VALUE: OnceLock<Regex> = OnceLock::new();
    VALUE.get_or_init(|| Regex::new(r"^\s*namespace\s*\{").unwrap())
}

fn regex_extern_c_open() -> &'static Regex {
    static VALUE: OnceLock<Regex> = OnceLock::new();
    VALUE.get_or_init(|| Regex::new(r#"^\s*extern\s+"C""#).unwrap())
}

fn regex_local_scope_keyword() -> &'static Regex {
    static VALUE: OnceLock<Regex> = OnceLock::new();
    VALUE.get_or_init(|| Regex::new(r"^\s*(?:class|struct|enum|union)\b").unwrap())
}

fn regex_preprocessor_line() -> &'static Regex {
    static VALUE: OnceLock<Regex> = OnceLock::new();
    VALUE.get_or_init(|| Regex::new(r"^\s*#").unwrap())
}

fn regex_any_include() -> &'static Regex {
    static VALUE: OnceLock<Regex> = OnceLock::new();
    VALUE.get_or_init(|| Regex::new(r#"^\s*#\s*include\s*[<"].*[>"]"#).unwrap())
}

fn regex_banned_header_include() -> &'static Regex {
    static VALUE: OnceLock<Regex> = OnceLock::new();
    VALUE.get_or_init(|| Regex::new(r#"#include\s+[<"]([^>"]+)[>"]"#).unwrap())
}

fn regex_private_libcpp_header() -> &'static Regex {
    static VALUE: OnceLock<Regex> = OnceLock::new();
    VALUE.get_or_init(|| Regex::new(r#"#include\s+["<](__[^_][^">]*)[">]"#).unwrap())
}

fn regex_namespace_include_using() -> &'static Regex {
    static VALUE: OnceLock<Regex> = OnceLock::new();
    VALUE.get_or_init(|| Regex::new(r"^\s*using\s+namespace\s+\w+\s*;").unwrap())
}

fn regex_namespace_include_open() -> &'static Regex {
    static VALUE: OnceLock<Regex> = OnceLock::new();
    VALUE.get_or_init(|| Regex::new(r"^\s*namespace\s+\w+\s*\{").unwrap())
}

fn regex_namespace_include_directive() -> &'static Regex {
    static VALUE: OnceLock<Regex> = OnceLock::new();
    VALUE.get_or_init(|| Regex::new(r"^\s*#\s*include").unwrap())
}

fn regex_named_enum() -> &'static Regex {
    static VALUE: OnceLock<Regex> = OnceLock::new();
    VALUE.get_or_init(|| Regex::new(r"\benum\s+([A-Za-z_]\w*)\s*[{:;]").unwrap())
}

fn regex_class_struct() -> &'static Regex {
    static VALUE: OnceLock<Regex> = OnceLock::new();
    VALUE.get_or_init(|| {
        Regex::new(r"\b(?:class|struct)\s+(?:[A-Za-z_]\w*\s+)*[A-Za-z_]\w*").unwrap()
    })
}

fn regex_noexcept_class_def() -> &'static Regex {
    static VALUE: OnceLock<Regex> = OnceLock::new();
    VALUE.get_or_init(|| Regex::new(r"\b(?:class|struct)\s+(\w+)").unwrap())
}

fn regex_noexcept_suppress_special() -> &'static Regex {
    static VALUE: OnceLock<Regex> = OnceLock::new();
    VALUE.get_or_init(|| Regex::new(r"(?i)//\s*(?:ok\s+no\s+FL_NO_EXCEPT|nolint)\b").unwrap())
}

fn regex_has_noexcept_special() -> &'static Regex {
    static VALUE: OnceLock<Regex> = OnceLock::new();
    VALUE.get_or_init(|| Regex::new(r"\b(?:FL_NO_EXCEPT|noexcept)\b").unwrap())
}

fn regex_destructor_decl() -> &'static Regex {
    static VALUE: OnceLock<Regex> = OnceLock::new();
    VALUE.get_or_init(|| Regex::new(r"~(\w+)\s*\(").unwrap())
}

fn regex_operator_assign_decl() -> &'static Regex {
    static VALUE: OnceLock<Regex> = OnceLock::new();
    VALUE.get_or_init(|| Regex::new(r"\boperator\s*=\s*\(").unwrap())
}

fn regex_member_trailing_underscore() -> &'static Regex {
    static VALUE: OnceLock<Regex> = OnceLock::new();
    VALUE.get_or_init(|| {
        Regex::new(
            r"\b(?:const\s+)?(?:static\s+)?(?:volatile\s+)?(?:mutable\s+)?[\w:]+(?:<[^>]+>)?[\s*&]+\s*(\w{2,}_)\s*[;=,)]",
        )
        .unwrap()
    })
}

fn regex_member_m_underscore() -> &'static Regex {
    static VALUE: OnceLock<Regex> = OnceLock::new();
    VALUE.get_or_init(|| {
        Regex::new(
            r"\b(?:const\s+)?(?:static\s+)?(?:volatile\s+)?(?:mutable\s+)?[\w:]+(?:<[^>]+>)?[\s*&]+\s*(m_\w+)\s*[;=,)]",
        )
        .unwrap()
    })
}

fn regex_allow_include_after_namespace() -> &'static Regex {
    static VALUE: OnceLock<Regex> = OnceLock::new();
    VALUE.get_or_init(|| Regex::new(r"//\s*allow-include-after-namespace").unwrap())
}

fn regex_nolint() -> &'static Regex {
    static VALUE: OnceLock<Regex> = OnceLock::new();
    VALUE.get_or_init(|| Regex::new(r"//\s*nolint").unwrap())
}

fn regex_namespace_fl_declaration() -> &'static Regex {
    static VALUE: OnceLock<Regex> = OnceLock::new();
    VALUE.get_or_init(|| Regex::new(r"\bnamespace\s+fl\s*\{").unwrap())
}

fn regex_namespace_platform_singular() -> &'static Regex {
    static VALUE: OnceLock<Regex> = OnceLock::new();
    VALUE.get_or_init(|| Regex::new(r"\bnamespace\s+platform\s*\{").unwrap())
}

fn regex_subdir_namespace_decl() -> &'static Regex {
    static VALUE: OnceLock<Regex> = OnceLock::new();
    VALUE.get_or_init(|| Regex::new(r"\bnamespace\s+([\w:]+)\s*\{").unwrap())
}

fn regex_using_namespace() -> &'static Regex {
    static VALUE: OnceLock<Regex> = OnceLock::new();
    VALUE.get_or_init(|| Regex::new(r"\busing\s+namespace\s+\w+(?:::\w+)*\s*;").unwrap())
}

fn regex_define_using_namespace() -> &'static Regex {
    static VALUE: OnceLock<Regex> = OnceLock::new();
    VALUE.get_or_init(|| Regex::new(r"#define\s+\w+.*using\s+namespace").unwrap())
}

fn regex_force_use_namespace() -> &'static Regex {
    static VALUE: OnceLock<Regex> = OnceLock::new();
    VALUE.get_or_init(|| Regex::new(r"#if\s+.*FORCE_USE_NAMESPACE").unwrap())
}

fn regex_fl_is_token() -> &'static Regex {
    static VALUE: OnceLock<Regex> = OnceLock::new();
    VALUE.get_or_init(|| Regex::new(r"\bFL_IS_\w+\b").unwrap())
}

fn regex_defined_fl_is() -> &'static Regex {
    static VALUE: OnceLock<Regex> = OnceLock::new();
    VALUE.get_or_init(|| Regex::new(r"!?\s*defined\s*\(\s*FL_IS_\w+\s*\)").unwrap())
}

fn regex_singleton() -> &'static Regex {
    static VALUE: OnceLock<Regex> = OnceLock::new();
    VALUE.get_or_init(|| Regex::new(r"\bSingleton\s*<").unwrap())
}

fn regex_singleton_shared() -> &'static Regex {
    static VALUE: OnceLock<Regex> = OnceLock::new();
    VALUE.get_or_init(|| Regex::new(r"\bSingletonShared\b").unwrap())
}

fn regex_friend_class() -> &'static Regex {
    static VALUE: OnceLock<Regex> = OnceLock::new();
    VALUE.get_or_init(|| Regex::new(r"\bfriend\s+class\b").unwrap())
}

fn regex_numeric_limit_macro() -> &'static Regex {
    static VALUE: OnceLock<Regex> = OnceLock::new();
    VALUE.get_or_init(|| Regex::new(&format!(r"\b({})\b", NUMERIC_LIMIT_MACROS.join("|"))).unwrap())
}

fn regex_platform_pragma() -> &'static Regex {
    static VALUE: OnceLock<Regex> = OnceLock::new();
    VALUE.get_or_init(|| {
        Regex::new(r"^\s*#\s*pragma\s+(?:GCC\s+diagnostic|clang\s+diagnostic|warning\s*\()")
            .unwrap()
    })
}

fn regex_raw_pragma() -> &'static Regex {
    static VALUE: OnceLock<Regex> = OnceLock::new();
    VALUE.get_or_init(|| Regex::new(r"\b_Pragma\s*\(").unwrap())
}

fn regex_raw_noexcept() -> &'static Regex {
    static VALUE: OnceLock<Regex> = OnceLock::new();
    VALUE.get_or_init(|| Regex::new(r"\bnoexcept\b").unwrap())
}

fn regex_define_fl_noexcept() -> &'static Regex {
    static VALUE: OnceLock<Regex> = OnceLock::new();
    VALUE.get_or_init(|| Regex::new(r"#\s*define\s+FL_NO_EXCEPT\b").unwrap())
}

fn regex_noexcept_suppression() -> &'static Regex {
    static VALUE: OnceLock<Regex> = OnceLock::new();
    VALUE.get_or_init(|| Regex::new(r"(?i)//\s*(?:ok\s+noexcept|nolint)\b").unwrap())
}

fn regex_std_usage() -> &'static Regex {
    static VALUE: OnceLock<Regex> = OnceLock::new();
    VALUE.get_or_init(|| Regex::new(r"std::\w+").unwrap())
}

fn regex_serial_method() -> &'static Regex {
    static VALUE: OnceLock<Regex> = OnceLock::new();
    VALUE.get_or_init(|| Regex::new(r"\bSerial\.(\w+)\s*\(").unwrap())
}

fn regex_iram_function() -> &'static Regex {
    static VALUE: OnceLock<Regex> = OnceLock::new();
    VALUE.get_or_init(|| {
        Regex::new(
            r"FL_IRAM[\s\n]+(?:static\s+)?(?:inline\s+)?(?:virtual\s+)?(?:const\s+)?(?:(\w+(?:\s*<[^>]+>)?)\s+)?(?:__attribute__\s*\([^)]*\)\s+)?([\w:]+)\s*\([^)]*\)(?:\s*(?:const|override|final))?[^{]*\{",
        )
        .unwrap()
    })
}

fn regex_iram_banned_macro() -> &'static Regex {
    static VALUE: OnceLock<Regex> = OnceLock::new();
    VALUE.get_or_init(|| Regex::new(r"\b(FL_WARN|FL_ERROR|FL_DBG|FL_ASSERT)\s*\(").unwrap())
}

fn regex_fl_log_macro() -> &'static Regex {
    static VALUE: OnceLock<Regex> = OnceLock::new();
    VALUE.get_or_init(|| Regex::new(r"\bFL_LOG_\w+\s*\(").unwrap())
}

fn regex_unit_test_macro_call() -> &'static Regex {
    static VALUE: OnceLock<Regex> = OnceLock::new();
    VALUE.get_or_init(|| Regex::new(r"\b([A-Z_]+)\s*\(").unwrap())
}

fn regex_deep_platform_header() -> &'static Regex {
    static VALUE: OnceLock<Regex> = OnceLock::new();
    VALUE.get_or_init(|| Regex::new(r#"(#include\s+"(platforms/[^/]+/[^"]+\.h[^"]*)")"#).unwrap())
}

fn regex_deep_platform_include() -> &'static Regex {
    static VALUE: OnceLock<Regex> = OnceLock::new();
    VALUE.get_or_init(|| {
        Regex::new(r#"^\s*#\s*include\s+[<"]platforms/([^/"]+)/([^"]+)[">]"#).unwrap()
    })
}

// --- Patterns for the bare/legacy ports (FastLED #3297) ----------------------

fn regex_bare_noinline() -> &'static Regex {
    static VALUE: OnceLock<Regex> = OnceLock::new();
    VALUE.get_or_init(|| {
        Regex::new(
            r"__attribute__\s*\(\s*\([^)]*\bnoinline\b[^)]*\)\s*\)|__declspec\s*\(\s*noinline\s*\)",
        )
        .unwrap()
    })
}

fn regex_bare_snprintf() -> &'static Regex {
    static VALUE: OnceLock<Regex> = OnceLock::new();
    VALUE.get_or_init(|| {
        // \b excludes identifier-char prefixes; the manual
        // `printf_family_has_qualifier_prefix` adds `:` / `.` exclusion to
        // match the Python lookbehind `(?<![A-Za-z0-9_:.\b])`.
        Regex::new(r"\b(snprintf|sprintf|vsnprintf|vsprintf|fprintf|vfprintf|printf|vprintf)\s*\(")
            .unwrap()
    })
}

fn regex_bare_libm() -> &'static Regex {
    static VALUE: OnceLock<Regex> = OnceLock::new();
    VALUE.get_or_init(|| {
        // Optional `::` is part of the match (mirrors Python `(?:::)?`).
        // `\b` excludes identifier-char prefixes; `libm_has_qualifier_prefix`
        // adds `:` exclusion to mirror Python lookbehind `(?<![A-Za-z0-9_:])`.
        const FUNCS: &str = concat!(
            // Trigonometry
            "sin|sinf|cos|cosf|tan|tanf|asin|asinf|acos|acosf|atan|atanf|atan2|atan2f|",
            // Power / log / exponential
            "sqrt|sqrtf|pow|powf|exp|expf|exp2|exp2f|log|logf|log10|log10f|log2|log2f|",
            // IEEE-754 manipulation
            "ldexp|ldexpf|frexp|frexpf|scalbn|scalbnf|",
            // Modulo / hypot / fabs / round
            "fmod|fmodf|hypot|hypotf|fabs|fabsf|lround|lroundf|round|roundf"
        );
        Regex::new(&format!(r"(?:::)?\b(?:{FUNCS})\s*\(")).unwrap()
    })
}

fn regex_fl_no_underscore() -> &'static Regex {
    static VALUE: OnceLock<Regex> = OnceLock::new();
    VALUE.get_or_init(|| Regex::new(r"\bFL_NO[A-Z][A-Z0-9_]*\b").unwrap())
}

fn regex_public_setter_decl() -> &'static Regex {
    static VALUE: OnceLock<Regex> = OnceLock::new();
    VALUE.get_or_init(|| {
        Regex::new(
            r"^(?:(?:inline|static|virtual|explicit|constexpr|FL_NO_EXCEPT|FASTLED_FORCE_INLINE)\s+)*\
(?:[\w:]+(?:<[^>]*>)?(?:\s*[*&])?\s+)+\
(?P<name>(?:set|enable|disable|use)_[a-z][a-zA-Z0-9_]*)\s*\(",
        )
        .unwrap()
    })
}

fn regex_public_settings_suppression() -> &'static Regex {
    static VALUE: OnceLock<Regex> = OnceLock::new();
    VALUE.get_or_init(|| Regex::new(r"(?i)//\s*(?:nolint|ok\s+public_settings)\b").unwrap())
}

fn regex_named_namespace() -> &'static Regex {
    static VALUE: OnceLock<Regex> = OnceLock::new();
    VALUE.get_or_init(|| Regex::new(r"^\s*namespace\s+(\w+)\s*\{").unwrap())
}

fn regex_anonymous_namespace() -> &'static Regex {
    static VALUE: OnceLock<Regex> = OnceLock::new();
    VALUE.get_or_init(|| Regex::new(r"^\s*namespace\s*\{").unwrap())
}

fn regex_class_or_struct_keyword() -> &'static Regex {
    static VALUE: OnceLock<Regex> = OnceLock::new();
    VALUE.get_or_init(|| Regex::new(r"\b(class|struct)\b").unwrap())
}

fn regex_class_or_struct_forward_decl() -> &'static Regex {
    static VALUE: OnceLock<Regex> = OnceLock::new();
    VALUE.get_or_init(|| Regex::new(r"\b(class|struct)\b\s+\w+\s*;").unwrap())
}

fn regex_unity_build_hpp_include() -> &'static Regex {
    static VALUE: OnceLock<Regex> = OnceLock::new();
    VALUE.get_or_init(|| {
        Regex::new(r#"(?m)^\s*#include\s+"([^"]+/_build\.cpp\.hpp)""#).unwrap()
    })
}

fn regex_unity_cpp_hpp_include() -> &'static Regex {
    static VALUE: OnceLock<Regex> = OnceLock::new();
    VALUE.get_or_init(|| Regex::new(r#"(?m)^\s*#include\s+"[^"]+\.cpp\.hpp""#).unwrap())
}

fn regex_unity_cpp_hpp_include_capture() -> &'static Regex {
    static VALUE: OnceLock<Regex> = OnceLock::new();
    VALUE.get_or_init(|| Regex::new(r#"(?m)^\s*#include\s+"([^"]+\.cpp\.hpp)""#).unwrap())
}

fn regex_unity_any_include() -> &'static Regex {
    static VALUE: OnceLock<Regex> = OnceLock::new();
    VALUE.get_or_init(|| Regex::new(r#"(?m)^\s*#include\s+"([^"]+)""#).unwrap())
}

fn regex_simple_identifier() -> &'static Regex {
    static VALUE: OnceLock<Regex> = OnceLock::new();
    VALUE.get_or_init(|| Regex::new(r"^[a-zA-Z_]\w*$").unwrap())
}

fn regex_legacy_log_macro() -> &'static Regex {
    static VALUE: OnceLock<Regex> = OnceLock::new();
    VALUE.get_or_init(|| {
        // Order matters: longer names come first so regex alternation prefers
        // them (e.g. FL_LOG_RMT_ASYNC_MAIN must beat FL_LOG_RMT). The list
        // mirrors LEGACY_MACROS in the Python source.
        const MACROS: &[&str] = &[
            "FL_LOG_OBJECTFLED_ASYNC_MAIN",
            "FL_LOG_INTERRUPT_ASYNC_MAIN",
            "FL_LOG_PARLIO_ASYNC_MAIN",
            "FL_LOG_AUDIO_ASYNC_MAIN",
            "FL_LOG_FLEXIO_ASYNC_MAIN",
            "FL_LOG_SPI_ASYNC_MAIN",
            "FL_LOG_RMT_ASYNC_MAIN",
            "FL_LOG_OBJECTFLED",
            "FL_LOG_INTERRUPT",
            "FL_LOG_FLEXIO",
            "FL_LOG_PARLIO",
            "FL_LOG_AUDIO",
            "FL_LOG_ASYNC",
            "FL_WARN_FMT_IF",
            "FL_WARN_EVERY",
            "FL_PRINT_EVERY",
            "FL_DBG_EVERY",
            "FL_LOG_SPI",
            "FL_LOG_RMT",
            "FL_WARN_ONCE",
            "FL_WARN_FMT",
            "FL_ERROR_IF",
            "FL_WARN_IF",
            "FL_DBG_IF",
            "FL_PRINT",
            "FL_ERROR",
            "FL_WARN",
            "FL_DBG",
        ];
        Regex::new(&format!(r"\b({})\s*\(", MACROS.join("|"))).unwrap()
    })
}

