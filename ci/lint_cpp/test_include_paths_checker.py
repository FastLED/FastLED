#!/usr/bin/env python3
# pyright: reportUnknownMemberType=false
"""
Lint check: Enforce fully-qualified include paths in test files.

Test files (tests/**) must use fully-qualified include paths — never bare
filenames or relative paths (../ or ./).  This prevents ambiguous include
resolution when both ``-I tests/`` (implicit from Meson source dir) and
``-I src/`` are on the search path.

Valid includes:
  - #include "fl/foo.h"              - src-relative (via -I src)
  - #include "platforms/foo.h"       - src-relative (via -I src)
  - #include <system_header>         - system/angle-bracket headers
  - #include "FastLED.h"             - top-level src header (unambiguous)
  - #include "test.h"                - top-level tests/ header (unambiguous)

Invalid includes:
  - #include "../foo.hpp"            - relative path traversal
  - #include "./foo.h"               - explicit current-dir
  - #include "map_range.hpp"         - bare test sub-header (ambiguous)
  - #include "test_helpers.h"        - bare test sub-header (ambiguous)
  - #include "mock_fastled.h"        - bare test sub-header (ambiguous)

Exception mechanism:
  Add ``// ok test include`` comment on the line to suppress.
"""

import re
from pathlib import Path

from ci.util.check_files import FileContent, FileContentChecker
from ci.util.paths import PROJECT_ROOT


TESTS_DIR = str(PROJECT_ROOT / "tests").replace("\\", "/")

# Pattern to extract include path from #include "..." statements
_INCLUDE_PATTERN = re.compile(r'^\s*#\s*include\s+"([^"]+)"')

# Filesystem scans below are deferred to first-use. Walking tests/ recursively
# costs ~314ms at module import, which is paid by every single-file lint —
# even when the file being linted is not under tests/ and this checker is a
# no-op for it. Lazy init keeps the import cheap.
_allowed_bare_test_headers_cache: frozenset[str] | None = None
_allowed_bare_src_headers_cache: frozenset[str] | None = None
_allowed_bare_includes_cache: frozenset[str] | None = None
_all_test_filenames_cache: frozenset[str] | None = None


def _allowed_bare_test_headers() -> frozenset[str]:
    """Top-level headers in tests/ that are unambiguous (no dir prefix needed)."""
    global _allowed_bare_test_headers_cache
    if _allowed_bare_test_headers_cache is None:
        _allowed_bare_test_headers_cache = frozenset(
            p.name
            for p in (PROJECT_ROOT / "tests").iterdir()
            if p.is_file() and p.suffix in (".h", ".hpp")
        )
    return _allowed_bare_test_headers_cache


def _allowed_bare_src_headers() -> frozenset[str]:
    """Top-level headers in src/ that are unambiguous (legacy compatibility).

    These live directly in src/ (e.g., FastLED.h, crgb.h) and have no
    counterpart in tests/.
    """
    global _allowed_bare_src_headers_cache
    if _allowed_bare_src_headers_cache is None:
        _allowed_bare_src_headers_cache = frozenset(
            p.name
            for p in (PROJECT_ROOT / "src").iterdir()
            if p.is_file() and p.suffix in (".h", ".hpp")
        )
    return _allowed_bare_src_headers_cache


def _allowed_bare_includes() -> frozenset[str]:
    """Union of allowed bare (no-directory) includes."""
    global _allowed_bare_includes_cache
    if _allowed_bare_includes_cache is None:
        _allowed_bare_includes_cache = (
            _allowed_bare_test_headers() | _allowed_bare_src_headers()
        )
    return _allowed_bare_includes_cache


def _all_test_filenames() -> frozenset[str]:
    """All filenames that exist somewhere inside tests/ (recursively).

    A bare include is only flagged if the filename is found here — otherwise
    it's probably a system or SDK header (e.g., <stdint.h>, <initializer_list>).
    Recursive scan is the dominant cost (~314ms); cached after first use.
    """
    global _all_test_filenames_cache
    if _all_test_filenames_cache is None:
        _all_test_filenames_cache = frozenset(
            p.name
            for p in (PROJECT_ROOT / "tests").rglob("*")
            if p.is_file() and p.suffix in (".h", ".hpp", ".cpp.hpp")
        )
    return _all_test_filenames_cache


# Valid directory prefixes for qualified includes (src-relative or test-relative).
_VALID_PREFIXES = (
    "fl/",
    "platforms/",
    "fx/",
    "sensors/",
    "third_party/",
    "tests/",
)


def _is_qualified(include_path: str) -> bool:
    """Return True if the include uses a directory-qualified path."""
    return any(include_path.startswith(p) for p in _VALID_PREFIXES)


def _is_relative(include_path: str) -> bool:
    """Return True if the include uses a relative path (../ or ./)."""
    return include_path.startswith("../") or include_path.startswith("./")


def _is_bare_filename(include_path: str) -> bool:
    """Return True if the include is a bare filename (no directory component)."""
    return "/" not in include_path and "\\" not in include_path


class TestIncludePathsChecker(FileContentChecker):
    """Checker for include path style in test files.

    Ensures all ``#include "..."`` directives in tests/ use fully-qualified
    paths rather than bare filenames or relative traversals.
    """

    def __init__(self) -> None:
        self.violations: dict[str, list[tuple[int, str]]] = {}

    def should_process_file(self, file_path: str) -> bool:
        """Only process C++ files in tests/."""
        normalized = file_path.replace("\\", "/")
        if not (TESTS_DIR + "/") in normalized:
            return False
        return file_path.endswith((".cpp", ".h", ".hpp", ".cc", ".cxx"))

    def check_file_content(self, file_content: FileContent) -> list[str]:
        """Check for non-qualified includes in test files."""
        violations: list[tuple[int, str]] = []
        in_multiline_comment = False

        for line_number, line in enumerate(file_content.lines, 1):
            stripped = line.strip()

            # Track multi-line comment state
            if "/*" in line:
                in_multiline_comment = True
            if "*/" in line:
                in_multiline_comment = False
                continue

            if in_multiline_comment:
                continue

            if stripped.startswith("//"):
                continue

            # Suppression comment
            if "ok test include" in line.lower():
                continue

            match = _INCLUDE_PATTERN.match(stripped)
            if not match:
                continue

            include_path = match.group(1)

            # 1) Relative includes are always wrong in tests.
            if _is_relative(include_path):
                violations.append(
                    (
                        line_number,
                        f'Relative include not allowed in tests: #include "{include_path}" — '
                        f"use a fully-qualified path (e.g., tests/fl/...). "
                        f"Add '// ok test include' to suppress.",
                    )
                )
                continue

            # 2) Qualified includes (fl/, platforms/, tests/, ...) are fine.
            if _is_qualified(include_path):
                continue

            # 3) Bare filename — allowed only for known top-level headers
            #    or system/SDK headers that don't exist in tests/.
            if _is_bare_filename(include_path):
                if include_path in _allowed_bare_includes():
                    continue
                # Only flag if this filename actually exists somewhere in tests/
                # (i.e., it's a test-local header).  System/SDK headers like
                # stdint.h, initializer_list, esp_err.h are NOT in tests/ and
                # are silently allowed.
                if include_path not in _all_test_filenames():
                    continue
                violations.append(
                    (
                        line_number,
                        f'Bare include not allowed in tests: #include "{include_path}" — '
                        f"use a fully-qualified path (e.g., fl/... or tests/...). "
                        f"Add '// ok test include' to suppress.",
                    )
                )
                continue

            # 4) Has a directory prefix but not one of the valid ones.
            #    Check if this is a current-dir-relative sub-path include
            #    (e.g., "detector/beat.hpp" from tests/fl/audio/) by seeing
            #    if it resolves to a file under the including file's directory.
            file_dir = Path(file_content.path).parent
            candidate = file_dir / include_path
            if candidate.exists():
                violations.append(
                    (
                        line_number,
                        f'Sub-path include not allowed in tests: #include "{include_path}" — '
                        f"use a fully-qualified path (e.g., tests/fl/...). "
                        f"Add '// ok test include' to suppress.",
                    )
                )
                continue

            # Also check if the first component looks like a test sub-dir
            # (e.g., "shared/fl_unittest.h" should be "tests/shared/fl_unittest.h")
            first_component = include_path.split("/")[0]
            _TEST_SUBDIRS = {"fl", "misc", "platforms", "shared", "profile"}
            if first_component in _TEST_SUBDIRS and not include_path.startswith(
                "tests/"
            ):
                violations.append(
                    (
                        line_number,
                        f'Include missing tests/ prefix: #include "{include_path}" — '
                        f'use #include "tests/{include_path}" instead. '
                        f"Add '// ok test include' to suppress.",
                    )
                )

        if violations:
            self.violations[file_content.path] = violations

        return []  # Violations collected internally
