#!/usr/bin/env python3
"""Temporary unit tests for NativePlatformDefinesChecker.

Tests both the fast-scan gate and the slow-scan precision to ensure:
  - No false negatives (every real violation is caught)
  - No false positives (non-violations are never flagged)
"""

import unittest

from ci.lint_cpp.native_platform_defines_checker import (
    NATIVE_TO_MODERN_DEFINES,
    NativePlatformDefinesChecker,
    _fast_scan_dominated,
)
from ci.util.check_files import FileContent


def _make_content(code: str, path: str = "test.h") -> FileContent:
    """Helper: build a FileContent from a code string."""
    lines = code.splitlines()
    return FileContent(path=path, content=code, lines=lines)


def _get_violations(code: str) -> list[tuple[int, str]]:
    """Run checker on code and return violations list (line_number, message)."""
    checker = NativePlatformDefinesChecker()
    fc = _make_content(code)
    checker.check_file_content(fc)
    return checker.violations.get("test.h", [])


# ── Fast scan tests ──────────────────────────────────────────────────


class TestFastScan(unittest.TestCase):
    """Tests for _fast_scan_dominated — the quick rejection gate."""

    def test_empty_file(self):
        self.assertFalse(_fast_scan_dominated(""))

    def test_no_conditionals_no_define(self):
        self.assertFalse(_fast_scan_dominated("int x = 1;\n"))

    def test_has_conditional_no_define(self):
        """#ifdef present but no native define → skip."""
        self.assertFalse(_fast_scan_dominated("#ifdef SOMETHING\n#endif\n"))

    def test_has_define_no_conditional(self):
        """__AVR__ present but no #if → skip."""
        self.assertFalse(_fast_scan_dominated('const char* s = "__AVR__";\n'))

    def test_has_both(self):
        """Both conditional and define present → must proceed to slow scan."""
        self.assertTrue(_fast_scan_dominated("#ifdef __AVR__\nint x;\n#endif\n"))

    def test_define_in_comment_still_passes_fast(self):
        """Fast scan doesn't parse comments — this is an allowed false positive."""
        self.assertTrue(_fast_scan_dominated("// #ifdef __AVR__\nint x;\n"))

    def test_define_in_string_still_passes_fast(self):
        """Fast scan doesn't parse strings — allowed false positive."""
        code = '#ifdef FOO\nconst char* s = "__AVR__";\n#endif\n'
        self.assertTrue(_fast_scan_dominated(code))

    def test_fast_scan_every_native_define(self):
        """Fast scan must pass for every define in the mapping."""
        for native in NATIVE_TO_MODERN_DEFINES:
            code = f"#ifdef {native}\n#endif\n"
            with self.subTest(native=native):
                self.assertTrue(
                    _fast_scan_dominated(code),
                    f"Fast scan missed {native}",
                )


# ── True positives: must detect ──────────────────────────────────────


class TestTruePositives(unittest.TestCase):
    """Every one of these MUST produce exactly one violation."""

    def test_ifdef(self):
        v = _get_violations("#ifdef __AVR__\n#endif\n")
        self.assertEqual(len(v), 1)
        self.assertIn("__AVR__", v[0][1])

    def test_ifndef(self):
        v = _get_violations("#ifndef __AVR__\n#endif\n")
        self.assertEqual(len(v), 1)

    def test_if_defined(self):
        v = _get_violations("#if defined(__AVR__)\n#endif\n")
        self.assertEqual(len(v), 1)

    def test_if_not_defined(self):
        v = _get_violations("#if !defined(__AVR__)\n#endif\n")
        self.assertEqual(len(v), 1)

    def test_elif_defined(self):
        v = _get_violations("#if 0\n#elif defined(__AVR__)\n#endif\n")
        self.assertEqual(len(v), 1)
        self.assertEqual(v[0][0], 2)  # line 2

    def test_if_bare(self):
        v = _get_violations("#if __AVR__\n#endif\n")
        self.assertEqual(len(v), 1)

    def test_indented(self):
        v = _get_violations("    #ifdef __AVR__\n    #endif\n")
        self.assertEqual(len(v), 1)

    def test_space_after_hash(self):
        v = _get_violations("# ifdef __AVR__\n# endif\n")
        self.assertEqual(len(v), 1)

    def test_compound_and(self):
        v = _get_violations("#if defined(__AVR__) && defined(OTHER)\n#endif\n")
        self.assertEqual(len(v), 1)

    def test_compound_or(self):
        v = _get_violations("#if defined(OTHER) || defined(__AVR__)\n#endif\n")
        self.assertEqual(len(v), 1)

    def test_trailing_comment_still_caught(self):
        """Define before the // comment portion is still flagged."""
        v = _get_violations("#ifdef __AVR__ // avr guard\n#endif\n")
        self.assertEqual(len(v), 1)

    def test_multiple_violations(self):
        code = "#ifdef __AVR__\n#endif\n#ifndef __AVR__\n#endif\n"
        v = _get_violations(code)
        self.assertEqual(len(v), 2)
        self.assertEqual(v[0][0], 1)
        self.assertEqual(v[1][0], 3)

    def test_elif_bare(self):
        v = _get_violations("#if 0\n#elif __AVR__\n#endif\n")
        self.assertEqual(len(v), 1)
        self.assertEqual(v[0][0], 2)

    def test_tabs_before_hash(self):
        v = _get_violations("\t#ifdef __AVR__\n\t#endif\n")
        self.assertEqual(len(v), 1)

    def test_mixed_whitespace(self):
        v = _get_violations("\t  #  ifdef __AVR__\n#endif\n")
        self.assertEqual(len(v), 1)


# ── Parametric: every define in the mapping must be caught ───────────


class TestAllDefinesCaught(unittest.TestCase):
    """For every (native, modern) pair in NATIVE_TO_MODERN_DEFINES,
    verify #ifdef <native> produces a violation suggesting <modern>."""

    def test_ifdef_every_define(self):
        for native, modern in NATIVE_TO_MODERN_DEFINES.items():
            code = f"#ifdef {native}\n#endif\n"
            with self.subTest(native=native, modern=modern):
                v = _get_violations(code)
                self.assertEqual(len(v), 1, f"Expected 1 violation for {native}")
                self.assertIn(native, v[0][1])
                self.assertIn(modern, v[0][1])

    def test_if_defined_every_define(self):
        for native, modern in NATIVE_TO_MODERN_DEFINES.items():
            code = f"#if defined({native})\n#endif\n"
            with self.subTest(native=native, modern=modern):
                v = _get_violations(code)
                self.assertEqual(len(v), 1, f"Expected 1 violation for {native}")

    def test_ifndef_every_define(self):
        for native, modern in NATIVE_TO_MODERN_DEFINES.items():
            code = f"#ifndef {native}\n#endif\n"
            with self.subTest(native=native, modern=modern):
                v = _get_violations(code)
                self.assertEqual(len(v), 1, f"Expected 1 violation for {native}")

    def test_elif_every_define(self):
        for native, modern in NATIVE_TO_MODERN_DEFINES.items():
            code = f"#if 0\n#elif defined({native})\n#endif\n"
            with self.subTest(native=native, modern=modern):
                v = _get_violations(code)
                self.assertEqual(len(v), 1, f"Expected 1 violation for {native}")
                self.assertEqual(v[0][0], 2)


# ── AVR sub-family specific tests ───────────────────────────────────


class TestAVRSubFamilies(unittest.TestCase):
    """Test representative defines from each AVR sub-family."""

    def test_avr_general(self):
        v = _get_violations("#ifdef __AVR__\n#endif\n")
        self.assertEqual(len(v), 1)
        self.assertIn("FL_IS_AVR", v[0][1])

    def test_atmega_328p_family(self):
        v = _get_violations("#ifdef __AVR_ATmega328P__\n#endif\n")
        self.assertEqual(len(v), 1)
        self.assertIn("FL_IS_AVR_ATMEGA_328P", v[0][1])

    def test_atmega_family(self):
        v = _get_violations("#ifdef __AVR_ATmega2560__\n#endif\n")
        self.assertEqual(len(v), 1)
        self.assertIn("FL_IS_AVR_ATMEGA", v[0][1])
        # Must NOT suggest FL_IS_AVR_ATMEGA_328P
        self.assertNotIn("328P", v[0][1])

    def test_megaavr_family(self):
        v = _get_violations("#ifdef __AVR_ATmega4809__\n#endif\n")
        self.assertEqual(len(v), 1)
        self.assertIn("FL_IS_AVR_MEGAAVR", v[0][1])

    def test_attiny_classic(self):
        v = _get_violations("#ifdef __AVR_ATtiny85__\n#endif\n")
        self.assertEqual(len(v), 1)
        self.assertIn("FL_IS_AVR_ATTINY", v[0][1])

    def test_attiny_modern(self):
        v = _get_violations("#ifdef __AVR_ATtiny412__\n#endif\n")
        self.assertEqual(len(v), 1)
        self.assertIn("FL_IS_AVR_ATTINY", v[0][1])

    def test_attiny_wildcard(self):
        v = _get_violations("#ifdef __AVR_ATtinyxy4__\n#endif\n")
        self.assertEqual(len(v), 1)
        self.assertIn("FL_IS_AVR_ATTINY", v[0][1])

    def test_digispark(self):
        v = _get_violations("#ifdef ARDUINO_AVR_DIGISPARK\n#endif\n")
        self.assertEqual(len(v), 1)
        self.assertIn("FL_IS_AVR_ATTINY", v[0][1])

    def test_is_bean(self):
        v = _get_violations("#ifdef IS_BEAN\n#endif\n")
        self.assertEqual(len(v), 1)
        self.assertIn("FL_IS_AVR_ATTINY", v[0][1])

    def test_at90usb(self):
        v = _get_violations("#ifdef __AVR_AT90USB1286__\n#endif\n")
        self.assertEqual(len(v), 1)
        self.assertIn("FL_IS_AVR_ATMEGA", v[0][1])

    def test_two_different_families_same_file(self):
        """Two different sub-family defines → two violations with different suggestions."""
        code = "#ifdef __AVR_ATtiny85__\n#endif\n#ifdef __AVR_ATmega2560__\n#endif\n"
        v = _get_violations(code)
        self.assertEqual(len(v), 2)
        self.assertIn("FL_IS_AVR_ATTINY", v[0][1])
        self.assertIn("FL_IS_AVR_ATMEGA", v[1][1])


# ── True negatives: must NOT detect ─────────────────────────────────


class TestTrueNegatives(unittest.TestCase):
    """None of these should produce any violations."""

    def test_empty_file(self):
        self.assertEqual(_get_violations(""), [])

    def test_no_preprocessor_at_all(self):
        self.assertEqual(_get_violations("int x = 1;\n"), [])

    def test_string_literal(self):
        code = '#ifdef FOO\nconst char* s = "__AVR__";\n#endif\n'
        self.assertEqual(_get_violations(code), [])

    def test_single_line_comment(self):
        self.assertEqual(_get_violations("// #ifdef __AVR__\nint x;\n"), [])

    def test_block_comment_single_line(self):
        self.assertEqual(_get_violations("/* #ifdef __AVR__ */\nint x;\n"), [])

    def test_block_comment_multiline(self):
        code = "/*\n#ifdef __AVR__\nsome text\n*/\nint x;\n"
        self.assertEqual(_get_violations(code), [])

    def test_define_directive(self):
        """#define __AVR__ is defining, not using."""
        code = "#define __AVR__\nint x;\n"
        self.assertEqual(_get_violations(code), [])

    def test_define_with_value(self):
        code = "#define __AVR__ 1\nint x;\n"
        self.assertEqual(_get_violations(code), [])

    def test_undef(self):
        code = "#undef __AVR__\nint x;\n"
        self.assertEqual(_get_violations(code), [])

    def test_include_directive(self):
        code = '#include "__AVR__.h"\nint x;\n'
        self.assertEqual(_get_violations(code), [])

    def test_regular_code_line(self):
        """__AVR__ on a non-preprocessor line → no violation."""
        code = "#ifdef SOMETHING\nint y = __AVR__;\n#endif\n"
        self.assertEqual(_get_violations(code), [])

    def test_variable_name(self):
        code = "#ifdef FOO\nint __AVR__ = 5;\n#endif\n"
        self.assertEqual(_get_violations(code), [])

    def test_function_call(self):
        code = '#ifdef FOO\nprintf("__AVR__");\n#endif\n'
        self.assertEqual(_get_violations(code), [])

    def test_class_name(self):
        code = "#ifdef FOO\nclass __AVR__ {};\n#endif\n"
        self.assertEqual(_get_violations(code), [])

    def test_else_line(self):
        """#else contains no define to flag."""
        code = "#ifdef OTHER\n#else\nint y;\n#endif\n"
        self.assertEqual(_get_violations(code), [])

    def test_endif_line(self):
        code = "#ifdef OTHER\n#endif\n"
        self.assertEqual(_get_violations(code), [])

    def test_pragma(self):
        code = "#pragma __AVR__\nint x;\n"
        self.assertEqual(_get_violations(code), [])

    def test_error_directive(self):
        code = '#error "Not supported on __AVR__"\n'
        self.assertEqual(_get_violations(code), [])

    def test_warning_directive(self):
        code = '#warning "Check __AVR__ support"\n'
        self.assertEqual(_get_violations(code), [])

    def test_define_avr_trailing_comment(self):
        """#ifdef on a DIFFERENT define should not match __AVR__ in comment."""
        code = "#ifdef FOO // something about __AVR__\n#endif\n"
        self.assertEqual(_get_violations(code), [])

    def test_fl_is_avr_not_flagged(self):
        """The modern replacement should never be flagged."""
        code = "#ifdef FL_IS_AVR\n#endif\n"
        self.assertEqual(_get_violations(code), [])

    def test_fl_is_avr_subfamilies_not_flagged(self):
        """None of the FL_IS_* targets should be flagged."""
        for modern in set(NATIVE_TO_MODERN_DEFINES.values()):
            code = f"#ifdef {modern}\n#endif\n"
            with self.subTest(modern=modern):
                self.assertEqual(_get_violations(code), [])

    def test_substring_no_match(self):
        """__AVR__SOMETHING should not match __AVR__ (word boundary)."""
        code = "#ifdef __AVR__SOMETHING\n#endif\n"
        self.assertEqual(_get_violations(code), [])

    def test_prefix_no_match(self):
        """SOMETHING__AVR__ should not match."""
        code = "#ifdef SOMETHING__AVR__\n#endif\n"
        self.assertEqual(_get_violations(code), [])

    def test_only_preprocessor_no_define(self):
        """File with conditionals but no native defines → no violations."""
        code = "#ifdef OTHER\nint x;\n#endif\n"
        self.assertEqual(_get_violations(code), [])

    def test_attiny_in_regular_code(self):
        """Native define on non-preprocessor line is not flagged."""
        code = "#ifdef FOO\nint chip = __AVR_ATtiny85__;\n#endif\n"
        self.assertEqual(_get_violations(code), [])

    def test_atmega_in_string(self):
        code = '#ifdef FOO\nconst char* n = "__AVR_ATmega2560__";\n#endif\n'
        self.assertEqual(_get_violations(code), [])

    def test_digispark_in_comment(self):
        code = "// #ifdef ARDUINO_AVR_DIGISPARK\nint x;\n"
        self.assertEqual(_get_violations(code), [])

    def test_no_false_positive_every_define_in_string(self):
        """Every native define inside a string literal must NOT trigger."""
        for native in NATIVE_TO_MODERN_DEFINES:
            code = f'#ifdef FOO\nconst char* s = "{native}";\n#endif\n'
            with self.subTest(native=native):
                self.assertEqual(_get_violations(code), [])

    def test_no_false_positive_every_define_in_comment(self):
        """Every native define inside a // comment must NOT trigger."""
        for native in NATIVE_TO_MODERN_DEFINES:
            code = f"// #ifdef {native}\nint x;\n"
            with self.subTest(native=native):
                self.assertEqual(_get_violations(code), [])

    def test_no_false_positive_every_define_on_code_line(self):
        """Every native define on a regular code line must NOT trigger."""
        for native in NATIVE_TO_MODERN_DEFINES:
            code = f"#ifdef FOO\nint x = {native};\n#endif\n"
            with self.subTest(native=native):
                self.assertEqual(_get_violations(code), [])


# ── Edge cases ───────────────────────────────────────────────────────


class TestEdgeCases(unittest.TestCase):
    def test_block_comment_open_and_close_same_line_then_ifdef(self):
        """Comment closed on same line, next line has real ifdef."""
        code = "/* comment */ int x;\n#ifdef __AVR__\n#endif\n"
        v = _get_violations(code)
        self.assertEqual(len(v), 1)
        self.assertEqual(v[0][0], 2)

    def test_block_comment_closes_then_ifdef_same_line(self):
        """Block comment closes, then #ifdef on same line — tricky."""
        code = "/* comment */#ifdef __AVR__\n#endif\n"
        v = _get_violations(code)
        # Line 1 has */ so the parser `continue`s past it — no violation.
        self.assertEqual(len(v), 0)

    def test_nested_block_comments_not_supported(self):
        """C doesn't support nested block comments; ensure we don't break."""
        code = "/* outer /* inner */ #ifdef __AVR__ */\nint x;\n"
        v = _get_violations(code)
        self.assertEqual(len(v), 0)

    def test_backslash_continuation(self):
        """Backslash continuation: __AVR__ on continuation line is NOT a directive."""
        code = "#if defined(FOO) \\\n    || defined(__AVR__)\n#endif\n"
        v = _get_violations(code)
        # Line 2 starts with spaces, not #, so it's not matched as a directive.
        self.assertEqual(len(v), 0)

    def test_backslash_continuation_define_on_first_line(self):
        """Backslash continuation with __AVR__ on the #if line itself."""
        code = "#if defined(__AVR__) \\\n    || defined(OTHER)\n#endif\n"
        v = _get_violations(code)
        self.assertEqual(len(v), 1)
        self.assertEqual(v[0][0], 1)

    def test_many_lines_before_violation(self):
        """Violation deep in a file."""
        lines = ["int x;\n"] * 100
        lines.append("#ifdef __AVR__\n")
        lines.append("#endif\n")
        code = "".join(lines)
        v = _get_violations(code)
        self.assertEqual(len(v), 1)
        self.assertEqual(v[0][0], 101)

    def test_multiple_defines_same_line(self):
        """Two identical native defines on one #if line → 1 violation (re.search)."""
        code = "#if defined(__AVR__) && defined(__AVR__)\n#endif\n"
        v = _get_violations(code)
        self.assertEqual(len(v), 1)

    def test_two_different_native_defines_same_line(self):
        """Two different native defines on one #if line → 2 violations."""
        code = "#if defined(__AVR__) || defined(__AVR_ATtiny85__)\n#endif\n"
        v = _get_violations(code)
        self.assertEqual(len(v), 2)

    def test_file_with_only_comments(self):
        code = "// just comments\n/* block comment */\n"
        self.assertEqual(_get_violations(code), [])

    def test_file_with_only_blank_lines(self):
        code = "\n\n\n\n"
        self.assertEqual(_get_violations(code), [])


# ── Fast-scan vs slow-scan consistency ───────────────────────────────


class TestFastSlowConsistency(unittest.TestCase):
    """Verify that fast scan never causes false negatives.

    For every true-positive case, fast scan MUST return True.
    """

    TRUE_POSITIVE_CASES = [
        "#ifdef __AVR__\n#endif\n",
        "#ifndef __AVR__\n#endif\n",
        "#if defined(__AVR__)\n#endif\n",
        "#elif defined(__AVR__)\n#endif\n",
        "    #ifdef __AVR__\n    #endif\n",
        "# ifdef __AVR__\n# endif\n",
        "#ifdef __AVR_ATmega328P__\n#endif\n",
        "#ifdef __AVR_ATtiny85__\n#endif\n",
        "#ifdef __AVR_ATmega4809__\n#endif\n",
        "#ifdef ARDUINO_AVR_DIGISPARK\n#endif\n",
        "#ifdef IS_BEAN\n#endif\n",
    ]

    def test_fast_scan_passes_all_true_positives(self):
        for code in self.TRUE_POSITIVE_CASES:
            with self.subTest(code=code[:40]):
                self.assertTrue(
                    _fast_scan_dominated(code),
                    f"Fast scan rejected a true positive: {code!r}",
                )

    TRUE_NEGATIVE_FAST_SKIP_CASES = [
        "",
        "int x = 1;\n",
        "#ifdef OTHER\nint x;\n#endif\n",
        'const char* s = "__AVR__";\n',
    ]

    def test_fast_scan_rejects_easy_negatives(self):
        for code in self.TRUE_NEGATIVE_FAST_SKIP_CASES:
            with self.subTest(code=code[:40]):
                self.assertFalse(
                    _fast_scan_dominated(code),
                    f"Fast scan accepted a case it should skip: {code!r}",
                )


# ── Mapping integrity ────────────────────────────────────────────────


class TestMappingIntegrity(unittest.TestCase):
    """Verify the NATIVE_TO_MODERN_DEFINES mapping is well-formed."""

    def test_all_modern_defines_start_with_FL_IS(self):
        """All FL_IS_* targets must start with FL_IS_ prefix."""
        for native, modern in NATIVE_TO_MODERN_DEFINES.items():
            with self.subTest(native=native):
                self.assertTrue(
                    modern.startswith("FL_IS_"),
                    f"{native} → {modern} does not start with FL_IS_",
                )

    def test_no_modern_defines_in_native_keys(self):
        """FL_IS_* should never appear as a native key."""
        for native in NATIVE_TO_MODERN_DEFINES:
            self.assertFalse(
                native.startswith("FL_IS_"),
                f"FL_IS_* define '{native}' found in native keys",
            )

    def test_mapping_has_expected_families(self):
        """Verify all expected FL_IS_* families are represented."""
        modern_values = set(NATIVE_TO_MODERN_DEFINES.values())
        expected = {
            # Hardware platforms
            "FL_IS_AVR",
            "FL_IS_AVR_ATMEGA",
            "FL_IS_AVR_ATMEGA_328P",
            "FL_IS_AVR_MEGAAVR",
            "FL_IS_AVR_ATTINY",
            "FL_IS_ESP32",
            "FL_IS_STM32_F1",
            "FL_IS_NRF52",
            "FL_IS_SAMD21",
            "FL_IS_TEENSY_3X",
            # OS detection
            "FL_IS_WIN",
            "FL_IS_WIN_MSVC",
            "FL_IS_WIN_MINGW",
            "FL_IS_APPLE",
            "FL_IS_LINUX",
            "FL_IS_BSD",
            "FL_IS_POSIX",
            "FL_IS_WASM",
            # Compiler detection
            "FL_IS_CLANG",
            "FL_IS_GCC",
        }
        for family in expected:
            with self.subTest(family=family):
                self.assertIn(family, modern_values)

    def test_mapping_count(self):
        """Sanity check: we have a reasonable number of entries."""
        # Hardware + OS + compiler defines
        self.assertGreaterEqual(len(NATIVE_TO_MODERN_DEFINES), 100)


if __name__ == "__main__":
    unittest.main()
