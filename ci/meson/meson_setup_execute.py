#!/usr/bin/env python3
"""Execution-side helpers for ``setup_meson_build``.

Companion module to ``meson_setup_phases.py``. Where the phases module
handles *detection* (hashes, markers, file changes), this module handles
*execution*: compiler/cache discovery, native-file writing, environment
construction, the skip-setup branch, and the actual ``meson setup`` call.
"""
# pyright: reportMissingImports=false, reportUnknownVariableType=false

import os
import platform
import shutil
import subprocess
import sys
from pathlib import Path
from typing import Optional, cast

from running_process import RunningProcess

from ci.meson.compiler import (
    get_compiler_version,
    get_meson_executable,
)
from ci.meson.io_utils import write_if_different
from ci.meson.meson_markers import (
    _write_configuration_markers,
    cleanup_stale_meson_lockfile,
    inject_ar_optimization_patches,
)
from ci.meson.meson_setup_phases import (
    CompilerDetection,
    MarkerPaths,
    SourceHashes,
)
from ci.meson.native_launchers import (
    _find_zccache_binary,
    _resolve_fast_compiler_binary,
    _resolve_fast_native_entries,
    get_zccache_version,
)
from ci.meson.output import print_banner
from ci.meson.path_normalization import (
    _enforce_strict_path_violations,
    normalize_meson_private_include_paths,
)
from ci.util.global_interrupt_handler import handle_keyboard_interrupt
from ci.util.output_formatter import TimestampFormatter
from ci.util.timestamp_print import ts_print as _ts_print


def detect_compiler_and_cache(markers: MarkerPaths, verbose: bool) -> CompilerDetection:
    """Detect clang-tool-chain wrappers + zccache + version strings.

    Caches the compiler and zccache version strings via marker mtimes so we
    skip the ~3.6s subprocess unless the binary itself has changed.
    """
    in_docker = os.environ.get("FASTLED_DOCKER", "0") == "1"

    cache_binary: Optional[str] = None
    cache_name: Optional[str] = None
    if not in_docker:
        zccache_binary = _find_zccache_binary()
        if zccache_binary:
            cache_binary = zccache_binary
            cache_name = "zccache"
        else:
            _ts_print(
                "[MESON] zccache not found, compilation will proceed without caching. To install, run: bash ./install"
            )
    else:
        _ts_print("[MESON] Skipping compiler cache in Docker (startup delay too long)")

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
            _ts_print("[MESON]   - clang-tool-chain-c")
        if not clangxx_wrapper:
            _ts_print("[MESON]   - clang-tool-chain-cpp")
        if not llvm_ar_wrapper:
            _ts_print("[MESON]   - clang-tool-chain-ar")
        raise RuntimeError("clang-tool-chain wrapper commands not available")

    current_compiler_version = ""
    version_from_cache = False
    clangxx_path = Path(clangxx_wrapper)
    if markers.compiler_version.exists() and clangxx_path.exists():
        try:
            compiler_mtime = clangxx_path.stat().st_mtime
            marker_mtime = markers.compiler_version.stat().st_mtime
            if compiler_mtime < marker_mtime:
                cached_ver = markers.compiler_version.read_text(
                    encoding="utf-8"
                ).strip()
                if cached_ver and cached_ver != "unknown":
                    current_compiler_version = cached_ver
                    version_from_cache = True
        except OSError:
            pass
    if not version_from_cache:
        fast_compiler = _resolve_fast_compiler_binary()
        current_compiler_version = get_compiler_version(
            fast_compiler if fast_compiler else clangxx_wrapper
        )

    current_zccache_version = ""
    if cache_binary:
        zccache_version_from_cache = False
        zccache_path = Path(cache_binary)
        if markers.zccache_version.exists() and zccache_path.exists():
            try:
                zccache_mtime = zccache_path.stat().st_mtime
                zc_marker_mtime = markers.zccache_version.stat().st_mtime
                if zccache_mtime < zc_marker_mtime:
                    cached_zc_ver = markers.zccache_version.read_text(
                        encoding="utf-8"
                    ).strip()
                    if cached_zc_ver:
                        current_zccache_version = cached_zc_ver
                        zccache_version_from_cache = True
            except OSError:
                pass
        if not zccache_version_from_cache:
            current_zccache_version = get_zccache_version(cache_binary)

    if verbose:
        if cache_name:
            print(f"Toolchain: {current_compiler_version} + {cache_name}")
        else:
            print(f"Toolchain: {current_compiler_version}")

    return CompilerDetection(
        clang_wrapper=clang_wrapper,
        clangxx_wrapper=clangxx_wrapper,
        llvm_ar_wrapper=llvm_ar_wrapper,
        cache_binary=cache_binary,
        cache_name=cache_name,
        compiler_version=current_compiler_version,
        zccache_version=current_zccache_version,
    )


def write_meson_native_file(
    *,
    native_file_path: Path,
    compiler: CompilerDetection,
) -> None:
    """Generate/update the Meson native file with tool paths + platform info."""
    try:
        fast = _resolve_fast_native_entries(compiler.cache_binary)
        if fast.cc is not None:
            c_compiler = fast.cc
            cpp_compiler = fast.cxx
            ar_tool = fast.ar
        else:
            c_compiler = f"['{compiler.clang_wrapper}']"
            cpp_compiler = f"['{compiler.clangxx_wrapper}']"
            ar_tool = f"['{compiler.llvm_ar_wrapper}']"

        is_windows = sys.platform.startswith("win") or os.name == "nt"
        is_darwin = sys.platform == "darwin"

        machine = platform.machine().lower()
        if machine in ("x86_64", "amd64"):
            cpu_family = "x86_64"
            cpu = "x86_64"
        elif machine in ("arm64", "aarch64"):
            cpu_family = "aarch64"
            cpu = "aarch64"
        else:
            cpu_family = "x86_64"
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


def build_setup_env(compiler: CompilerDetection) -> dict[str, str]:
    """Build the environment dict for ``meson setup``.

    Uses raw compiler binaries when resolvable (avoids ~3.6s Python wrapper
    overhead) but falls back to the wrappers if clang_tool_chain isn't
    available.
    """
    env = os.environ.copy()
    if compiler.cache_binary:
        env.setdefault("ZCCACHE_STRICT_PATHS", "absolute")

    fast_compiler = _resolve_fast_compiler_binary()
    if fast_compiler:
        try:
            from clang_tool_chain.platform.paths import find_tool_binary

            fast_c = str(find_tool_binary("clang"))
            fast_ar = str(find_tool_binary("llvm-ar"))
            env["CC"] = fast_c
            env["CXX"] = fast_compiler
            env["AR"] = fast_ar
        except KeyboardInterrupt as ki:
            handle_keyboard_interrupt(ki)
        except Exception:
            env["CC"] = compiler.clang_wrapper
            env["CXX"] = compiler.clangxx_wrapper
            env["AR"] = compiler.llvm_ar_wrapper
    else:
        env["CC"] = compiler.clang_wrapper
        env["CXX"] = compiler.clangxx_wrapper
        env["AR"] = compiler.llvm_ar_wrapper

    return env


def handle_skip_meson_setup(
    *,
    build_dir: Path,
    source_dir: Path,
    use_thin_archives: bool,
    markers: MarkerPaths,
    hashes: SourceHashes,
    debug: bool,
    check: bool,
    build_mode: str,
    enable_examples: bool,
    enable_unit_tests: bool,
    compiler: CompilerDetection,
) -> None:
    """Handle the skip-setup branch: thin archive cleanup + missing markers + ar patches."""
    libfastled_a = build_dir / "libfastled.a"
    if libfastled_a.exists():
        try:
            with open(libfastled_a, "rb") as f:
                header = f.read(8)
                is_thin_archive = header == b"!<thin>\n"

            if is_thin_archive and not use_thin_archives:
                _ts_print("[MESON] ⚠️  Detected thin archive from previous build")
                _ts_print(
                    "[MESON] 🗑️  Deleting libfastled.a to force rebuild with regular archive"
                )
                libfastled_a.unlink()
            elif not is_thin_archive and use_thin_archives:
                _ts_print("[MESON] ℹ️  Detected regular archive from previous build")
                _ts_print(
                    "[MESON] 🗑️  Deleting libfastled.a to force rebuild with thin archive"
                )
                libfastled_a.unlink()
        except (OSError, IOError) as e:
            _ts_print(f"[MESON] Warning: Could not check/delete libfastled.a: {e}")

    _write_configuration_markers(
        build_mode_marker=markers.build_mode,
        build_mode=build_mode,
        thin_archive_marker=markers.thin_archive,
        use_thin_archives=use_thin_archives,
        debug_marker=markers.debug,
        debug=debug,
        check_marker=markers.check,
        check=check,
        source_files_marker=markers.source_files,
        current_source_hash=hashes.source_hash,
        current_source_files=hashes.source_files,
        src_files_marker=markers.src_files,
        current_src_hash=hashes.src_hash,
        test_files_marker=markers.test_files,
        current_test_hash=hashes.test_hash,
        compiler_version_marker=markers.compiler_version,
        current_compiler_version=compiler.compiler_version,
        zccache_version_marker=markers.zccache_version,
        current_zccache_version=compiler.zccache_version or None,
        enable_examples_marker=markers.enable_examples,
        enable_examples=enable_examples,
        enable_unit_tests_marker=markers.enable_unit_tests,
        enable_unit_tests=enable_unit_tests,
        only_missing=True,
        verb="Created",
    )

    inject_ar_optimization_patches(build_dir, source_dir)
    if normalize_meson_private_include_paths(build_dir):
        _ts_print("[MESON] Normalized private include paths for zccache strict mode")
    _enforce_strict_path_violations(build_dir)


def _get_zccache_meson_configure_path() -> Optional[Path]:
    """Resolve the venv zccache binary IF it supports `meson configure`.

    Returns the path to ``zccache.exe`` only when the binary is found and
    is recent enough to ship the configure-cache wrapper (zccache 1.11.12+,
    zackees/zccache#649). Returns ``None`` otherwise so the caller falls
    back to a plain ``meson setup`` invocation.

    Detection is intentionally cheap: we don't parse the version string —
    instead we check whether ``zccache meson configure --help`` prints the
    wrapper's docstring rather than passing through to meson. That sidesteps
    the upgrade window where the binary is installed but predates #649.
    """
    script_dir = Path(__file__).resolve().parent
    project_root = script_dir.parent.parent
    is_windows = sys.platform.startswith("win") or os.name == "nt"
    venv_zccache = (
        project_root
        / ".venv"
        / "Scripts"
        / ("zccache.exe" if is_windows else "zccache")
    )
    if not venv_zccache.exists():
        return None
    try:
        # Cheap probe: ask zccache to describe its meson configure subcommand.
        # The wrapper-equipped binary prints "Cache-aware `meson setup`..." at
        # the top; a passthrough binary forwards to real meson which prints
        # "usage: meson [-h]..." instead.
        result = subprocess.run(
            [str(venv_zccache), "meson", "configure", "--help"],
            capture_output=True,
            text=True,
            timeout=10,
            check=False,
        )
    except KeyboardInterrupt as ki:
        handle_keyboard_interrupt(ki)
        raise ki
    except (subprocess.SubprocessError, OSError):
        return None
    if "Cache-aware" not in result.stdout:
        return None
    return venv_zccache


def build_meson_setup_cmd(
    *,
    native_file_path: Path,
    build_dir: Path,
    build_mode: str,
    enable_examples: bool,
    enable_unit_tests: bool,
    reconfigure: bool,
) -> list[str]:
    """Build the ``meson setup [--reconfigure] ...`` command list.

    When the venv ships a zccache (>= 1.11.12) that supports the
    ``meson configure`` wrapper (zackees/zccache#649) AND we are NOT
    forcing a reconfigure, route the invocation through that wrapper so a
    warm hit can restore the configured build directory and skip meson
    entirely. Reconfigures bypass the wrapper because FastLED's
    metadata-cache layer flagged a source/test/example file change — the
    wrapper hashes only ``meson.build`` files and would otherwise serve a
    stale-state hit.
    """
    meson_exe = get_meson_executable()
    meson_args: list[str] = [
        "--native-file",
        str(native_file_path),
        f"-Dbuild_mode={build_mode}",
        f"-Denable_examples={str(enable_examples).lower()}",
        f"-Denable_unit_tests={str(enable_unit_tests).lower()}",
    ]

    if not reconfigure:
        zccache_exe = _get_zccache_meson_configure_path()
        if zccache_exe is not None:
            # The wrapper provides `meson setup <build_dir> <source_dir>`
            # itself; pass `.` as source-dir so it resolves to the cwd that
            # `run_meson_setup_command` sets (the project root). The `--`
            # separator is required so trailing `-D...` args aren't parsed
            # as zccache flags.
            return [
                str(zccache_exe),
                "meson",
                "configure",
                "--source-dir",
                ".",
                "--build-dir",
                str(build_dir),
                "--meson-bin",
                meson_exe,
                "--",
                *meson_args,
            ]

    cmd: list[str] = [meson_exe, "setup"]
    if reconfigure:
        cmd.append("--reconfigure")
    cmd.extend([*meson_args[:2], str(build_dir), *meson_args[2:]])
    return cmd


def run_meson_setup_command(
    *,
    cmd: list[str],
    source_dir: Path,
    build_dir: Path,
    env: dict[str, str],
    markers: MarkerPaths,
    hashes: SourceHashes,
    debug: bool,
    check: bool,
    build_mode: str,
    enable_examples: bool,
    enable_unit_tests: bool,
    use_thin_archives: bool,
    compiler: CompilerDetection,
) -> bool:
    """Run ``meson setup``, with one self-healing retry, then persist markers."""
    print_banner("MESON CONFIGURATION", "⚙️")

    def _run_meson_setup() -> tuple[int, str]:
        proc = RunningProcess(
            cmd,
            cwd=source_dir,
            timeout=600,
            auto_run=True,
            check=False,
            env=env,
            output_formatter=TimestampFormatter(),
        )
        returncode = cast(int, proc.wait(echo=True))
        return returncode, str(proc.stdout)

    def _clear_stale_caches() -> None:
        _ts_print("[MESON] 🔄 Clearing stale test metadata caches...")
        cache_files = [
            build_dir / "tests" / "test_metadata.cache",
            build_dir / "test_list_cache.txt",
            markers.source_files,
        ]
        for cache_file in cache_files:
            if cache_file.exists():
                try:
                    cache_file.unlink()
                    _ts_print(f"[MESON]   Deleted: {cache_file.name}")
                except (OSError, IOError) as e:
                    _ts_print(
                        f"[MESON]   Warning: Could not delete {cache_file.name}: {e}"
                    )

    try:
        cleanup_stale_meson_lockfile(build_dir)
        returncode, stdout = _run_meson_setup()

        if returncode != 0 and "does not exist" in stdout:
            _ts_print(
                "[MESON] ⚠️  Setup failed due to missing file (stale cache detected)"
            )
            _clear_stale_caches()
            _ts_print("[MESON] 🔄 Retrying meson setup...")
            cleanup_stale_meson_lockfile(build_dir)
            returncode, stdout = _run_meson_setup()

        if returncode != 0:
            _ts_print(
                f"[MESON] Setup failed with return code {returncode}", file=sys.stderr
            )
            return False

        _ts_print("[MESON] Setup successful")

        _write_configuration_markers(
            build_mode_marker=markers.build_mode,
            build_mode=build_mode,
            thin_archive_marker=markers.thin_archive,
            use_thin_archives=use_thin_archives,
            debug_marker=markers.debug,
            debug=debug,
            check_marker=markers.check,
            check=check,
            source_files_marker=markers.source_files,
            current_source_hash=hashes.source_hash,
            current_source_files=hashes.source_files,
            src_files_marker=markers.src_files,
            current_src_hash=hashes.src_hash,
            test_files_marker=markers.test_files,
            current_test_hash=hashes.test_hash,
            compiler_version_marker=markers.compiler_version,
            current_compiler_version=compiler.compiler_version,
            zccache_version_marker=markers.zccache_version,
            current_zccache_version=compiler.zccache_version or None,
            enable_examples_marker=markers.enable_examples,
            enable_examples=enable_examples,
            enable_unit_tests_marker=markers.enable_unit_tests,
            enable_unit_tests=enable_unit_tests,
            only_missing=False,
            verb="Saved",
        )

        inject_ar_optimization_patches(build_dir, source_dir)
        if normalize_meson_private_include_paths(build_dir):
            _ts_print(
                "[MESON] Normalized private include paths for zccache strict mode"
            )
        _enforce_strict_path_violations(build_dir)
        return True

    except KeyboardInterrupt as ki:
        handle_keyboard_interrupt(ki)
        raise
    except Exception as e:
        _ts_print(f"[MESON] Setup failed with exception: {e}", file=sys.stderr)
        return False
