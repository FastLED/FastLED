#!/usr/bin/env python3
"""Meson build system integration for FastLED unit tests."""

import hashlib
import json
import os
import re
import shutil
import subprocess
import sys
import time
from collections.abc import Callable
from dataclasses import dataclass
from pathlib import Path
from typing import Optional

from running_process import RunningProcess

from ci.util.build_lock import libfastled_build_lock
from ci.util.output_formatter import TimestampFormatter
from ci.util.timestamp_print import ts_print as _ts_print


@dataclass
class MesonTestResult:
    """Result from running Meson build and tests"""

    success: bool
    duration: float  # Total duration in seconds
    num_tests_run: int = 0  # Number of tests executed
    num_tests_passed: int = 0  # Number of tests that passed
    num_tests_failed: int = 0  # Number of tests that failed


def check_meson_installed() -> bool:
    """Check if Meson is installed and accessible."""
    try:
        result = subprocess.run(
            ["meson", "--version"],
            capture_output=True,
            text=True,
            encoding="utf-8",
            errors="replace",
            timeout=5,
        )
        return result.returncode == 0
    except (subprocess.SubprocessError, FileNotFoundError):
        return False


def get_fuzzy_test_candidates(build_dir: Path, test_name: str) -> list[str]:
    """
    Get fuzzy match candidates for a test name using meson introspect.

    This queries the Meson build system for all test targets containing the
    test name substring, enabling fuzzy matching even when the build directory
    is freshly created and no executables exist yet.

    Args:
        build_dir: Meson build directory
        test_name: Test name to search for (e.g., "async")

    Returns:
        List of matching target names (e.g., ["fl_async", "fl_async_logger"])
    """
    if not build_dir.exists():
        return []

    try:
        # Query Meson for all targets
        result = subprocess.run(
            ["meson", "introspect", str(build_dir), "--targets"],
            capture_output=True,
            text=True,
            encoding="utf-8",
            errors="replace",
            timeout=10,
            check=False,
        )

        if result.returncode != 0:
            return []

        # Parse JSON output
        targets = json.loads(result.stdout)

        # Filter targets:
        # 1. Must be an executable (type == "executable")
        # 2. Must be in tests/ directory
        # 3. Must contain the test name substring (case-insensitive)
        test_name_lower = test_name.lower()
        candidates: list[str] = []

        for target in targets:
            if target.get("type") != "executable":
                continue

            target_name = target.get("name", "")
            # Check if this is a test executable (defined in tests/ directory)
            defined_in = target.get("defined_in", "")
            if "tests" not in defined_in.lower():
                continue

            # Check if target name contains the test name substring
            if test_name_lower in target_name.lower():
                candidates.append(target_name)

        return candidates

    except (subprocess.SubprocessError, json.JSONDecodeError, OSError) as e:
        _ts_print(f"[MESON] Warning: Failed to get fuzzy test candidates: {e}")
        return []


def get_source_files_hash(source_dir: Path) -> tuple[str, list[str]]:
    """
    Get hash of all .cpp/.h source and test files in src/ and tests/ directories.

    This detects when source or test files are added or removed, which requires
    Meson reconfiguration to update the build graph (especially for test discovery).

    Args:
        source_dir: Project root directory

    Returns:
        Tuple of (hash_string, sorted_file_list)
    """
    try:
        # Recursively discover all .cpp and .h files in src/ directory
        src_path = source_dir / "src"
        source_files = sorted(
            str(p.relative_to(source_dir)) for p in src_path.rglob("*.cpp")
        )
        source_files.extend(
            sorted(str(p.relative_to(source_dir)) for p in src_path.rglob("*.h"))
        )

        # Recursively discover all .cpp and .h files in tests/ directory
        # This is CRITICAL for detecting when tests are added or removed
        tests_path = source_dir / "tests"
        if tests_path.exists():
            source_files.extend(
                sorted(
                    str(p.relative_to(source_dir)) for p in tests_path.rglob("*.cpp")
                )
            )
            source_files.extend(
                sorted(str(p.relative_to(source_dir)) for p in tests_path.rglob("*.h"))
            )

        # Re-sort the combined list for consistent ordering
        source_files = sorted(source_files)

        # Hash the list of file paths (not contents - just detect add/remove)
        hasher = hashlib.sha256()
        for file_path in source_files:
            hasher.update(file_path.encode("utf-8"))
            hasher.update(b"\n")  # Separator

        return (hasher.hexdigest(), source_files)

    except Exception as e:
        _ts_print(f"[MESON] Warning: Failed to get source file hash: {e}")
        return ("", [])


def write_if_different(path: Path, content: str, mode: Optional[int] = None) -> bool:
    """
    Write file only if content differs from existing file.

    This prevents unnecessary file modifications that would trigger Meson
    regeneration. If the file doesn't exist or content has changed, writes
    the new content.

    Args:
        path: Path to file to write
        content: Content to write
        mode: Optional file permissions (Unix only)

    Returns:
        True if file was written (created or modified), False if unchanged
    """
    try:
        if path.exists():
            existing_content = path.read_text(encoding="utf-8")
            if existing_content == content:
                return False  # Content unchanged, skip write

        # Content changed or file doesn't exist - write it
        path.write_text(content, encoding="utf-8")

        # Set permissions if specified (Unix only)
        if mode is not None and not sys.platform.startswith("win"):
            path.chmod(mode)

        return True
    except (OSError, IOError) as e:
        # On error, try to write anyway - caller will handle failures
        _ts_print(f"[MESON] Warning: Error checking file {path}: {e}")
        path.write_text(content, encoding="utf-8")
        if mode is not None and not sys.platform.startswith("win"):
            path.chmod(mode)
        return True


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
    unity: bool = False,
    debug: bool = False,
) -> bool:
    """
    Set up Meson build directory.

    Args:
        source_dir: Project root directory containing meson.build
        build_dir: Build output directory
        reconfigure: Force reconfiguration of existing build
        unity: Enable unity builds (default: False)
        debug: Enable debug mode with full symbols and sanitizers (default: False)

    Returns:
        True if setup successful, False otherwise
    """
    # Create build directory if it doesn't exist
    build_dir.mkdir(parents=True, exist_ok=True)

    # Check if already configured
    meson_info = build_dir / "meson-info"
    already_configured = meson_info.exists()

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
    unity_marker = build_dir / ".unity_config"
    source_files_marker = build_dir / ".source_files_hash"
    debug_marker = build_dir / ".debug_config"
    force_reconfigure = False

    # Get current source file hash (used for change detection and saving after setup)
    current_source_hash, current_source_files = get_source_files_hash(source_dir)

    if already_configured:
        # Check if thin archive setting has changed since last configure
        if thin_archive_marker.exists():
            try:
                last_thin_setting = thin_archive_marker.read_text().strip() == "True"
                if last_thin_setting != use_thin_archives:
                    _ts_print(
                        f"[MESON] ⚠️  Thin archive setting changed: {last_thin_setting} → {use_thin_archives}"
                    )
                    _ts_print(
                        "[MESON] 🔄 Forcing reconfigure to update AR tool configuration"
                    )
                    force_reconfigure = True
            except (OSError, IOError):
                # If we can't read the marker, force reconfigure to be safe
                _ts_print(
                    "[MESON] ⚠️  Could not read thin archive marker, forcing reconfigure"
                )
                force_reconfigure = True
        else:
            # No marker file exists from previous configure - create one after setup
            # This can happen on first run after updating to this version
            _ts_print(
                "[MESON] ℹ️  No thin archive marker found, will create after setup"
            )

        # Check if unity setting has changed since last configure
        unity_changed = False
        if unity_marker.exists():
            try:
                last_unity_setting = unity_marker.read_text().strip() == "True"
                if last_unity_setting != unity:
                    _ts_print(
                        f"[MESON] ⚠️  Unity build setting changed: {last_unity_setting} → {unity}"
                    )
                    _ts_print("[MESON] 🔄 Forcing reconfigure to update build targets")
                    force_reconfigure = True
                    unity_changed = True
            except (OSError, IOError):
                # If we can't read the marker, force reconfigure to be safe
                _ts_print("[MESON] ⚠️  Could not read unity marker, forcing reconfigure")
                force_reconfigure = True
        else:
            # No marker file exists from previous configure
            # Force reconfigure to ensure Meson is configured with correct unity setting
            _ts_print("[MESON] ℹ️  No unity marker found, forcing reconfigure")
            force_reconfigure = True

        # CRITICAL: Delete thin archives when unity build settings change
        # Thin archives store file paths to object files. When unity builds change,
        # object file names change (e.g., src_foo.cpp.obj → fastled-unity0.cpp.obj),
        # causing thin archives to reference non-existent files.
        # NOTE: We must delete archives regardless of current thin_archives setting,
        # because old thin archives from previous builds may still exist.
        if unity_changed:
            # Delete main FastLED library archive
            libfastled_a = build_dir / "libfastled.a"
            if libfastled_a.exists():
                try:
                    _ts_print(
                        "[MESON] 🗑️  Deleting libfastled.a due to unity build change"
                    )
                    libfastled_a.unlink()
                except (OSError, IOError) as e:
                    _ts_print(f"[MESON] Warning: Could not delete libfastled.a: {e}")

            # Delete all platforms_shared static library archives
            # These may be thin archives that reference individual object files
            import glob

            platforms_shared_archives = glob.glob(
                str(build_dir / "libplatforms_shared_*.a")
            )
            if platforms_shared_archives:
                deleted_count = 0
                for archive_path in platforms_shared_archives:
                    try:
                        Path(archive_path).unlink()
                        deleted_count += 1
                    except (OSError, IOError) as e:
                        _ts_print(
                            f"[MESON] Warning: Could not delete {archive_path}: {e}"
                        )
                if deleted_count > 0:
                    _ts_print(
                        f"[MESON] 🗑️  Deleted {deleted_count} platforms_shared archives due to unity build change"
                    )

        # Check if source/test files have changed since last configure
        # This detects when files are added or removed, which requires reconfigure
        # CRITICAL: Test file tracking ensures organize_tests.py runs with current file list
        if current_source_hash:  # Only check if we successfully got the hash
            if source_files_marker.exists():
                try:
                    last_hash = source_files_marker.read_text().strip()
                    if last_hash != current_source_hash:
                        _ts_print(
                            "[MESON] ⚠️  Source/test file list changed (files added/removed)"
                        )
                        _ts_print(
                            "[MESON] 🔄 Forcing reconfigure to update build graph and test discovery"
                        )
                        force_reconfigure = True
                except (OSError, IOError):
                    # If we can't read the marker, force reconfigure to be safe
                    _ts_print(
                        "[MESON] ⚠️  Could not read source/test files marker, forcing reconfigure"
                    )
                    force_reconfigure = True
            else:
                # No marker file exists from previous configure
                # Force reconfigure to ensure build graph matches current source files
                _ts_print(
                    "[MESON] ℹ️  No source/test files marker found, forcing reconfigure"
                )
                force_reconfigure = True

        # Check if debug mode setting has changed since last configure
        # This is critical because debug mode changes compiler flags (sanitizers, optimization)
        # Using old object files with different flags causes linker errors
        # CRITICAL: When debug mode changes, we must delete all object files and archives
        # because they were compiled with different sanitizer/optimization flags
        debug_changed = False
        if debug_marker.exists():
            try:
                last_debug_setting = debug_marker.read_text().strip() == "True"
                if last_debug_setting != debug:
                    _ts_print(
                        f"[MESON] ⚠️  Debug mode changed: {last_debug_setting} → {debug}"
                    )
                    _ts_print(
                        "[MESON] 🔄 Forcing reconfigure to update sanitizer/optimization flags"
                    )
                    force_reconfigure = True
                    debug_changed = True
            except (OSError, IOError):
                # If we can't read the marker, force reconfigure to be safe
                _ts_print("[MESON] ⚠️  Could not read debug marker, forcing reconfigure")
                force_reconfigure = True
        else:
            # No marker file exists from previous configure
            # This can happen on first run after updating to this version
            _ts_print("[MESON] ℹ️  No debug marker found, will create after setup")

        # CRITICAL: Delete all object files and archives when debug mode changes
        # Object files compiled with sanitizers cannot be linked without sanitizer runtime
        # We must force a complete rebuild with the new compiler flags
        if debug_changed:
            import glob
            import shutil

            _ts_print(
                "[MESON] 🗑️  Debug mode changed - cleaning all object files and archives"
            )

            # Delete all .obj and .o files (object files)
            obj_files = glob.glob(str(build_dir / "**" / "*.obj"), recursive=True)
            obj_files.extend(glob.glob(str(build_dir / "**" / "*.o"), recursive=True))
            deleted_obj_count = 0
            for obj_file in obj_files:
                try:
                    Path(obj_file).unlink()
                    deleted_obj_count += 1
                except (OSError, IOError) as e:
                    _ts_print(f"[MESON] Warning: Could not delete {obj_file}: {e}")

            # Delete all .a files (static libraries)
            archive_files = glob.glob(str(build_dir / "**" / "*.a"), recursive=True)
            deleted_archive_count = 0
            for archive_file in archive_files:
                try:
                    Path(archive_file).unlink()
                    deleted_archive_count += 1
                except (OSError, IOError) as e:
                    _ts_print(f"[MESON] Warning: Could not delete {archive_file}: {e}")

            # Delete all .exe files (test executables)
            exe_files = glob.glob(str(build_dir / "**" / "*.exe"), recursive=True)
            deleted_exe_count = 0
            for exe_file in exe_files:
                try:
                    Path(exe_file).unlink()
                    deleted_exe_count += 1
                except (OSError, IOError) as e:
                    _ts_print(f"[MESON] Warning: Could not delete {exe_file}: {e}")

            _ts_print(
                f"[MESON] 🗑️  Deleted {deleted_obj_count} object files, {deleted_archive_count} archives, {deleted_exe_count} executables"
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
    if meson_build_modified:
        _ts_print("[MESON] 🔄 Forcing reconfigure due to meson.build modifications")
        force_reconfigure = True

    # Check if test files have been added or removed (requires reconfigure)
    # This ensures Meson discovers new tests and removes deleted tests
    test_files_changed = False
    if already_configured:
        try:
            # Import test discovery functions
            tests_py_path = source_dir / "tests"
            if tests_py_path not in [Path(p) for p in sys.path]:
                sys.path.insert(0, str(tests_py_path))

            from discover_tests import (  # type: ignore[import-not-found]
                discover_test_files,  # type: ignore[misc]
            )
            from test_config import (  # type: ignore[import-not-found]
                EXCLUDED_TEST_FILES,  # type: ignore[misc]
                TEST_SUBDIRS,  # type: ignore[misc]
            )

            # Get current test files
            tests_dir = source_dir / "tests"
            current_test_files: list[str] = sorted(  # type: ignore[misc]
                discover_test_files(tests_dir, EXCLUDED_TEST_FILES, TEST_SUBDIRS)  # type: ignore[misc]
            )

            # Check cached test file list
            test_list_cache = build_dir / "test_list_cache.txt"
            cached_test_files: list[str] = []
            if test_list_cache.exists():
                with open(test_list_cache, "r") as f:
                    cached_test_files = [
                        line.strip()
                        for line in f
                        if line.strip() and not line.startswith("#")
                    ]

            # Compare lists
            if current_test_files != cached_test_files:
                # Determine what changed
                added: set[str] = set(current_test_files) - set(cached_test_files)  # type: ignore[misc]
                removed: set[str] = set(cached_test_files) - set(current_test_files)  # type: ignore[misc]

                _ts_print("[MESON] ⚠️  Detected test file changes:")
                if added:
                    _ts_print(f"[MESON]     Added: {', '.join(sorted(added))}")  # type: ignore[misc]
                if removed:
                    _ts_print(f"[MESON]     Removed: {', '.join(sorted(removed))}")  # type: ignore[misc]

                test_files_changed = True

                # Update cache
                with open(test_list_cache, "w") as f:
                    f.write(
                        "# Auto-generated test file list cache for Meson reconfiguration\n"
                    )
                    f.write(f"# Total tests: {len(current_test_files)}\n")  # type: ignore[misc]
                    f.write("# Generated by meson_runner.py\n\n")
                    for test_file in current_test_files:  # type: ignore[misc]
                        f.write(f"{test_file}\n")

        except Exception as e:
            _ts_print(f"[MESON] Warning: Could not check test file changes: {e}")
            # On error, assume files may have changed to be safe
            test_files_changed = True

    # Force reconfigure if test files were added or removed
    if test_files_changed:
        _ts_print("[MESON] 🔄 Forcing reconfigure due to test file additions/removals")
        force_reconfigure = True

    # Determine if we need to run meson setup/reconfigure
    # We skip meson setup only if already configured, not explicitly reconfiguring,
    # AND thin archive/unity/source file settings haven't changed
    skip_meson_setup = already_configured and not reconfigure and not force_reconfigure

    # Declare native file path early (needed for meson commands)
    # The native file will be generated later after tool detection
    native_file_path = build_dir / "meson_native.txt"

    cmd: Optional[list[str]] = None
    if skip_meson_setup:
        # Build already configured, check wrappers below
        _ts_print(f"[MESON] Build directory already configured: {build_dir}")
    elif already_configured and (reconfigure or force_reconfigure):
        # Reconfigure existing build (explicitly requested or forced by thin archive change)
        reason = (
            "forced by thin archive change"
            if force_reconfigure
            else "explicitly requested"
        )
        _ts_print(f"[MESON] Reconfiguring build directory ({reason}): {build_dir}")
        cmd = [
            "meson",
            "setup",
            "--reconfigure",
            "--native-file",
            str(native_file_path),
            str(build_dir),
        ]
        if unity:
            cmd.extend(["-Dunity=on"])
        if debug:
            cmd.extend(["-Dbuild_mode=debug"])
    else:
        # Initial setup
        _ts_print(f"[MESON] Setting up build directory: {build_dir}")
        cmd = ["meson", "setup", "--native-file", str(native_file_path), str(build_dir)]
        if unity:
            cmd.extend(["-Dunity=on"])
        if debug:
            cmd.extend(["-Dbuild_mode=debug"])

    # Print unity build status
    if unity:
        _ts_print("[MESON] ✅ Unity builds ENABLED (--unity flag)")
    else:
        _ts_print("[MESON] Unity builds disabled (use --unity to enable)")

    # Print debug mode status
    if debug:
        _ts_print("[MESON] ✅ Debug mode ENABLED (-g3 + sanitizers)")
    else:
        _ts_print("[MESON] Debug mode disabled (using -g1 for stack traces)")

    is_windows = sys.platform.startswith("win") or os.name == "nt"

    # Thin archives configuration (faster builds, smaller disk usage when supported)
    thin_flag = " --thin" if use_thin_archives else ""

    if use_thin_archives:
        _ts_print(
            "[MESON] ✅ Thin archives ENABLED (using clang-tool-chain LLVM tools)"
        )
        _ts_print("[MESON]     Benefits: Faster builds, ~99% smaller archive files")
    else:
        # Show warning when thin archives are manually disabled
        _ts_print("=" * 80)
        _ts_print("⚠️  WARNING: Thin archives DISABLED")
        _ts_print("    Reason: FASTLED_DISABLE_THIN_ARCHIVES environment variable set")
        _ts_print("    Impact: +200ms archive creation, +45MB disk usage per build")
        _ts_print("=" * 80)

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
    import shutil

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

    _ts_print("[MESON] ✓ Using clang-tool-chain wrapper commands")
    if use_sccache:
        _ts_print(f"[MESON]   C compiler: {clang_wrapper} (with sccache)")
        _ts_print(f"[MESON]   C++ compiler: {clangxx_wrapper} (with sccache)")
    else:
        _ts_print(f"[MESON]   C compiler: {clang_wrapper}")
        _ts_print(f"[MESON]   C++ compiler: {clangxx_wrapper}")
    _ts_print(f"[MESON]   Archiver: {llvm_ar_wrapper}")

    # Generate native file for Meson that persists tool configuration across regenerations
    # When Meson regenerates (e.g., when ninja detects meson.build changes),
    # environment variables are lost. Native file ensures tools are configured.
    try:
        # Use clang-tool-chain wrapper commands directly (they already include sccache if requested)
        # Do NOT wrap with external sccache - clang-tool-chain handles that internally
        c_compiler = f"['{clang_wrapper}']"
        cpp_compiler = f"['{clangxx_wrapper}']"

        # Use llvm-ar wrapper from clang-tool-chain
        ar_tool = f"['{llvm_ar_wrapper}']"

        if is_windows:
            native_file_content = f"""# ============================================================================
# Meson Native Build Configuration for FastLED (Auto-generated)
# ============================================================================
# This file is auto-generated by meson_runner.py to configure tool paths.
# It persists across build regenerations when ninja detects meson.build changes.

[binaries]
c = {c_compiler}
cpp = {cpp_compiler}
ar = {ar_tool}

[host_machine]
system = 'windows'
cpu_family = 'x86_64'
cpu = 'x86_64'
endian = 'little'

[properties]
# No additional properties needed - compiler flags are in meson.build
"""
        else:
            native_file_content = f"""# ============================================================================
# Meson Native Build Configuration for FastLED (Auto-generated)
# ============================================================================
# This file is auto-generated by meson_runner.py to configure tool paths.
# It persists across build regenerations when ninja detects meson.build changes.

[binaries]
c = {c_compiler}
cpp = {cpp_compiler}
ar = {ar_tool}

[host_machine]
system = 'linux'
cpu_family = 'x86_64'
cpu = 'x86_64'
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

    if use_sccache:
        _ts_print("[MESON] ✅ sccache integration active (via clang-tool-chain)")
    else:
        _ts_print("[MESON] Note: sccache not found - using direct compilation")

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
        return True

    # Run meson setup using RunningProcess for proper streaming output
    assert cmd is not None, "cmd should be set when not skipping meson setup"
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

        # Write marker file to track thin archive setting for future runs
        try:
            thin_archive_marker.write_text(str(use_thin_archives), encoding="utf-8")
            _ts_print(
                f"[MESON] ✅ Saved thin archive configuration: {use_thin_archives}"
            )
        except (OSError, IOError) as e:
            # Not critical if marker file write fails
            _ts_print(f"[MESON] Warning: Could not write thin archive marker: {e}")

        # Write marker file to track unity setting for future runs
        try:
            unity_marker.write_text(str(unity), encoding="utf-8")
            _ts_print(f"[MESON] ✅ Saved unity configuration: {unity}")
        except (OSError, IOError) as e:
            # Not critical if marker file write fails
            _ts_print(f"[MESON] Warning: Could not write unity marker: {e}")

        # Write marker file to track debug setting for future runs
        try:
            debug_marker.write_text(str(debug), encoding="utf-8")
            _ts_print(f"[MESON] ✅ Saved debug configuration: {debug}")
        except (OSError, IOError) as e:
            # Not critical if marker file write fails
            _ts_print(f"[MESON] Warning: Could not write debug marker: {e}")

        # Write marker file to track source/test file list for future runs
        if current_source_hash:
            try:
                source_files_marker.write_text(current_source_hash, encoding="utf-8")
                _ts_print(
                    f"[MESON] ✅ Saved source/test files hash ({len(current_source_files)} files)"
                )
            except (OSError, IOError) as e:
                # Not critical if marker file write fails
                _ts_print(
                    f"[MESON] Warning: Could not write source/test files marker: {e}"
                )

        return True

    except Exception as e:
        _ts_print(f"[MESON] Setup failed with exception: {e}", file=sys.stderr)
        return False


def compile_meson(build_dir: Path, target: Optional[str] = None) -> bool:
    """
    Compile using Meson.

    Args:
        build_dir: Meson build directory
        target: Specific target to build (None = all)

    Returns:
        True if compilation successful, False otherwise
    """
    cmd = ["meson", "compile", "-C", str(build_dir)]

    if target:
        cmd.append(target)
        _ts_print(f"[MESON] Compiling target: {target}")
    else:
        _ts_print(f"[MESON] Compiling all targets...")

    try:
        # Use RunningProcess for streaming output
        # Inherit environment to ensure compiler wrappers are available
        proc = RunningProcess(
            cmd,
            timeout=600,  # 10 minute timeout for compilation
            auto_run=True,
            check=False,  # We'll check returncode manually
            env=os.environ.copy(),  # Pass current environment with wrapper paths
            output_formatter=TimestampFormatter(),
        )

        returncode = proc.wait(echo=True)

        # Check for Ninja dependency database corruption
        # This appears as "ninja: warning: premature end of file; recovering"
        # When detected, automatically repair the .ninja_deps file
        output = proc.stdout  # RunningProcess combines stdout and stderr
        if "premature end of file" in output.lower():
            _ts_print(
                "[MESON] ⚠️  Detected corrupted Ninja dependency database (.ninja_deps)",
                file=sys.stderr,
            )
            _ts_print(
                "[MESON] 🔧 Auto-repairing: Running ninja -t recompact...",
                file=sys.stderr,
            )

            # Run ninja -t recompact to repair the dependency database
            try:
                repair_proc = RunningProcess(
                    ["ninja", "-C", str(build_dir), "-t", "recompact"],
                    timeout=60,
                    auto_run=True,
                    check=False,
                    env=os.environ.copy(),
                    output_formatter=TimestampFormatter(),
                )
                repair_returncode = repair_proc.wait(echo=False)

                if repair_returncode == 0:
                    _ts_print(
                        "[MESON] ✓ Dependency database repaired successfully",
                        file=sys.stderr,
                    )
                    _ts_print(
                        "[MESON] 💡 Next build should be fast (incremental)",
                        file=sys.stderr,
                    )
                else:
                    _ts_print(
                        "[MESON] ⚠️  Repair failed, but continuing anyway",
                        file=sys.stderr,
                    )
            except Exception as repair_error:
                _ts_print(
                    f"[MESON] ⚠️  Repair failed with exception: {repair_error}",
                    file=sys.stderr,
                )

        if returncode != 0:
            _ts_print(
                f"[MESON] Compilation failed with return code {returncode}",
                file=sys.stderr,
            )

            # Check for stale build cache error (missing files)
            if "missing and no known rule to make it" in output.lower():
                _ts_print(
                    "[MESON] ⚠️  ERROR: Build cache references missing source files",
                    file=sys.stderr,
                )
                _ts_print(
                    "[MESON] 💡 TIP: Source files may have been deleted. Run with --clean to rebuild.",
                    file=sys.stderr,
                )
                _ts_print(
                    "[MESON] 💡 NOTE: Future builds should auto-detect this and reconfigure.",
                    file=sys.stderr,
                )

            return False

        _ts_print(f"[MESON] Compilation successful")
        return True

    except Exception as e:
        _ts_print(f"[MESON] Compilation failed with exception: {e}", file=sys.stderr)
        return False


def _create_error_context_filter(context_lines: int = 20) -> Callable[[str], None]:
    """
    Create a filter function that only shows output when errors are detected.

    The filter accumulates output in a circular buffer. When an error pattern
    is detected, it outputs the buffered context (lines before the error) plus
    the error line, and then continues outputting for context_lines after.

    Args:
        context_lines: Number of lines to show before and after error detection

    Returns:
        Filter function that takes a line and returns None (consumes line)
    """
    from collections import deque

    # Circular buffer for context before errors
    pre_error_buffer: deque[str] = deque(maxlen=context_lines)

    # Counter for lines after error (to show context after error)
    post_error_lines = 0

    # Track if we've seen any errors
    error_detected = False

    # Error patterns (case-insensitive)
    error_patterns = [
        "error:",
        "failed",
        "failure",
        "FAILED",
        "ERROR",
        ": fatal",
        "assertion",
        "segmentation fault",
        "core dumped",
    ]

    def filter_line(line: str) -> None:
        """Process a line and print it if it's part of error context."""
        nonlocal post_error_lines, error_detected

        # Check if this line contains an error pattern
        line_lower = line.lower()
        is_error_line = any(pattern.lower() in line_lower for pattern in error_patterns)

        if is_error_line:
            # Error detected! Output all buffered pre-context
            if not error_detected:
                # First error - show header and pre-context
                _ts_print(
                    "\n[MESON] ⚠️  Test failures detected - showing error context:"
                )
                _ts_print("-" * 80)
                for buffered_line in pre_error_buffer:
                    _ts_print(buffered_line)
                error_detected = True

            # Output this error line with red color highlighting
            _ts_print(f"\033[91m{line}\033[0m")

            # Start counting post-error lines
            post_error_lines = context_lines

            # Don't buffer this line (already printed)
            return

        if post_error_lines > 0:
            # We're in the post-error context window
            _ts_print(line)
            post_error_lines -= 1
            return

        # No error detected yet - buffer this line for potential future context
        # Don't print anything - just accumulate in buffer
        pre_error_buffer.append(line)

    return filter_line


def run_meson_test(
    build_dir: Path, test_name: Optional[str] = None, verbose: bool = False
) -> MesonTestResult:
    """
    Run tests using Meson.

    Args:
        build_dir: Meson build directory
        test_name: Specific test to run (None = all)
        verbose: Enable verbose test output

    Returns:
        MesonTestResult with success status, duration, and test counts
    """
    cmd = ["meson", "test", "-C", str(build_dir), "--print-errorlogs"]

    if verbose:
        cmd.append("-v")

    if test_name:
        cmd.append(test_name)
        _ts_print(f"[MESON] Running test: {test_name}")
    else:
        _ts_print(f"[MESON] Running all tests...")

    start_time = time.time()
    num_passed = 0
    num_failed = 0
    num_run = 0

    try:
        # Use RunningProcess for streaming output
        # Inherit environment to ensure compiler wrappers are available
        proc = RunningProcess(
            cmd,
            timeout=600,  # 10 minute timeout for tests
            auto_run=True,
            check=False,  # We'll check returncode manually
            env=os.environ.copy(),  # Pass current environment with wrapper paths
            output_formatter=TimestampFormatter(),
        )

        # Parse output in real-time to show test progress
        # Pattern matches: "1/143 test_name       OK     0.12s"
        test_pattern = re.compile(
            r"^\s*(\d+)/(\d+)\s+(\S+)\s+(OK|FAIL|SKIP|TIMEOUT)\s+([\d.]+)s"
        )

        if verbose:
            returncode = proc.wait(echo=True)
        else:
            # Stream output line by line to show test progress
            with proc.line_iter(timeout=None) as it:
                for line in it:
                    # Try to parse test result line
                    match = test_pattern.match(line)
                    if match:
                        current = int(match.group(1))
                        total = int(match.group(2))
                        test_name_match = match.group(3)
                        status = match.group(4)
                        duration_str = match.group(5)

                        num_run = current
                        if status == "OK":
                            num_passed += 1
                            # Show brief progress for passed tests
                            _ts_print(
                                f"  [{current}/{total}] ✓ {test_name_match} ({duration_str}s)"
                            )
                        elif status == "FAIL":
                            num_failed += 1
                            _ts_print(
                                f"  [{current}/{total}] ✗ {test_name_match} FAILED ({duration_str}s)"
                            )
                        elif status == "TIMEOUT":
                            num_failed += 1
                            _ts_print(
                                f"  [{current}/{total}] ⏱ {test_name_match} TIMEOUT ({duration_str}s)"
                            )
                    elif verbose or "FAILED" in line or "ERROR" in line:
                        # Show error/important lines
                        _ts_print(f"  {line}")

            returncode = proc.wait()

            # If test failed and we didn't see individual test results, show error context
            if returncode != 0 and num_run == 0:
                # Create error-detecting filter
                error_filter: Callable[[str], None] = _create_error_context_filter(
                    context_lines=20
                )

                # Process accumulated output to show error context
                output_lines = proc.stdout.splitlines()
                for line in output_lines:
                    error_filter(line)

        duration = time.time() - start_time

        if returncode != 0:
            _ts_print(
                f"[MESON] Tests failed with return code {returncode}", file=sys.stderr
            )
            return MesonTestResult(
                success=False,
                duration=duration,
                num_tests_run=num_run,
                num_tests_passed=num_passed,
                num_tests_failed=num_failed,
            )

        _ts_print(
            f"[MESON] All tests passed ({num_passed}/{num_run} tests in {duration:.2f}s)"
        )
        return MesonTestResult(
            success=True,
            duration=duration,
            num_tests_run=num_run,
            num_tests_passed=num_passed,
            num_tests_failed=num_failed,
        )

    except Exception as e:
        duration = time.time() - start_time
        _ts_print(f"[MESON] Test execution failed with exception: {e}", file=sys.stderr)
        return MesonTestResult(
            success=False,
            duration=duration,
            num_tests_run=num_run,
            num_tests_passed=num_passed,
            num_tests_failed=num_failed,
        )


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

    except Exception as e:
        _ts_print(
            f"[MESON] ⚠️  Maintenance failed with exception: {e} (non-fatal)",
            file=sys.stderr,
        )
        return True  # Non-fatal, continue anyway


def run_meson_build_and_test(
    source_dir: Path,
    build_dir: Path,
    test_name: Optional[str] = None,
    clean: bool = False,
    verbose: bool = False,
    unity: bool = False,
    debug: bool = False,
) -> MesonTestResult:
    """
    Complete Meson build and test workflow.

    Args:
        source_dir: Project root directory
        build_dir: Build output directory
        test_name: Specific test to run (without test_ prefix, e.g., "json")
        clean: Clean build directory before setup
        verbose: Enable verbose output
        unity: Enable unity builds (default: False)
        debug: Enable debug mode with full symbols and sanitizers (default: False)

    Returns:
        MesonTestResult with success status, duration, and test counts
    """
    start_time = time.time()

    # Check if Meson is installed
    if not check_meson_installed():
        _ts_print("[MESON] Error: Meson build system is not installed", file=sys.stderr)
        _ts_print("[MESON] Install with: pip install meson ninja", file=sys.stderr)
        return MesonTestResult(success=False, duration=time.time() - start_time)

    # Clean if requested
    if clean and build_dir.exists():
        _ts_print(f"[MESON] Cleaning build directory: {build_dir}")
        import shutil

        shutil.rmtree(build_dir)

    # Setup build
    if not setup_meson_build(
        source_dir, build_dir, reconfigure=False, unity=unity, debug=debug
    ):
        return MesonTestResult(success=False, duration=time.time() - start_time)

    # Perform periodic maintenance on Ninja dependency database (once per day)
    # This helps prevent .ninja_deps corruption and keeps builds fast
    perform_ninja_maintenance(build_dir)

    # Note: PCH dependency tracking is handled automatically by Ninja via depfiles.
    # The compile_pch.py wrapper ensures depfiles reference the .pch output correctly,
    # and Ninja loads these dependencies into .ninja_deps database (897 headers tracked).
    # No manual staleness checking needed - Ninja rebuilds PCH when any dependency changes.

    # Convert test name to executable name (convert to lowercase to match Meson target naming)
    # Support both test_*.exe and *.exe naming patterns with fallback
    # Try test_<name> first (for tests like test_async.cpp), then fallback to <name> (for async.cpp)
    meson_test_name: Optional[str] = None
    fallback_test_name: Optional[str] = None
    fuzzy_candidates: list[str] = []
    if test_name:
        # Convert to lowercase to match Meson target naming convention
        test_name_lower = test_name.lower()

        # Check if test name already starts with "test_"
        if test_name_lower.startswith("test_"):
            # Already has test_ prefix, try as-is first, then without prefix as fallback
            meson_test_name = test_name_lower
            # Fallback: strip test_ prefix to try bare name
            fallback_test_name = test_name_lower[5:]  # Remove "test_" prefix
        else:
            # Try with test_ prefix first, then fallback to without prefix
            meson_test_name = f"test_{test_name_lower}"
            fallback_test_name = test_name_lower

        # Build fuzzy candidate list using meson introspect after setup
        # This handles patterns like fl_async.exe when user specifies "async"
        # Note: We'll populate this after meson setup, since introspect requires
        # a configured build directory
        # For now, try legacy file-based approach as fallback
        tests_dir = build_dir / "tests"
        if tests_dir.exists():
            # Look for executables matching *<name>*
            import glob

            exe_pattern = f"*{test_name_lower}*.exe"
            matches = glob.glob(str(tests_dir / exe_pattern))
            # Extract just the executable name without path and extension
            for match_path in matches:
                exe_name = Path(match_path).stem  # Remove .exe extension
                # Add to candidates if not already in primary/fallback
                if exe_name not in [meson_test_name, fallback_test_name]:
                    fuzzy_candidates.append(exe_name)

    # Compile with build lock to prevent conflicts with example builds
    try:
        with libfastled_build_lock(timeout=600):  # 10 minute timeout
            # In unity mode, always build all_tests target (no individual test targets exist)
            compile_target: Optional[str] = None
            if unity:
                compile_target = "all_tests"
            elif meson_test_name:
                compile_target = meson_test_name

            compilation_success = compile_meson(build_dir, target=compile_target)

            # If compilation failed and we have a fallback name, try the fallback
            if not compilation_success and fallback_test_name and not unity:
                _ts_print(
                    f"[MESON] Target '{compile_target}' not found, trying fallback: '{fallback_test_name}'"
                )
                compile_target = fallback_test_name
                meson_test_name = fallback_test_name
                compilation_success = compile_meson(build_dir, target=compile_target)

            # If still failed, try fuzzy matching candidates
            # If fuzzy_candidates is empty (e.g., after fresh build), use meson introspect
            if not compilation_success and test_name and not unity:
                if not fuzzy_candidates:
                    # Query meson for fuzzy match candidates
                    fuzzy_candidates = get_fuzzy_test_candidates(build_dir, test_name)
                    # Remove primary and fallback names from candidates
                    fuzzy_candidates = [
                        c
                        for c in fuzzy_candidates
                        if c not in [meson_test_name, fallback_test_name]
                    ]

                if fuzzy_candidates:
                    for candidate in fuzzy_candidates:
                        _ts_print(
                            f"[MESON] Target '{compile_target}' not found, trying fuzzy match: '{candidate}'"
                        )
                        compile_target = candidate
                        meson_test_name = candidate
                        compilation_success = compile_meson(
                            build_dir, target=compile_target
                        )
                        if compilation_success:
                            break  # Found a match, stop trying

            if not compilation_success:
                return MesonTestResult(success=False, duration=time.time() - start_time)
    except TimeoutError as e:
        _ts_print(f"[MESON] {e}", file=sys.stderr)
        return MesonTestResult(success=False, duration=time.time() - start_time)

    # Run tests
    if unity:
        # Unity mode: Use the test_runner executable
        # The test_runner loads and executes test DLLs dynamically
        test_runner_path = build_dir / "tests" / "test_runner.exe"
        if not test_runner_path.exists():
            # Try Unix variant (no .exe extension)
            test_runner_path = build_dir / "tests" / "test_runner"

        if not test_runner_path.exists():
            _ts_print(
                f"[MESON] Error: test_runner executable not found in {build_dir / 'tests'}",
                file=sys.stderr,
            )
            return MesonTestResult(success=False, duration=time.time() - start_time)

        # Build test command
        test_cmd = [str(test_runner_path)]

        # If specific test requested, add doctest filter
        if meson_test_name:
            filter_name = meson_test_name.replace("test_", "")
            test_cmd.extend(["--test-case", f"*{filter_name}*"])
            _ts_print(f"[MESON] Running unity tests with filter: {filter_name}")
        else:
            _ts_print("[MESON] Running all unity tests...")

        # Execute test runner
        try:
            proc = RunningProcess(
                test_cmd,
                cwd=build_dir / "tests",  # Run from tests dir where DLLs are located
                timeout=600,  # 10 minute timeout for tests
                auto_run=True,
                check=False,
                env=os.environ.copy(),
                output_formatter=TimestampFormatter(),
            )

            # In verbose mode, show all output
            # In normal mode, only show errors with context
            if verbose:
                returncode = proc.wait(echo=True)
            else:
                # Wait silently, capturing all output
                returncode = proc.wait(echo=False)

                # If test failed, show error context from accumulated output
                if returncode != 0:
                    _ts_print("[MESON] Unity tests failed:", file=sys.stderr)
                    # Create error-detecting filter
                    error_filter: Callable[[str], None] = _create_error_context_filter(
                        context_lines=20
                    )

                    # Process accumulated output to show error context
                    output_lines = proc.stdout.splitlines()
                    for line in output_lines:
                        error_filter(line)

            duration = time.time() - start_time

            if returncode != 0:
                _ts_print(
                    f"[MESON] Unity tests failed with return code {returncode}",
                    file=sys.stderr,
                )
                return MesonTestResult(
                    success=False,
                    duration=duration,
                    num_tests_run=1,
                    num_tests_passed=0,
                    num_tests_failed=1,
                )

            _ts_print("[MESON] All unity tests passed")
            return MesonTestResult(
                success=True,
                duration=duration,
                num_tests_run=1,
                num_tests_passed=1,
                num_tests_failed=0,
            )

        except KeyboardInterrupt:
            raise
        except Exception as e:
            duration = time.time() - start_time
            _ts_print(
                f"[MESON] Unity test execution failed with exception: {e}",
                file=sys.stderr,
            )
            return MesonTestResult(
                success=False,
                duration=duration,
                num_tests_run=1,
                num_tests_passed=0,
                num_tests_failed=1,
            )
    else:
        # Normal mode: When a specific test is requested, run the executable directly
        # to avoid meson test discovery issues
        if meson_test_name:
            # Find the test executable
            test_exe_path = build_dir / "tests" / f"{meson_test_name}.exe"
            if not test_exe_path.exists():
                # Try Unix variant (no .exe extension)
                test_exe_path = build_dir / "tests" / meson_test_name

            if not test_exe_path.exists():
                _ts_print(
                    f"[MESON] Error: test executable not found: {test_exe_path}",
                    file=sys.stderr,
                )
                return MesonTestResult(success=False, duration=time.time() - start_time)

            _ts_print(f"[MESON] Running test: {meson_test_name}")

            # Run the test executable directly
            try:
                proc = RunningProcess(
                    [str(test_exe_path)],
                    cwd=source_dir,  # Run from project root
                    timeout=600,  # 10 minute timeout
                    auto_run=True,
                    check=False,
                    env=os.environ.copy(),
                    output_formatter=TimestampFormatter(),
                )

                returncode = proc.wait(echo=verbose)
                duration = time.time() - start_time

                if returncode != 0:
                    _ts_print(
                        f"[MESON] Test failed with return code {returncode}",
                        file=sys.stderr,
                    )
                    return MesonTestResult(
                        success=False,
                        duration=duration,
                        num_tests_run=1,
                        num_tests_passed=0,
                        num_tests_failed=1,
                    )

                _ts_print(f"[MESON] All tests passed (1/1 tests in {duration:.2f}s)")
                return MesonTestResult(
                    success=True,
                    duration=duration,
                    num_tests_run=1,
                    num_tests_passed=1,
                    num_tests_failed=0,
                )

            except KeyboardInterrupt:
                raise
            except Exception as e:
                duration = time.time() - start_time
                _ts_print(
                    f"[MESON] Test execution failed with exception: {e}",
                    file=sys.stderr,
                )
                return MesonTestResult(
                    success=False,
                    duration=duration,
                    num_tests_run=1,
                    num_tests_passed=0,
                    num_tests_failed=1,
                )
        else:
            # No specific test - use Meson's test runner for all tests
            return run_meson_test(build_dir, test_name=None, verbose=verbose)


if __name__ == "__main__":
    # Simple CLI for testing
    import argparse

    parser = argparse.ArgumentParser(
        description="Meson build system runner for FastLED"
    )
    parser.add_argument("--test", help="Specific test to build and run")
    parser.add_argument("--clean", action="store_true", help="Clean build directory")
    parser.add_argument("--verbose", "-v", action="store_true", help="Verbose output")
    parser.add_argument("--build-dir", default=".build/meson", help="Build directory")

    args = parser.parse_args()

    source_dir = Path.cwd()
    build_dir = Path(args.build_dir)

    success = run_meson_build_and_test(
        source_dir=source_dir,
        build_dir=build_dir,
        test_name=args.test,
        clean=args.clean,
        verbose=args.verbose,
    )

    sys.exit(0 if success else 1)
