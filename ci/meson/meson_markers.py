#!/usr/bin/env python3
"""Configuration marker files and AR optimization patches for Meson builds.

Marker files track per-build-dir state (build mode, debug flag, compiler
version, source hashes, etc.) so we can detect when a reconfigure is
required. AR optimization patches edit ``build.ninja`` in-place to apply two
optimizations to the STATIC_LINKER rule: ``restat=1`` (lets ninja skip the
relink cascade when the archive is unchanged) and the
``ar_content_preserving.py`` wrapper (preserves the archive's mtime when
content is unchanged).

Also contains stale-lockfile cleanup for the Windows DirectoryLock bug.
"""
# pyright: reportMissingImports=false, reportUnknownVariableType=false

import os
import sys
from pathlib import Path
from typing import Optional

from ci.meson.io_utils import atomic_write_text
from ci.meson.path_normalization import _ar_opt_status_cache
from ci.util.global_interrupt_handler import handle_keyboard_interrupt
from ci.util.timestamp_print import ts_print as _ts_print


def cleanup_stale_meson_lockfile(build_dir: Path) -> bool:
    """Remove a stale ``meson-private/meson.lock`` left by a killed meson process.

    On Windows, meson <= 1.10.x crashed cryptically in ``DirectoryLock.__enter__``
    when the underlying lockfile open() raised an OSError (e.g., from a stale
    lockfile left by a killed/abandoned meson process). meson 1.11.0 fixed the
    crash itself, but the underlying cause — a stale ``meson-private/meson.lock``
    file — still produces an avoidable setup failure on Windows. Deleting the
    stale lockfile before invoking ``meson setup`` avoids the failure entirely.

    Args:
        build_dir: The meson build directory (parent of ``meson-private``).

    Returns:
        True if a stale lockfile was found and successfully removed, False
        otherwise (including when no lockfile exists or removal failed).
    """
    # Stale lockfile cleanup is intended for the Windows DirectoryLock bug.
    # On POSIX, meson uses fcntl/flock (advisory) — removing an in-use lockfile
    # could let a concurrent meson setup bypass the intended lock.
    if os.name != "nt":
        return False

    lockfile = build_dir / "meson-private" / "meson.lock"
    if not lockfile.exists():
        return False
    try:
        lockfile.unlink()
        _ts_print(f"[MESON] Removed stale lockfile: {lockfile}")
        return True
    except KeyboardInterrupt as ki:
        handle_keyboard_interrupt(ki)
        raise
    except OSError as e:
        # Be tolerant of file-in-use errors on Windows — if removal fails, log
        # and continue. The caller will hit the original OSError from meson,
        # which is now visible thanks to the 1.11.0 bump.
        _ts_print(f"[MESON] Warning: Could not remove stale lockfile {lockfile}: {e}")
        return False


def _write_configuration_markers(
    *,
    build_mode_marker: Path,
    build_mode: str,
    thin_archive_marker: Path,
    use_thin_archives: bool,
    debug_marker: Path,
    debug: bool,
    check_marker: Path,
    check: bool,
    source_files_marker: Path,
    current_source_hash: str,
    current_source_files: list[str],
    src_files_marker: Path,
    current_src_hash: str,
    test_files_marker: Path,
    current_test_hash: str,
    compiler_version_marker: Path,
    current_compiler_version: str,
    zccache_version_marker: Optional[Path] = None,
    current_zccache_version: Optional[str] = None,
    enable_examples_marker: Optional[Path] = None,
    enable_examples: Optional[bool] = None,
    enable_unit_tests_marker: Optional[Path] = None,
    enable_unit_tests: Optional[bool] = None,
    only_missing: bool = False,
    verb: str = "Saved",
) -> None:
    """Write configuration marker files for cache invalidation.

    Args:
        only_missing: If True, only write markers that don't exist yet (migration path).
                      If False, write all markers unconditionally (after meson setup).
        verb: Display verb for log messages ("Saved" or "Created").
    """
    markers: list[tuple[Path, str, str]] = [
        (build_mode_marker, build_mode, f"build_mode: {build_mode}"),
        (
            thin_archive_marker,
            str(use_thin_archives),
            f"thin archive: {use_thin_archives}",
        ),
        (debug_marker, str(debug), f"debug: {debug}"),
        (check_marker, str(check), f"check: {check}"),
        (
            compiler_version_marker,
            current_compiler_version,
            f"compiler version: {current_compiler_version}",
        ),
    ]

    # Add optional zccache version marker
    if zccache_version_marker is not None and current_zccache_version is not None:
        markers.append(
            (
                zccache_version_marker,
                current_zccache_version,
                f"zccache version: {current_zccache_version}",
            )
        )

    # Add optional enable_examples/enable_unit_tests markers
    if enable_examples_marker is not None and enable_examples is not None:
        markers.append(
            (
                enable_examples_marker,
                str(enable_examples),
                f"enable_examples: {enable_examples}",
            )
        )
    if enable_unit_tests_marker is not None and enable_unit_tests is not None:
        markers.append(
            (
                enable_unit_tests_marker,
                str(enable_unit_tests),
                f"enable_unit_tests: {enable_unit_tests}",
            )
        )

    for marker_path, value, description in markers:
        if only_missing and marker_path.exists():
            continue
        try:
            atomic_write_text(marker_path, value)
            _ts_print(f"[MESON] ✅ {verb} {description}")
        except (OSError, IOError) as e:
            _ts_print(f"[MESON] Warning: Could not write {marker_path.name}: {e}")

    # Source files markers: write combined hash (backward compat) and split hashes
    if current_source_hash:
        if not only_missing or not source_files_marker.exists():
            try:
                atomic_write_text(source_files_marker, current_source_hash)
                _ts_print(
                    f"[MESON] ✅ {verb} source/test files hash ({len(current_source_files)} files)"
                )
            except (OSError, IOError) as e:
                _ts_print(f"[MESON] Warning: Could not write source files marker: {e}")
    # Write split markers for granular change detection
    if current_src_hash:
        if not only_missing or not src_files_marker.exists():
            try:
                atomic_write_text(src_files_marker, current_src_hash)
            except (OSError, IOError):
                pass
    if current_test_hash:
        if not only_missing or not test_files_marker.exists():
            try:
                atomic_write_text(test_files_marker, current_test_hash)
            except (OSError, IOError):
                pass


def inject_ar_optimization_patches(build_dir: Path, source_dir: Path) -> bool:
    """
    Apply both ar optimization patches to build.ninja in a single read-modify-write.

    Reads build.ninja only ONCE and writes at most ONCE.

    Also populates _ar_opt_status_cache so that a subsequent call to
    is_ar_content_preserving_active() can return from cache without reading
    build.ninja a third time. This saves ~4ms per incremental build.

    Patches applied:
    1. ``restat = 1`` added to STATIC_LINKER rule (enables ninja cascade suppression)
    2. ar_content_preserving.py wrapper prepended to STATIC_LINKER command

    Args:
        build_dir: Meson build directory containing build.ninja
        source_dir: Project source root (parent of ci/)

    Returns:
        True if both patches are active after this call, False if not applicable
    """
    ar_wrapper_script = source_dir / "ci" / "meson" / "ar_content_preserving.py"
    build_ninja_path = build_dir / "build.ninja"

    if not ar_wrapper_script.exists() or not build_ninja_path.exists():
        return False

    try:
        content = build_ninja_path.read_text(encoding="utf-8")
    except OSError:
        return False

    if "rule STATIC_LINKER" not in content:
        return False

    # Idempotency check: if both patches already present, no write needed
    has_ar_wrapper = ar_wrapper_script.name in content
    has_restat = "restat = 1" in content

    if has_ar_wrapper and has_restat:
        _ar_opt_status_cache[str(build_dir)] = True
        return True

    # Need to apply one or both patches; find the STATIC_LINKER rule block
    static_linker_pos = content.find("rule STATIC_LINKER")
    if static_linker_pos == -1:
        return False

    rule_end = content.find("\n\n", static_linker_pos)
    if rule_end == -1:
        rule_end = len(content)

    rule_text = content[static_linker_pos:rule_end]
    new_rule_text = rule_text

    # Patch 1: restat = 1 (enables ninja cascade suppression after archive)
    if not has_restat:
        description_line = " description = Linking static target $out"
        if description_line in new_rule_text:
            new_rule_text = new_rule_text.replace(
                description_line,
                description_line + "\n restat = 1",
            )

    # Patch 2: ar_content_preserving.py wrapper (preserves mtime when content unchanged)
    if not has_ar_wrapper:
        command_prefix = " command = "
        if command_prefix in new_rule_text:
            cmd_start = new_rule_text.find(command_prefix)
            cmd_line_start = cmd_start + len(command_prefix)
            cmd_line_end = new_rule_text.find("\n", cmd_line_start)
            if cmd_line_end == -1:
                cmd_line_end = len(new_rule_text)
            original_cmd = new_rule_text[cmd_line_start:cmd_line_end]
            if "$LINK_ARGS $out $in" in original_cmd:
                python_exe = sys.executable
                new_cmd = f'"{python_exe}" "{ar_wrapper_script}" {original_cmd}'
                new_rule_text = (
                    new_rule_text[:cmd_line_start]
                    + new_cmd
                    + new_rule_text[cmd_line_end:]
                )

    # Write modified content if anything changed (at most one write)
    if new_rule_text != rule_text:
        new_content = content[:static_linker_pos] + new_rule_text + content[rule_end:]
        try:
            build_ninja_path.write_text(new_content, encoding="utf-8")
        except OSError:
            _ar_opt_status_cache[str(build_dir)] = False
            return False

    # Verify both patches are now active and populate cache
    patches_active = (
        ar_wrapper_script.name in new_rule_text and "restat = 1" in new_rule_text
    )
    _ar_opt_status_cache[str(build_dir)] = patches_active
    return patches_active
