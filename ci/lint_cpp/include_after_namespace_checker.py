"""Checker for detecting includes after namespace declarations."""

import re

from ci.util.check_files import FileContent, FileContentChecker


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
NOLINT_PATTERN = re.compile(r"//\s*nolint")


class IncludeAfterNamespaceChecker(FileContentChecker):
    """FileContentChecker implementation for detecting includes after namespace declarations."""

    def __init__(self):
        self.violations: dict[str, list[tuple[int, str]]] = {}

    def should_process_file(self, file_path: str) -> bool:
        """Check if file should be processed."""
        # Skip files matching skip patterns
        if any(pattern in file_path for pattern in SKIP_PATTERNS):
            return False
        # Only check C++ files
        return any(
            file_path.endswith(ext) for ext in [".cpp", ".h", ".hpp", ".cc", ".ino"]
        )

    def check_file_content(self, file_content: FileContent) -> list[str]:
        """Check file content for includes after namespace declarations."""
        # Check if the file has the allow directive
        for line in file_content.lines:
            if ALLOW_DIRECTIVE_PATTERN.search(line):
                return []  # Return empty if directive is found

        namespace_started = False
        violations: list[tuple[int, str]] = []

        for i, line in enumerate(file_content.lines, 1):
            # Check if we're entering a namespace
            if NAMESPACE_PATTERN.match(line):
                namespace_started = True
                continue

            # Check for includes after namespace started
            if namespace_started and INCLUDE_PATTERN.match(line):
                # Skip if the line has a // nolint comment
                if NOLINT_PATTERN.search(line):
                    continue
                violations.append((i, line.rstrip("\n")))

        if violations:
            self.violations[file_content.path] = violations

        return []  # We collect violations internally


def main() -> None:
    """Run include after namespace checker standalone."""
    from ci.util.check_files import run_checker_standalone
    from ci.util.paths import PROJECT_ROOT

    checker = IncludeAfterNamespaceChecker()
    run_checker_standalone(
        checker,
        [str(PROJECT_ROOT / "src")],
        "Found #include after namespace declarations",
        extensions=[".cpp", ".h", ".hpp", ".cc", ".ino"],
    )


if __name__ == "__main__":
    main()
