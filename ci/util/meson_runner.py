#!/usr/bin/env python3
"""DEPRECATED: Backward compatibility shim for ci.util.meson_runner

This module has been refactored into the ci.meson package for better organization.
Import from ci.meson submodules instead.

Migration guide:
  OLD: from ci.util.meson_runner import MesonTestResult
  NEW: from ci.meson.test_execution import MesonTestResult

  OLD: from ci.util.meson_runner import run_meson_build_and_test
  NEW: from ci.meson.runner import run_meson_build_and_test

  OLD: from ci.util.meson_runner import setup_meson_build
  NEW: from ci.meson.build_config import setup_meson_build

Full module structure:
  - ci.meson.output: Output formatting (print_success, print_error, etc.)
  - ci.meson.io_utils: File I/O (atomic_write_text, write_if_different)
  - ci.meson.compiler: Compiler detection (get_meson_executable, check_meson_installed)
  - ci.meson.test_discovery: Test discovery (get_fuzzy_test_candidates, get_source_files_hash)
  - ci.meson.build_config: Build configuration (setup_meson_build, perform_ninja_maintenance, etc.)
  - ci.meson.compile: Compilation (compile_meson, _create_error_context_filter)
  - ci.meson.test_execution: Test execution (MesonTestResult, run_meson_test)
  - ci.meson.streaming: Parallel streaming (stream_compile_and_run_tests)
  - ci.meson.runner: Main orchestration (run_meson_build_and_test, main)
"""

import warnings


warnings.warn(
    "ci.util.meson_runner is deprecated. Import from ci.meson package instead. "
    "See module documentation for migration guide.",
    DeprecationWarning,
    stacklevel=2,
)

# Re-export public API for backward compatibility
from ci.meson import (
    MesonTestResult,
    check_meson_installed,
    get_meson_executable,
    perform_ninja_maintenance,
    print_banner,
    run_meson_build_and_test,
    setup_meson_build,
)


__all__ = [
    "MesonTestResult",
    "check_meson_installed",
    "get_meson_executable",
    "perform_ninja_maintenance",
    "run_meson_build_and_test",
    "setup_meson_build",
    "print_banner",
]

if __name__ == "__main__":
    from ci.meson.runner import main

    main()
