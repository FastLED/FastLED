"""Regression tests for the zccache mtime stabilizer (FastLED#4212).

The stabilizer only ever globbed `*.dll`, so on Linux and macOS -- where the
build system emits `*.so` / `*.dylib` -- it matched nothing and silently
touched no shared libraries at all. Cache-restored outputs kept their original
(old) mtimes, and the #3011 stale-artifact guard then failed every affected
test with "DLL mtime predates this build's start".
"""

import os
from dataclasses import dataclass
from pathlib import Path

from typeguard import typechecked

from ci.meson.mtime_stabilizer import restore_executable_bits, stabilize_dll_mtimes


@typechecked
@dataclass
class BuildTree:
    """The two paths a stabilizer test needs to reason about."""

    symbols: Path
    dll: Path


def _build_tree(root: Path, suffix: str) -> BuildTree:
    """Lay out the parts of a build directory the stabilizer looks at."""
    native = root / "ci" / "meson" / "native"
    (native / f"fastled.{suffix}.p").mkdir(parents=True)
    symbols = native / f"fastled.{suffix}.p" / f"fastled.{suffix}.symbols"
    symbols.write_text("symbols")

    (root / "tests").mkdir()
    (root / "examples").mkdir()
    dll = root / "tests" / f"cled_controller.{suffix}"
    dll.write_text("payload")
    return BuildTree(symbols=symbols, dll=dll)


def _age(path: Path, seconds: float) -> None:
    stamp = path.stat().st_mtime - seconds
    os.utime(path, (stamp, stamp))


def test_stabilizes_every_host_shared_library_suffix(tmp_path: Path) -> None:
    """A cache-restored output is touched on Windows, Linux and macOS alike."""
    for suffix in ("dll", "so", "dylib"):
        root = tmp_path / suffix
        root.mkdir()
        tree = _build_tree(root, suffix)
        symbols, dll = tree.symbols, tree.dll

        # zccache restores the link result with its original, older mtime.
        _age(dll, 7 * 3600)
        assert dll.stat().st_mtime < symbols.stat().st_mtime

        assert stabilize_dll_mtimes(root) == 1, f"{suffix} output not stabilized"
        assert dll.stat().st_mtime > symbols.stat().st_mtime


def test_leaves_already_fresh_outputs_alone(tmp_path: Path) -> None:
    """A fresh link must not be touched, so real staleness still shows."""
    tree = _build_tree(tmp_path, "so")
    symbols, dll = tree.symbols, tree.dll
    _age(symbols, 60)
    before = dll.stat().st_mtime

    assert stabilize_dll_mtimes(tmp_path) == 0
    assert dll.stat().st_mtime == before


def test_symbols_file_alone_establishes_the_input_mtime(tmp_path: Path) -> None:
    """libcrash_handler.a is absent here; the suffix-matched symbols file must
    still be found, otherwise the stabilizer bails out with nothing to do."""
    tree = _build_tree(tmp_path, "so")
    symbols, dll = tree.symbols, tree.dll
    _age(dll, 7 * 3600)

    assert not (tmp_path / "ci" / "meson" / "native" / "libcrash_handler.a").exists()
    assert stabilize_dll_mtimes(tmp_path) == 1
    assert dll.stat().st_mtime > symbols.stat().st_mtime


def _elf(path: Path, mode: int = 0o644) -> Path:
    path.write_bytes(b"\x7fELF" + b"\x00" * 60)
    os.chmod(path, mode)
    return path


def test_restores_lost_executable_bit(tmp_path: Path) -> None:
    """A cache-restored binary comes back without +x; the run dies on it."""
    (tmp_path / "tests").mkdir()
    (tmp_path / "examples").mkdir()
    runner = _elf(tmp_path / "tests" / "runner")
    other = _elf(tmp_path / "examples" / "example_runner")

    assert restore_executable_bits(tmp_path) == 2
    assert runner.stat().st_mode & 0o111
    assert other.stat().st_mode & 0o111
    # Idempotent: a second pass has nothing left to do.
    assert restore_executable_bits(tmp_path) == 0


def test_leaves_libraries_and_data_files_alone(tmp_path: Path) -> None:
    """Shared objects and non-binaries must not be marked executable."""
    (tmp_path / "tests").mkdir()
    (tmp_path / "examples").mkdir()
    lib = _elf(tmp_path / "tests" / "fastled_core.so")
    data = tmp_path / "tests" / "testlog.json"
    data.write_text("{}")

    assert restore_executable_bits(tmp_path) == 0
    assert not lib.stat().st_mode & 0o111
    assert not data.stat().st_mode & 0o111
