#!/usr/bin/env python3
"""Checker to ensure headers in src/fl/<subdir>/ use proper fl::<subdir>:: namespaces.

Directory-to-namespace mapping (example for subdir="net"):
  src/fl/net/*.h       → must use namespace fl::net
  src/fl/net/http/*.h  → must use namespace fl::net::http
  src/fl/net/ble/*.h   → must use namespace fl::net::ble
  (and so on for any future subdirectory)

Pure-include umbrella headers (no namespace declarations at all) are skipped.

No suppression — this rule cannot be bypassed.
"""

import re

from ci.util.check_files import FileContent, FileContentChecker
from ci.util.paths import PROJECT_ROOT


# Regex for namespace declarations: 'namespace foo {' or 'namespace foo::bar {'
_NAMESPACE_RE = re.compile(r"\bnamespace\s+([\w:]+)\s*\{")


class SubdirNamespaceChecker(FileContentChecker):
    """Checks that headers in src/fl/<subdir>/ use the proper fl::<subdir>:: namespace."""

    def __init__(self, subdir: str) -> None:
        self.subdir = subdir
        self.subdir_root = str(PROJECT_ROOT / "src" / "fl" / subdir).replace("\\", "/")
        self.violations: dict[str, list[tuple[int, str]]] = {}

    def should_process_file(self, file_path: str) -> bool:
        normalized = file_path.replace("\\", "/")
        if (
            not normalized.startswith(self.subdir_root + "/")
            and normalized != self.subdir_root
        ):
            return False
        return file_path.endswith(".h")

    def _expected_namespace_parts(self, file_path: str) -> list[str]:
        """Compute expected namespace parts from file path.

        Only requires the first-level subdirectory namespace (fl::<subdir>),
        not deeper levels, since deeper directory names may collide with
        C++ type names (e.g., 'fixed_point' is both a directory and a class).

        For subdir="net":
          src/fl/net/fetch.h              -> ["fl", "net"]
          src/fl/net/http/stream_client.h -> ["fl", "net"]

        For subdir="math":
          src/fl/math/random.h            -> ["fl", "math"]
          src/fl/math/filter/kalman.h     -> ["fl", "math"]
        """
        return ["fl", self.subdir]

    def check_file_content(self, file_content: FileContent) -> list[str]:
        expected = self._expected_namespace_parts(file_content.path)
        # e.g. ["fl", "net"] or ["fl", "net", "http"]

        # Collect all namespace identifiers from declarations in the file
        found_parts: set[str] = set()
        has_any_namespace = False

        for line in file_content.lines:
            stripped = line.strip()
            # Skip obvious comment-only lines
            if (
                stripped.startswith("//")
                or stripped.startswith("/*")
                or stripped.startswith("*")
            ):
                continue
            # Remove inline comments
            code = stripped.split("//")[0]

            for match in _NAMESPACE_RE.finditer(code):
                has_any_namespace = True
                ns_name = match.group(1)
                for part in ns_name.split("::"):
                    if part:
                        found_parts.add(part)

        # Pure-include umbrella headers with no namespace declarations — skip
        if not has_any_namespace:
            return []

        # Check that all expected namespace parts appear
        missing = [p for p in expected if p not in found_parts]

        if missing:
            expected_ns = "::".join(expected)
            self.violations[file_content.path] = [
                (
                    1,
                    f"Header in fl/{self.subdir}/ must use namespace {expected_ns}; "
                    f"missing namespace part(s): {', '.join(missing)}",
                )
            ]

        return []


# Backward-compatible alias
def NetNamespaceChecker() -> SubdirNamespaceChecker:  # noqa: N802
    return SubdirNamespaceChecker("net")


def MathNamespaceChecker() -> SubdirNamespaceChecker:  # noqa: N802
    return SubdirNamespaceChecker("math")


def VideoNamespaceChecker() -> SubdirNamespaceChecker:  # noqa: N802
    return SubdirNamespaceChecker("video")


def TaskNamespaceChecker() -> SubdirNamespaceChecker:  # noqa: N802
    return SubdirNamespaceChecker("task")


def main() -> None:
    """Run checker standalone for all registered subdirs."""
    import sys

    from ci.util.check_files import (
        MultiCheckerFileProcessor,
        collect_files_to_check,
    )

    had_violations = False
    for subdir in ("net", "math", "video", "task"):
        checker = SubdirNamespaceChecker(subdir)
        desc = f"fl/{subdir}/ headers with incorrect namespace"
        files = collect_files_to_check(
            [str(PROJECT_ROOT / "src" / "fl" / subdir)], [".h"]
        )
        processor = MultiCheckerFileProcessor()
        processor.process_files_with_checkers(files, [checker])
        if checker.violations:
            had_violations = True
            for path, issues in checker.violations.items():
                for lineno, msg in issues:
                    print(f"  {path}:{lineno}: {msg}")
        else:
            print(f"✅ {desc}: No violations found.")

    sys.exit(1 if had_violations else 0)


if __name__ == "__main__":
    main()
