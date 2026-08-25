from __future__ import annotations

import importlib.util
import sys
from pathlib import Path

from pytest import MonkeyPatch


PROJECT_ROOT = Path(__file__).parents[2]
BLINK_OUTPUT = (
    PROJECT_ROOT / "examples" / "Blink" / "fastled_js" / "fastled.js"
).as_posix()


def _load_ci_compile_module():
    spec = importlib.util.spec_from_file_location(
        "ci_compile_under_test", PROJECT_ROOT / "ci" / "ci-compile.py"
    )
    assert spec is not None
    assert spec.loader is not None
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


def test_wasm_fast_path_consumes_clean_and_forces_reconfigure(
    monkeypatch: MonkeyPatch,
) -> None:
    import ci.wasm_build

    ci_compile = _load_ci_compile_module()
    build_calls: list[dict[str, object]] = []

    def fake_build(**kwargs: object) -> int:
        build_calls.append(kwargs)
        return 0

    monkeypatch.setattr(ci.wasm_build, "build", fake_build)
    monkeypatch.setattr(
        sys,
        "argv",
        ["ci-compile.py", "--clean", "wasm", "--examples", "Blink"],
    )

    assert ci_compile._wasm_fast_path() == 0
    assert build_calls == [
        {
            "example": "Blink",
            "output": BLINK_OUTPUT,
            "verbose": False,
            "force": True,
        }
    ]


def test_wasm_fast_path_forwards_compiler_defines(
    monkeypatch: MonkeyPatch,
) -> None:
    import ci.wasm_build

    ci_compile = _load_ci_compile_module()
    build_calls: list[dict[str, object]] = []

    def fake_build(**kwargs: object) -> int:
        build_calls.append(kwargs)
        return 0

    monkeypatch.setattr(ci.wasm_build, "build", fake_build)
    monkeypatch.setattr(
        sys,
        "argv",
        [
            "ci-compile.py",
            "wasm",
            "--examples",
            "Blink",
            "--defines",
            "FASTLED_MP3_BACKEND_MINIMP3,VALUE=2",
        ],
    )

    assert ci_compile._wasm_fast_path() == 0
    assert build_calls == [
        {
            "example": "Blink",
            "output": BLINK_OUTPUT,
            "verbose": False,
            "force": False,
            "defines": ["FASTLED_MP3_BACKEND_MINIMP3", "VALUE=2"],
        }
    ]
