#!/usr/bin/env python3
"""Meson build configuration and setup orchestration for FastLED.

This module is the public entry point for ``setup_meson_build`` and
``perform_ninja_maintenance``. The bulk of the setup logic lives in dedicated
helper modules:

- ``ci.meson.native_launchers``: ctc-* native launcher resolution + zccache
- ``ci.meson.path_normalization``: zccache strict-path normalization
- ``ci.meson.meson_markers``: per-build marker files + AR optimization patches
- ``ci.meson.meson_cleanup``: build artifact cleanup + LLVM tool detection
- ``ci.meson.meson_setup_phases``: state detection helpers (hashes, markers, file changes)
- ``ci.meson.meson_setup_execute``: execution helpers (compiler detect, native file, run setup)

Names re-exported from this module are kept stable for external callers
(``wasm_build``, ``test_runner``, regression tests).
"""
# pyright: reportMissingImports=false, reportUnknownVariableType=false

import os
import sys
import time
from pathlib import Path

from running_process import RunningProcess

from ci.meson.compiler import check_meson_version_compatibility
from ci.meson.meson_cleanup import (
    CleanupResult,
    cleanup_build_artifacts,
    detect_system_llvm_tools,
)
from ci.meson.meson_markers import (
    cleanup_stale_meson_lockfile,
    inject_ar_optimization_patches,
)
from ci.meson.meson_setup_execute import (
    build_meson_setup_cmd,
    build_setup_env,
    detect_compiler_and_cache,
    handle_skip_meson_setup,
    run_meson_setup_command,
    write_meson_native_file,
)
from ci.meson.meson_setup_phases import (
    MarkerPaths,
    check_meson_build_modified,
    check_obsolete_zig_wrappers,
    check_reconfigure_markers,
    compute_source_hashes,
    detect_example_file_changes,
    detect_test_file_changes,
)
from ci.meson.native_launchers import (
    EmccNativeLaunchers,
    FastNativeEntries,
    WasmNativeEntries,
    _ensure_emcc_native_launcher,
    _find_zccache_binary,
    get_zccache_version,
    resolve_wasm_native_entries,
)
from ci.meson.path_normalization import (
    _enforce_strict_path_violations,
    find_strict_path_violations,
    is_ar_content_preserving_active,
    normalize_meson_private_include_paths,
)
from ci.util.global_interrupt_handler import handle_keyboard_interrupt
from ci.util.output_formatter import TimestampFormatter
from ci.util.timestamp_print import ts_print as _ts_print


# Re-exported names — kept here so external callers don't break.
__all__ = [
    "CleanupResult",
    "EmccNativeLaunchers",
    "FastNativeEntries",
    "WasmNativeEntries",
    "_ensure_emcc_native_launcher",
    "_find_zccache_binary",
    "cleanup_build_artifacts",
    "cleanup_stale_meson_lockfile",
    "detect_system_llvm_tools",
    "find_strict_path_violations",
    "get_zccache_version",
    "inject_ar_optimization_patches",
    "is_ar_content_preserving_active",
    "normalize_meson_private_include_paths",
    "perform_ninja_maintenance",
    "resolve_wasm_native_entries",
    "setup_meson_build",
]


def setup_meson_build(
    source_dir: Path,
    build_dir: Path,
    reconfigure: bool = False,
    debug: bool = False,
    check: bool = False,
    build_mode: "str | None" = None,
    verbose: bool = False,
    enable_examples: bool = True,
    enable_unit_tests: bool = True,
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
        enable_examples: Enable example compilation targets (default: True)
        enable_unit_tests: Enable unit test compilation targets (default: True)

    Returns:
        True if setup successful, False otherwise
    """
    # Derive build_mode from debug flag if not explicitly provided
    if build_mode is None:
        build_mode = "debug" if debug else "quick"
    if build_mode == "debug":
        debug = True
    elif build_mode in ("quick", "release", "profile"):
        debug = False

    build_dir.mkdir(parents=True, exist_ok=True)
    meson_info = build_dir / "meson-info"
    already_configured = meson_info.exists()

    force_reconfigure = False
    force_reconfigure_reason: "str | None" = None
    compiler_version_changed = False

    # CRITICAL: Meson 1.9.x and 1.10.x produce incompatible build directories.
    # If we detect a mismatch, auto-heal by forcing reconfigure + artifact cleanup.
    if already_configured:
        is_compatible, compatibility_message = check_meson_version_compatibility(
            build_dir
        )
        if not is_compatible:
            _ts_print("=" * 80)
            _ts_print(
                "[MESON] ⚠️  MESON VERSION INCOMPATIBILITY DETECTED - AUTO-HEALING"
            )
            _ts_print("=" * 80)
            _ts_print(f"[MESON] {compatibility_message}")
            _ts_print("[MESON]")
            _ts_print("[MESON] This typically happens when:")
            _ts_print("[MESON]   1. Using a system meson instead of the venv meson")
            _ts_print("[MESON]   2. The pyproject.toml meson version was upgraded")
            _ts_print("[MESON]")
            _ts_print(
                "[MESON] 🔧 Auto-fix: Forcing reconfiguration with current meson version"
            )
            _ts_print("=" * 80)
            force_reconfigure = True
            force_reconfigure_reason = "meson version mismatch (auto-healing)"
            cleanup_build_artifacts(build_dir, "Meson version mismatch")

    # Thin archives — enabled for clang-tool-chain LLVM toolchain; off via env var.
    disable_thin_archives = os.environ.get("FASTLED_DISABLE_THIN_ARCHIVES", "0") == "1"
    use_thin_archives = not disable_thin_archives
    if not use_thin_archives and os.environ.get("FASTLED_DISABLE_THIN_ARCHIVES"):
        _ts_print(
            "[MESON] ⚠️  Thin archives DISABLED (FASTLED_DISABLE_THIN_ARCHIVES set)"
        )

    markers = MarkerPaths.for_build_dir(build_dir)
    hashes = compute_source_hashes(source_dir, markers)

    # Marker-based reconfigure detection (consolidated message + cleanup).
    if already_configured:
        decision = check_reconfigure_markers(
            build_dir=build_dir,
            markers=markers,
            hashes=hashes,
            debug=debug,
            check=check,
            build_mode=build_mode,
            enable_examples=enable_examples,
            enable_unit_tests=enable_unit_tests,
            use_thin_archives=use_thin_archives,
        )
        if decision.force_reconfigure:
            force_reconfigure = True
            force_reconfigure_reason = decision.force_reason

    # meson.build files modified after build.ninja → reconfigure.
    if already_configured and check_meson_build_modified(source_dir, build_dir):
        force_reconfigure = True
        force_reconfigure_reason = "meson.build modified"

    # Test files added/removed → reconfigure.
    if already_configured and detect_test_file_changes(
        source_dir, build_dir, hashes.max_tests_dir_mtime
    ):
        force_reconfigure = True
        force_reconfigure_reason = "test files changed"

    # Example files added/removed/modified → reconfigure.
    if already_configured and detect_example_file_changes(source_dir, build_dir):
        force_reconfigure = True
        force_reconfigure_reason = "example files changed"

    skip_meson_setup = already_configured and not reconfigure and not force_reconfigure
    native_file_path = build_dir / "meson_native.txt"

    # Build the meson setup command (or defer it for compiler-version-late reconfigure).
    cmd: "list[str] | None" = None
    if skip_meson_setup:
        pass
    elif already_configured and (reconfigure or force_reconfigure):
        reason = (
            force_reconfigure_reason
            if (force_reconfigure and force_reconfigure_reason)
            else (
                "configuration changed" if force_reconfigure else "explicitly requested"
            )
        )
        _ts_print(f"[MESON] Reconfiguring build directory ({reason}): {build_dir}")
        cmd = build_meson_setup_cmd(
            native_file_path=native_file_path,
            build_dir=build_dir,
            build_mode=build_mode,
            enable_examples=enable_examples,
            enable_unit_tests=enable_unit_tests,
            reconfigure=True,
            source_hashes=hashes,
        )
    else:
        _ts_print(f"[MESON] Setting up build directory: {build_dir}")
        cmd = build_meson_setup_cmd(
            native_file_path=native_file_path,
            build_dir=build_dir,
            build_mode=build_mode,
            enable_examples=enable_examples,
            enable_unit_tests=enable_unit_tests,
            reconfigure=False,
            source_hashes=hashes,
        )

    check_obsolete_zig_wrappers(source_dir)

    compiler = detect_compiler_and_cache(markers, verbose)

    # Late-stage compiler-version check: requires the detected compiler.
    if already_configured:
        compiler_version_reason: "str | None" = None
        if markers.compiler_version.exists():
            try:
                last_compiler_version = markers.compiler_version.read_text().strip()
                if last_compiler_version != compiler.compiler_version:
                    compiler_version_reason = f"compiler version changed: {last_compiler_version} → {compiler.compiler_version}"
                    compiler_version_changed = True
            except (OSError, IOError):
                compiler_version_reason = "compiler version marker unreadable"
        else:
            compiler_version_reason = "compiler version marker missing"

        if compiler_version_reason:
            if not force_reconfigure:
                _ts_print(
                    f"[MESON] 🔄 Reconfiguration required: {compiler_version_reason}"
                )
            force_reconfigure = True
            force_reconfigure_reason = compiler_version_reason
            skip_meson_setup = False

        if compiler_version_changed:
            cleanup_build_artifacts(build_dir, "Compiler version changed")

        # Late-stage zccache version check
        if compiler.zccache_version:
            zccache_version_reason: "str | None" = None
            if markers.zccache_version.exists():
                try:
                    last_zccache_version = markers.zccache_version.read_text().strip()
                    if last_zccache_version != compiler.zccache_version:
                        zccache_version_reason = f"zccache version changed: {last_zccache_version} → {compiler.zccache_version}"
                except (OSError, IOError):
                    zccache_version_reason = "zccache version marker unreadable"
            else:
                zccache_version_reason = "zccache version marker missing"

            if zccache_version_reason:
                if not force_reconfigure:
                    _ts_print(
                        f"[MESON] 🔄 Reconfiguration required: {zccache_version_reason}"
                    )
                force_reconfigure = True
                force_reconfigure_reason = zccache_version_reason
                skip_meson_setup = False
                cleanup_build_artifacts(build_dir, "zccache version changed")

        # If compiler/zccache version forced reconfigure but we hadn't built a cmd yet
        if force_reconfigure and cmd is None:
            _ts_print(f"[MESON] Reconfiguring build directory: {build_dir}")
            cmd = build_meson_setup_cmd(
                native_file_path=native_file_path,
                build_dir=build_dir,
                build_mode=build_mode,
                enable_examples=enable_examples,
                enable_unit_tests=enable_unit_tests,
                reconfigure=True,
            )

    write_meson_native_file(native_file_path=native_file_path, compiler=compiler)

    env = build_setup_env(compiler)

    if skip_meson_setup:
        handle_skip_meson_setup(
            build_dir=build_dir,
            source_dir=source_dir,
            use_thin_archives=use_thin_archives,
            markers=markers,
            hashes=hashes,
            debug=debug,
            check=check,
            build_mode=build_mode,
            enable_examples=enable_examples,
            enable_unit_tests=enable_unit_tests,
            compiler=compiler,
        )
        return True

    assert cmd is not None, "cmd should be set when not skipping meson setup"
    return run_meson_setup_command(
        cmd=cmd,
        source_dir=source_dir,
        build_dir=build_dir,
        env=env,
        markers=markers,
        hashes=hashes,
        debug=debug,
        check=check,
        build_mode=build_mode,
        enable_examples=enable_examples,
        enable_unit_tests=enable_unit_tests,
        use_thin_archives=use_thin_archives,
        compiler=compiler,
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
    marker_file = build_dir / ".ninja_deps_maintenance"

    if marker_file.exists():
        try:
            last_maintenance = marker_file.stat().st_mtime
            time_since_maintenance = time.time() - last_maintenance
            hours_since_maintenance = time_since_maintenance / 3600
            if hours_since_maintenance < 24:
                return True
        except (OSError, IOError):
            pass

    _ts_print("[MESON] 🔧 Performing periodic Ninja dependency database maintenance...")

    try:
        repair_proc = RunningProcess(
            ["ninja", "-C", str(build_dir), "-t", "recompact"],
            timeout=60,
            auto_run=True,
            check=False,
            env=os.environ.copy(),
            output_formatter=TimestampFormatter(),
        )
        returncode = repair_proc.wait(echo=True)

        if returncode == 0:
            _ts_print(
                "[MESON] ✓ Dependency database maintenance completed successfully"
            )
            try:
                marker_file.touch()
            except (OSError, IOError):
                pass
            return True
        _ts_print(
            "[MESON] ⚠️  Maintenance completed with warnings (non-fatal)",
            file=sys.stderr,
        )
        return True

    except KeyboardInterrupt as ki:
        handle_keyboard_interrupt(ki)
        raise
    except Exception as e:
        _ts_print(
            f"[MESON] ⚠️  Maintenance failed with exception: {e} (non-fatal)",
            file=sys.stderr,
        )
        return True
