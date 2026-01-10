# pyright: reportUnknownMemberType=false
import re
from pathlib import Path

from ..base_checker import BaseChecker
from ..result import CheckResult


class StdNamespaceChecker(BaseChecker):
    """Checks that code uses fl:: instead of std::."""

    def name(self) -> str:
        return "no-std-namespace"

    def should_check(self, file_path: Path) -> bool:
        if file_path.suffix not in {".cpp", ".h", ".hpp", ".cc"}:
            return False

        path_str = str(file_path).replace("\\", "/")
        path_parts = path_str.split("/")

        # Skip test files
        if "test" in path_str.lower():
            return False

        # Skip third_party directories
        if "third_party" in path_parts:
            return False

        # Skip platforms directory (platforms can use std::)
        if "platforms" in path_parts:
            return False

        return True

    def check_file(self, file_path: Path, content: str) -> list[CheckResult]:
        results: list[CheckResult] = []
        lines = content.split("\n")
        in_multiline_comment = False

        for line_num, line in enumerate(lines, 1):
            # Track multi-line comment state
            if "/*" in line:
                in_multiline_comment = True
            if "*/" in line:
                in_multiline_comment = False
                continue  # Skip the line with closing */

            # Skip if we're inside a multi-line comment
            if in_multiline_comment:
                continue

            # Skip single-line comment lines
            if re.match(r"^\s*//", line):
                continue

            # Remove single-line comment portion before checking
            code_part = line.split("//")[0]

            # Check for std:: usage in code portion only
            if "std::" in code_part:
                # Allow if there's a escape hatch comment
                # Support both new format and legacy format for backward compatibility
                if (
                    "// ok std" in line
                    or "// OK std" in line
                    or "// okay std namespace" in line  # Legacy format from old test
                ):
                    continue

                results.append(
                    CheckResult(
                        checker_name=self.name(),
                        file_path=str(file_path),
                        line_number=line_num,
                        severity="ERROR",
                        message="Use fl:: namespace instead of std::",
                        suggestion="Use 'fl::' instead of 'std::' or add '// okay std namespace' comment if std:: is required",
                    )
                )

        return results
