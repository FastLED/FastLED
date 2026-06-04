#!/usr/bin/env python3
"""Build artifact cleanup and system LLVM tool detection."""

import glob
import subprocess
from dataclasses import dataclass
from pathlib import Path

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

    # Check for system lld
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
