# pyright: reportUnknownMemberType=false
import re
from pathlib import Path

from ..base_checker import BaseChecker
from ..result import CheckResult


class NamespaceIncludeChecker(BaseChecker):
    """Checks that #include directives come before namespace declarations."""

    # Regex patterns
    NAMESPACE_PATTERN = re.compile(
        r"""
        ^\s*                    # Start of line with optional whitespace
        (                       # Capture group for namespace patterns
            namespace\s+\w+     # namespace followed by identifier
            |                   # OR
            namespace\s*\{      # namespace followed by optional whitespace and {
        )
    """,
        re.VERBOSE,
    )

    INCLUDE_PATTERN = re.compile(
        r"""
        ^\s*                    # Start of line with optional whitespace
        \#\s*                   # Hash with optional whitespace
        include\s*              # include with optional whitespace
        [<"]                    # Opening bracket or quote
        .*                      # Anything in between
        [>"]                    # Closing bracket or quote
    """,
        re.VERBOSE,
    )

    ALLOW_DIRECTIVE_PATTERN = re.compile(r"//\s*allow-include-after-namespace")

    def name(self) -> str:
        return "namespace-include-order"

    def should_check(self, file_path: Path) -> bool:
        # Skip directories with third-party code
        skip_patterns = [
            ".venv",
            "node_modules",
            "build",
            ".build",
            "third_party",
            "ziglang",
            "greenlet",
            ".git",
        ]
        for pattern in skip_patterns:
            if pattern in str(file_path):
                return False

        # Skip third-party test frameworks
        if "doctest.h" in str(file_path):
            return False

        return file_path.suffix in {".cpp", ".h", ".hpp", ".cc", ".ino"}

    def check_file(self, file_path: Path, content: str) -> list[CheckResult]:
        results: list[CheckResult] = []
        lines = content.split("\n")

        namespace_started = False
        namespace_line = None

        for line_num, line in enumerate(lines, 1):
            # Check if we're entering a namespace
            if self.NAMESPACE_PATTERN.match(line):
                namespace_started = True
                namespace_line = line_num
                continue

            # Check for includes after namespace started
            if namespace_started and self.INCLUDE_PATTERN.match(line):
                # Check if this include has an allow directive
                if self.ALLOW_DIRECTIVE_PATTERN.search(line):
                    continue  # Skip this violation

                results.append(
                    CheckResult(
                        checker_name=self.name(),
                        file_path=str(file_path),
                        line_number=line_num,
                        severity="ERROR",
                        message=f"#include directive found after namespace declaration (line {namespace_line})",
                        suggestion="Move all #include directives before namespace declarations",
                    )
                )

        return results
