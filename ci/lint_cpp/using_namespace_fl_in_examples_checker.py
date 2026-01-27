#!/usr/bin/env python3
"""Checker to ensure 'using namespace fl;' is not used in example files.

This checker prevents namespace conflicts between fl:: and Arduino built-in functions.
When FastLED.h does 'using fl::delay;' and an example also does 'using namespace fl;',
it can create ambiguity with Arduino's delay() function.

Examples should use:
- Explicit qualification: fl::delay(), fl::XYMap, etc.
- Individual using declarations: using fl::XYMap; using fl::vector;
"""

import re

from ci.util.check_files import FileContent, FileContentChecker
from ci.util.paths import PROJECT_ROOT


EXAMPLES_ROOT = PROJECT_ROOT / "examples"


class UsingNamespaceFlInExamplesChecker(FileContentChecker):
    """Checker for 'using namespace fl;' in example files."""

    def __init__(self):
        self.violations: dict[str, list[tuple[int, str]]] = {}

    def should_process_file(self, file_path: str) -> bool:
        """Check if file should be processed."""
        # Only check files in examples directory
        if not file_path.startswith(str(EXAMPLES_ROOT)):
            return False

        # Check C++ file extensions
        if not file_path.endswith((".cpp", ".h", ".hpp", ".ino", ".c", ".cc", ".cxx")):
            return False

        return True

    def check_file_content(self, file_content: FileContent) -> list[str]:
        """Check file content for 'using namespace fl;' statements."""
        violations: list[tuple[int, str]] = []

        for line_number, line in enumerate(file_content.lines, 1):
            # Match 'using namespace fl;' with optional whitespace
            # This regex allows for variations like:
            # - using namespace fl;
            # - using namespace fl ;
            # - using   namespace   fl  ;
            if re.search(r"\busing\s+namespace\s+fl\s*;", line):
                violations.append((line_number, line.rstrip()))

        # Store violations if any found
        if violations:
            self.violations[file_content.path] = violations

        return []  # We collect violations internally


def main() -> None:
    """Run checker standalone."""
    from ci.util.check_files import run_checker_standalone

    checker = UsingNamespaceFlInExamplesChecker()
    run_checker_standalone(
        checker,
        [str(EXAMPLES_ROOT)],
        "Found 'using namespace fl;' in example files",
        extensions=[".cpp", ".h", ".hpp", ".ino"],
    )


if __name__ == "__main__":
    main()
