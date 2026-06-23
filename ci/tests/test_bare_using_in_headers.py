"""Tests for BareUsingChecker — bare using declarations in headers.

Validates that:
1. No bare 'using X::Y;' or 'using namespace X;' at file/namespace scope
   exists in src/fl/ headers (unity build safety).
2. The checker correctly flags bad patterns and allows good patterns.
"""

import unittest

from ci.lint_cpp.bare_using_checker import BareUsingChecker
from ci.util.check_files import FileContent


def _check(code: str, path: str = "/project/src/fl/test.h") -> list[tuple[int, str]]:
    """Run checker on inline code string and return violations.

    Default path uses a fake absolute path containing /src/fl/ to pass
    the should_process_file filter.
    """
    checker = BareUsingChecker()
    fc = FileContent(path=path, content=code, lines=code.splitlines())
    if not checker.should_process_file(path):
        return []
    checker.check_file_content(fc)
    return checker.violations.get(path, [])


class TestBareUsingCheckerLogic(unittest.TestCase):
    """Unit tests for the checker logic using inline code strings."""

    def test_flags_using_declaration_at_file_scope(self):
        code = "using fl::ByteStreamPtr;\n"
        violations = _check(code)
        self.assertEqual(len(violations), 1)

    def test_flags_using_namespace_at_file_scope(self):
        code = "using namespace std;\n"
        violations = _check(code)
        self.assertEqual(len(violations), 1)

    def test_flags_using_inside_named_namespace(self):
        code = "namespace fl {\nusing platforms::condition_variable;\n}\n"
        violations = _check(code)
        self.assertEqual(len(violations), 1)

    def test_allows_using_inside_class_body(self):
        code = "class Foo {\n  using Base::member;\n};\n"
        violations = _check(code)
        self.assertEqual(len(violations), 0)

    def test_allows_using_inside_function_body(self):
        code = "void foo() {\n  using fl::swap;\n  swap(a, b);\n}\n"
        violations = _check(code)
        self.assertEqual(len(violations), 0)

    def test_allows_using_inside_anonymous_namespace(self):
        code = "namespace {\nusing fl::third_party::hexwave::HexWave;\n}\n"
        violations = _check(code)
        self.assertEqual(len(violations), 0)

    def test_allows_type_alias(self):
        code = "using GammaKey = fl::ufixed_point<4, 12>;\n"
        violations = _check(code)
        self.assertEqual(len(violations), 0)

    def test_allows_type_alias_in_namespace(self):
        code = "namespace fl {\nusing mutex = platforms::mutex;\n}\n"
        violations = _check(code)
        self.assertEqual(len(violations), 0)

    def test_suppression_ok_bare_using(self):
        code = "using fl::fract8;  // ok bare using\n"
        violations = _check(code)
        self.assertEqual(len(violations), 0)

    def test_suppression_okay_bare_using(self):
        code = "using fl::fract8;  // okay bare using\n"
        violations = _check(code)
        self.assertEqual(len(violations), 0)

    def test_skips_non_fl_files(self):
        code = "using fl::ByteStreamPtr;\n"
        violations = _check(code, path="/project/src/platforms/wasm/ui.cpp.hpp")
        self.assertEqual(len(violations), 0)

    def test_skips_cpp_files(self):
        """Only headers are checked, not .cpp files."""
        code = "using fl::ByteStreamPtr;\n"
        violations = _check(code, path="/project/src/fl/test.cpp")
        self.assertEqual(len(violations), 0)

    def test_nested_namespace_using_flagged(self):
        code = "namespace fl {\nnamespace simd {\nusing platforms::load_u8_16;\n}\n}\n"
        violations = _check(code)
        self.assertEqual(len(violations), 1)

    def test_block_comment_does_not_swallow_code(self):
        """Ensure /* inside // comments doesn't break block comment tracking."""
        code = (
            "// implementations in viz/*.cpp.hpp\n"
            "namespace fl {\n"
            "using platforms::condition_variable;\n"
            "}\n"
        )
        violations = _check(code)
        self.assertEqual(len(violations), 1)

    def test_struct_body_is_local_scope(self):
        code = "namespace fl {\nstruct Foo {\n  using Base::x;\n};\n}\n"
        violations = _check(code)
        self.assertEqual(len(violations), 0)

    def test_allows_constructor_inheritance(self):
        """using vec2<T>::vec2; inside class is not flagged."""
        code = (
            "template <typename T> struct pair_xy : public vec2<T> {\n"
            "    using value_type = T;\n"
            "    using vec2<T>::vec2;\n"
            "};\n"
        )
        violations = _check(code)
        self.assertEqual(len(violations), 0)


class TestBareUsingInSourceFiles(unittest.TestCase):
    """Integration test: scan actual src/fl/ files for violations."""

    def test_no_bare_using_violations_in_src_fl(self):
        """All src/fl/ header files must have zero bare using violations."""
        import os

        from ci.util.check_files import (
            MultiCheckerFileProcessor,
            collect_files_to_check,
        )
        from ci.util.paths import PROJECT_ROOT

        fl_dir = str(PROJECT_ROOT / "src" / "fl")
        files = collect_files_to_check([fl_dir])

        checker = BareUsingChecker()
        processor = MultiCheckerFileProcessor()
        processor.process_files_with_checkers(files, [checker])

        if checker.violations:
            msg = "Found bare using declarations in src/fl/ headers:\n"
            for file_path, violation_list in sorted(checker.violations.items()):
                rel = os.path.relpath(file_path, str(PROJECT_ROOT))
                msg += f"  {rel}:\n"
                for line_num, content in violation_list:
                    msg += f"    Line {line_num}: {content}\n"
            msg += (
                "\nFix by:\n"
                "  - Removing unnecessary using declarations\n"
                "  - Using namespace aliases (namespace hw = third_party::hexwave;)\n"
                "  - Adding '// ok bare using' suppression for intentional API design\n"
            )
            self.fail(msg)


if __name__ == "__main__":
    unittest.main()
