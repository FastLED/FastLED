#!/usr/bin/env python3
"""Unit tests for PublicSettingsPatternChecker.

Tests cover:
  - File filtering (should_process_file)
  - Positive cases: violations that SHOULD be flagged
  - Negative cases: valid patterns that MUST NOT be flagged
  - Grandfathered allowlist (pre-existing offenders)
  - Carve-outs (per-object setters, function-pointer installers, sub-namespaces)
  - Comment exemptions (single-line, multi-line)
  - Suppression tokens (// nolint, // ok public_settings)
  - Cross-file wrapper check (functions referenced in a fake FastLED.h)
  - Edge cases

Helper convention:
  _violations(code, fastled_h="") → list[tuple[int, str]]
  Returns the list of (line_number, message) pairs the checker records for `code`.
"""

import unittest

from ci.lint_cpp.public_settings_pattern_checker import (
    GRANDFATHERED_NAMES,
    PublicSettingsPatternChecker,
    _is_per_object_setter,
    _is_wrapped_in_fastled_h,
)
from ci.util.check_files import FileContent


# ---------------------------------------------------------------------------
# Helpers
# ---------------------------------------------------------------------------

_FL_PATH = "src/fl/config/my_module.h"
_THIRD_PARTY_PATH = "src/fl/third_party/some_lib.h"
_NON_FL_PATH = "src/platforms/esp32/driver.h"
_TESTS_PATH = "tests/fl/test_module.cpp"


def _make(
    code: str,
    path: str = _FL_PATH,
    fastled_h: str = "",
) -> tuple[PublicSettingsPatternChecker, FileContent]:
    """Build a checker with an injected (fake) FastLED.h and a FileContent."""
    checker = PublicSettingsPatternChecker.__new__(PublicSettingsPatternChecker)
    checker.violations = {}
    checker._fastled_h_content = fastled_h
    lines = code.splitlines()
    fc = FileContent(path=path, content=code, lines=lines)
    return checker, fc


def _violations(
    code: str,
    path: str = _FL_PATH,
    fastled_h: str = "",
) -> list[tuple[int, str]]:
    """Run the checker on *code* and return violations for *path*."""
    checker, fc = _make(code, path, fastled_h)
    if not checker.should_process_file(path):
        return []
    checker.check_file_content(fc)
    return checker.violations.get(path, [])


# ---------------------------------------------------------------------------
# File filtering
# ---------------------------------------------------------------------------


class TestShouldProcessFile(unittest.TestCase):
    def _checker(self) -> PublicSettingsPatternChecker:
        c = PublicSettingsPatternChecker.__new__(PublicSettingsPatternChecker)
        c.violations = {}
        c._fastled_h_content = ""
        return c

    def test_fl_header_accepted(self) -> None:
        c = self._checker()
        self.assertTrue(c.should_process_file("src/fl/config/module.h"))

    def test_fl_header_absolute_accepted(self) -> None:
        c = self._checker()
        self.assertTrue(c.should_process_file("C:/dev/fastled/src/fl/gfx/rgbw.h"))

    def test_third_party_rejected(self) -> None:
        c = self._checker()
        self.assertFalse(c.should_process_file("src/fl/third_party/some_lib.h"))

    def test_non_fl_src_rejected(self) -> None:
        c = self._checker()
        self.assertFalse(c.should_process_file("src/platforms/esp32/driver.h"))

    def test_tests_directory_rejected(self) -> None:
        c = self._checker()
        self.assertFalse(c.should_process_file("tests/fl/test_module.cpp"))

    def test_cpp_file_rejected(self) -> None:
        c = self._checker()
        self.assertFalse(c.should_process_file("src/fl/module.cpp"))

    def test_cpp_hpp_rejected(self) -> None:
        c = self._checker()
        self.assertFalse(c.should_process_file("src/fl/module.cpp.hpp"))

    def test_hpp_file_rejected(self) -> None:
        # Only .h files are checked — .hpp is excluded
        c = self._checker()
        self.assertFalse(c.should_process_file("src/fl/module.hpp"))

    def test_windows_backslash_paths_accepted(self) -> None:
        c = self._checker()
        self.assertTrue(c.should_process_file("C:\\dev\\fastled\\src\\fl\\module.h"))


# ---------------------------------------------------------------------------
# Positive cases — SHOULD flag
# ---------------------------------------------------------------------------


class TestShouldFlag(unittest.TestCase):
    """Functions that must be reported as violations."""

    def test_simple_global_setter_no_wrapper(self) -> None:
        code = "namespace fl {\nvoid set_my_option(int val);\n}"
        v = _violations(code)
        self.assertEqual(len(v), 1)
        self.assertIn("set_my_option", v[0][1])
        self.assertEqual(v[0][0], 2)  # line 2

    def test_enable_global_no_wrapper(self) -> None:
        code = "namespace fl {\nbool enable_some_feature(int level);\n}"
        v = _violations(code)
        self.assertEqual(len(v), 1)
        self.assertIn("enable_some_feature", v[0][1])

    def test_disable_global_no_wrapper(self) -> None:
        code = "namespace fl {\nvoid disable_some_feature();\n}"
        v = _violations(code)
        self.assertEqual(len(v), 1)
        self.assertIn("disable_some_feature", v[0][1])

    def test_use_global_no_wrapper(self) -> None:
        code = "namespace fl {\nvoid use_new_algorithm(bool on);\n}"
        v = _violations(code)
        self.assertEqual(len(v), 1)
        self.assertIn("use_new_algorithm", v[0][1])

    def test_violation_message_suggests_wrapper_name(self) -> None:
        code = "namespace fl {\nvoid set_my_option(int val);\n}"
        v = _violations(code)
        # Message must name both the free function and the camelCase wrapper
        self.assertIn("setMyOption", v[0][1])
        self.assertIn("CFastLED", v[0][1])

    def test_multiple_violations_in_one_file(self) -> None:
        code = (
            "namespace fl {\nvoid set_option_a(int x);\nbool enable_mode_b(bool y);\n}"
        )
        v = _violations(code)
        self.assertEqual(len(v), 2)

    def test_fl_noexcept_decorated_setter_flagged(self) -> None:
        """FL_NOEXCEPT attribute must not suppress the violation."""
        code = "namespace fl {\nvoid set_bright(int v) FL_NOEXCEPT;\n}"
        v = _violations(code)
        self.assertEqual(len(v), 1)

    def test_setter_with_const_ref_param_flagged(self) -> None:
        """const ref first param is read-only — not per-object; should flag."""
        code = "namespace fl {\nvoid set_model(const PowerModel& m);\n}"
        v = _violations(code)
        self.assertEqual(len(v), 1)

    def test_no_params_setter_flagged(self) -> None:
        """Zero-param setter that disables something — global state."""
        code = "namespace fl {\nvoid disable_feature();\n}"
        v = _violations(code)
        self.assertEqual(len(v), 1)


# ---------------------------------------------------------------------------
# Negative cases — must NOT flag
# ---------------------------------------------------------------------------


class TestShouldNotFlag(unittest.TestCase):
    """Patterns that must not produce any violations."""

    def test_function_not_matching_pattern_skipped(self) -> None:
        code = "namespace fl {\nvoid do_something(int v);\n}"
        self.assertEqual(len(_violations(code)), 0)

    def test_class_member_setter_skipped(self) -> None:
        """Methods on a class/struct are per-object, not global."""
        code = "namespace fl {\nstruct Foo {\n    void set_color(int c);\n};\n}"
        self.assertEqual(len(_violations(code)), 0)

    def test_getter_not_flagged(self) -> None:
        code = "namespace fl {\nint get_brightness();\n}"
        self.assertEqual(len(_violations(code)), 0)

    def test_sub_namespace_depth_not_flagged(self) -> None:
        """Functions inside fl::detail:: must be ignored."""
        code = "namespace fl {\nnamespace detail {\nvoid set_internal(int x);\n}\n}"
        self.assertEqual(len(_violations(code)), 0)

    def test_anonymous_namespace_not_flagged(self) -> None:
        """Functions inside anonymous namespaces are file-local."""
        code = "namespace fl {\nnamespace {\nvoid set_hidden(int x);\n}\n}"
        self.assertEqual(len(_violations(code)), 0)

    def test_no_namespace_context_not_flagged(self) -> None:
        """Free functions not inside namespace fl are skipped."""
        code = "void set_something(int v);\n"
        self.assertEqual(len(_violations(code)), 0)

    def test_wrapper_in_fastled_h_suppresses_violation(self) -> None:
        """Function referenced in FastLED.h must not be flagged."""
        fake_fastled_h = (
            "class CFastLED {\n"
            "    inline void setSomething(int v) { fl::set_something(v); }\n"
            "};\n"
        )
        code = "namespace fl {\nvoid set_something(int val);\n}"
        self.assertEqual(len(_violations(code, fastled_h=fake_fastled_h)), 0)

    def test_enum_body_does_not_confuse_scope(self) -> None:
        """An enum class inside namespace fl must not break scope tracking."""
        code = (
            "namespace fl {\n"
            "enum class Mode { kA, kB };\n"
            "void set_something(int v);\n"  # still in namespace fl, should flag
            "}"
        )
        v = _violations(code)
        self.assertEqual(len(v), 1)
        self.assertIn("set_something", v[0][1])

    def test_plain_enum_body_does_not_confuse_scope(self) -> None:
        """A plain `enum { }` must not pop the fl namespace prematurely."""
        code = (
            "namespace fl {\n"
            "enum { kFoo = 0, kBar = 1 };\n"
            "void set_brightness(int v);\n"  # still in fl, should flag
            "}"
        )
        v = _violations(code)
        self.assertEqual(len(v), 1)
        self.assertIn("set_brightness", v[0][1])

    def test_function_body_inline_does_not_confuse_scope(self) -> None:
        """An inline function definition must not interfere with ns tracking."""
        code = (
            "namespace fl {\n"
            "inline void helper() { do_work(); }\n"
            "void set_brightness(int v);\n"  # still in fl
            "}"
        )
        v = _violations(code)
        self.assertEqual(len(v), 1)

    def test_nested_sub_namespace_with_return_to_fl(self) -> None:
        """After a sub-namespace closes, we should be back in fl scope."""
        code = (
            "namespace fl {\n"
            "namespace isr {\n"
            "void set_isr_mode(int x);\n"  # in fl::isr — NOT flagged
            "}\n"
            "void set_global_thing(int y);\n"  # back in fl — FLAGGED
            "}"
        )
        v = _violations(code)
        self.assertEqual(len(v), 1)
        self.assertIn("set_global_thing", v[0][1])

    def test_empty_file(self) -> None:
        self.assertEqual(len(_violations("")), 0)

    def test_file_without_setter_keywords(self) -> None:
        code = "namespace fl {\nvoid do_work();\n}"
        self.assertEqual(len(_violations(code)), 0)


# ---------------------------------------------------------------------------
# Grandfathered allowlist
# ---------------------------------------------------------------------------


class TestGrandfatheredAllowlist(unittest.TestCase):
    def test_set_rgbw_colorimetric_profile_not_flagged(self) -> None:
        code = (
            "namespace fl {\n"
            "void set_rgbw_colorimetric_profile(const DiodeProfile* p) FL_NOEXCEPT;\n"
            "}"
        )
        self.assertEqual(len(_violations(code)), 0)

    def test_set_input_gamut_not_flagged(self) -> None:
        code = (
            "namespace fl {\n"
            "void set_input_gamut(DiodeProfile* p, InputGamut g) FL_NOEXCEPT;\n"
            "}"
        )
        self.assertEqual(len(_violations(code)), 0)

    def test_enable_rgbw_colorimetric_lut_not_flagged(self) -> None:
        code = (
            "namespace fl {\n"
            "bool enable_rgbw_colorimetric_lut(int grid_n) FL_NOEXCEPT;\n"
            "}"
        )
        self.assertEqual(len(_violations(code)), 0)

    def test_disable_rgbw_colorimetric_lut_not_flagged(self) -> None:
        code = "namespace fl {\nvoid disable_rgbw_colorimetric_lut() FL_NOEXCEPT;\n}"
        self.assertEqual(len(_violations(code)), 0)

    def test_set_rgbww_colorimetric_profile_not_flagged(self) -> None:
        code = (
            "namespace fl {\n"
            "void set_rgbww_colorimetric_profile(const RgbcctProfile* p) FL_NOEXCEPT;\n"
            "}"
        )
        self.assertEqual(len(_violations(code)), 0)

    def test_allowlist_frozenset_contains_expected_names(self) -> None:
        """Sanity-check the allowlist has the expected names."""
        self.assertIn("set_rgbw_colorimetric_profile", GRANDFATHERED_NAMES)
        self.assertIn("set_input_gamut", GRANDFATHERED_NAMES)
        self.assertIn("enable_rgbw_colorimetric_lut", GRANDFATHERED_NAMES)
        self.assertIn("disable_rgbw_colorimetric_lut", GRANDFATHERED_NAMES)
        self.assertIn("set_rgbww_colorimetric_profile", GRANDFATHERED_NAMES)


# ---------------------------------------------------------------------------
# Per-object setter carve-outs
# ---------------------------------------------------------------------------


class TestPerObjectSetterCarveout(unittest.TestCase):
    """_is_per_object_setter() and the checker integration."""

    def test_non_const_ptr_first_param_is_per_object(self) -> None:
        # Non-const pointer to user type → per-object, not flagged
        line = "void set_input_gamut(DiodeProfile* obj, InputGamut g);"
        self.assertTrue(_is_per_object_setter(line))

    def test_const_ref_first_param_is_not_per_object(self) -> None:
        # const ref → read-only → global setter candidate
        line = "void set_power_model(const PowerModel& m);"
        self.assertFalse(_is_per_object_setter(line))

    def test_fundamental_int_first_param_not_per_object(self) -> None:
        line = "bool enable_lut(int grid_n);"
        self.assertFalse(_is_per_object_setter(line))

    def test_no_params_not_per_object(self) -> None:
        line = "void disable_lut();"
        self.assertFalse(_is_per_object_setter(line))

    def test_non_const_ref_user_type_is_per_object(self) -> None:
        line = "void set_color(CRGB& out, int r, int g, int b);"
        self.assertTrue(_is_per_object_setter(line))

    def test_per_object_not_flagged_by_checker(self) -> None:
        """A non-const ptr first param must NOT produce a violation."""
        code = "namespace fl {\nvoid set_diode(DiodeProfile* p, int v);\n}"
        self.assertEqual(len(_violations(code)), 0)


# ---------------------------------------------------------------------------
# Function-pointer setter carve-outs
# ---------------------------------------------------------------------------


class TestFunctionPointerCarveout(unittest.TestCase):
    def test_set_rgb_2_rgbw_function_not_flagged(self) -> None:
        code = (
            "namespace fl {\n"
            "void set_rgb_2_rgbw_function(rgb_2_rgbw_function func) FL_NOEXCEPT;\n"
            "}"
        )
        self.assertEqual(len(_violations(code)), 0)

    def test_set_rgb_2_rgbww_function_not_flagged(self) -> None:
        code = (
            "namespace fl {\n"
            "void set_rgb_2_rgbww_function(rgb_2_rgbww_function func) FL_NOEXCEPT;\n"
            "}"
        )
        self.assertEqual(len(_violations(code)), 0)


# ---------------------------------------------------------------------------
# Comment exemptions
# ---------------------------------------------------------------------------


class TestCommentExemptions(unittest.TestCase):
    def test_single_line_comment_ignored(self) -> None:
        code = "namespace fl {\n// void set_my_option(int val);\n}"
        self.assertEqual(len(_violations(code)), 0)

    def test_multiline_comment_block_ignored(self) -> None:
        code = "namespace fl {\n/*\n * void set_my_option(int val);\n */\n}"
        self.assertEqual(len(_violations(code)), 0)

    def test_inline_comment_does_not_suppress(self) -> None:
        """The setter is in code; comment appended after — still flagged."""
        code = "namespace fl {\nvoid set_my_option(int val); // some note\n}"
        self.assertEqual(len(_violations(code)), 1)


# ---------------------------------------------------------------------------
# Suppression tokens
# ---------------------------------------------------------------------------


class TestSuppressionTokens(unittest.TestCase):
    def test_nolint_suppresses(self) -> None:
        code = "namespace fl {\nvoid set_my_option(int val); // nolint\n}"
        self.assertEqual(len(_violations(code)), 0)

    def test_ok_public_settings_suppresses(self) -> None:
        code = "namespace fl {\nvoid set_my_option(int val); // ok public_settings\n}"
        self.assertEqual(len(_violations(code)), 0)

    def test_suppression_case_insensitive(self) -> None:
        code = "namespace fl {\nvoid set_my_option(int val); // NOLINT\n}"
        self.assertEqual(len(_violations(code)), 0)

    def test_suppression_does_not_bleed_to_next_line(self) -> None:
        code = (
            "namespace fl {\n"
            "void set_option_a(int v); // nolint\n"
            "void set_option_b(int v);\n"
            "}"
        )
        v = _violations(code)
        self.assertEqual(len(v), 1)
        self.assertIn("set_option_b", v[0][1])


# ---------------------------------------------------------------------------
# Cross-file wrapper check (_is_wrapped_in_fastled_h)
# ---------------------------------------------------------------------------


class TestCrossFileWrapperCheck(unittest.TestCase):
    def test_function_in_fastled_h_body_passes(self) -> None:
        fastled_h = (
            "class CFastLED {\n"
            "    inline void setThing(int v) { fl::set_thing(v); }\n"
            "};\n"
        )
        self.assertTrue(_is_wrapped_in_fastled_h("set_thing", fastled_h))

    def test_function_only_in_include_comment_not_wrapped(self) -> None:
        """#include lines must not count as wrappers."""
        fastled_h = '#include "fl/thing.h"  // set_thing declared here\n'
        self.assertFalse(_is_wrapped_in_fastled_h("set_thing", fastled_h))

    def test_function_name_in_comment_not_wrapped(self) -> None:
        fastled_h = "// call set_thing to configure\n"
        self.assertFalse(_is_wrapped_in_fastled_h("set_thing", fastled_h))

    def test_empty_fastled_h_not_wrapped(self) -> None:
        self.assertFalse(_is_wrapped_in_fastled_h("set_thing", ""))

    def test_partial_name_not_matched(self) -> None:
        fastled_h = "fl::set_thingamajig(v);\n"
        self.assertFalse(_is_wrapped_in_fastled_h("set_thing", fastled_h))


# ---------------------------------------------------------------------------
# Edge cases
# ---------------------------------------------------------------------------


class TestEdgeCases(unittest.TestCase):
    def test_multiple_namespaces_on_one_line(self) -> None:
        """namespace fl { namespace detail { on one line is a sub-namespace."""
        code = "namespace fl { namespace detail {\nvoid set_internal(int x);\n}}\n"
        # set_internal is in fl::detail — must NOT be flagged
        self.assertEqual(len(_violations(code)), 0)

    def test_forward_struct_decl_not_class_body(self) -> None:
        """struct Foo; (forward decl) must not push a class scope."""
        code = "namespace fl {\nstruct PowerModel;\nvoid set_model(int v);\n}"
        v = _violations(code)
        self.assertEqual(len(v), 1)

    def test_class_with_inline_body_resets_after_close(self) -> None:
        """After a class body closes we should be back at fl scope."""
        code = (
            "namespace fl {\n"
            "struct Cfg {\n"
            "    void set_pin(int p);\n"  # inside class — NOT flagged
            "};\n"
            "void set_global_setting(int v);\n"  # back in fl — FLAGGED
            "}"
        )
        v = _violations(code)
        self.assertEqual(len(v), 1)
        self.assertIn("set_global_setting", v[0][1])

    def test_line_number_in_violation(self) -> None:
        code = "// header\nnamespace fl {\n\nvoid set_my_thing(int v);\n}"
        v = _violations(code)
        self.assertEqual(len(v), 1)
        self.assertEqual(v[0][0], 4)  # declaration is on line 4

    def test_real_rgbww_header_shape(self) -> None:
        """Simulate rgbww.h: forward-decl sub-ns then fl:: free functions."""
        code = (
            "namespace fl {\n"
            "namespace colorimetric_detail {\n"
            "struct RgbcctProfile;\n"
            "}\n"
            "enum class RGBWW_MODE : int { kA, kB };\n"
            "enum { kDefaultCct = 2700 };\n"
            "struct Rgbww { int warm; };\n"
            "void set_rgb_2_rgbww_function(rgb_2_rgbww_function func);\n"  # FP setter
            "void set_rgbww_colorimetric_profile(const RgbcctProfile* p);\n"  # allowlist
            "}"
        )
        # Both should be exempted: one via FUNCTION_POINTER_SETTERS, one via allowlist
        v = _violations(code)
        self.assertEqual(len(v), 0)

    def test_real_rgbw_header_shape(self) -> None:
        """Simulate rgbw.h: grandfathered setters inside namespace fl."""
        code = (
            "namespace fl {\n"
            "void set_rgbw_colorimetric_profile(const DiodeProfile* p) FL_NOEXCEPT;\n"
            "bool enable_rgbw_colorimetric_lut(int grid_n) FL_NOEXCEPT;\n"
            "void disable_rgbw_colorimetric_lut() FL_NOEXCEPT;\n"
            "void set_rgb_2_rgbw_function(rgb_2_rgbw_function func) FL_NOEXCEPT;\n"
            "}"
        )
        v = _violations(code)
        self.assertEqual(len(v), 0)


if __name__ == "__main__":
    unittest.main()
