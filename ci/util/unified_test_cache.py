#!/usr/bin/env python3
"""
Unified Test Caching

Provides unified safe fingerprint caching for test.py to replace the multiple
custom fingerprint implementations with the single safe approach.
"""

from pathlib import Path
from typing import Optional

from ci.fingerprint import HashFingerprintCache


class UnifiedTestCache:
    """Unified safe cache for test.py operations."""

    def __init__(
        self,
        cache_name: str,
        cache_dir: Optional[Path] = None,
        timestamp_file: Optional[Path] = None,
    ):
        if cache_dir is None:
            cache_dir = Path(".cache")
        cache_dir.mkdir(parents=True, exist_ok=True)
        self.cache = HashFingerprintCache(
            cache_dir, cache_name, timestamp_file=timestamp_file
        )
        self.cache_name = cache_name

    def check_needs_update(self, files_to_monitor: list[Path]) -> bool:
        """
        Check if files have changed and need processing.

        Uses the unified safe pattern: stores fingerprint for later success marking.
        """
        return self.cache.check_needs_update(files_to_monitor)

    def mark_success(self) -> None:
        """Mark the test/process as successful."""
        self.cache.mark_success()

    def invalidate(self) -> None:
        """Invalidate cache on failure."""
        self.cache.invalidate()


def create_src_code_cache() -> UnifiedTestCache:
    """
    Create cache for source code fingerprinting.

    Writes timestamp file (.build/src_code.timestamp) when src/** changes.
    This allows other tools to quickly check if rebuild is needed.
    """
    timestamp_file = Path(".build") / "src_code.timestamp"
    return UnifiedTestCache("src_code", timestamp_file=timestamp_file)


def create_cpp_test_cache() -> UnifiedTestCache:
    """Create cache for C++ test fingerprinting."""
    return UnifiedTestCache("cpp_test")


def create_examples_cache() -> UnifiedTestCache:
    """Create cache for examples fingerprinting."""
    return UnifiedTestCache("examples")


def create_python_test_cache() -> UnifiedTestCache:
    """Create cache for Python test fingerprinting."""
    return UnifiedTestCache("python_test")


def get_src_code_files() -> list[Path]:
    """Get all source code files to monitor."""
    files: list[Path] = []

    # Add all source files
    src_dir = Path("src")
    if src_dir.exists():
        for pattern in ("**/*.h", "**/*.hpp", "**/*.cpp", "**/*.c"):
            files.extend(src_dir.glob(pattern))

    return sorted(files, key=str)


def get_cpp_test_files() -> list[Path]:
    """Get all C++ test files to monitor."""
    files: list[Path] = []

    # Add test files
    tests_dir = Path("tests")
    if tests_dir.exists():
        for pattern in ("**/*.h", "**/*.hpp", "**/*.cpp", "**/*.c"):
            files.extend(tests_dir.glob(pattern))

    # Also include source files as dependencies
    files.extend(get_src_code_files())

    return sorted(files, key=str)


def get_examples_files() -> list[Path]:
    """Get all example files to monitor."""
    files: list[Path] = []

    # Add example files
    examples_dir = Path("examples")
    if examples_dir.exists():
        for pattern in ("**/*.ino", "**/*.h", "**/*.hpp", "**/*.cpp", "**/*.c"):
            files.extend(examples_dir.glob(pattern))

    # Also include source files as dependencies
    files.extend(get_src_code_files())

    return sorted(files, key=str)


def get_python_test_files() -> list[Path]:
    """Get all Python test files to monitor."""
    files: list[Path] = []

    # Add Python test files
    for pattern in ("test*.py", "ci/**/*.py", "**/test_*.py"):
        files.extend(Path(".").glob(pattern))

    # Add pyproject.toml for configuration changes
    pyproject = Path("pyproject.toml")
    if pyproject.exists():
        files.append(pyproject)

    return sorted(files, key=str)
