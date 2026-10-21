"""Tests for ci/check_released_sources.py (FastLED #4704)."""

from ci.check_released_sources import _COMPILED, missing_sources


def test_renamed_unity_unit_is_reported() -> None:
    old = {"src/fl/build/fl.system.sd+.cpp", "src/fl/build/fl.fs+.cpp"}
    new = {"src/fl/build/fl.fs.sd+.cpp", "src/fl/build/fl.fs+.cpp"}
    assert missing_sources(old, new) == ["src/fl/build/fl.system.sd+.cpp"]


def test_tombstone_satisfies_check() -> None:
    old = {"src/fl/build/fl.system.sd+.cpp"}
    new = {"src/fl/build/fl.system.sd+.cpp", "src/fl/build/fl.fs.sd+.cpp"}
    assert missing_sources(old, new) == []


def test_only_compiled_files_count() -> None:
    assert _COMPILED.match("src/fl/build/fl.fs.sd+.cpp")
    assert _COMPILED.match("src/third_party/x/a.S")
    assert not _COMPILED.match("src/fl/fs/sd/file_system_sd.cpp.hpp")
    assert not _COMPILED.match("src/fl/fs/fs.h")
