import os
import re
import unittest
from typing import Dict, List, Tuple

from ci.util.paths import PROJECT_ROOT


SRC_ROOT = PROJECT_ROOT / "src"
EXAMPLES_ROOT = PROJECT_ROOT / "examples"
TESTS_ROOT = PROJECT_ROOT / "tests"

# Skip patterns for directories that contain third-party code or build artifacts
SKIP_PATTERNS = [
    ".venv",
    "node_modules",
    "build",
    ".build",
    "third_party",
    "ziglang",
    "greenlet",
    ".git",
]

# Regex patterns as triple-quoted constants
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


def find_includes_after_namespace(file_path: str) -> List[Tuple[int, str]]:
    """
    Check if a C++ file has #include directives after namespace declarations.

    Args:
        file_path (str): Path to the C++ file to check

    Returns:
        List[Tuple[int, str]]: List of tuples containing line numbers and line content
                              where includes appear after namespaces. Returns empty list
                              if no violations found or file cannot be decoded.
    """
    try:
        with open(file_path, "r", encoding="utf-8") as f:
            lines = f.readlines()
    except UnicodeDecodeError:
        # Skip files that can't be decoded as UTF-8
        return []

    # Check if the file has the allow directive
    for line in lines:
        if ALLOW_DIRECTIVE_PATTERN.search(line):
            return []  # Return empty list if directive is found

    namespace_started = False
    violations: List[Tuple[int, str]] = []

    for i, line in enumerate(lines, 1):
        # Check if we're entering a namespace
        if NAMESPACE_PATTERN.match(line):
            namespace_started = True
            continue

        # Check for includes after namespace started
        if namespace_started and INCLUDE_PATTERN.match(line):
            violations.append((i, line.rstrip("\n")))

    return violations


def scan_cpp_files(directory: str = ".") -> Dict[str, List[Tuple[int, str]]]:
    """
    Scan all C++ files in a directory for includes after namespace declarations.

    Args:
        directory (str): Directory to scan for C++ files

    Returns:
        Dict[str, List[Tuple[int, str]]]: Dictionary mapping file paths to list of tuples
                                         (line numbers, line content) of violations
    """
    violations: Dict[str, List[Tuple[int, str]]] = {}

    # Find all C++ files
    cpp_extensions = [".cpp", ".h", ".hpp", ".cc", ".ino"]

    for root, dirs, files in os.walk(directory):
        # Skip directories with third-party code
        if any(pattern in root for pattern in SKIP_PATTERNS):
            continue

        for file in files:
            if any(file.endswith(ext) for ext in cpp_extensions):
                file_path = os.path.join(root, file)
                try:
                    line_info = find_includes_after_namespace(file_path)
                    if line_info:
                        violations[file_path] = line_info
                except Exception as e:
                    print(f"Error processing {file_path}: {e}")

    return violations


class TestNoIncludeAfterNamespace(unittest.TestCase):
    def test_no_includes_after_namespace_in_src(self) -> None:
        """Check that src/ directory doesn't have includes after namespace declarations."""
        violations = scan_cpp_files(str(SRC_ROOT))

        if violations:
            msg = "Found includes after namespace declarations in src/:\n"
            for file_path, line_info in violations.items():
                msg += f"  {file_path}:\n"
                for line_num, line_content in line_info:
                    msg += f"    Line {line_num}: {line_content}\n"
            self.fail(
                msg
                + "\nPlease fix these issues by moving includes to the top of the file.\n"
                "See TEST_NAMESPACE_INCLUDES.md for more information."
            )
        else:
            print(
                "No violations found in src/! All includes are properly placed before namespace declarations."
            )

    def test_no_includes_after_namespace_in_examples(self) -> None:
        """Check that examples/ directory doesn't have includes after namespace declarations."""
        violations = scan_cpp_files(str(EXAMPLES_ROOT))

        if violations:
            msg = "Found includes after namespace declarations in examples/:\n"
            for file_path, line_info in violations.items():
                msg += f"  {file_path}:\n"
                for line_num, line_content in line_info:
                    msg += f"    Line {line_num}: {line_content}\n"
            self.fail(
                msg
                + "\nPlease fix these issues by moving includes to the top of the file.\n"
                "See TEST_NAMESPACE_INCLUDES.md for more information."
            )
        else:
            print(
                "No violations found in examples/! All includes are properly placed before namespace declarations."
            )

    def test_no_includes_after_namespace_in_tests(self) -> None:
        """Check that tests/ directory doesn't have includes after namespace declarations."""
        violations = scan_cpp_files(str(TESTS_ROOT))

        if violations:
            msg = "Found includes after namespace declarations in tests/:\n"
            for file_path, line_info in violations.items():
                msg += f"  {file_path}:\n"
                for line_num, line_content in line_info:
                    msg += f"    Line {line_num}: {line_content}\n"
            self.fail(
                msg
                + "\nPlease fix these issues by moving includes to the top of the file.\n"
                "See TEST_NAMESPACE_INCLUDES.md for more information."
            )
        else:
            print(
                "No violations found in tests/! All includes are properly placed before namespace declarations."
            )


if __name__ == "__main__":
    unittest.main()
