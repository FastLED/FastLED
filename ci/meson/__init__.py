"""Meson build system integration for FastLED.

This package provides modular tools for building and testing FastLED using Meson.
Refactored from monolithic ci/util/meson_runner.py into focused modules.

Module Structure:
- output.py: Output formatting and colored printing
- io_utils.py: Safe file I/O operations
- compiler.py: Compiler detection and Meson executable resolution
- test_discovery.py: Test discovery and fuzzy matching
- build_config.py: Build directory setup and configuration
- compile.py: Compilation execution
- test_execution.py: Test execution via meson test
- streaming.py: Parallel streaming compilation and testing
- runner.py: Main orchestration and CLI entry point
"""

from ci.meson.build_config import perform_ninja_maintenance, setup_meson_build
from ci.meson.compiler import check_meson_installed, get_meson_executable
from ci.meson.output import _print_banner
from ci.meson.runner import run_meson_build_and_test
from ci.meson.test_execution import MesonTestResult


__all__ = [
    "check_meson_installed",
    "get_meson_executable",
    "run_meson_build_and_test",
    "MesonTestResult",
    "perform_ninja_maintenance",
    "setup_meson_build",
    "_print_banner",
]
