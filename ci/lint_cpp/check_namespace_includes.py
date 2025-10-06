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
) -> List[Tuple[int, str, Optional[Tuple[int, str]]]]:
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

        violations: List[Tuple[int, str, Optional[Tuple[int, str]]]] = []
        current_namespace: Optional[Tuple[int, str]] = None

        # Compiled regex patterns for validation (only used after fast string search)
        using_namespace_pattern = re.compile(r"^\s*using\s+namespace\s+\w+\s*;")
        include_pattern = re.compile(r"^\s*#\s*include")
        namespace_ok_pattern = re.compile(r"//\s*namespace\s+ok\b")

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

            # Fast string search for "using namespace" before regex validation
            if "using namespace" in line:
                if using_namespace_pattern.match(line):
                    current_namespace = (i, truncate_snippet(original_line))

            # Fast string search for "#include" before regex validation
            if current_namespace and "#include" in line:
                if include_pattern.match(line):
                    # Fast string search for suppression comment before regex validation
                    has_suppression = False
                    if "namespace ok" in original_line:
                        has_suppression = namespace_ok_pattern.search(original_line)

                    if not has_suppression:
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


def scan_cpp_files(directory: str = ".") -> Dict[str, Any]:
    """
    Scan all C++ files in a directory for includes after namespace declarations.

    Args:
        directory (str): Directory to scan for C++ files

    Returns:
        Dict[str, Any]: Dictionary mapping file paths to lists of violation data
    """
    # Only check .h and .hpp files
    cpp_extensions = [".h", ".hpp"]
    violations: Dict[str, Any] = {}

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
    # Only scan src/ directory for .h files
    src_dir = os.path.join(PROJECT_ROOT, "src")
    violations: Dict[str, Any] = scan_cpp_files(str(src_dir))

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
            namespace_groups: Dict[Optional[str], List[Tuple[int, str]]] = {}

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
        print(
            "   Or add '// namespace ok' to the end of specific include lines to suppress this warning"
        )
        sys.exit(1)
    else:
        print("No violations found.")
        sys.exit(0)


if __name__ == "__main__":
    main()
