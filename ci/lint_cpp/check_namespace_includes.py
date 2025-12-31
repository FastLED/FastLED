# pyright: reportUnknownMemberType=false, reportMissingParameterType=false
"""Test for checking namespace includes in C++ headers."""

#!/usr/bin/env python3
"""
Script to check for includes after namespace declarations in C++ files.
This is used in CI/CD to prevent bad code patterns.
"""

import os
import re
import sys
from pathlib import Path
from typing import Any, Dict, List, Optional, Tuple

from ci.util.check_files import (
    FileContent,
    FileContentChecker,
    MultiCheckerFileProcessor,
    collect_files_to_check,
)
from ci.util.paths import PROJECT_ROOT


def strip_comments(content: str) -> str:
    """
    Strip C/C++ comments from content.

    Args:
        content (str): The content to strip comments from

    Returns:
        str: Content with comments removed
    """
    # Remove multi-line comments
    content = re.sub(r"/\*.*?\*/", "", content, flags=re.DOTALL)
    # Remove single-line comments
    content = re.sub(r"//.*$", "", content, flags=re.MULTILINE)
    return content


def is_bracket_balanced(file_path: Path) -> bool:
    """
    Check if brackets are balanced in a file (after stripping comments).

    Args:
        file_path (Path): Path to the file to check

    Returns:
        bool: True if brackets are balanced, False otherwise
    """
    try:
        with open(file_path, "r", encoding="utf-8") as f:
            content = f.read()

        # Strip comments first
        content = strip_comments(content)

        # Count brackets
        open_count = content.count("{")
        close_count = content.count("}")

        return open_count == close_count
    except (UnicodeDecodeError, IOError):
        return True  # Assume balanced if we can't read


def find_includes_after_namespace(
    file_path: Path,
) -> list[tuple[int, str, Optional[tuple[int, str]]]]:
    """
    Check if a C++ file has #include directives after namespace declarations.

    Args:
        file_path (Path): Path to the C++ file to check

    Returns:
        List[Tuple[int, str, Optional[Tuple[int, str]]]]: List of (include_line, include_snippet, (namespace_line, namespace_snippet))
    """
    try:
        with open(file_path, "r", encoding="utf-8") as f:
            content = f.readlines()

        violations: list[tuple[int, str, Optional[tuple[int, str]]]] = []
        current_namespace: Optional[tuple[int, str]] = None

        # Compiled regex patterns for validation (only used after fast string search)
        using_namespace_pattern = re.compile(r"^\s*using\s+namespace\s+\w+\s*;")
        namespace_open_pattern = re.compile(r"^\s*namespace\s+\w+\s*\{")
        include_pattern = re.compile(r"^\s*#\s*include")

        def truncate_snippet(text: str, max_len: int = 50) -> str:
            """Truncate text to max_len characters."""
            text = text.strip()
            if len(text) <= max_len:
                return text
            return text[: max_len - 3] + "..."

        for i, line in enumerate(content, 1):
            original_line = line
            line = line.strip()

            # Skip empty lines and comments
            if not line or line.startswith("//") or line.startswith("/*"):
                continue

            # Check for closing namespace brace - reset namespace tracking
            if current_namespace and line.startswith("}"):
                current_namespace = None

            # Fast string search for "using namespace" before regex validation
            if "using namespace" in line:
                if using_namespace_pattern.match(line):
                    current_namespace = (i, truncate_snippet(original_line))

            # Fast string search for "namespace X {" before regex validation
            if "namespace" in line and "{" in line:
                if namespace_open_pattern.match(line):
                    current_namespace = (i, truncate_snippet(original_line))

            # Fast string search for "#include" before regex validation
            if current_namespace and "#include" in line:
                if include_pattern.match(line):
                    include_snippet = truncate_snippet(original_line)
                    violations.append((i, include_snippet, current_namespace))

        # Second pass: if we found violations, verify with bracket counting
        if violations:
            if not is_bracket_balanced(file_path):
                # Brackets are unbalanced, keep violations as they might be false positives
                # But file has syntax errors, so just clear violations to avoid noise
                return []
            # Brackets are balanced, violations are real
            return violations

        return violations
    except (UnicodeDecodeError, IOError):
        # Skip files that can't be read
        return []


class NamespaceIncludesChecker(FileContentChecker):
    """FileContentChecker implementation for detecting includes after namespace declarations."""

    def __init__(self):
        self.violations: dict[
            str, list[tuple[int, str, Optional[tuple[int, str]]]]
        ] = {}

    def should_process_file(self, file_path: str) -> bool:
        """Check if file should be processed (only .h and .hpp files in src/)."""
        # Skip build directories, third-party code, and tests
        if any(
            part in file_path.lower()
            for part in [
                ".build",
                ".pio",
                ".venv",
                "libdeps",
                "third_party",
                "vendor",
                "tests",
            ]
        ):
            return False

        # Only check .h and .hpp files
        return file_path.endswith((".h", ".hpp"))

    def check_file_content(self, file_content: FileContent) -> list[str]:
        """Check file content for includes after namespace declarations."""
        violations: list[tuple[int, str, Optional[tuple[int, str]]]] = []
        current_namespace: Optional[tuple[int, str]] = None

        # Compiled regex patterns for validation (only used after fast string search)
        using_namespace_pattern = re.compile(r"^\s*using\s+namespace\s+\w+\s*;")
        namespace_open_pattern = re.compile(r"^\s*namespace\s+\w+\s*\{")
        include_pattern = re.compile(r"^\s*#\s*include")

        def truncate_snippet(text: str, max_len: int = 50) -> str:
            """Truncate text to max_len characters."""
            text = text.strip()
            if len(text) <= max_len:
                return text
            return text[: max_len - 3] + "..."

        # Get lines with newlines for original_line tracking
        lines_with_newlines = file_content.content.splitlines(keepends=True)

        for i, line in enumerate(file_content.lines, 1):
            original_line = (
                lines_with_newlines[i - 1] if i - 1 < len(lines_with_newlines) else line
            )
            line_stripped = line.strip()

            # Skip empty lines and comments
            if (
                not line_stripped
                or line_stripped.startswith("//")
                or line_stripped.startswith("/*")
            ):
                continue

            # Check for closing namespace brace - reset namespace tracking
            if current_namespace and line_stripped.startswith("}"):
                current_namespace = None

            # Fast string search for "using namespace" before regex validation
            if "using namespace" in line_stripped:
                if using_namespace_pattern.match(line_stripped):
                    current_namespace = (i, truncate_snippet(original_line))

            # Fast string search for "namespace X {" before regex validation
            if "namespace" in line_stripped and "{" in line_stripped:
                if namespace_open_pattern.match(line_stripped):
                    current_namespace = (i, truncate_snippet(original_line))

            # Fast string search for "#include" before regex validation
            if current_namespace and "#include" in line_stripped:
                if include_pattern.match(line_stripped):
                    include_snippet = truncate_snippet(original_line)
                    violations.append((i, include_snippet, current_namespace))

        # Second pass: if we found violations, verify with bracket counting
        if violations:
            if not is_bracket_balanced(Path(file_content.path)):
                # Brackets are unbalanced, keep violations as they might be false positives
                # But file has syntax errors, so just clear violations to avoid noise
                return []
            # Brackets are balanced, violations are real
            self.violations[file_content.path] = violations

        return []  # We collect violations internally


def scan_cpp_files(directory: str = ".") -> dict[str, Any]:
    """
    Scan all C++ files in a directory for includes after namespace declarations.

    Args:
        directory (str): Directory to scan for C++ files

    Returns:
        Dict[str, Any]: Dictionary mapping file paths to lists of violation data
    """
    # Only check .h and .hpp files
    cpp_extensions = [".h", ".hpp"]
    violations: dict[str, Any] = {}

    for root, dirs, files in os.walk(directory):
        # Skip build directories, third-party code, and tests
        if any(
            part in root.lower()
            for part in [
                ".build",
                ".pio",
                ".venv",
                "libdeps",
                "third_party",
                "vendor",
                "tests",
            ]
        ):
            continue

        for file in files:
            file_path = os.path.join(root, file)

            # Check if it's a C++ file
            if any(file.endswith(ext) for ext in cpp_extensions):
                violation_data = find_includes_after_namespace(Path(file_path))
                if violation_data:
                    violations[file_path] = violation_data

    return violations


def main() -> None:
    # Use the new FileContentChecker-based approach
    src_dir = str(PROJECT_ROOT / "src")

    # Collect files using collect_files_to_check
    files_to_check = collect_files_to_check([src_dir], extensions=[".h", ".hpp"])

    # Create checker and processor
    checker = NamespaceIncludesChecker()
    processor = MultiCheckerFileProcessor()

    # Process all files in a single pass
    processor.process_files_with_checkers(files_to_check, [checker])

    # Get violations from checker
    violations = checker.violations

    if violations:
        # Count total violations
        total_violations = sum(len(v) for v in violations.values())
        file_count = len(violations)

        print(f"❌ Found #include directives after using namespace declarations")
        print()
        print(f"📁 Project Files ({file_count} files, {total_violations} violations):")
        print()

        # Sort files by relative path for consistent output
        project_root = str(PROJECT_ROOT)
        for file_path in sorted(violations.keys()):
            # Convert to relative path
            rel_path = os.path.relpath(file_path, project_root).replace("\\", "/")
            violation_data = violations[file_path]

            print(f"{rel_path}:")

            # Group violations by namespace
            namespace_groups: dict[Optional[str], list[tuple[int, str]]] = {}

            for include_line, include_snippet, namespace_info in violation_data:
                namespace_key = None
                if namespace_info:
                    namespace_line, namespace_snippet = namespace_info
                    namespace_key = f"{namespace_line}: {namespace_snippet}"

                if namespace_key not in namespace_groups:
                    namespace_groups[namespace_key] = []
                namespace_groups[namespace_key].append((include_line, include_snippet))

            # Print each namespace group
            for namespace_key, includes in namespace_groups.items():
                if namespace_key:
                    print(f"  {namespace_key}")
                else:
                    print(f"  (no namespace found)")

                for include_line, include_snippet in includes:
                    print(f"  {include_line}: {include_snippet}")

            print()

        print(
            "💡 Fix: Move all #include directives before using namespace declarations"
        )
        sys.exit(1)
    else:
        print("No violations found.")
        sys.exit(0)


if __name__ == "__main__":
    main()
