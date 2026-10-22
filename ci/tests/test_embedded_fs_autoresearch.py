"""Argument contract for the reproducible embedded-storage device checks."""

from pathlib import Path

import pytest

from ci.autoresearch.args import Args


def test_embedded_fs_autoresearch_args() -> None:
    args = Args.parse_args(
        [
            "esp32dev_idf44",
            "--embedded-fs",
            "--embedded-fs-provider",
            "--embedded-fs-format-fixture",
            "--upload-port",
            "/dev/ttyUSB0",
        ]
    )
    assert args.embedded_fs
    assert args.embedded_fs_provider
    assert args.embedded_fs_format_fixture
    assert args.environment_positional == "esp32dev_idf44"


def test_embedded_fs_staging_preserves_canonical_sources(tmp_path: Path) -> None:
    from ci.autoresearch.embedded_fs import BINDING, inject_embedded_fs_fixture

    sketch = tmp_path / "sketch"
    sketch.mkdir()
    rpc = sketch / "AutoResearchRemoteDriverMethods.cpp"
    rpc.write_text(BINDING + "\n}\n")
    fixture = tmp_path / "fixture.hpp"
    fixture.write_text("// hardware-only fixture\n")
    inject_embedded_fs_fixture(sketch, fixture)
    assert "embeddedFsSelfTest" in rpc.read_text()
    assert (sketch / "EmbeddedFsHil.hpp").read_bytes() == fixture.read_bytes()
    assert fixture.read_text() == "// hardware-only fixture\n"
    with pytest.raises(ValueError, match="already instrumented"):
        inject_embedded_fs_fixture(sketch, fixture)


def test_embedded_fs_dependencies_preserve_board_configuration(tmp_path: Path) -> None:
    from ci.autoresearch.embedded_fs import configure_embedded_fs_dependencies
    from ci.compiler.project_ini import ProjectIni

    ini = tmp_path / "platformio.ini"
    ini.write_text("[env:board]\nlib_deps = FS, LittleFS\nboard = actual-board\n")
    configure_embedded_fs_dependencies(ini, "board")
    configure_embedded_fs_dependencies(ini, "board")
    project = ProjectIni.parseFile(ini)
    assert project.get_option("env:board", "lib_deps") == "FS, LittleFS"
    assert project.get_option("env:board", "board") == "actual-board"
