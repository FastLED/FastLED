#!/usr/bin/env python3
"""Meson build system integration for FastLED unit tests."""
# pyright: reportMissingImports=false, reportUnknownVariableType=false

import hashlib
import json
import os
import queue
import re
import shutil
import subprocess
import sys
import threading
import time
from collections.abc import Callable
from dataclasses import dataclass
from pathlib import Path
from typing import Optional

from running_process import RunningProcess

from ci.util.build_lock import libfastled_build_lock
from ci.util.color_output import print_blue, print_green, print_red, print_yellow
from ci.util.global_interrupt_handler import handle_keyboard_interrupt_properly
from ci.util.output_formatter import TimestampFormatter, create_filtering_echo_callback
from ci.util.timestamp_print import ts_print as _ts_print


@dataclass
class MesonTestResult:
    """Result from running Meson build and tests"""

    success: bool
    duration: float  # Total duration in seconds
    num_tests_run: int  # Number of tests executed
    num_tests_passed: int  # Number of tests that passed
    num_tests_failed: int  # Number of tests that failed

    @staticmethod
    def construct_build_error(duration: float) -> "MesonTestResult":
        """Construct MesonTestResult with error status and duration"""
        out = MesonTestResult(
            success=False,
            duration=duration,
            num_tests_run=0,
            num_tests_passed=0,
            num_tests_failed=0,
        )
        return out


# Colored output helpers (timestamps removed for cleaner output)
def _print_success(msg: str) -> None:
    """Print success message in green."""
    print_green(msg)


def _print_error(msg: str) -> None:
    """Print error message in red."""
    print_red(msg)


def _print_warning(msg: str) -> None:
    """Print warning message in yellow."""
    print_yellow(msg)


def _print_info(msg: str) -> None:
    """Print info message in blue."""
    print_blue(msg)


def _print_banner(
    title: str, emoji: str = "", width: int = 50, verbose: bool | None = None
) -> None:
    """Print a lightweight section separator.

    Args:
        title: The banner title text
        emoji: Optional emoji to prefix the title (e.g., "🔧", "📦")
        width: Total banner width (default 50)
        verbose: If False, suppress banner. If None, always print (backward compat)
    """
    # Skip banner in non-verbose mode when verbose is explicitly False
    if verbose is False:
        return

    # Format title with emoji if provided
    display_title = f"{emoji} {title}" if emoji else title

    # Simple single-line separator
    padding = width - len(display_title) - 2  # -2 for leading "── "
    if padding < 0:
        padding = 0
    separator = f"── {display_title} " + "─" * padding

    # Print simple separator (no blank line before for compactness)
    print(separator)


def _resolve_meson_executable() -> str:
    """
    Resolve meson executable path, preferring venv installation.

    This ensures consistent meson version usage regardless of invocation method
    (direct Python vs uv run). The venv meson version is controlled by pyproject.toml.

    Returns:
        Path to meson executable (venv version preferred, falls back to PATH)
    """
    # Try to find venv meson first
    script_dir = Path(__file__).resolve().parent
    project_root = script_dir.parent.parent

    # Platform-specific meson executable name
    is_windows = sys.platform.startswith("win") or os.name == "nt"
    meson_exe_name = "meson.exe" if is_windows else "meson"
    venv_meson = project_root / ".venv" / "Scripts" / meson_exe_name

    if venv_meson.exists():
        return str(venv_meson)

    # Fallback to PATH resolution (will use system meson)
    return "meson"


def check_meson_installed() -> bool:
    """Check if Meson is installed and accessible."""
    try:
        result = subprocess.run(
            [_resolve_meson_executable(), "--version"],
            capture_output=True,
            text=True,
            encoding="utf-8",
            errors="replace",
            timeout=5,
        )
        return result.returncode == 0
    except (subprocess.SubprocessError, FileNotFoundError):
        return False


def get_compiler_version(compiler_path: str) -> str:
    """
    Get compiler version string for cache invalidation.

    This function queries the compiler's version to detect when the compiler
    has been upgraded, which requires invalidating cached PCH and object files.

    Args:
        compiler_path: Path to compiler executable (e.g., clang-tool-chain-cpp)

    Returns:
        Version string (e.g., "clang version 21.1.5") or "unknown" on error
    """
    try:
        result = subprocess.run(
            [compiler_path, "--version"],
            capture_output=True,
            text=True,
            encoding="utf-8",
            errors="replace",
            check=True,
            timeout=10,
        )
        # Return first line which contains version info
        return result.stdout.split("\n")[0].strip()
    except KeyboardInterrupt:
        handle_keyboard_interrupt_properly()
        raise
    except Exception as e:
        _ts_print(f"[MESON] Warning: Could not get compiler version: {e}")
        return "unknown"


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
            [_resolve_meson_executable(), "introspect", str(build_dir), "--targets"],
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

    except KeyboardInterrupt:
        handle_keyboard_interrupt_properly()
        raise
    except Exception as e:
        _ts_print(f"[MESON] Warning: Failed to get source file hash: {e}")
        return ("", [])


def atomic_write_text(path: Path, content: str, encoding: str = "utf-8") -> None:
    """
    Atomically write text to a file using temp file + rename pattern.

    This ensures the file is never left in a corrupted/partial state if the
    process crashes mid-write. The rename operation is atomic on POSIX systems
    and nearly atomic on Windows (uses ReplaceFile API via Path.replace()).

    Args:
        path: Path to the file to write
        content: Text content to write
        encoding: Text encoding (default: utf-8)

    Raises:
        OSError/IOError: If the write fails and cleanup cannot complete
    """
    # Ensure parent directory exists
    path.parent.mkdir(parents=True, exist_ok=True)

    # Write to temp file first, then atomic rename
    temp_path = path.with_suffix(path.suffix + ".tmp")
    try:
        with open(temp_path, "w", encoding=encoding) as f:
            f.write(content)
            f.flush()
            # Ensure data is written to disk before rename
            os.fsync(f.fileno())
        # Atomic rename - either completes fully or not at all
        temp_path.replace(path)
    except (OSError, IOError):
        # Clean up temp file on error
        try:
            temp_path.unlink()
        except (OSError, IOError):
            pass
        raise


def write_if_different(path: Path, content: str, mode: Optional[int] = None) -> bool:
    """
    Write file only if content differs from existing file.

    This prevents unnecessary file modifications that would trigger Meson
    regeneration. If the file doesn't exist or content has changed, writes
    the new content atomically.

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

        # Content changed or file doesn't exist - write it atomically
        atomic_write_text(path, content)

        # Set permissions if specified (Unix only)
        if mode is not None and not sys.platform.startswith("win"):
            path.chmod(mode)

        return True
    except (OSError, IOError) as e:
        # On error, try to write anyway - caller will handle failures
        _ts_print(f"[MESON] Warning: Error checking file {path}: {e}")
        atomic_write_text(path, content)
        if mode is not None and not sys.platform.startswith("win"):
            path.chmod(mode)
        return True


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
    import glob

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
            # No marker file exists from previous configure
            # This can happen on first run after updating to this version
            # Force reconfigure because we don't know what thin archive setting
            # the cached libfastled.a was built with
            _ts_print("[MESON] ℹ️  No thin archive marker found, forcing reconfigure")
            force_reconfigure = True

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
            # Force reconfigure because we don't know what debug setting
            # the cached objects/PCH were built with - sanitizers could mismatch
            _ts_print("[MESON] ℹ️  No debug marker found, forcing reconfigure")
            force_reconfigure = True

        # CRITICAL: Delete all object files and archives when debug mode changes
        # Object files compiled with sanitizers cannot be linked without sanitizer runtime
        # We must force a complete rebuild with the new compiler flags
        if debug_changed:
            cleanup_build_artifacts(build_dir, "Debug mode changed")

        # Check if IWYU check setting has changed since last configure
        # When check mode changes, we must reconfigure to update the C++ compiler wrapper
        if check_marker.exists():
            try:
                last_check_setting = check_marker.read_text().strip() == "True"
                if last_check_setting != check:
                    _ts_print(
                        f"[MESON] ⚠️  IWYU check mode changed: {last_check_setting} → {check}"
                    )
                    _ts_print(
                        "[MESON] 🔄 Forcing reconfigure to update C++ compiler wrapper"
                    )
                    force_reconfigure = True
            except (OSError, IOError):
                # If we can't read the marker, force reconfigure to be safe
                _ts_print("[MESON] ⚠️  Could not read check marker, forcing reconfigure")
                force_reconfigure = True
        else:
            # No marker file exists from previous configure
            # This can happen on first run after updating to this version
            # Force reconfigure because we don't know what IWYU setting
            # the cached compiler wrapper was configured with
            _ts_print("[MESON] ℹ️  No check marker found, forcing reconfigure")
            force_reconfigure = True

        # Check if build_mode setting has changed since last configure
        # CRITICAL: This detects quick <-> release transitions which both have debug=False
        # but have different optimization flags (-O0 vs -O3)
        # When build_mode changes, we must clean objects to avoid mixing different compiler flags
        build_mode_changed = False
        last_build_mode: Optional[str] = None
        if build_mode_marker.exists():
            try:
                last_build_mode = build_mode_marker.read_text().strip()
                if last_build_mode != build_mode:
                    _ts_print(
                        f"[MESON] ⚠️  Build mode changed: {last_build_mode} → {build_mode}"
                    )
                    _ts_print(
                        "[MESON] 🔄 Forcing reconfigure to update optimization/debug flags"
                    )
                    force_reconfigure = True
                    build_mode_changed = True
            except (OSError, IOError):
                # If we can't read the marker, force reconfigure to be safe
                _ts_print(
                    "[MESON] ⚠️  Could not read build_mode marker, forcing reconfigure"
                )
                force_reconfigure = True
        else:
            # No marker file exists from previous configure
            # This can happen on first run after updating to this version
            # CRITICAL: Force reconfigure because we don't know what mode the cached
            # objects/PCH were built with - they could be incompatible with current mode
            _ts_print("[MESON] ℹ️  No build_mode marker found, forcing reconfigure")
            force_reconfigure = True

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
    if test_files_changed:
        _ts_print("[MESON] 🔄 Forcing reconfigure due to test file additions/removals")
        force_reconfigure = True

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
        # Reconfigure existing build (explicitly requested or forced by thin archive change)
        reason = (
            "forced by thin archive change"
            if force_reconfigure
            else "explicitly requested"
        )
        _ts_print(f"[MESON] Reconfiguring build directory ({reason}): {build_dir}")
        cmd = [
            _resolve_meson_executable(),
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
            _resolve_meson_executable(),
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

    if already_configured:
        if compiler_version_marker.exists():
            try:
                last_compiler_version = compiler_version_marker.read_text().strip()
                if last_compiler_version != current_compiler_version:
                    _ts_print(
                        f"[MESON] ⚠️  Compiler version changed: {last_compiler_version} → {current_compiler_version}"
                    )
                    _ts_print(
                        "[MESON] 🔄 Forcing reconfigure and cleaning cached objects/PCH"
                    )
                    force_reconfigure = True
                    compiler_version_changed = True
                    # Re-evaluate skip_meson_setup since we're forcing reconfigure
                    skip_meson_setup = False
            except (OSError, IOError):
                _ts_print(
                    "[MESON] ⚠️  Could not read compiler version marker, forcing reconfigure"
                )
                force_reconfigure = True
                skip_meson_setup = False
        else:
            # No marker file exists from previous configure
            _ts_print(
                "[MESON] ℹ️  No compiler version marker found, forcing reconfigure"
            )
            force_reconfigure = True
            skip_meson_setup = False

        # CRITICAL: Delete all object files, archives, and PCH when compiler version changes
        # Objects compiled with a different compiler version can have incompatible ABI
        if compiler_version_changed:
            cleanup_build_artifacts(build_dir, "Compiler version changed")

        # CRITICAL: If compiler version forced a reconfigure, we need to update cmd
        # The original cmd assignment was based on pre-compiler-check values
        if force_reconfigure and cmd is None:
            _ts_print(
                f"[MESON] Reconfiguring build directory (forced by compiler version change): {build_dir}"
            )
            cmd = [
                _resolve_meson_executable(),
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
        cpp_compiler = f"['{clangxx_wrapper}']"

        # Store check flag for later use during compilation phase
        # IWYU will be injected via environment if check=True

        # Use llvm-ar wrapper from clang-tool-chain
        ar_tool = f"['{llvm_ar_wrapper}']"

        # Detect platform for native file
        import platform

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


def compile_meson(
    build_dir: Path,
    target: Optional[str] = None,
    check: bool = False,
    quiet: bool = False,
    verbose: bool = False,
) -> bool:
    """
    Compile using Meson.

    Args:
        build_dir: Meson build directory
        target: Specific target to build (None = all)
        check: Enable IWYU static analysis during compilation (default: False)
        quiet: Suppress banner and progress output (used during target fallback retries)
        verbose: Enable verbose output with section banners

    Returns:
        True if compilation successful, False otherwise
    """
    cmd = [_resolve_meson_executable(), "compile", "-C", str(build_dir)]

    if target:
        cmd.append(target)
        if not quiet:
            _print_banner("Compile", "📦", verbose=verbose)
            print(f"Compiling: {target}")
    else:
        _print_banner("Compile", "📦", verbose=verbose)
        print("Compiling all targets...")

    # Inject IWYU wrapper by modifying build.ninja when check mode is enabled
    # This avoids meson setup probe issues while still running IWYU during compilation
    env = os.environ.copy()
    if check:
        # Use custom IWYU wrapper (fixes clang-tool-chain-iwyu argument forwarding issues)
        iwyu_wrapper_path = Path(__file__).parent.parent / "iwyu_wrapper.py"
        if iwyu_wrapper_path.exists():
            # Use Python to invoke our custom wrapper
            python_exe = sys.executable
            iwyu_wrapper = f'"{python_exe}" "{iwyu_wrapper_path}"'
            _ts_print(f"[MESON] Using custom IWYU wrapper: {iwyu_wrapper_path.name}")
        else:
            iwyu_wrapper = None
            _ts_print(f"[MESON] ⚠️ Custom IWYU wrapper not found: {iwyu_wrapper_path}")

        if iwyu_wrapper:
            # Modify build.ninja to wrap C++ compiler with IWYU
            # This is done after meson setup (which uses normal compiler) but before ninja runs
            build_ninja_path = build_dir / "build.ninja"
            if build_ninja_path.exists():
                try:
                    # Read build.ninja
                    build_ninja_content = build_ninja_path.read_text(encoding="utf-8")

                    # Replace C++ compiler command in cpp_COMPILER rule with IWYU wrapper
                    # Pattern matches EITHER:
                    #   1. Single executable: command = "path\to\clang++.EXE" $ARGS
                    #   2. Python wrapper: command = "python.exe" "wrapper.py" $ARGS

                    # Flexible pattern that captures the entire compiler command before $ARGS
                    # Group 1: "rule cpp_COMPILER\n command = "
                    # Group 2: The entire compiler command (could be single or multiple quoted strings)
                    # Group 3: " $ARGS" and rest
                    pattern = r'(rule cpp_COMPILER\s+command = )((?:"[^"]*"\s*)+)(\$ARGS.*?)(?=\n\s*(?:deps|$))'

                    def replace_compiler(match: re.Match[str]) -> str:
                        prefix = match.group(1)
                        compiler_cmd = match.group(2).strip()
                        args_and_rest = match.group(3)

                        # Escape backslashes in paths for re.sub
                        iwyu_escaped = iwyu_wrapper.replace("\\", "\\\\")

                        # Wrap the entire compiler command with IWYU
                        # iwyu_wrapper already contains proper quoting (e.g., "python.exe" "script.py")
                        # Format: python.exe script.py -- original_compiler_command $ARGS...
                        return (
                            f"{prefix}{iwyu_escaped} -- {compiler_cmd} {args_and_rest}"
                        )

                    modified_content = re.sub(
                        pattern, replace_compiler, build_ninja_content, flags=re.DOTALL
                    )

                    # Also strip out PCH flags from all ARGS variables since IWYU doesn't support PCH
                    # Pattern matches ARGS lines containing PCH flags
                    # Example: ARGS = ... -include-pch "path.pch" -Werror=invalid-pch -fpch-validate-input-files-content ...
                    # We need to remove these three flags together
                    pch_pattern = r'(-include-pch\s+"[^"]+"\s+-Werror=invalid-pch\s+-fpch-validate-input-files-content\s+)'
                    modified_content = re.sub(
                        pch_pattern, "", modified_content, flags=re.MULTILINE
                    )

                    if modified_content != build_ninja_content:
                        build_ninja_path.write_text(modified_content, encoding="utf-8")
                        _ts_print(
                            f"[MESON] ✅ IWYU wrapper injected into build.ninja: {iwyu_wrapper}"
                        )
                        _ts_print(
                            f"[MESON] ✅ PCH flags removed from all compile commands (IWYU does not support PCH)"
                        )
                    else:
                        _ts_print(
                            f"[MESON] ⚠️ Failed to modify build.ninja (content unchanged)"
                        )
                        _ts_print(
                            f"[MESON] DEBUG: Looking for cpp_COMPILER rule in build.ninja"
                        )
                except (OSError, IOError) as e:
                    _ts_print(f"[MESON] ⚠️ Failed to modify build.ninja: {e}")
            else:
                _ts_print(f"[MESON] ⚠️ build.ninja not found, skipping IWYU injection")
        else:
            _ts_print(f"[MESON] ⚠️ IWYU wrapper not found, skipping static analysis")

    try:
        # Use RunningProcess for streaming output
        # Pass modified environment with IWYU wrapper if check=True
        proc = RunningProcess(
            cmd,
            timeout=600,  # 10 minute timeout for compilation
            auto_run=True,
            check=False,  # We'll check returncode manually
            env=env,  # Pass environment with IWYU wrapper
            output_formatter=TimestampFormatter(),
        )

        # Pattern to detect linking operations (same as streaming path)
        # Example: "[142/143] Linking target tests/test_foo.exe"
        # Note: TimestampFormatter already adds timestamp, so line comes with that prefix
        # We need to match after the TimestampFormatter's output (e.g., "7.11 3.14 [1/1] Linking...")
        link_pattern = re.compile(
            r"\[\d+/\d+\]\s+Linking (?:CXX executable|target)\s+(.+)$"
        )

        # Stream output line by line to rewrite Ninja paths
        # In quiet mode, suppress all output (used during target fallback retries)
        with proc.line_iter(timeout=None) as it:
            for line in it:
                # Filter out noisy Meson/Ninja INFO lines that clutter output
                # Note: TimestampFormatter may add timestamp prefix, so check contains not startswith
                stripped = line.strip()
                if " INFO:" in stripped or stripped.startswith("INFO:"):
                    continue  # Skip Meson INFO lines
                if "Entering directory" in stripped:
                    continue  # Skip Ninja directory change messages
                if "calculating backend command" in stripped.lower():
                    continue  # Skip Meson backend calculation message
                if "ERROR: Can't invoke target" in stripped:
                    continue  # Skip target-not-found errors (handled by fallback)
                if "ninja: no work to do" in stripped.lower():
                    continue  # Skip no-work ninja message (already up to date)

                # In quiet mode, only collect output for error checking, don't print
                if quiet:
                    continue

                # Rewrite Ninja paths to show full build-relative paths for clarity
                # Ninja outputs paths relative to build directory (e.g., "tests/fx_frame.exe")
                # Users expect to see full paths (e.g., ".build/meson-quick/tests/fx_frame.exe")
                display_line = line
                link_match = link_pattern.search(line)
                if link_match:
                    rel_path = link_match.group(1)
                    # Convert build-relative path to project-relative path
                    full_path = build_dir / rel_path
                    try:
                        # Make path relative to project root for cleaner display
                        display_path = full_path.relative_to(Path.cwd())
                        # Rewrite the line with full path
                        display_line = line.replace(rel_path, str(display_path))
                    except ValueError:
                        # If path is outside project, show absolute path
                        display_line = line.replace(rel_path, str(full_path))

                # Echo the (possibly rewritten) line
                _ts_print(display_line)

        returncode = proc.wait()

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
            except KeyboardInterrupt:
                handle_keyboard_interrupt_properly()
                raise
            except Exception as repair_error:
                _ts_print(
                    f"[MESON] ⚠️  Repair failed with exception: {repair_error}",
                    file=sys.stderr,
                )

        if returncode != 0:
            # In quiet mode, don't print failure messages (fallback retries handle this)
            if not quiet:
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

        # Don't print "Compilation successful" - the transition to Running phase implies success
        # This was previously conditional on quiet mode, but it's always redundant
        return True

    except KeyboardInterrupt:
        handle_keyboard_interrupt_properly()
        raise
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
    cmd = [
        _resolve_meson_executable(),
        "test",
        "-C",
        str(build_dir),
        "--print-errorlogs",
    ]

    if verbose:
        cmd.append("-v")

    if test_name:
        cmd.append(test_name)
        _print_banner("Test", "▶️", verbose=verbose)
        print(f"Running: {test_name}")
    else:
        _print_banner("Test", "▶️", verbose=verbose)
        print("Running all tests...")

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
        # Note: TimestampFormatter adds "XX.XX " prefix, so we need to skip it
        test_pattern = re.compile(
            r"^[\d.]+\s+(\d+)/(\d+)\s+(\S+)\s+(OK|FAIL|SKIP|TIMEOUT)\s+([\d.]+)s"
        )

        if verbose:
            # Use filtering callback in verbose mode to suppress noise patterns
            echo_callback = create_filtering_echo_callback()
            returncode = proc.wait(echo=echo_callback)
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
                            # Show brief progress for passed tests in green
                            _print_success(
                                f"  [{current}/{total}] ✓ {test_name_match} ({duration_str}s)"
                            )
                        elif status == "FAIL":
                            num_failed += 1
                            _print_error(
                                f"  [{current}/{total}] ✗ {test_name_match} FAILED ({duration_str}s)"
                            )
                        elif status == "TIMEOUT":
                            num_failed += 1
                            _print_error(
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
            _print_error(f"[MESON] ❌ Tests failed with return code {returncode}")
            return MesonTestResult(
                success=False,
                duration=duration,
                num_tests_run=num_run,
                num_tests_passed=num_passed,
                num_tests_failed=num_failed,
            )

        _print_success(
            f"✅ All tests passed ({num_passed}/{num_run} in {duration:.2f}s)"
        )
        return MesonTestResult(
            success=True,
            duration=duration,
            num_tests_run=num_run,
            num_tests_passed=num_passed,
            num_tests_failed=num_failed,
        )

    except KeyboardInterrupt:
        handle_keyboard_interrupt_properly()
        raise
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

    except KeyboardInterrupt:
        handle_keyboard_interrupt_properly()
        raise
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
    debug: bool = False,
    build_mode: Optional[str] = None,
    check: bool = False,
) -> MesonTestResult:
    """
    Complete Meson build and test workflow.

    Args:
        source_dir: Project root directory
        build_dir: Build output directory (will be modified to be mode-specific)
        test_name: Specific test to run (without test_ prefix, e.g., "json")
        clean: Clean build directory before setup
        verbose: Enable verbose output
        debug: Enable debug mode with full symbols and sanitizers (default: False)
        build_mode: Build mode override ("quick", "debug", "release"). If None, uses debug parameter.
        check: Enable IWYU static analysis (default: False)

    Returns:
        MesonTestResult with success status, duration, and test counts
    """
    start_time = time.time()

    # Determine build mode: explicit build_mode parameter takes precedence over debug flag
    if build_mode is None:
        build_mode = "debug" if debug else "quick"

    # Validate build_mode
    if build_mode not in ["quick", "debug", "release"]:
        _ts_print(
            f"[MESON] Error: Invalid build_mode '{build_mode}'. Must be 'quick', 'debug', or 'release'",
            file=sys.stderr,
        )
        return MesonTestResult(
            success=False,
            duration=time.time() - start_time,
            num_tests_run=0,
            num_tests_passed=0,
            num_tests_failed=0,
        )

    # Construct mode-specific build directory
    # This enables caching libfastled.a per mode when source unchanged but flags differ
    # Example: .build/meson-quick, .build/meson-debug, .build/meson-release
    original_build_dir = build_dir
    build_dir = build_dir.parent / f"{build_dir.name}-{build_mode}"

    # Build dir and mode info is consolidated in setup_meson_build output
    # No need for separate verbose messages here

    # Check if Meson is installed
    if not check_meson_installed():
        _ts_print("[MESON] Error: Meson build system is not installed", file=sys.stderr)
        _ts_print("[MESON] Install with: pip install meson ninja", file=sys.stderr)
        return MesonTestResult(
            success=False,
            duration=time.time() - start_time,
            num_tests_run=0,
            num_tests_passed=0,
            num_tests_failed=0,
        )

    # Clean if requested
    if clean and build_dir.exists():
        _ts_print(f"[MESON] Cleaning build directory: {build_dir}")
        import shutil

        shutil.rmtree(build_dir)

    # Setup build
    # Pass debug=True when build_mode is "debug" to enable sanitizers and full symbols
    # Also pass explicit build_mode to ensure proper cache invalidation on mode changes
    use_debug = build_mode == "debug"
    if not setup_meson_build(
        source_dir,
        build_dir,
        reconfigure=False,
        debug=use_debug,
        check=check,
        build_mode=build_mode,
        verbose=verbose,
    ):
        return MesonTestResult(
            success=False,
            duration=time.time() - start_time,
            num_tests_run=0,
            num_tests_passed=0,
            num_tests_failed=0,
        )

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

    # Compile and test with build lock to prevent conflicts with example builds
    # STREAMING MODE: When no specific test requested, use stream_compile_and_run_tests()
    # for parallel compilation + execution
    use_streaming = not meson_test_name

    try:
        with libfastled_build_lock(timeout=600):  # 10 minute timeout
            if use_streaming:
                # STREAMING EXECUTION PATH
                # Compile and run tests in parallel - as soon as a test finishes linking,
                # it's immediately executed while other tests continue compiling
                if verbose:
                    _ts_print(
                        "[MESON] Using streaming execution (compile + test in parallel)"
                    )

                # Create test callback for streaming execution
                def test_callback(test_path: Path) -> bool:
                    """Run a single test executable and return success status"""
                    try:
                        proc = RunningProcess(
                            [str(test_path)],
                            cwd=source_dir,  # Run from project root
                            timeout=600,  # 10 minute timeout per test
                            auto_run=True,
                            check=False,
                            env=os.environ.copy(),
                            output_formatter=TimestampFormatter(),
                        )

                        # Use filtering callback in verbose mode to suppress noise patterns
                        echo_callback = (
                            create_filtering_echo_callback() if verbose else False
                        )
                        returncode = proc.wait(echo=echo_callback)
                        return returncode == 0

                    except KeyboardInterrupt:
                        handle_keyboard_interrupt_properly()
                        raise
                    except Exception as e:
                        _ts_print(f"[MESON] Test execution error: {e}", file=sys.stderr)
                        return False

                # Run streaming compilation and testing for UNIT TESTS
                overall_success_tests, num_passed_tests, num_failed_tests = (
                    stream_compile_and_run_tests(
                        build_dir=build_dir,
                        test_callback=test_callback,
                        target=None,  # Build all default test targets (unit tests)
                        verbose=verbose,
                    )
                )

                # Run streaming compilation and testing for EXAMPLES
                if verbose:
                    _ts_print("[MESON] Starting examples compilation and execution...")
                overall_success_examples, num_passed_examples, num_failed_examples = (
                    stream_compile_and_run_tests(
                        build_dir=build_dir,
                        test_callback=test_callback,
                        target="examples-host",  # Build examples explicitly
                        verbose=verbose,
                    )
                )

                # Combine results
                overall_success = overall_success_tests and overall_success_examples
                num_passed = num_passed_tests + num_passed_examples
                num_failed = num_failed_tests + num_failed_examples

                duration = time.time() - start_time
                num_tests_run = num_passed + num_failed

                # FALLBACK: If no tests were run during streaming (e.g., everything already compiled),
                # fall back to running all tests via Meson test runner
                # Show simplified message instead of detailed "0/0" breakdown
                if num_tests_run == 0 and overall_success:
                    if verbose:
                        _print_info(
                            "[MESON] Build up-to-date, running existing tests..."
                        )
                    # Note: run_meson_test already prints the RUNNING TESTS banner
                    result = run_meson_test(build_dir, test_name=None, verbose=verbose)
                    return result

                if not overall_success:
                    _print_error(
                        f"[MESON] ❌ Some tests failed ({num_passed}/{num_tests_run} tests in {duration:.2f}s)"
                    )
                    return MesonTestResult(
                        success=False,
                        duration=duration,
                        num_tests_run=num_tests_run,
                        num_tests_passed=num_passed,
                        num_tests_failed=num_failed,
                    )

                _print_success(
                    f"✅ All tests passed ({num_passed}/{num_tests_run} in {duration:.2f}s)"
                )
                return MesonTestResult(
                    success=True,
                    duration=duration,
                    num_tests_run=num_tests_run,
                    num_tests_passed=num_passed,
                    num_tests_failed=num_failed,
                )

            else:
                # TRADITIONAL SEQUENTIAL PATH
                # Used for specific test requests
                compile_target: Optional[str] = None
                if meson_test_name:
                    compile_target = meson_test_name

                # Try target resolution with fallback - suppress all output during discovery
                # to avoid confusing users with internal target name guessing
                targets_to_try = [compile_target]
                if fallback_test_name and fallback_test_name != compile_target:
                    targets_to_try.append(fallback_test_name)

                # Build the full list of candidates up front so we can try quietly
                if test_name:
                    # Get fuzzy candidates early
                    fuzzy_candidates = (
                        get_fuzzy_test_candidates(build_dir, test_name)
                        if not fuzzy_candidates
                        else fuzzy_candidates
                    )
                    # Add fuzzy candidates not already in the list
                    for c in fuzzy_candidates:
                        if c not in targets_to_try:
                            targets_to_try.append(c)

                # When there are multiple candidates, run all in quiet mode
                # Only show banner once with final resolved target
                has_fallbacks = len(targets_to_try) > 1
                compilation_success = False
                successful_target: Optional[str] = None

                for candidate in targets_to_try:
                    # Use quiet mode when there are fallbacks (suppress failure noise)
                    compilation_success = compile_meson(
                        build_dir,
                        target=candidate,
                        quiet=has_fallbacks,
                        verbose=verbose,
                    )
                    if compilation_success:
                        successful_target = candidate
                        compile_target = candidate
                        meson_test_name = candidate
                        break

                # Show the final result after target resolution
                if compilation_success and has_fallbacks:
                    _print_banner("Compile", "📦", verbose=verbose)
                    print(f"Compiling: {successful_target}")
                    # Note: Don't print "Compilation successful" - redundant before Running phase

                if not compilation_success:
                    return MesonTestResult(
                        success=False,
                        duration=time.time() - start_time,
                        num_tests_run=0,
                        num_tests_passed=0,
                        num_tests_failed=0,
                    )
    except TimeoutError as e:
        _ts_print(f"[MESON] {e}", file=sys.stderr)
        return MesonTestResult(
            success=False,
            duration=time.time() - start_time,
            num_tests_run=0,
            num_tests_passed=0,
            num_tests_failed=0,
        )

    # In IWYU check mode, we only compile (to run static analysis)
    # Skip test execution and return success after successful compilation
    if check:
        duration = time.time() - start_time
        _ts_print(
            "[MESON] ✅ IWYU analysis complete (test execution skipped in check mode)"
        )
        return MesonTestResult(
            success=True,
            duration=duration,
            num_tests_run=0,
            num_tests_passed=0,
            num_tests_failed=0,
        )

    # Run tests (traditional path only - streaming already ran tests above)
    # When a specific test is requested, run the executable directly
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
            return MesonTestResult(
                success=False,
                duration=time.time() - start_time,
                num_tests_run=0,
                num_tests_passed=0,
                num_tests_failed=0,
            )

        _print_banner("Test", "▶️", verbose=verbose)
        print(f"Running: {meson_test_name}")

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

            # Use filtering callback in verbose mode to suppress noise patterns
            echo_callback = create_filtering_echo_callback() if verbose else False
            returncode = proc.wait(echo=echo_callback)
            duration = time.time() - start_time

            if returncode != 0:
                _print_error(f"[MESON] ❌ Test failed with return code {returncode}")
                # Show test output for failures (even if not verbose)
                if not verbose and proc.stdout:
                    _print_error("[MESON] Test output:")
                    for line in proc.stdout.splitlines()[-50:]:  # Last 50 lines
                        _ts_print(f"  {line}")
                return MesonTestResult(
                    success=False,
                    duration=duration,
                    num_tests_run=1,
                    num_tests_passed=0,
                    num_tests_failed=1,
                )

            _print_success(f"✅ All tests passed (1/1 in {duration:.2f}s)")
            return MesonTestResult(
                success=True,
                duration=duration,
                num_tests_run=1,
                num_tests_passed=1,
                num_tests_failed=0,
            )

        except KeyboardInterrupt:
            handle_keyboard_interrupt_properly()
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


def stream_compile_and_run_tests(
    build_dir: Path,
    test_callback: Callable[[Path], bool],
    target: Optional[str] = None,
    verbose: bool = False,
) -> tuple[bool, int, int]:
    """
    Stream test compilation and execution in parallel.

    Monitors Ninja output to detect when test executables finish linking,
    then immediately queues them for execution via the callback.

    Args:
        build_dir: Meson build directory
        test_callback: Function called with each completed test path.
                      Returns True if test passed, False if failed.
        target: Specific target to build (None = all)
        verbose: Show detailed progress messages (default: False)

    Returns:
        Tuple of (overall_success, num_passed, num_failed)
    """
    cmd = [_resolve_meson_executable(), "compile", "-C", str(build_dir)]

    if target:
        cmd.append(target)
        if verbose:
            _ts_print(f"[MESON] Streaming compilation of target: {target}")
    else:
        # Build default targets (unit tests) - no target specified builds defaults
        # Note: examples have build_by_default: false, so we need to build them separately
        if verbose:
            _ts_print(f"[MESON] Streaming compilation of all test targets...")

    # Pattern to detect test executable linking
    # Example: "[142/143] Linking target tests/test_foo.exe"
    link_pattern = re.compile(
        r"^\[\d+/\d+\]\s+Linking (?:CXX executable|target)\s+(.+)$"
    )

    # Track compilation success and test results
    compilation_failed = False
    num_passed = 0
    num_failed = 0
    tests_run = 0

    # Queue for completed test executables
    test_queue: queue.Queue[Optional[Path]] = queue.Queue()

    # Sentinel value to signal end of compilation
    COMPILATION_DONE = None

    def producer_thread() -> None:
        """Parse Ninja output and queue completed test executables"""
        nonlocal compilation_failed

        try:
            # Use RunningProcess for streaming output
            # Note: No formatter here - we need raw Ninja output for regex pattern matching
            proc = RunningProcess(
                cmd,
                timeout=600,  # 10 minute timeout for compilation
                auto_run=True,
                check=False,
                env=os.environ.copy(),
            )

            # Stream output line by line
            with proc.line_iter(timeout=None) as it:
                for line in it:
                    # Filter out noisy Meson/Ninja INFO lines that clutter output
                    # These provide no useful information for normal operation
                    stripped = line.strip()
                    if stripped.startswith("INFO:"):
                        continue  # Skip Meson INFO lines
                    if "Entering directory" in stripped:
                        continue  # Skip Ninja directory change messages

                    # Rewrite Ninja paths to show full build-relative paths for clarity
                    # Ninja outputs paths relative to build directory (e.g., "tests/fx_frame.exe")
                    # Users expect to see full paths (e.g., ".build/meson-quick/tests/fx_frame.exe")
                    display_line = line
                    link_match = link_pattern.match(stripped)
                    if link_match:
                        rel_path = link_match.group(1)
                        # Convert build-relative path to project-relative path
                        full_path = build_dir / rel_path
                        try:
                            # Make path relative to project root for cleaner display
                            display_path = full_path.relative_to(Path.cwd())
                            # Rewrite the line with full path
                            display_line = line.replace(rel_path, str(display_path))
                        except ValueError:
                            # If path is outside project, show absolute path
                            display_line = line.replace(rel_path, str(full_path))

                    # Echo output for visibility (skip empty lines) - only in verbose mode
                    if stripped and verbose:
                        _ts_print(f"[BUILD] {display_line}")

                    # Check for link completion
                    match = (
                        link_match if link_match else link_pattern.match(line.strip())
                    )
                    if match:
                        # Extract test executable path
                        rel_path = match.group(1)
                        test_path = build_dir / rel_path

                        # Only queue if it's a test executable (in tests/ or examples/ directory)
                        if (
                            "tests/" in rel_path
                            or "tests\\" in rel_path
                            or "examples/" in rel_path
                            or "examples\\" in rel_path
                        ):
                            # Exclude infrastructure executables that aren't actual tests
                            test_name = test_path.stem  # Get filename without extension
                            if test_name in ("runner", "test_runner", "example_runner"):
                                continue  # Skip these infrastructure executables

                            # Skip DLL/shared library files
                            if test_path.suffix.lower() in (".dll", ".so", ".dylib"):
                                continue  # Skip DLL/shared library files

                            if verbose:
                                _ts_print(f"[MESON] Test ready: {test_path.name}")
                            test_queue.put(test_path)

            # Check compilation result
            returncode = proc.wait()
            if returncode != 0:
                _ts_print(
                    f"[MESON] Compilation failed with return code {returncode}",
                    file=sys.stderr,
                )
                compilation_failed = True
            else:
                if verbose:
                    _ts_print(f"[MESON] Compilation completed successfully")

        except KeyboardInterrupt:
            handle_keyboard_interrupt_properly()
            raise
        except Exception as e:
            _ts_print(f"[MESON] Producer thread error: {e}", file=sys.stderr)
            compilation_failed = True
        finally:
            # Signal end of compilation
            test_queue.put(COMPILATION_DONE)

    # Start producer thread
    producer = threading.Thread(
        target=producer_thread, name="NinjaProducer", daemon=True
    )
    producer.start()

    # Consumer: Run tests as they become available
    try:
        while True:
            try:
                # Get next test from queue (blocking with timeout for responsiveness)
                test_path = test_queue.get(timeout=1.0)

                if test_path is COMPILATION_DONE:
                    # Compilation finished
                    break

                # Type narrowing: test_path is now guaranteed to be Path (not None)
                assert test_path is not None

                # Run the test
                tests_run += 1
                if verbose:
                    _ts_print(f"[TEST {tests_run}] Running: {test_path.name}")

                try:
                    success = test_callback(test_path)
                    if success:
                        num_passed += 1
                        if verbose:
                            _ts_print(f"[TEST {tests_run}] ✓ PASSED: {test_path.name}")
                    else:
                        num_failed += 1
                        # Always show failures even in non-verbose mode
                        _ts_print(f"[TEST {tests_run}] ✗ FAILED: {test_path.name}")
                except KeyboardInterrupt:
                    handle_keyboard_interrupt_properly()
                    raise
                except Exception as e:
                    # Always show errors even in non-verbose mode
                    _ts_print(f"[TEST {tests_run}] ✗ ERROR: {test_path.name}: {e}")
                    num_failed += 1

            except queue.Empty:
                # No test available yet, check if producer is still alive
                if not producer.is_alive() and test_queue.empty():
                    # Producer died unexpectedly
                    _ts_print(
                        "[MESON] Producer thread died unexpectedly", file=sys.stderr
                    )
                    break
                continue

    except KeyboardInterrupt:
        _ts_print("[MESON] Interrupted by user", file=sys.stderr)
        handle_keyboard_interrupt_properly()
        raise
    finally:
        # Wait for producer to finish
        producer.join(timeout=10.0)

    # Determine overall success
    overall_success = not compilation_failed and num_failed == 0

    # Only show streaming test summary if there were actual tests run
    # Skip verbose "0/0" output when build was up-to-date
    if tests_run > 0 or compilation_failed:
        _ts_print(f"[MESON] Streaming test execution complete:")
        _ts_print(f"  Tests run: {tests_run}")
        if num_passed > 0:
            _print_success(f"  Passed: {num_passed}")
        else:
            _ts_print(f"  Passed: {num_passed}")
        if num_failed > 0:
            _print_error(f"  Failed: {num_failed}")
        else:
            _ts_print(f"  Failed: {num_failed}")
        if compilation_failed:
            _print_error(f"  Compilation: ✗ FAILED")
        else:
            _print_success(f"  Compilation: ✓ OK")

    return overall_success, num_passed, num_failed


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
