from pathlib import Path
from types import SimpleNamespace
from unittest.mock import patch

import pytest

from ci.autoresearch.staging import synthesise_autoresearch_project


def test_autoresearch_staging_enables_objectfled_diagnostics(
    tmp_path: Path,
) -> None:
    with patch("ci.compiler.pio._init_platformio_build") as mock_init:
        mock_init.return_value = SimpleNamespace(success=True, output="")

        build_dir = synthesise_autoresearch_project(
            "teensy41",
            project_root=tmp_path,
            verbose=False,
        )

    assert build_dir == tmp_path / ".build" / "pio" / "teensy41"
    _, kwargs = mock_init.call_args
    assert kwargs["additional_defines"] == ["FASTLED_OBJECTFLED_DIAGNOSTICS=1"]


@pytest.mark.parametrize("environment", ("rp2350", "rpipico2", "rp2350w", "rpipico2w"))
def test_rp2350_family_autoresearch_enables_target_bound_picotool_reset(
    tmp_path: Path, environment: str
) -> None:
    with patch("ci.compiler.pio._init_platformio_build") as mock_init:
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
    with patch("ci.compiler.pio._init_platformio_build") as mock_init:
        mock_init.return_value = SimpleNamespace(success=True, output="")

        synthesise_autoresearch_project(
            "rp2040",
            project_root=tmp_path,
            verbose=False,
        )

    _, kwargs = mock_init.call_args
    assert "ENABLE_PICOTOOL_USB=1" not in kwargs["additional_defines"]
