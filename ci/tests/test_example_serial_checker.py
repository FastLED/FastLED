#!/usr/bin/env python3
"""Unit tests for ExampleSerialChecker.

The checker forbids raw `Serial.print*` / `Serial.read*` / `Serial.begin` in
enforced example sketches (currently `examples/AutoResearch/`), while allowing
platform-specific config calls (`setTxBufferSize`, `setTxTimeoutMs`) and any
line carrying a `// ok serial - <reason>` suppression.
"""

import unittest

from ci.lint_cpp.example_serial_checker import ExampleSerialChecker
from ci.util.check_files import FileContent


def _make_content(path: str, lines: list[str]) -> FileContent:
    """Build a FileContent matching what the runner passes to checkers."""
    return FileContent(path=path, lines=lines, content="\n".join(lines))


class TestExampleSerialChecker(unittest.TestCase):
    """Tests for ExampleSerialChecker file-selection and violation logic."""

    def setUp(self) -> None:
        self.checker = ExampleSerialChecker()

    # ------------------------------------------------------------------ #
    # should_process_file                                                #
    # ------------------------------------------------------------------ #

    def test_enforces_autoresearch_ino(self) -> None:
        self.assertTrue(
            self.checker.should_process_file(
                "/proj/examples/AutoResearch/AutoResearch.ino"
            )
        )

    def test_enforces_autoresearch_helper_cpp(self) -> None:
        self.assertTrue(
            self.checker.should_process_file(
                "/proj/examples/AutoResearch/AutoResearchRemote.cpp"
            )
        )

    def test_skips_other_examples(self) -> None:
        # Tutorial examples are not yet migrated; they should be skipped.
        self.assertFalse(
            self.checker.should_process_file("/proj/examples/Blink/Blink.ino")
        )

    def test_skips_non_source_extensions(self) -> None:
        self.assertFalse(
            self.checker.should_process_file("/proj/examples/AutoResearch/README.md")
        )

    def test_skips_src_tree(self) -> None:
        self.assertFalse(
            self.checker.should_process_file("/proj/src/fl/stl/cstdio.cpp")
        )

    # ------------------------------------------------------------------ #
    # check_file_content — forbidden methods                              #
    # ------------------------------------------------------------------ #

    def test_flags_serial_println(self) -> None:
        content = _make_content(
            "/proj/examples/AutoResearch/AutoResearch.ino",
            ["void setup() {", '    Serial.println("hi");', "}"],
        )
        self.checker.check_file_content(content)
        self.assertIn(content.path, self.checker.violations)
        line_num, msg = self.checker.violations[content.path][0]
        self.assertEqual(line_num, 2)
        self.assertIn("println", msg)
        self.assertIn("fl::println", msg)

    def test_flags_serial_begin(self) -> None:
        content = _make_content(
            "/proj/examples/AutoResearch/AutoResearch.ino",
            ["    Serial.begin(115200);"],
        )
        self.checker.check_file_content(content)
        self.assertIn(content.path, self.checker.violations)
        self.assertIn("fl::serial_begin", self.checker.violations[content.path][0][1])

    def test_flags_serial_printf(self) -> None:
        content = _make_content(
            "/proj/examples/AutoResearch/AutoResearch.ino",
            ['    Serial.printf("x=%d", x);'],
        )
        self.checker.check_file_content(content)
        self.assertIn(content.path, self.checker.violations)

    def test_flags_serial_read_and_available(self) -> None:
        content = _make_content(
            "/proj/examples/AutoResearch/AutoResearch.ino",
            [
                "    while (Serial.available()) {",
                "        int c = Serial.read();",
                "    }",
            ],
        )
        self.checker.check_file_content(content)
        self.assertIn(content.path, self.checker.violations)
        self.assertEqual(len(self.checker.violations[content.path]), 2)

    # ------------------------------------------------------------------ #
    # check_file_content — allowed methods                                #
    # ------------------------------------------------------------------ #

    def test_allows_set_tx_buffer_size(self) -> None:
        content = _make_content(
            "/proj/examples/AutoResearch/AutoResearch.ino",
            ["    Serial.setTxBufferSize(4096);"],
        )
        self.checker.check_file_content(content)
        self.assertNotIn(content.path, self.checker.violations)

    def test_allows_set_tx_timeout_ms(self) -> None:
        content = _make_content(
            "/proj/examples/AutoResearch/AutoResearch.ino",
            ["    Serial.setTxTimeoutMs(0);"],
        )
        self.checker.check_file_content(content)
        self.assertNotIn(content.path, self.checker.violations)

    def test_allows_bool_serial_check(self) -> None:
        # `if (!Serial)` and `while (!Serial)` don't match the `Serial.<m>(`
        # pattern at all, so they're implicitly allowed.
        content = _make_content(
            "/proj/examples/AutoResearch/AutoResearch.ino",
            ["    while (!Serial && millis() < 120000);"],
        )
        self.checker.check_file_content(content)
        self.assertNotIn(content.path, self.checker.violations)

    # ------------------------------------------------------------------ #
    # check_file_content — suppression                                    #
    # ------------------------------------------------------------------ #

    def test_suppression_marker_silences_violation(self) -> None:
        content = _make_content(
            "/proj/examples/AutoResearch/AutoResearch.ino",
            ['    Serial.println("hi");  // ok serial - intentional bypass'],
        )
        self.checker.check_file_content(content)
        self.assertNotIn(content.path, self.checker.violations)

    def test_suppression_only_on_same_line(self) -> None:
        # Suppression comment on a separate line does NOT silence the next line.
        content = _make_content(
            "/proj/examples/AutoResearch/AutoResearch.ino",
            [
                "    // ok serial - not on same line",
                '    Serial.println("hi");',
            ],
        )
        self.checker.check_file_content(content)
        self.assertIn(content.path, self.checker.violations)

    # ------------------------------------------------------------------ #
    # check_file_content — comments                                       #
    # ------------------------------------------------------------------ #

    def test_skips_calls_in_line_comments(self) -> None:
        content = _make_content(
            "/proj/examples/AutoResearch/AutoResearch.ino",
            ['    // Serial.println("this is just documentation")'],
        )
        self.checker.check_file_content(content)
        self.assertNotIn(content.path, self.checker.violations)

    def test_skips_calls_in_block_comments(self) -> None:
        content = _make_content(
            "/proj/examples/AutoResearch/AutoResearch.ino",
            [
                "    /*",
                '    Serial.println("docs example");',
                "    */",
            ],
        )
        self.checker.check_file_content(content)
        self.assertNotIn(content.path, self.checker.violations)


if __name__ == "__main__":
    unittest.main()
