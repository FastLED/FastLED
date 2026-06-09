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
from typing import NamedTuple, Optional, cast

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
    inject_zccache_wrapping,
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
    """Generate/update the Meson native file with tool paths + platform info.

    The compiler entries point at the BARE ctc-clang/ctc-clang++ binaries.
    zccache wrapping is applied later as a post-patch on ``build.ninja`` via
    ``inject_zccache_wrapping`` so the meson configure-phase probes (e.g.
    ``-xc++ -E -v -``) run against the bare compiler and complete in <1s
    instead of timing out under zccache. See issue #2714.
    """
    try:
        fast = _resolve_fast_native_entries()
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
    if compiler.cache_binary and not inject_zccache_wrapping(
        build_dir, compiler.cache_binary
    ):
        _ts_print(
            f"[MESON] Warning: Failed to inject zccache wrapping into build.ninja "
            f"(build_dir={build_dir}, zccache={compiler.cache_binary})",
            file=sys.stderr,
        )
    if normalize_meson_private_include_paths(build_dir):
        _ts_print("[MESON] Normalized private include paths for zccache strict mode")
    _enforce_strict_path_violations(build_dir)


# FastLED's full set of meson.build files, relative to the source dir
# (the project root). Listed explicitly so the wrapper can be invoked
# with `--no-walk` and avoid the recursive `--source-dir` traversal
# whose dominant cost was walking `.venv/` (~100k Python files, ~5s
# cold-with-cache). Keep this list in sync with `find . -name meson.build`
# (excluding scratch/worktree copies). See zackees/zccache#659.
FASTLED_MESON_BUILD_FILES: tuple[str, ...] = (
    "meson.build",
    "tests/meson.build",
    "tests/profile/meson.build",
    "examples/meson.build",
    "ci/meson/native/meson.build",
    "ci/meson/shared/meson.build",
    "ci/meson/wasm/meson.build",
)


class ZccacheCapability(NamedTuple):
    """Result of probing the venv zccache binary.

    ``path`` — absolute path to the binary.
    ``supports_no_walk`` — True when the binary's ``meson configure``
    subcommand accepts ``--no-walk`` (zccache 1.11.14+,
    zackees/zccache#660). Older wrappers (1.11.12 / 1.11.13) still work,
    they just walk ``--source-dir`` and pay the directory-traversal cost
    on every invocation.
    """

    path: Path
    supports_no_walk: bool


def _get_zccache_meson_configure_path() -> Optional[ZccacheCapability]:
    """Resolve the venv zccache binary IF it supports `meson configure`.

    Returns a ``ZccacheCapability`` only when the binary is found and is
    recent enough to ship the configure-cache wrapper (zccache 1.11.12+,
    zackees/zccache#649). Returns ``None`` otherwise so the caller falls
    back to a plain ``meson setup`` invocation.

    Detection is intentionally cheap: we don't parse the version string —
    instead we read ``zccache meson configure --help`` and look for the
    wrapper's docstring (any wrapper-equipped binary) plus the ``--no-walk``
    flag name (1.11.14+). That sidesteps the upgrade window where the
    binary is installed but predates a flag we want to use.
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
    return ZccacheCapability(
        path=venv_zccache,
        supports_no_walk="--no-walk" in result.stdout,
    )


def _write_zccache_input_sidecar(
    *,
    build_dir: Path,
    build_mode: str,
    source_hashes: "SourceHashes",
) -> Optional[Path]:
    """Persist FastLED's source/test/example hashes to a sidecar file the
    zccache configure-cache wrapper can hash via ``--input-file``.

    The sidecar lives in ``build_dir.parent`` (typically ``.build/``) so
    it survives ``bash test --clean`` (which wipes the inner build dir
    but leaves ``.build/`` intact). The per-build-mode suffix prevents
    cross-mode cache pollution. Returns the sidecar path on success or
    ``None`` if the parent isn't writable (in which case the caller
    falls back to the non-input-file wrapper invocation).
    """
    sidecar_dir = build_dir.parent
    try:
        sidecar_dir.mkdir(parents=True, exist_ok=True)
    except OSError:
        return None
    sidecar = sidecar_dir / f".zccache-meson-inputs-{build_mode}.hash"
    payload = "\n".join(
        [
            f"src={source_hashes.src_hash}",
            f"test={source_hashes.test_hash}",
            f"source={source_hashes.source_hash}",
        ]
    )
    try:
        sidecar.write_text(payload, encoding="utf-8")
    except OSError:
        return None
    return sidecar


def build_meson_setup_cmd(
    *,
    native_file_path: Path,
    build_dir: Path,
    build_mode: str,
    enable_examples: bool,
    enable_unit_tests: bool,
    reconfigure: bool,
    source_hashes: Optional["SourceHashes"] = None,
) -> list[str]:
    """Build the ``meson setup [--reconfigure] ...`` command list.

    When the venv ships a zccache (>= 1.11.13) that supports the
    ``meson configure`` wrapper (zackees/zccache#649) plus ``--input-file``
    (zackees/zccache#655, closes zackees/zccache#654), route the
    invocation through that wrapper AND extend the cache key with a
    sidecar file containing FastLED's source/test/example hashes. The
    sidecar lets the wrapper correctly invalidate the cached configure
    tree when those globs change — so the reconfigure path also benefits
    from cache restoration instead of having to bypass the wrapper.

    When the wrapper is 1.11.14+ (zackees/zccache#660), pass ``--no-walk``
    plus an explicit ``--input-file`` for each of FastLED's known
    meson.build paths (see ``FASTLED_MESON_BUILD_FILES``). This skips
    the wrapper's recursive ``--source-dir`` walk entirely — that walk
    was dominated by ``.venv/`` traversal (~100k Python files, ~5s on
    cold-with-cache) and is pure overhead given we already enumerate
    the input set.

    When ``source_hashes`` is unavailable (caller didn't supply, or
    sidecar write failed), the reconfigure path falls back to a direct
    ``meson setup --reconfigure`` invocation. The non-reconfigure path
    still uses the wrapper without the sidecar (the wrapper hashes
    meson.build either way, which is stable for non-source-trigger
    configures).
    """
    meson_exe = get_meson_executable()
    meson_args: list[str] = [
        "--native-file",
        str(native_file_path),
        f"-Dbuild_mode={build_mode}",
        f"-Denable_examples={str(enable_examples).lower()}",
        f"-Denable_unit_tests={str(enable_unit_tests).lower()}",
    ]

    capability = _get_zccache_meson_configure_path()
    sidecar: Optional[Path] = None
    if capability is not None and source_hashes is not None:
        sidecar = _write_zccache_input_sidecar(
            build_dir=build_dir,
            build_mode=build_mode,
            source_hashes=source_hashes,
        )

    # Wrapper path: requires zccache+wrapper. The sidecar is OPTIONAL —
    # without it, the wrapper still works but only invalidates on
    # meson.build changes, so we keep the old reconfigure-bypass guard
    # to avoid stale-tree hits on source-trigger reconfigures.
    if capability is not None and (sidecar is not None or not reconfigure):
        wrapped: list[str] = [
            str(capability.path),
            "meson",
            "configure",
            "--source-dir",
            ".",
            "--build-dir",
            str(build_dir),
            "--meson-bin",
            meson_exe,
        ]
        if capability.supports_no_walk:
            # 1.11.14+: skip the recursive walk; we enumerate inputs.
            wrapped.append("--no-walk")
            for meson_file in FASTLED_MESON_BUILD_FILES:
                wrapped.extend(["--input-file", meson_file])
        if sidecar is not None:
            wrapped.extend(["--input-file", str(sidecar)])
        # `--` separates wrapper flags from the trailing meson args.
        wrapped.append("--")
        wrapped.extend(meson_args)
        return wrapped

    cmd: list[str] = [meson_exe, "setup"]
    if reconfigure:
        cmd.append("--reconfigure")
    cmd.extend([*meson_args[:2], str(build_dir), *meson_args[2:]])
    return cmd


_ZCCACHE_MESON_HIT_MARKER = "[zccache-meson] hit"
_PCH_ARTIFACT_SUFFIXES = (".pch", ".pch.input_hash", ".d.cache")


def _purge_restored_pch_artifacts(build_dir: Path) -> None:
    """Delete PCH binaries and their compile_pch.py sidecars from build_dir.

    zccache meson snapshots capture the whole build dir (zackees/zccache#710),
    so a restore can resurrect a stale clang PCH whose embedded file identities
    silently defeat ``#pragma once`` dedup — along with the ``.input_hash`` /
    ``.d.cache`` sidecars that make ``compile_pch.py`` skip rebuilding it
    forever. Purging after a snapshot hit forces a fresh PCH compile.
    """
    removed = 0
    for root, _dirs, files in os.walk(build_dir):
        for name in files:
            if name.endswith(_PCH_ARTIFACT_SUFFIXES):
                path = Path(root) / name
                try:
                    path.unlink()
                    removed += 1
                except OSError as e:
                    _ts_print(
                        f"[MESON] Warning: could not delete {path}: {e}",
                        file=sys.stderr,
                    )
    if removed:
        _ts_print(
            f"[MESON] Purged {removed} PCH artifact(s) restored by zccache "
            f"meson snapshot (zackees/zccache#710)"
        )


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

        if _ZCCACHE_MESON_HIT_MARKER in stdout:
            _purge_restored_pch_artifacts(build_dir)

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
        if compiler.cache_binary and not inject_zccache_wrapping(
            build_dir, compiler.cache_binary
        ):
            _ts_print(
                f"[MESON] Warning: Failed to inject zccache wrapping into build.ninja "
                f"(build_dir={build_dir}, zccache={compiler.cache_binary})",
                file=sys.stderr,
            )
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
