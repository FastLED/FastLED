from concurrent.futures import Future
from pathlib import Path
from typing import Any

import pytest

from ci.autoresearch.args import Args
from ci.autoresearch.build_driver import FbuildDriver, select_build_driver
from ci.boards import Board
from ci.compiler.compilation_orchestrator import compile_board_examples
from ci.compiler.compiler import SketchResult
from ci.debug_attached import parse_args as parse_debug_attached_args


def test_compile_board_examples_always_uses_fbuild(
    monkeypatch: pytest.MonkeyPatch,
) -> None:
    """compile_board_examples should always pass use_fbuild=True."""
    captured: dict[str, bool] = {}
    missing = object()

    monkeypatch.setattr(
        "ci.compiler.compilation_orchestrator.get_filtered_examples",
        lambda _board, examples: (examples, []),
    )

    class FakePioCompiler:
        def __init__(self, *args: Any, **kwargs: Any) -> None:
            use_fbuild = kwargs.get("use_fbuild", missing)
            if use_fbuild is missing and len(args) >= 7:
                use_fbuild = args[6]
            assert use_fbuild is not missing, "PioCompiler use_fbuild argument missing"
            captured["use_fbuild"] = bool(use_fbuild)

        def build(self, examples: list[str]) -> list[Future[SketchResult]]:
            future: Future[SketchResult] = Future()
            future.set_result(
                SketchResult(
                    success=True,
                    output="ok",
                    build_dir=Path("."),
                    example=examples[0],
                )
            )
            return [future]

        def cancel_all(self) -> None:
            pass

    monkeypatch.setattr(
        "ci.compiler.compilation_orchestrator.PioCompiler", FakePioCompiler
    )

    result = compile_board_examples(
        board=Board(board_name="digispark-tiny"),
        examples=["Blink"],
        defines=[],
        verbose=False,
    )

    assert result.ok is True
    assert captured["use_fbuild"] is True


def test_autoresearch_always_selects_fbuild() -> None:
    """select_build_driver should always return the fbuild driver."""
    driver = select_build_driver("digispark-tiny", False, True)
    assert isinstance(driver, FbuildDriver)


def test_autoresearch_parse_args_warns_for_deprecated_fbuild_flags(
    capsys: pytest.CaptureFixture[str],
) -> None:
    """Args.parse_args should warn when deprecated fbuild flags are used."""
    parsed = Args.parse_args(["--no-fbuild"])
    captured = capsys.readouterr()

    assert parsed.no_fbuild is True
    assert "--no-fbuild is deprecated and has no effect" in captured.err


def test_debug_attached_parse_args_warns_for_deprecated_fbuild_flags(
    monkeypatch: pytest.MonkeyPatch, capsys: pytest.CaptureFixture[str]
) -> None:
    """debug_attached.parse_args should warn when deprecated fbuild flags are used."""
    monkeypatch.setattr("sys.argv", ["debug_attached", "--use-fbuild"])
    parsed = parse_debug_attached_args()
    captured = capsys.readouterr()

    assert parsed.use_fbuild is True
    assert "--use-fbuild is deprecated and has no effect" in captured.err
