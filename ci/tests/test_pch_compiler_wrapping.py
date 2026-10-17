"""PCH consumers need zccache's private input view on every native host."""

from pathlib import Path
from unittest.mock import patch

from ci.meson.meson_markers import inject_zccache_wrapping
from ci.meson.native_launchers import wrap_compiler_with_zccache


def test_windows_wraps_compilers_but_preserves_bare_links(tmp_path: Path) -> None:
    ninja = tmp_path / "build.ninja"
    ninja.write_text(
        "rule cpp_COMPILER\n command = clang++ $ARGS\n\n"
        "rule cpp_LINKER\n command = clang++ $LINK_ARGS\n\n"
        "rule c_COMPILER\n command = clang $ARGS\n\n"
        "rule c_LINKER\n command = clang $LINK_ARGS\n\n",
        encoding="utf-8",
    )
    with patch("ci.meson.meson_markers.os.name", "nt"):
        assert inject_zccache_wrapping(tmp_path, "C:/venv/zccache.exe")
        first = ninja.read_text(encoding="utf-8")
        assert inject_zccache_wrapping(tmp_path, "C:/venv/zccache.exe")
    assert ninja.read_text(encoding="utf-8") == first
    assert 'command = "C:/venv/zccache.exe" clang++ $ARGS' in first
    assert 'command = "C:/venv/zccache.exe" clang $ARGS' in first
    assert "command = clang++ $LINK_ARGS" in first
    assert "command = clang $LINK_ARGS" in first


def test_already_wrapped_compiler_is_not_wrapped_twice() -> None:
    command = ["/venv/bin/zccache", "clang++", "-c", "source.cpp"]
    assert wrap_compiler_with_zccache(command) == command
    command = ["C:/venv/zccache.exe", "clang++", "-c", "source.cpp"]
    assert wrap_compiler_with_zccache(command) == command
