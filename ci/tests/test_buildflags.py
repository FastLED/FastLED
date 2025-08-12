#!/usr/bin/env python3

"""
Tests for BuildFlags TOML parsing in ci/ directory.

This test discovers all top-level *.toml files under the project ci/ folder
and verifies they can be parsed by BuildFlags.parse without throwing.
"""

import unittest
from pathlib import Path
from typing import List

from ci.compiler.clang_compiler import BuildFlags


def find_ci_toml_files(ci_dir: Path) -> List[Path]:
    """Return a list of top-level *.toml files in ci/.

    Args:
        ci_dir: Absolute path to the ci/ directory

    Returns:
        List of absolute paths to .toml files directly under ci/.
    """
    toml_files: List[Path] = []
    for entry in ci_dir.iterdir():
        if entry.is_file() and entry.suffix == ".toml":
            toml_files.append(entry.resolve())
    return toml_files


class TestBuildFlagsParsing(unittest.TestCase):
    def test_parse_all_ci_tomls(self) -> None:
        project_root: Path = Path(__file__).resolve().parent.parent.parent
        ci_dir: Path = project_root / "ci"
        self.assertTrue(ci_dir.exists(), f"ci directory not found at {ci_dir}")

        toml_files: List[Path] = find_ci_toml_files(ci_dir)
        self.assertTrue(len(toml_files) > 0, "No .toml files found in ci/")

        for toml_path in toml_files:
            with self.subTest(toml=str(toml_path)):
                # Ensure each ci/*.toml parses without throwing
                _ = BuildFlags.parse(toml_path, quick_build=True, strict_mode=False)


if __name__ == "__main__":
    unittest.main()
