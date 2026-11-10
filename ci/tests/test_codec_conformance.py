"""Compiler-provider selection for the standalone native conformance harness."""

import sys
from pathlib import Path

import pytest

from ci.codec_conformance.run import compiler_path


@pytest.mark.skipif(not sys.platform.startswith("linux"), reason="Linux-only provider")
@pytest.mark.parametrize("provider", ["", "soldr"])
def test_conformance_compiler_honors_native_provider(
    tmp_path: Path, monkeypatch: pytest.MonkeyPatch, provider: str
) -> None:
    """Select soldr on opt-in while preserving the ordinary PATH compiler."""
    monkeypatch.setenv("FASTLED_NATIVE_CLANG", provider)
    monkeypatch.setenv("PATH", str(tmp_path))
    system = tmp_path / "clang++"
    system.touch(mode=0o755)
    soldr = tmp_path / "soldr"
    soldr.write_text('#!/bin/sh\nprintf "%s\\n" "$0-tool"\n', encoding="utf-8")
    soldr.chmod(0o755)
    managed = tmp_path / "soldr-tool"
    managed.touch(mode=0o755)

    assert compiler_path() == (managed if provider else system)


@pytest.mark.skipif(not sys.platform.startswith("linux"), reason="Linux-only provider")
def test_missing_soldr_does_not_fall_back_to_system_clang(
    tmp_path: Path, monkeypatch: pytest.MonkeyPatch
) -> None:
    """A requested managed compiler must be available even if system clang is."""
    monkeypatch.setenv("FASTLED_NATIVE_CLANG", "soldr")
    monkeypatch.setenv("PATH", str(tmp_path))
    (tmp_path / "clang++").touch(mode=0o755)

    with pytest.raises(RuntimeError, match="soldr.*not on PATH"):
        compiler_path()
