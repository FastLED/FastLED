"""The native linker override is part of C++ test cache identity."""

from pathlib import Path

import pytest

from ci.util.fingerprint import FingerprintManager
from ci.util.test_types import (
    FingerprintResult,
    calculate_cpp_test_fingerprint,
    native_linker_signature,
)
from ci.util.test_types import (
    TestArgs as Args,
)


def test_linker_override_invalidates_cpp_and_example_fast_paths(
    tmp_path: Path, monkeypatch: pytest.MonkeyPatch
) -> None:
    monkeypatch.chdir(tmp_path)
    (tmp_path / "src").mkdir()
    (tmp_path / "tests").mkdir()
    (tmp_path / "examples").mkdir()
    linker = tmp_path / "linker"
    linker.write_bytes(b"first")
    monkeypatch.setenv("FASTLED_NATIVE_LINKER", str(linker))
    manager = FingerprintManager(tmp_path / ".cache", build_mode="debug-thin")
    args = Args(build_mode="debug-thin")
    for name in ("cpp_test", "examples"):
        manager.write(
            name, FingerprintResult(hash="cached", status="success", source_max_mtime=0)
        )
    assert manager.check_cpp(args)
    assert manager.check_examples(args)
    signature = native_linker_signature()
    for name in ("cpp_test", "examples"):
        manager.write(
            name,
            FingerprintResult(
                hash="cached",
                status="success",
                source_max_mtime=0,
                native_linker_signature=signature,
            ),
        )
    assert not manager.check_cpp(args)
    assert not manager.check_examples(args)
    linker.write_bytes(b"second")
    assert manager.check_cpp(args)
    assert manager.check_examples(args)


def test_linker_path_and_content_affect_cpp_hash(
    tmp_path: Path, monkeypatch: pytest.MonkeyPatch
) -> None:
    monkeypatch.chdir(tmp_path)
    first = tmp_path / "first"
    second = tmp_path / "second"
    first.write_bytes(b"same")
    second.write_bytes(b"same")
    monkeypatch.setenv("FASTLED_NATIVE_LINKER", str(first))
    first_hash = calculate_cpp_test_fingerprint().hash
    first.write_bytes(b"changed")
    changed_hash = calculate_cpp_test_fingerprint().hash
    assert changed_hash != first_hash
    second.write_bytes(b"changed")
    monkeypatch.setenv("FASTLED_NATIVE_LINKER", str(second))
    assert calculate_cpp_test_fingerprint().hash != changed_hash
    monkeypatch.delenv("FASTLED_NATIVE_LINKER")
    default_hash = calculate_cpp_test_fingerprint().hash
    assert calculate_cpp_test_fingerprint().hash == default_hash


def test_no_override_preserves_legacy_fast_path(
    tmp_path: Path, monkeypatch: pytest.MonkeyPatch
) -> None:
    monkeypatch.chdir(tmp_path)
    monkeypatch.delenv("FASTLED_NATIVE_LINKER", raising=False)
    (tmp_path / "src").mkdir()
    (tmp_path / "tests").mkdir()
    (tmp_path / "examples").mkdir()
    manager = FingerprintManager(tmp_path / ".cache", build_mode="debug-thin")
    for name in ("cpp_test", "examples"):
        manager.write(
            name, FingerprintResult(hash="cached", status="success", source_max_mtime=0)
        )
    args = Args(build_mode="debug-thin")
    assert not manager.check_cpp(args)
    assert not manager.check_examples(args)


def test_empty_override_is_not_treated_as_default(
    tmp_path: Path, monkeypatch: pytest.MonkeyPatch
) -> None:
    monkeypatch.chdir(tmp_path)
    monkeypatch.delenv("FASTLED_NATIVE_LINKER", raising=False)
    assert native_linker_signature() == ""
    monkeypatch.setenv("FASTLED_NATIVE_LINKER", "")
    assert native_linker_signature() != ""
