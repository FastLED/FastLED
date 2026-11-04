from pathlib import Path
from types import SimpleNamespace
from unittest.mock import patch

import pytest

from ci.autoresearch.staging import synthesise_autoresearch_project


def test_autoresearch_staging_enables_objectfled_diagnostics(
    tmp_path: Path,
) -> None:
    with patch("ci.compiler.board_compiler.init_fbuild_project") as mock_init:
        mock_init.return_value = SimpleNamespace(success=True, output="")

        build_dir = synthesise_autoresearch_project(
            "teensy41",
            project_root=tmp_path,
            verbose=False,
        )

    assert build_dir == tmp_path / ".build" / "fbuild" / "teensy41"
    _, kwargs = mock_init.call_args
    assert kwargs["additional_defines"] == ["FASTLED_OBJECTFLED_DIAGNOSTICS=1"]


@pytest.mark.parametrize("environment", ("rp2350", "rpipico2", "rp2350w", "rpipico2w"))
def test_rp2350_family_autoresearch_enables_target_bound_picotool_reset(
    tmp_path: Path, environment: str
) -> None:
    with patch("ci.compiler.board_compiler.init_fbuild_project") as mock_init:
        mock_init.return_value = SimpleNamespace(success=True, output="")

        synthesise_autoresearch_project(
            environment,
            project_root=tmp_path,
            verbose=False,
        )

    _, kwargs = mock_init.call_args
    assert "ENABLE_PICOTOOL_USB=1" in kwargs["additional_defines"]


def test_rp2040_autoresearch_does_not_enable_picotool_reset(
    tmp_path: Path,
) -> None:
    with patch("ci.compiler.board_compiler.init_fbuild_project") as mock_init:
        mock_init.return_value = SimpleNamespace(success=True, output="")

        synthesise_autoresearch_project(
            "rp2040",
            project_root=tmp_path,
            verbose=False,
        )

    _, kwargs = mock_init.call_args
    assert "ENABLE_PICOTOOL_USB=1" not in kwargs["additional_defines"]


@pytest.mark.parametrize("lto", (True, False))
def test_static_init_staging_applies_lto_to_compilation_and_link(
    tmp_path: Path, lto: bool
) -> None:
    with patch("ci.compiler.board_compiler.init_fbuild_project") as mock_init:
        mock_init.return_value = SimpleNamespace(success=True, output="")
        build_dir = synthesise_autoresearch_project(
            "esp32c6", tmp_path, False, static_init_lto=lto
        )

    board = mock_init.call_args.args[0]
    flag = "-flto=auto" if lto else "-fno-lto"
    assert flag in board.build_flags
    assert "-fthreadsafe-statics" in board.build_flags
    assert "-fno-threadsafe-statics" in board.build_unflags
    assert ("-fno-lto" if lto else "-flto=auto") in board.build_unflags
    script = build_dir / "static_init_link.py"
    assert f"post:{script.as_posix()}" in board.extra_scripts
    assert f'env.Append(LINKFLAGS=["{flag}"])' in script.read_text()


def test_static_init_staging_rejects_non_esp32(tmp_path: Path) -> None:
    with pytest.raises(RuntimeError, match="requires an ESP32 board"):
        synthesise_autoresearch_project("rp2350", tmp_path, False, static_init_lto=True)
