from __future__ import annotations

import importlib.util
from pathlib import Path


PROJECT_ROOT = Path(__file__).parents[2]


def _load_ci_compile_module():
    spec = importlib.util.spec_from_file_location(
        "ci_compile_build_info_under_test", PROJECT_ROOT / "ci" / "ci-compile.py"
    )
    assert spec is not None
    assert spec.loader is not None
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


def test_requested_build_info_fails_when_source_is_missing(
    tmp_path: Path,
    capsys,
) -> None:
    ci_compile = _load_ci_compile_module()
    source = tmp_path / "missing.json"
    destination = tmp_path / "requested.json"

    assert not ci_compile._copy_requested_build_info(source, destination)
    assert not destination.exists()
    assert str(source) in capsys.readouterr().out


def test_requested_build_info_copies_source_unchanged(tmp_path: Path) -> None:
    ci_compile = _load_ci_compile_module()
    source = tmp_path / "build_info_Blink.json"
    destination = tmp_path / "requested.json"
    contents = '{"board": {"prog_size": 123}}\n'
    source.write_text(contents, encoding="utf-8")

    assert ci_compile._copy_requested_build_info(source, destination)
    assert destination.read_text(encoding="utf-8") == contents
