from concurrent.futures import Future
from pathlib import Path

from ci.autoresearch.build_driver import FbuildDriver, select_build_driver
from ci.boards import Board
from ci.compiler.compilation_orchestrator import compile_board_examples
from ci.compiler.compiler import SketchResult
from ci.debug_attached import _should_use_fbuild


def test_compile_board_examples_always_uses_fbuild(monkeypatch) -> None:
    captured: dict[str, bool] = {}

    monkeypatch.setattr(
        "ci.compiler.compilation_orchestrator.get_filtered_examples",
        lambda _board, examples: (examples, []),
    )

    class FakePioCompiler:
        def __init__(self, *args, **kwargs) -> None:
            captured["use_fbuild"] = kwargs["use_fbuild"]

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


def test_debug_attached_always_uses_fbuild() -> None:
    assert _should_use_fbuild("digispark-tiny", False, False) is True
    assert _should_use_fbuild("digispark-tiny", True, False) is True
    assert _should_use_fbuild("digispark-tiny", False, True) is True


def test_autoresearch_always_selects_fbuild() -> None:
    driver = select_build_driver("digispark-tiny", False, True)
    assert isinstance(driver, FbuildDriver)
