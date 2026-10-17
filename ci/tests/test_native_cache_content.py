"""Native skip gates must survive source mtimes restored by PCH replay."""

import json
import os
from pathlib import Path

import pytest

from ci.early_exit_cache import (
    fingerprint_aux_hash,
    full_run_cache,
    native_source_content_hash,
    ninja_skip,
)
from ci.meson.cache_utils import (
    capture_source_watermarks,
    check_full_run_cache,
    check_ninja_skip,
    save_full_run_result,
    save_ninja_skip_state,
)
from ci.util.fingerprint import FingerprintManager
from ci.util.test_types import TestArgs as NativeTestArgs


@pytest.mark.parametrize("touch_only", [False, True])
def test_native_skip_gates_verify_content(
    tmp_path: Path, monkeypatch: pytest.MonkeyPatch, touch_only: bool
) -> None:
    monkeypatch.chdir(tmp_path)
    header = tmp_path / "src/fl/build/input.h"
    header.parent.mkdir(parents=True)
    header.write_text("#define VALUE 2\n")
    os.utime(header, (100, 100))
    newer = tmp_path / "src/newer.h"
    newer.write_text("// unrelated newest input\n")
    os.utime(newer, (1000, 1000))
    build = tmp_path / ".build/meson-quick"
    (build / "tests").mkdir(parents=True)
    (build / "build.ninja").write_text("# fixture\n")
    (build / "tests/example.so").write_bytes(b"tested B")
    marks = capture_source_watermarks()
    save_ninja_skip_state(build, "example")
    save_full_run_result(build, 1, 1, 0.1, watermarks=marks)
    aux = fingerprint_aux_hash("cpp_test")
    assert check_ninja_skip(build, "example")
    assert ninja_skip(build, "example")
    assert check_full_run_cache(build)
    assert full_run_cache(build)
    if touch_only:
        os.utime(header, (200, 200))
    else:
        header.write_text("#define VALUE 1\n")
        os.utime(header, (100, 100))
    assert check_ninja_skip(build, "example") is touch_only
    assert ninja_skip(build, "example") is touch_only
    assert (check_full_run_cache(build) is not None) is touch_only
    assert (full_run_cache(build) is not None) is touch_only
    assert (fingerprint_aux_hash("cpp_test") == aux) is touch_only


def test_content_signature_tracks_inventory_and_excludes_artifacts(
    tmp_path: Path,
) -> None:
    source = tmp_path / "src/fl/build/input.h"
    source.parent.mkdir(parents=True)
    source.write_text("// canonical build source\n")
    baseline = native_source_content_hash(tmp_path)
    for directory in (".build", "examples/node_modules", "tests/.cache"):
        artifact = tmp_path / directory / "generated.h"
        artifact.parent.mkdir(parents=True)
        artifact.write_text("// artifact\n")
    assert native_source_content_hash(tmp_path) == baseline
    added = tmp_path / "src/extra.inc"
    added.write_text("// new input\n")
    assert native_source_content_hash(tmp_path) != baseline
    added.rename(tmp_path / "extra.inc")
    assert native_source_content_hash(tmp_path) == baseline


def test_missing_content_signature_fails_closed(
    tmp_path: Path, monkeypatch: pytest.MonkeyPatch
) -> None:
    monkeypatch.chdir(tmp_path)
    build = tmp_path / ".build/meson-quick"
    (build / "tests").mkdir(parents=True)
    (build / "build.ninja").write_text("# fixture\n")
    (build / "tests/example.so").write_bytes(b"tested B")
    save_ninja_skip_state(build, "example")
    save_full_run_result(build, 1, 1, 0.1, watermarks=capture_source_watermarks())
    for name in (".ninja_skip_state.json", ".full_run_cache.json"):
        path = build / name
        entry = json.loads(path.read_text())
        entry.pop("native_source_hash")
        path.write_text(json.dumps(entry))
    assert not check_ninja_skip(build, "example")
    assert not ninja_skip(build, "example")
    assert check_full_run_cache(build) is None
    assert full_run_cache(build) is None


def test_normal_fingerprint_fallback_cannot_reuse_changed_native_input(
    tmp_path: Path, monkeypatch: pytest.MonkeyPatch
) -> None:
    monkeypatch.chdir(tmp_path)
    header = tmp_path / "src/input.inc"
    header.parent.mkdir()
    header.write_text("#define VALUE 2\n")
    os.utime(header, (100, 100))
    args = NativeTestArgs()
    manager = FingerprintManager(tmp_path / ".cache")
    assert manager.check_cpp(args)
    manager.save_success("cpp_test")
    assert not FingerprintManager(tmp_path / ".cache").check_cpp(args)
    header.write_text("#define VALUE 1\n")
    os.utime(header, (100, 100))
    assert FingerprintManager(tmp_path / ".cache").check_cpp(args)


def test_ninja_writer_preserves_precompile_content(
    tmp_path: Path, monkeypatch: pytest.MonkeyPatch
) -> None:
    monkeypatch.chdir(tmp_path)
    header = tmp_path / "src/input.h"
    header.parent.mkdir()
    header.write_text("#define VALUE 1\n")
    before_compile = native_source_content_hash()
    header.write_text("#define VALUE 2\n")
    build = tmp_path / ".build/meson-quick"
    (build / "tests").mkdir(parents=True)
    (build / "build.ninja").write_text("# fixture\n")
    (build / "tests/example.so").write_bytes(b"compiled A")
    save_ninja_skip_state(build, "example", source_hash=before_compile)
    assert not check_ninja_skip(build, "example")
    assert not ninja_skip(build, "example")


def test_build_driver_changes_invalidate_cached_results(
    tmp_path: Path, monkeypatch: pytest.MonkeyPatch
) -> None:
    monkeypatch.chdir(tmp_path)
    driver = tmp_path / "ci/meson/streaming.py"
    driver.parent.mkdir(parents=True)
    driver.write_text("# original build driver\n")
    build = tmp_path / ".build/meson-quick"
    (build / "tests").mkdir(parents=True)
    (build / "build.ninja").write_text("# fixture\n")
    (build / "tests/example.so").write_bytes(b"compiled A")
    save_ninja_skip_state(build, "example")
    save_full_run_result(build, 1, 1, 0.1, watermarks=capture_source_watermarks())
    assert check_ninja_skip(build, "example")
    assert ninja_skip(build, "example")
    assert check_full_run_cache(build)
    assert full_run_cache(build)
    driver.write_text("# changed build driver\n")
    assert not check_ninja_skip(build, "example")
    assert not ninja_skip(build, "example")
    assert check_full_run_cache(build) is None
    assert full_run_cache(build) is None
