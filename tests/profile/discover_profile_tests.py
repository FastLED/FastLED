#!/usr/bin/env python3
"""Discover profile test .cpp files under tests/profile/.

Called by tests/profile/meson.build via run_command. Prints a
semicolon-separated list of files (semicolons avoid newline issues
on Windows when meson splits run_command stdout).

Why a script, not embedded python in meson.build:
- agents/docs/build-system.md "NO Embedded Python Scripts" rule.
- An external script is greppable, lintable, testable, and benefits
  from the project's regular tooling.

Usage:
    python tests/profile/discover_profile_tests.py <source_root>

Exits 0 on success even when no files are found (prints empty line).
"""

from __future__ import annotations

import glob
import os
import sys


def discover(source_dir: str) -> list[str]:
    """Return the sorted list of profile test .cpp files under tests/profile/.

    Mirrors the meson.build embedded-python globbing exactly:
    - tests/profile/*.cpp (top-level)
    - tests/profile/**/*.cpp (any depth)
    """
    top = set(glob.glob(os.path.join(source_dir, "tests/profile/*.cpp")))
    deep = set(
        glob.glob(os.path.join(source_dir, "tests/profile/**/*.cpp"), recursive=True)
    )
    return sorted(top | deep)


def main() -> int:
    if len(sys.argv) != 2:
        print(
            f"usage: {sys.argv[0]} <source_root>",
            file=sys.stderr,
        )
        return 2
    source_dir = sys.argv[1]
    if not os.path.isdir(source_dir):
        print(
            f"error: source_root does not exist or is not a directory: {source_dir}",
            file=sys.stderr,
        )
        return 2
    files = discover(source_dir)
    if files:
        print(";".join(files))
    return 0


if __name__ == "__main__":
    sys.exit(main())
