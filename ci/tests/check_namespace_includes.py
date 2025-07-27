#!/usr/bin/env python3
"""
Script to check for includes after namespace declarations in C++ files.
This is used in CI/CD to prevent bad code patterns.
"""

import os
import re
import sys


def find_includes_after_namespace(file_path):
    """
    Check if a C++ file has #include directives after namespace declarations.

    Args:
        file_path (str): Path to the C++ file to check

    Returns:
        list: List of line numbers where includes appear after namespaces
    """
    try:
        with open(file_path, "r", encoding="utf-8") as f:
            lines = f.readlines()
    except UnicodeDecodeError:
        # Skip files that can't be decoded as UTF-8
        return []

    namespace_started = False
    violations = []

    namespace_pattern = re.compile(r"^\s*(namespace\s+\w+|namespace\s*{)")
    include_pattern = re.compile(r'^\s*#\s*include\s*[<"].*[>"]')

    for i, line in enumerate(lines, 1):
        # Check if we're entering a namespace
        if namespace_pattern.match(line):
            namespace_started = True
            continue

        # Check for includes after namespace started
        if namespace_started and include_pattern.match(line):
            violations.append(i)

    return violations


def scan_cpp_files(directory="."):
    """
    Scan all C++ files in a directory for includes after namespace declarations.

    Args:
        directory (str): Directory to scan for C++ files

    Returns:
        dict: Dictionary mapping file paths to line numbers of violations
    """
    violations = {}

    # Find all C++ files
    cpp_extensions = [".cpp", ".h", ".hpp", ".cc"]
    skip_patterns = [
        ".venv",
        "node_modules",
        "build",
        ".build",
        "third_party",
        "ziglang",
        "greenlet",
    ]

    for root, dirs, files in os.walk(directory):
        # Skip directories with third-party code
        if any(pattern in root for pattern in skip_patterns):
            continue

        for file in files:
            if any(file.endswith(ext) for ext in cpp_extensions):
                file_path = os.path.join(root, file)
                try:
                    line_numbers = find_includes_after_namespace(file_path)
                    if line_numbers:
                        violations[file_path] = line_numbers
                except Exception as e:
                    print(f"Error processing {file_path}: {e}")

    return violations


def main():
    """Main function to run the script."""
    print("Scanning for #include directives after namespace declarations...")

    violations = scan_cpp_files(".")

    if violations:
        print("\nFound violations:")
        print("=================")
        for file_path, line_numbers in violations.items():
            print(f"{file_path}:")
            for line_num in line_numbers:
                print(f"  Line {line_num}")
        print("\nPlease fix these issues by moving includes to the top of the file.")
        return 1  # Return error code for CI/CD
    else:
        print(
            "\nNo violations found! All includes are properly placed before namespace declarations."
        )
        return 0  # Return success code


if __name__ == "__main__":
    sys.exit(main())
