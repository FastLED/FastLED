from pathlib import Path
from types import SimpleNamespace
from unittest.mock import patch

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
