#!/usr/bin/env python3
"""Meson build configuration and setup management for FastLED."""
# pyright: reportMissingImports=false, reportUnknownVariableType=false

import glob
import os
import platform
import re
import shutil
import subprocess
import sys
import time
from dataclasses import dataclass
from pathlib import Path
from typing import Optional

from running_process import RunningProcess

from ci.meson.compiler import (
    check_meson_installed,
    check_meson_version_compatibility,
    get_compiler_version,
    get_meson_executable,
    get_meson_version,
)
from ci.meson.io_utils import atomic_write_text, write_if_different
from ci.meson.output import _print_banner, _print_warning
from ci.meson.test_discovery import get_source_files_hash
from ci.util.global_interrupt_handler import handle_keyboard_interrupt_properly
from ci.util.output_formatter import TimestampFormatter
from ci.util.timestamp_print import ts_print as _ts_print


@dataclass
class CleanupResult:
    """Result of build artifact cleanup operation."""

    deleted: int
    failed: int
    failed_files: list[str]


def cleanup_build_artifacts(build_dir: Path, reason: str) -> dict[str, CleanupResult]:
    """
    Clean all build artifacts from a build directory.

    This function centralizes all build artifact cleanup logic and provides
    consistent failure tracking and reporting. It's designed to be called
    when debug mode, build mode, or compiler version changes.

    Args:
        build_dir: Meson build directory to clean
        reason: Human-readable reason for cleanup (e.g., "debug mode changed")

    Returns:
        Dictionary mapping artifact type to CleanupResult with deletion stats
    """
    _ts_print(f"[MESON] 🗑️  {reason} - cleaning all build artifacts")

    # Artifact types with their file extensions
    # Format: (display_name, list of glob patterns)
    artifact_types: list[tuple[str, list[str]]] = [
        ("object files", ["*.obj", "*.o"]),
        ("static libraries", ["*.a"]),
        ("executables", ["*.exe"]),
        ("precompiled headers", ["*.pch"]),
        ("Windows static libraries", ["*.lib"]),
        ("Windows DLLs", ["*.dll"]),
        ("Linux shared objects", ["*.so"]),
        ("macOS dynamic libraries", ["*.dylib"]),
    ]

    results: dict[str, CleanupResult] = {}

    for display_name, patterns in artifact_types:
        deleted = 0
        failed = 0
        failed_files: list[str] = []

        for pattern in patterns:
            files = glob.glob(str(build_dir / "**" / pattern), recursive=True)
            for file_path in files:
                try:
                    Path(file_path).unlink()
                    deleted += 1
                except (OSError, IOError) as e:
                    failed += 1
                    failed_files.append(f"{file_path}: {e}")

        results[display_name] = CleanupResult(
            deleted=deleted, failed=failed, failed_files=failed_files
        )

    # Build summary message
    summary_parts: list[str] = []
    total_failed = 0
    for display_name, result in results.items():
        if result.deleted > 0 or result.failed > 0:
            summary_parts.append(f"{result.deleted} {display_name}")
            total_failed += result.failed

    if summary_parts:
        _ts_print(f"[MESON] 🗑️  Deleted {', '.join(summary_parts)}")

    # Report failures if any occurred
    if total_failed > 0:
        _ts_print(f"[MESON] ⚠️  Failed to delete {total_failed} files:")
        for display_name, result in results.items():
            for failed_file in result.failed_files[:5]:  # Limit to 5 per type
                _ts_print(f"[MESON]     {failed_file}")
            if len(result.failed_files) > 5:
                _ts_print(
                    f"[MESON]     ... and {len(result.failed_files) - 5} more {display_name}"
                )

    return results


def detect_system_llvm_tools() -> tuple[bool, bool]:
    """
    Detect if system has LLD and LLVM-AR that support thin archives.

    Returns:
        Tuple of (has_lld, has_llvm_ar)
    """
    has_lld = False
    has_llvm_ar = False

    # Check for system lld (not zig's bundled lld)
    # Try 'lld' first (created by workflow symlink or available by default)
    # Then try 'ld.lld' (Linux naming) as fallback
    for lld_cmd in ["lld", "ld.lld"]:
        try:
            result = subprocess.run(
                [lld_cmd, "--version"],
                capture_output=True,
                text=True,
                encoding="utf-8",
                errors="replace",
                timeout=5,
            )
            if result.returncode == 0:
                has_lld = True
                break
        except (subprocess.SubprocessError, FileNotFoundError):
            continue

    # Check for llvm-ar with thin archive support
    # Try unversioned first, then common versioned variants (llvm-ar-20, llvm-ar-19, etc.)
    for ar_cmd in ["llvm-ar", "llvm-ar-20", "llvm-ar-19", "llvm-ar-18"]:
        try:
            result = subprocess.run(
                [ar_cmd, "--help"],
                capture_output=True,
                encoding="utf-8",
                errors="replace",
                timeout=5,
            )
            if result.returncode == 0 and "thin" in result.stdout.lower():
                has_llvm_ar = True
                break
        except (subprocess.SubprocessError, FileNotFoundError):
            continue

    return has_lld, has_llvm_ar


def setup_meson_build(
    source_dir: Path,
    build_dir: Path,
    reconfigure: bool = False,
    debug: bool = False,
    check: bool = False,
    build_mode: Optional[str] = None,
    verbose: bool = False,
) -> bool:
    """
    Set up Meson build directory.

    Args:
        source_dir: Project root directory containing meson.build
        build_dir: Build output directory
        reconfigure: Force reconfiguration of existing build
        debug: Enable debug mode with full symbols and sanitizers (default: False)
        check: Enable IWYU static analysis (default: False)
        build_mode: Build mode ("quick", "debug", or "release"). If None, derived from debug flag.
        verbose: Show detailed output including toolchain info (default: False)

    Returns:
        True if setup successful, False otherwise
    """
    # Derive build_mode from debug flag if not explicitly provided
    if build_mode is None:
        build_mode = "debug" if debug else "quick"

    # Ensure debug flag matches build_mode for consistency
    if build_mode == "debug":
        debug = True
    elif build_mode in ("quick", "release"):
        debug = False
    # Create build directory if it doesn't exist
    build_dir.mkdir(parents=True, exist_ok=True)

    # Check if already configured
    meson_info = build_dir / "meson-info"
    already_configured = meson_info.exists()

    # ============================================================================
    # CRITICAL: Check meson version compatibility BEFORE proceeding
    # ============================================================================
    # Meson 1.9.x and 1.10.x create INCOMPATIBLE build directories!
    # A build directory created by one version cannot be used by another version.
    # If we detect a version mismatch, warn the user and suggest solutions.
    if already_configured:
        is_compatible, compatibility_message = check_meson_version_compatibility(
            build_dir
        )
        if not is_compatible:
            _ts_print("=" * 80)
            _ts_print("[MESON] ❌ MESON VERSION INCOMPATIBILITY DETECTED!")
            _ts_print("=" * 80)
            _ts_print(f"[MESON] {compatibility_message}")
            _ts_print("[MESON]")
            _ts_print("[MESON] This typically happens when:")
            _ts_print("[MESON]   1. Using a system meson instead of the venv meson")
            _ts_print("[MESON]   2. The pyproject.toml meson version was upgraded")
            _ts_print("[MESON]")
            _ts_print("[MESON] Solutions:")
            _ts_print(f"[MESON]   1. Delete the build directory: rm -rf {build_dir}")
            _ts_print("[MESON]   2. Always use 'uv run test.py' or 'uv run meson'")
            _ts_print("[MESON]   3. Never run bare 'meson' commands from the terminal")
            _ts_print("=" * 80)
            # Don't raise - let it fail naturally so user sees the full error
            # This warning helps them understand WHY it's failing

    # ============================================================================
    # THIN ARCHIVES: ENABLED FOR CLANG-TOOL-CHAIN
    # ============================================================================
    #
    # Background:
    # -----------
    # Thin archives (created with `ar crT` or `ar --thin`) store only file paths
    # instead of embedding object files directly. This provides significant benefits:
    # - Faster archive creation (no file copying)
    # - Smaller disk usage (object files not duplicated in archive)
    # - Faster incremental builds (archive update is just path table change)
    #
    # What Are Thin Archives?
    # -----------------------
    # Regular archive:  [header][obj1_data][obj2_data][obj3_data]  (~50MB)
    # Thin archive:     [header][path_to_obj1][path_to_obj2]...    (~50KB)
    #
    # Regular archives embed copies of all object files. Thin archives only
    # store file paths and let the linker read objects from their original
    # locations. This is safe because build systems control object lifetime.
    #
    # Current Implementation:
    # -----------------------
    # As of commit 6bbe2725e7, FastLED now uses clang-tool-chain instead of Zig.
    # The clang-tool-chain package provides LLVM-based tools (clang, lld, llvm-ar)
    # that fully support thin archives.
    #
    # Historical Context:
    # -------------------
    # Previously, thin archives were disabled because Zig's bundled linker (lld)
    # did not support the thin archive format (!<thin> header). This caused link
    # failures when llvm-ar created thin archives but Zig's lld tried to read them.
    #
    # With the migration to clang-tool-chain, all toolchain components (compiler,
    # linker, archiver) are now from the same LLVM toolchain and fully support
    # thin archives. There is no longer any compatibility issue.
    #
    # Benefits of Re-enabling:
    # ------------------------
    # ✓ Faster archive creation (~100-500ms saved for libfastled.a)
    # ✓ Smaller disk usage (~50MB → ~50KB for libfastled.a, 99% reduction)
    # ✓ Faster incremental builds (archive update is just path table change)
    # ✓ No compatibility issues with clang-tool-chain's LLVM toolchain
    #
    # Configuration:
    # --------------
    # Thin archives are automatically enabled with clang-tool-chain.
    # They can be manually disabled by setting FASTLED_DISABLE_THIN_ARCHIVES=1
    # environment variable if needed for debugging or compatibility testing.
    # ============================================================================

    disable_thin_archives = os.environ.get("FASTLED_DISABLE_THIN_ARCHIVES", "0") == "1"

    # Enable thin archives for clang-tool-chain (LLVM-based toolchain)
    # clang-tool-chain provides llvm-ar and lld that fully support thin archives
    use_thin_archives = not disable_thin_archives

    # Legacy detection for standalone LLVM tools (kept for diagnostic messages)
    has_lld, has_llvm_ar = detect_system_llvm_tools()

    # ============================================================================
    # CRITICAL: Check if thin archive configuration has changed
    # ============================================================================
    # When Meson is configured, it caches the AR tool path and flags in its
    # build files. If we previously configured with thin archives enabled but
    # now have them disabled (or vice versa), we MUST reconfigure Meson.
    # Otherwise, Meson will continue using the old AR tool/settings.
    #
    # We use a marker file to track the last successful thin archive setting.
    # If it doesn't match the current setting, we force a reconfigure.
    # ============================================================================
    thin_archive_marker = build_dir / ".thin_archive_config"
    source_files_marker = build_dir / ".source_files_hash"
    debug_marker = build_dir / ".debug_config"
    check_marker = build_dir / ".check_config"
    build_mode_marker = build_dir / ".build_mode_config"
    compiler_version_marker = build_dir / ".compiler_version_config"
    force_reconfigure = False
    compiler_version_changed = False  # Track for late check after compiler detection

    # Get current source file hash (used for change detection and saving after setup)
    current_source_hash, current_source_files = get_source_files_hash(source_dir)

    # ============================================================================
    # CONSOLIDATED MARKER CHECKING
    # ============================================================================
    # Instead of printing multiple "forcing reconfigure" messages for each missing
    # marker, we collect all reasons and print a single consolidated message.
    # This reduces noise when switching build modes or on first run.
    # ============================================================================
    reconfigure_reasons: list[str] = []  # Collect reasons for reconfiguration
    force_reconfigure_reason: Optional[str] = (
        None  # Primary reason for forced reconfigure
    )
    debug_changed = False
    build_mode_changed = False
    last_build_mode: Optional[str] = None

    if already_configured:
        # Check if thin archive setting has changed since last configure
        if thin_archive_marker.exists():
            try:
                last_thin_setting = thin_archive_marker.read_text().strip() == "True"
                if last_thin_setting != use_thin_archives:
                    reconfigure_reasons.append(
                        f"thin archive changed: {last_thin_setting} → {use_thin_archives}"
                    )
            except (OSError, IOError):
                reconfigure_reasons.append("thin archive marker unreadable")
        else:
            reconfigure_reasons.append("thin archive marker missing")

        # Check if source/test files have changed since last configure
        # This detects when files are added or removed, which requires reconfigure
        # CRITICAL: Test file tracking ensures organize_tests.py runs with current file list
        if current_source_hash:  # Only check if we successfully got the hash
            if source_files_marker.exists():
                try:
                    last_hash = source_files_marker.read_text().strip()
                    if last_hash != current_source_hash:
                        reconfigure_reasons.append("source/test files changed")
                except (OSError, IOError):
                    reconfigure_reasons.append("source files marker unreadable")
            else:
                reconfigure_reasons.append("source files marker missing")

        # Check if debug mode setting has changed since last configure
        # This is critical because debug mode changes compiler flags (sanitizers, optimization)
        # Using old object files with different flags causes linker errors
        # CRITICAL: When debug mode changes, we must delete all object files and archives
        # because they were compiled with different sanitizer/optimization flags
        if debug_marker.exists():
            try:
                last_debug_setting = debug_marker.read_text().strip() == "True"
                if last_debug_setting != debug:
                    reconfigure_reasons.append(
                        f"debug mode changed: {last_debug_setting} → {debug}"
                    )
                    debug_changed = True
            except (OSError, IOError):
                reconfigure_reasons.append("debug marker unreadable")
        else:
            reconfigure_reasons.append("debug marker missing")

        # Check if IWYU check setting has changed since last configure
        # When check mode changes, we must reconfigure to update the C++ compiler wrapper
        if check_marker.exists():
            try:
                last_check_setting = check_marker.read_text().strip() == "True"
                if last_check_setting != check:
                    reconfigure_reasons.append(
                        f"IWYU check mode changed: {last_check_setting} → {check}"
                    )
            except (OSError, IOError):
                reconfigure_reasons.append("check marker unreadable")
        else:
            reconfigure_reasons.append("check marker missing")

        # Check if build_mode setting has changed since last configure
        # CRITICAL: This detects quick <-> release transitions which both have debug=False
        # but have different optimization flags (-O0 vs -O3)
        # When build_mode changes, we must clean objects to avoid mixing different compiler flags
        if build_mode_marker.exists():
            try:
                last_build_mode = build_mode_marker.read_text().strip()
                if last_build_mode != build_mode:
                    reconfigure_reasons.append(
                        f"build mode changed: {last_build_mode} → {build_mode}"
                    )
                    build_mode_changed = True
            except (OSError, IOError):
                reconfigure_reasons.append("build_mode marker unreadable")
        else:
            reconfigure_reasons.append("build_mode marker missing")

        # Print consolidated reconfigure message if there are any reasons
        if reconfigure_reasons:
            force_reconfigure = True
            force_reconfigure_reason = "configuration markers changed"
            # For missing markers (common case on first run), use a simpler message
            missing_markers = [r for r in reconfigure_reasons if "missing" in r]
            changed_settings = [
                r
                for r in reconfigure_reasons
                if "missing" not in r and "unreadable" not in r
            ]
            unreadable_markers = [r for r in reconfigure_reasons if "unreadable" in r]

            if missing_markers and not changed_settings and not unreadable_markers:
                # All reasons are just missing markers - common on first run or mode switch
                num_missing = len(missing_markers)
                _ts_print(
                    f"[MESON] ℹ️  Build directory needs configuration ({num_missing} marker files missing)"
                )
            else:
                # Mix of reasons - show details
                _ts_print("[MESON] 🔄 Reconfiguration required:")
                for reason in reconfigure_reasons:
                    _ts_print(f"[MESON]     - {reason}")

        # CRITICAL: Delete all object files and archives when debug mode changes
        # Object files compiled with sanitizers cannot be linked without sanitizer runtime
        # We must force a complete rebuild with the new compiler flags
        if debug_changed:
            cleanup_build_artifacts(build_dir, "Debug mode changed")

        # CRITICAL: Delete all object files and archives when build_mode changes
        # Object files compiled with different optimization flags cannot be safely mixed
        # E.g., -O0 vs -O3 produces incompatible object code
        # Note: last_build_mode is guaranteed to be set if build_mode_changed is True
        if build_mode_changed:
            cleanup_build_artifacts(
                build_dir, f"Build mode changed ({last_build_mode} → {build_mode})"
            )

    # Check if any meson.build files are newer than build.ninja (requires reconfigure)
    build_ninja_path = build_dir / "build.ninja"
    meson_build_modified = False
    if already_configured and build_ninja_path.exists():
        try:
            build_ninja_mtime = build_ninja_path.stat().st_mtime
            # Check all meson.build files (root + subdirs)
            meson_build_files = [
                source_dir / "meson.build",
                source_dir / "tests" / "meson.build",
                source_dir / "examples" / "meson.build",
            ]
            for meson_file in meson_build_files:
                if meson_file.exists():
                    meson_file_mtime = meson_file.stat().st_mtime
                    if meson_file_mtime > build_ninja_mtime:
                        _ts_print(
                            f"[MESON] ⚠️  Detected modified meson.build: {meson_file.relative_to(source_dir)}"
                        )
                        _ts_print(
                            f"[MESON]     File mtime: {meson_file_mtime:.6f} > build.ninja mtime: {build_ninja_mtime:.6f}"
                        )
                        meson_build_modified = True
                        break
        except (OSError, IOError) as e:
            _ts_print(f"[MESON] Warning: Could not check meson.build timestamps: {e}")
            # If we can't check, assume modification to be safe
            meson_build_modified = True

    # Force reconfigure if meson.build files were modified
    # Note: The detection message is already printed above, no need for duplicate
    if meson_build_modified:
        force_reconfigure = True
        force_reconfigure_reason = "meson.build modified"

    # Check if test files have been added or removed (requires reconfigure)
    # This ensures Meson discovers new tests and removes deleted tests
    test_files_changed = False
    if already_configured:
        try:
            # Import test discovery functions
            tests_py_path = source_dir / "tests"
            if tests_py_path not in [Path(p) for p in sys.path]:
                sys.path.insert(0, str(tests_py_path))  # noqa: SPI001

            from discover_tests import (
                discover_test_files,  # type: ignore[import-not-found]
            )
            from test_config import (  # type: ignore[import-not-found]
                EXCLUDED_TEST_FILES,
                TEST_SUBDIRS,
            )

            # Get current test files
            tests_dir = source_dir / "tests"
            current_test_files: list[str] = sorted(
                discover_test_files(tests_dir, EXCLUDED_TEST_FILES, TEST_SUBDIRS)  # type: ignore[no-untyped-call]
            )

            # Check cached test file list
            # NOTE: We use try-except instead of exists() check to avoid TOCTOU race condition
            # This is more robust when multiple builds could run concurrently
            test_list_cache = build_dir / "test_list_cache.txt"
            cached_test_files: list[str] = []
            try:
                with open(test_list_cache, "r") as f:
                    cached_test_files = [
                        line.strip()
                        for line in f
                        if line.strip() and not line.startswith("#")
                    ]
            except FileNotFoundError:
                # File doesn't exist yet - treat as empty cache
                cached_test_files = []
            except (IOError, OSError) as e:
                # Corrupted or inaccessible - treat as stale
                _ts_print(f"[MESON] Warning: Could not read test cache: {e}")
                cached_test_files = []

            # Compare lists
            if current_test_files != cached_test_files:
                # Determine what changed
                added: set[str] = set(current_test_files) - set(cached_test_files)  # type: ignore[misc]
                removed: set[str] = set(cached_test_files) - set(current_test_files)  # type: ignore[misc]

                _print_warning("[MESON] ⚠️  Detected test file changes:")
                if added:
                    added_list = sorted(added)  # type: ignore[misc]
                    if len(added_list) > 10:
                        # Summarize long lists
                        _print_warning(
                            f"[MESON]     Added: {len(added_list)} test files"
                        )
                        _print_warning(
                            f"[MESON]     (First 10: {', '.join(added_list[:10])}...)"
                        )
                    else:
                        _print_warning(f"[MESON]     Added: {', '.join(added_list)}")
                if removed:
                    removed_list = sorted(removed)  # type: ignore[misc]
                    if len(removed_list) > 10:
                        _print_warning(
                            f"[MESON]     Removed: {len(removed_list)} test files"
                        )
                        _print_warning(
                            f"[MESON]     (First 10: {', '.join(removed_list[:10])}...)"
                        )
                    else:
                        _print_warning(
                            f"[MESON]     Removed: {', '.join(removed_list)}"
                        )

                test_files_changed = True

                # Update cache atomically using temp file + rename pattern
                # This prevents torn reads if another process is reading the cache
                temp_cache = test_list_cache.with_suffix(".tmp")
                try:
                    with open(temp_cache, "w") as f:
                        f.write(
                            "# Auto-generated test file list cache for Meson reconfiguration\n"
                        )
                        f.write(f"# Total tests: {len(current_test_files)}\n")  # type: ignore[misc]
                        f.write("# Generated by meson_runner.py\n\n")
                        for test_file in current_test_files:  # type: ignore[misc]
                            f.write(f"{test_file}\n")
                    # Atomic rename - either completes fully or not at all
                    temp_cache.replace(test_list_cache)
                except (OSError, IOError) as e:
                    _ts_print(
                        f"[MESON] Warning: Could not update test cache atomically: {e}"
                    )
                    # Clean up temp file if it exists
                    try:
                        temp_cache.unlink()
                    except (OSError, IOError):
                        pass

        except KeyboardInterrupt:
            handle_keyboard_interrupt_properly()
            raise
        except Exception as e:
            _ts_print(f"[MESON] Warning: Could not check test file changes: {e}")
            # On error, assume files may have changed to be safe
            test_files_changed = True

    # Force reconfigure if test files were added or removed
    # Note: The detection message is already printed above, no need for duplicate
    if test_files_changed:
        force_reconfigure = True
        force_reconfigure_reason = "test files changed"

    # Determine if we need to run meson setup/reconfigure
    # We skip meson setup only if already configured, not explicitly reconfiguring,
    # AND thin archive/source file settings haven't changed
    skip_meson_setup = already_configured and not reconfigure and not force_reconfigure

    # Declare native file path early (needed for meson commands)
    # The native file will be generated later after tool detection
    native_file_path = build_dir / "meson_native.txt"

    cmd: Optional[list[str]] = None
    if skip_meson_setup:
        # Build already configured, check wrappers below (message consolidated below)
        pass
    elif already_configured and (reconfigure or force_reconfigure):
        # Reconfigure existing build (explicitly requested or forced by config change)
        if force_reconfigure and force_reconfigure_reason:
            reason = force_reconfigure_reason
        elif force_reconfigure:
            reason = "configuration changed"
        else:
            reason = "explicitly requested"
        _ts_print(f"[MESON] Reconfiguring build directory ({reason}): {build_dir}")
        cmd = [
            get_meson_executable(),
            "setup",
            "--reconfigure",
            "--native-file",
            str(native_file_path),
            str(build_dir),
        ]
        # Always pass explicit build_mode to ensure meson uses correct optimization flags
        cmd.extend([f"-Dbuild_mode={build_mode}"])
    else:
        # Initial setup
        _ts_print(f"[MESON] Setting up build directory: {build_dir}")
        cmd = [
            get_meson_executable(),
            "setup",
            "--native-file",
            str(native_file_path),
            str(build_dir),
        ]
        # Always pass explicit build_mode to ensure meson uses correct optimization flags
        cmd.extend([f"-Dbuild_mode={build_mode}"])

    is_windows = sys.platform.startswith("win") or os.name == "nt"

    # Thin archives configuration (faster builds, smaller disk usage when supported)
    thin_flag = " --thin" if use_thin_archives else ""

    # Build config summary is shown in toolchain line and mode lines elsewhere
    # No need for separate config message here

    # Only show thin archives warning when explicitly disabled (rare case)
    if not use_thin_archives and os.environ.get("FASTLED_DISABLE_THIN_ARCHIVES"):
        _ts_print(
            "[MESON] ⚠️  Thin archives DISABLED (FASTLED_DISABLE_THIN_ARCHIVES set)"
        )

    # Check for obsolete zig wrapper artifacts before proceeding
    # These wrappers were used in the old zig-based compiler system and must be removed
    meson_dir = source_dir / ".build" / "meson"
    if meson_dir.exists():
        obsolete_wrappers = list(meson_dir.glob("zig-*-wrapper.exe"))
        if obsolete_wrappers:
            _ts_print("=" * 80)
            _ts_print("[MESON] ❌ ERROR: Obsolete zig wrapper artifacts detected!")
            _ts_print("[MESON]")
            _ts_print(
                "[MESON] Found old zig-based wrapper executables in .build/meson/ directory:"
            )
            for wrapper in obsolete_wrappers:
                _ts_print(f"[MESON]   - {wrapper.name}")
            _ts_print("[MESON]")
            _ts_print(
                "[MESON] These wrappers are from the old zig-based compiler system"
            )
            _ts_print("[MESON] and are no longer compatible with clang-tool-chain.")
            _ts_print("[MESON]")
            _ts_print("[MESON] To fix this issue, delete the .build/meson directory:")
            _ts_print("[MESON]   rm -rf .build/meson")
            _ts_print("[MESON]")
            _ts_print("[MESON] Then run your build command again. The system will use")
            _ts_print("[MESON] clang-tool-chain wrappers instead.")
            _ts_print("=" * 80)
            raise RuntimeError(
                "Obsolete zig wrapper artifacts detected in .build/meson/ directory. "
                "Delete .build/meson/ directory and try again."
            )

    # Get clang-tool-chain wrapper commands
    # Use the wrapper commands (clang-tool-chain-c/cpp) instead of raw clang binaries
    # The wrappers automatically handle GNU ABI setup on Windows
    # For sccache integration, use clang-tool-chain's built-in sccache wrappers

    # Check if sccache-enabled wrappers are available
    sccache_c_wrapper = shutil.which("clang-tool-chain-sccache-c")
    sccache_cpp_wrapper = shutil.which("clang-tool-chain-sccache-cpp")
    sccache_available = (
        sccache_c_wrapper is not None and sccache_cpp_wrapper is not None
    )
    use_sccache = sccache_available

    # Use sccache wrappers if available, otherwise fall back to plain wrappers
    if use_sccache:
        clang_wrapper = sccache_c_wrapper
        clangxx_wrapper = sccache_cpp_wrapper
    else:
        clang_wrapper = shutil.which("clang-tool-chain-c")
        clangxx_wrapper = shutil.which("clang-tool-chain-cpp")

    llvm_ar_wrapper = shutil.which("clang-tool-chain-ar")

    if not clang_wrapper or not clangxx_wrapper or not llvm_ar_wrapper:
        _ts_print("[MESON] ERROR: clang-tool-chain wrapper commands not found in PATH")
        _ts_print(
            "[MESON] Install clang-tool-chain with: uv pip install clang-tool-chain"
        )
        _ts_print("[MESON] Missing commands:")
        if not clang_wrapper:
            _ts_print("[MESON]   - clang-tool-chain-c (or clang-tool-chain-sccache-c)")
        if not clangxx_wrapper:
            _ts_print(
                "[MESON]   - clang-tool-chain-cpp (or clang-tool-chain-sccache-cpp)"
            )
        if not llvm_ar_wrapper:
            _ts_print("[MESON]   - clang-tool-chain-ar")
        raise RuntimeError("clang-tool-chain wrapper commands not available")

    # Get compiler version for consolidated output
    current_compiler_version = get_compiler_version(clangxx_wrapper)

    # Consolidated single-line toolchain summary (verbose mode only)
    # Before: 7 lines of compiler details
    # After: "Toolchain: clang 21.1.5 + sccache"
    if verbose:
        if use_sccache:
            print(f"Toolchain: {current_compiler_version} + sccache")
        else:
            print(f"Toolchain: {current_compiler_version}")

    # Late-stage compiler version check (requires compiler detection above)
    # This check is separate because we need the detected compiler version
    if already_configured:
        compiler_version_reason: Optional[str] = None
        if compiler_version_marker.exists():
            try:
                last_compiler_version = compiler_version_marker.read_text().strip()
                if last_compiler_version != current_compiler_version:
                    compiler_version_reason = f"compiler version changed: {last_compiler_version} → {current_compiler_version}"
                    compiler_version_changed = True
            except (OSError, IOError):
                compiler_version_reason = "compiler version marker unreadable"
        else:
            compiler_version_reason = "compiler version marker missing"

        if compiler_version_reason:
            # Only print if we haven't already printed a reconfigure message
            if not force_reconfigure:
                _ts_print(
                    f"[MESON] 🔄 Reconfiguration required: {compiler_version_reason}"
                )
            force_reconfigure = True
            force_reconfigure_reason = compiler_version_reason
            skip_meson_setup = False

        # CRITICAL: Delete all object files, archives, and PCH when compiler version changes
        # Objects compiled with a different compiler version can have incompatible ABI
        if compiler_version_changed:
            cleanup_build_artifacts(build_dir, "Compiler version changed")

        # CRITICAL: If compiler version forced a reconfigure, we need to update cmd
        # The original cmd assignment was based on pre-compiler-check values
        if force_reconfigure and cmd is None:
            _ts_print(f"[MESON] Reconfiguring build directory: {build_dir}")
            cmd = [
                get_meson_executable(),
                "setup",
                "--reconfigure",
                "--native-file",
                str(native_file_path),
                str(build_dir),
            ]
            cmd.extend([f"-Dbuild_mode={build_mode}"])

    # Generate native file for Meson that persists tool configuration across regenerations
    # When Meson regenerates (e.g., when ninja detects meson.build changes),
    # environment variables are lost. Native file ensures tools are configured.
    try:
        # Use clang-tool-chain wrapper commands directly (they already include sccache if requested)
        # Do NOT wrap with external sccache - clang-tool-chain handles that internally
        c_compiler = f"['{clang_wrapper}']"

        # IWYU wrapper is NOT added to native file (causes meson probe issues)
        # Instead, IWYU is injected via CXX environment variable during ninja phase
        # This allows meson setup to probe the compiler without IWYU interference
        # This allows meson setup to probe the compiler without IWYU interference
        cpp_compiler = f"['{clangxx_wrapper}']"

        # Store check flag for later use during compilation phase
        # IWYU will be injected via environment if check=True

        # Use llvm-ar wrapper from clang-tool-chain
        ar_tool = f"['{llvm_ar_wrapper}']"

        # Detect platform for native file
        is_darwin = sys.platform == "darwin"

        # Detect CPU architecture
        machine = platform.machine().lower()
        if machine in ("x86_64", "amd64"):
            cpu_family = "x86_64"
            cpu = "x86_64"
        elif machine in ("arm64", "aarch64"):
            cpu_family = "aarch64"
            cpu = "aarch64"
        else:
            cpu_family = "x86_64"  # Default fallback
            cpu = "x86_64"

        if is_windows:
            host_system = "windows"
        elif is_darwin:
            host_system = "darwin"
        else:
            host_system = "linux"

        native_file_content = f"""# ============================================================================
# Meson Native Build Configuration for FastLED (Auto-generated)
# ============================================================================
# This file is auto-generated by meson_runner.py to configure tool paths.
# It persists across build regenerations when ninja detects meson.build changes.
#
# IMPORTANT: LLD linker is forced via -fuse-ld=lld in meson.build link_args.
# This ensures consistent linker behavior across Windows, Linux, and macOS.

[binaries]
c = {c_compiler}
cpp = {cpp_compiler}
ar = {ar_tool}

[host_machine]
system = '{host_system}'
cpu_family = '{cpu_family}'
cpu = '{cpu}'
endian = 'little'

[properties]
# No additional properties needed - compiler flags are in meson.build
"""
        native_file_changed = write_if_different(native_file_path, native_file_content)

        if native_file_changed:
            _ts_print(f"[MESON] Regenerated native file: {native_file_path}")
    except (OSError, IOError) as e:
        _ts_print(f"[MESON] Warning: Could not write native file: {e}", file=sys.stderr)

    env = os.environ.copy()
    env["CC"] = clang_wrapper
    env["CXX"] = clangxx_wrapper
    env["AR"] = llvm_ar_wrapper

    # sccache status already shown in toolchain summary above

    # If we're skipping meson setup (already configured), check for thin archive conflicts
    if skip_meson_setup:
        # CRITICAL FIX: Check if libfastled.a exists as a thin archive
        # If we've disabled thin archives but a thin archive exists from a previous build,
        # we must delete it to force a rebuild. Otherwise the linker will fail.
        libfastled_a = build_dir / "libfastled.a"
        if libfastled_a.exists():
            try:
                # Read first 8 bytes to check for thin archive magic header
                with open(libfastled_a, "rb") as f:
                    header = f.read(8)
                    is_thin_archive = header == b"!<thin>\n"

                if is_thin_archive and not use_thin_archives:
                    # Thin archive exists but we've disabled thin archives
                    # Delete it to force rebuild with regular archive
                    _ts_print("[MESON] ⚠️  Detected thin archive from previous build")
                    _ts_print(
                        "[MESON] 🗑️  Deleting libfastled.a to force rebuild with regular archive"
                    )
                    libfastled_a.unlink()
                elif not is_thin_archive and use_thin_archives:
                    # Regular archive exists but we've enabled thin archives
                    # Delete it to force rebuild with thin archive
                    _ts_print("[MESON] ℹ️  Detected regular archive from previous build")
                    _ts_print(
                        "[MESON] 🗑️  Deleting libfastled.a to force rebuild with thin archive"
                    )
                    libfastled_a.unlink()
            except (OSError, IOError) as e:
                # If we can't read/delete, that's okay - build will handle it
                _ts_print(f"[MESON] Warning: Could not check/delete libfastled.a: {e}")
                pass

        # CRITICAL: Create all marker files if they don't exist (migration from older code)
        # This ensures cache invalidation works correctly on first run after update
        # Without this, marker files would never be created in the skip path
        # NOTE: All marker writes use atomic_write_text to prevent corruption on crash
        if not build_mode_marker.exists():
            try:
                atomic_write_text(build_mode_marker, build_mode)
                _ts_print(f"[MESON] ✅ Created build_mode marker: {build_mode}")
            except (OSError, IOError) as e:
                _ts_print(f"[MESON] Warning: Could not write build_mode marker: {e}")

        if not thin_archive_marker.exists():
            try:
                atomic_write_text(thin_archive_marker, str(use_thin_archives))
                _ts_print(
                    f"[MESON] ✅ Created thin archive marker: {use_thin_archives}"
                )
            except (OSError, IOError) as e:
                _ts_print(f"[MESON] Warning: Could not write thin archive marker: {e}")

        if not debug_marker.exists():
            try:
                atomic_write_text(debug_marker, str(debug))
                _ts_print(f"[MESON] ✅ Created debug marker: {debug}")
            except (OSError, IOError) as e:
                _ts_print(f"[MESON] Warning: Could not write debug marker: {e}")

        if not check_marker.exists():
            try:
                atomic_write_text(check_marker, str(check))
                _ts_print(f"[MESON] ✅ Created check marker: {check}")
            except (OSError, IOError) as e:
                _ts_print(f"[MESON] Warning: Could not write check marker: {e}")

        if current_source_hash and not source_files_marker.exists():
            try:
                atomic_write_text(source_files_marker, current_source_hash)
                _ts_print(
                    f"[MESON] ✅ Created source/test files marker ({len(current_source_files)} files)"
                )
            except (OSError, IOError) as e:
                _ts_print(
                    f"[MESON] Warning: Could not write source/test files marker: {e}"
                )

        if not compiler_version_marker.exists():
            try:
                atomic_write_text(compiler_version_marker, current_compiler_version)
                _ts_print(
                    f"[MESON] ✅ Created compiler version marker: {current_compiler_version}"
                )
            except (OSError, IOError) as e:
                _ts_print(
                    f"[MESON] Warning: Could not write compiler version marker: {e}"
                )

        return True

    # Run meson setup using RunningProcess for proper streaming output
    assert cmd is not None, "cmd should be set when not skipping meson setup"
    _print_banner("MESON CONFIGURATION", "⚙️")
    try:
        # Disable sccache during Meson setup phase to avoid probe command conflicts
        # sccache tries to detect compilers with -E flag which confuses Zig's command structure
        # This will be unset for the actual ninja build phase
        setup_env = env.copy()
        setup_env["FASTLED_DISABLE_SCCACHE"] = "1"

        proc = RunningProcess(
            cmd,
            cwd=source_dir,
            timeout=600,
            auto_run=True,
            check=False,  # We'll check returncode manually
            env=setup_env,
            output_formatter=TimestampFormatter(),
        )

        returncode = proc.wait(echo=True)

        if returncode != 0:
            _ts_print(
                f"[MESON] Setup failed with return code {returncode}", file=sys.stderr
            )
            return False

        _ts_print(f"[MESON] Setup successful")

        # Write marker files to track settings for future runs
        # NOTE: All marker writes use atomic_write_text to prevent corruption on crash
        try:
            atomic_write_text(thin_archive_marker, str(use_thin_archives))
            _ts_print(
                f"[MESON] ✅ Saved thin archive configuration: {use_thin_archives}"
            )
        except (OSError, IOError) as e:
            # Not critical if marker file write fails
            _ts_print(f"[MESON] Warning: Could not write thin archive marker: {e}")

        # Write marker file to track debug setting for future runs
        try:
            atomic_write_text(debug_marker, str(debug))
            _ts_print(f"[MESON] ✅ Saved debug configuration: {debug}")
        except (OSError, IOError) as e:
            # Not critical if marker file write fails
            _ts_print(f"[MESON] Warning: Could not write debug marker: {e}")

        # Write marker file to track check setting for future runs
        try:
            atomic_write_text(check_marker, str(check))
            _ts_print(f"[MESON] ✅ Saved check configuration: {check}")
        except (OSError, IOError) as e:
            # Not critical if marker file write fails
            _ts_print(f"[MESON] Warning: Could not write check marker: {e}")

        # Write marker file to track build_mode setting for future runs
        # CRITICAL: This enables detection of quick <-> release transitions
        try:
            atomic_write_text(build_mode_marker, build_mode)
            _ts_print(f"[MESON] ✅ Saved build_mode configuration: {build_mode}")
        except (OSError, IOError) as e:
            # Not critical if marker file write fails
            _ts_print(f"[MESON] Warning: Could not write build_mode marker: {e}")

        # Write marker file to track source/test file list for future runs
        if current_source_hash:
            try:
                atomic_write_text(source_files_marker, current_source_hash)
                _ts_print(
                    f"[MESON] ✅ Saved source/test files hash ({len(current_source_files)} files)"
                )
            except (OSError, IOError) as e:
                # Not critical if marker file write fails
                _ts_print(
                    f"[MESON] Warning: Could not write source/test files marker: {e}"
                )

        # Write marker file to track compiler version for future runs
        # CRITICAL: This enables detection of compiler upgrades that invalidate PCH/objects
        try:
            atomic_write_text(compiler_version_marker, current_compiler_version)
            _ts_print(f"[MESON] ✅ Saved compiler version: {current_compiler_version}")
        except (OSError, IOError) as e:
            # Not critical if marker file write fails
            _ts_print(f"[MESON] Warning: Could not write compiler version marker: {e}")

        return True

    except KeyboardInterrupt:
        handle_keyboard_interrupt_properly()
        raise
    except Exception as e:
        _ts_print(f"[MESON] Setup failed with exception: {e}", file=sys.stderr)
        return False


def perform_ninja_maintenance(build_dir: Path) -> bool:
    """
    Perform periodic maintenance on Ninja dependency database.

    This function runs 'ninja -t recompact' to optimize and repair the
    .ninja_deps file. It only runs once per day (tracked via marker file)
    to avoid unnecessary overhead.

    Args:
        build_dir: Meson build directory containing .ninja_deps

    Returns:
        True if maintenance was performed or skipped successfully, False on error
    """
    # Create marker file to track last maintenance time
    marker_file = build_dir / ".ninja_deps_maintenance"

    # Check if maintenance was recently performed (within last 24 hours)
    if marker_file.exists():
        try:
            last_maintenance = marker_file.stat().st_mtime
            time_since_maintenance = time.time() - last_maintenance
            hours_since_maintenance = time_since_maintenance / 3600

            # Skip if maintenance was done within last 24 hours
            if hours_since_maintenance < 24:
                return True
        except (OSError, IOError):
            # If we can't read the marker, proceed with maintenance
            pass

    _ts_print("[MESON] 🔧 Performing periodic Ninja dependency database maintenance...")

    try:
        # Run ninja -t recompact to optimize dependency database
        repair_proc = RunningProcess(
            ["ninja", "-C", str(build_dir), "-t", "recompact"],
            timeout=60,
            auto_run=True,
            check=False,
            env=os.environ.copy(),
            output_formatter=TimestampFormatter(),
        )
        returncode = repair_proc.wait(echo=False)

        if returncode == 0:
            _ts_print(
                "[MESON] ✓ Dependency database maintenance completed successfully"
            )

            # Update marker file timestamp
            try:
                marker_file.touch()
            except (OSError, IOError):
                # Not critical if marker update fails
                pass

            return True
        else:
            _ts_print(
                "[MESON] ⚠️  Maintenance completed with warnings (non-fatal)",
                file=sys.stderr,
            )
            return True  # Non-fatal, continue anyway

    except KeyboardInterrupt:
        handle_keyboard_interrupt_properly()
        raise
    except Exception as e:
        _ts_print(
            f"[MESON] ⚠️  Maintenance failed with exception: {e} (non-fatal)",
            file=sys.stderr,
        )
        return True  # Non-fatal, continue anyway
