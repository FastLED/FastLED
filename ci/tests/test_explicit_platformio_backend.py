"""Tests for the explicit ``--backend platformio`` opt-in (#3279 Phase 3).

``compile_board_examples`` now rejects programmatic ``use_fbuild=False``
flips unless the user has explicitly opted into the comparison-only
PlatformIO backend on the command line. The CLI sets
``FASTLED_BACKEND_PLATFORMIO_EXPLICIT=1`` when ``--backend platformio``
(or the ``--platformio`` / ``--pio`` shortcuts) is passed; nothing else
should set that env var.

These tests pin the contract so future refactors can't quietly route
new code paths around fbuild.
"""

from __future__ import annotations

from concurrent.futures import Future
from pathlib import Path
from typing import Any

import pytest

from ci.boards import Board
from ci.compiler.compilation_orchestrator import (
    PLATFORMIO_BACKEND_OPT_IN_ENV,
    compile_board_examples,
)
from ci.compiler.compiler import SketchResult


class _FakePioCompiler:
    """Stand-in for ``PioCompiler`` that records what the orchestrator passed."""

    last_use_fbuild: bool | None = None

    def __init__(self, *args: Any, **kwargs: Any) -> None:
        _FakePioCompiler.last_use_fbuild = bool(kwargs.get("use_fbuild", True))

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

    def supports_merged_bin(self) -> bool:
        return False


def _patch_orchestrator(monkeypatch: pytest.MonkeyPatch) -> None:
    """Patch the heavy deps of ``compile_board_examples`` for unit testing."""
    _FakePioCompiler.last_use_fbuild = None
    monkeypatch.setattr(
        "ci.compiler.compilation_orchestrator.get_filtered_examples",
        lambda _board, examples: (examples, []),
    )
    monkeypatch.setattr(
        "ci.compiler.compilation_orchestrator.PioCompiler",
        _FakePioCompiler,
    )


def test_programmatic_platformio_flip_rejected_without_env_var(
    monkeypatch: pytest.MonkeyPatch,
) -> None:
    """``use_fbuild=False`` without the explicit CLI opt-in must raise."""
    _patch_orchestrator(monkeypatch)
    monkeypatch.delenv(PLATFORMIO_BACKEND_OPT_IN_ENV, raising=False)

    with pytest.raises(RuntimeError) as ctx:
        compile_board_examples(
            board=Board(board_name="uno"),
            examples=["Blink"],
            defines=[],
            verbose=False,
            use_fbuild=False,
        )
    assert "comparison-only" in str(ctx.value)
    assert PLATFORMIO_BACKEND_OPT_IN_ENV in str(ctx.value)
    # PioCompiler must NOT have been constructed.
    assert _FakePioCompiler.last_use_fbuild is None


def test_programmatic_platformio_flip_allowed_when_user_opted_in(
    monkeypatch: pytest.MonkeyPatch,
) -> None:
    """``use_fbuild=False`` + env-var opt-in routes through PioCompiler."""
    _patch_orchestrator(monkeypatch)
    monkeypatch.setenv(PLATFORMIO_BACKEND_OPT_IN_ENV, "1")

    result = compile_board_examples(
        board=Board(board_name="uno"),
        examples=["Blink"],
        defines=[],
        verbose=False,
        use_fbuild=False,
    )
    assert result.ok is True
    assert _FakePioCompiler.last_use_fbuild is False


def test_fbuild_default_does_not_consult_env_var(
    monkeypatch: pytest.MonkeyPatch,
) -> None:
    """Default ``use_fbuild=True`` path must not require any opt-in."""
    _patch_orchestrator(monkeypatch)
    monkeypatch.delenv(PLATFORMIO_BACKEND_OPT_IN_ENV, raising=False)

    result = compile_board_examples(
        board=Board(board_name="uno"),
        examples=["Blink"],
        defines=[],
        verbose=False,
        # use_fbuild=None -> defaults to BOARD_BUILDS_USE_FBUILD = True
    )
    assert result.ok is True
    assert _FakePioCompiler.last_use_fbuild is True
