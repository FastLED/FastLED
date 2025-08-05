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
from typing import Any, Dict, List


def find_includes_after_namespace(file_path: Path) -> List[int]:
    """
    Check if a C++ file has #include directives after namespace declarations.

    Args:
        file_path (Path): Path to the C++ file to check

    Returns:
        List[int]: List of line numbers where includes appear after namespaces
    """
    try:
        with open(file_path, "r", encoding="utf-8") as f:
            content = f.readlines()

        violations: List[int] = []
        namespace_started = False

        # Basic patterns
        namespace_pattern = re.compile(r"^\s*namespace\s+\w+\s*\{")
        include_pattern = re.compile(r"^\s*#\s*include")

        for i, line in enumerate(content, 1):
            line = line.strip()

            # Skip empty lines and comments
            if not line or line.startswith("//") or line.startswith("/*"):
                continue

            # Check for namespace declaration
            if namespace_pattern.match(line):
                namespace_started = True

            # Check for #include after namespace started
            if namespace_started and include_pattern.match(line):
                violations.append(i)

        return violations
    except (UnicodeDecodeError, IOError):
        # Skip files that can't be read
        return []


def scan_cpp_files(directory: str = ".") -> Dict[str, Any]:
    """
    Scan all C++ files in a directory for includes after namespace declarations.

    Args:
        directory (str): Directory to scan for C++ files

    Returns:
        Dict[str, Any]: Dictionary mapping file paths to lists of violation line numbers
    """
    cpp_extensions = [
        ".cpp",
        ".cc",
        ".cxx",
        ".c++",
        ".hpp",
        ".h",
        ".hh",
        ".hxx",
        ".h++",
    ]
    violations: Dict[str, Any] = {}

    for root, dirs, files in os.walk(directory):
        for file in files:
            file_path = os.path.join(root, file)

            # Check if it's a C++ file
            if any(file.endswith(ext) for ext in cpp_extensions):
                line_numbers: List[int] = find_includes_after_namespace(Path(file_path))
                if line_numbers:
                    violations[file_path] = line_numbers

    return violations


def main() -> None:
    violations: Dict[str, Any] = scan_cpp_files()

    if violations:
        print("Found #include directives after namespace declarations:")
        for file_path, line_numbers in violations.items():
            print(f"\n{file_path}:")
            for line_num in line_numbers:
                print(f"  Line {line_num}")

        failing: List[str] = list(violations.keys())
        print(f"\nFailing files: {failing}")
        sys.exit(1)
    else:
        print("No violations found.")
        sys.exit(0)


if __name__ == "__main__":
    main()
