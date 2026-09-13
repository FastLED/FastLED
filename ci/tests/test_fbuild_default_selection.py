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


def test_compile_board_examples_builds_through_board_compiler(
    monkeypatch: pytest.MonkeyPatch,
) -> None:
    """compile_board_examples drives every build through BoardCompiler (fbuild)."""
    captured: dict[str, Any] = {}

    monkeypatch.setattr(
        "ci.compiler.compilation_orchestrator.get_filtered_examples",
        lambda _board, examples: (examples, []),
    )

    class FakeBoardCompiler:
        def __init__(self, *args: Any, **kwargs: Any) -> None:
            captured["kwargs"] = kwargs

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
        "ci.compiler.compilation_orchestrator.BoardCompiler", FakeBoardCompiler
    )

    result = compile_board_examples(
        board=Board(board_name="digispark-tiny"),
        examples=["Blink"],
        defines=[],
        verbose=False,
    )

    assert result.ok is True
    assert captured["kwargs"]["board"].board_name == "digispark-tiny"
    assert captured["kwargs"]["additional_defines"] == []


def test_autoresearch_always_selects_fbuild() -> None:
    """select_build_driver should always return the fbuild driver."""
    driver = select_build_driver("digispark-tiny", False, True)
    assert isinstance(driver, FbuildDriver)


def test_autoresearch_fbuild_install_packages_is_a_no_op() -> None:
    """fbuild AutoResearch lets fbuild resolve packages during deploy."""
    assert FbuildDriver().install_packages(Path("."), "esp32s3") is True


def test_autoresearch_fbuild_firmware_path_uses_release_artifact(
    monkeypatch: pytest.MonkeyPatch,
    tmp_path: Path,
) -> None:
    firmware = tmp_path / "resolved" / "firmware.bin"
    captured: list[tuple[str, str, str | None]] = []

    def fake_find_firmware(
        project_dir: str,
        environment: str,
        firmware_name: str | None = None,
    ) -> str:
        captured.append((project_dir, environment, firmware_name))
        return str(firmware)

    monkeypatch.setattr("fbuild.find_firmware", fake_find_firmware)

    assert FbuildDriver().firmware_path(tmp_path, "rp2350w") == firmware
    assert captured == [(str(tmp_path), "rp2350w", "firmware.bin")]


def test_autoresearch_fbuild_firmware_path_returns_none_when_missing(
    monkeypatch: pytest.MonkeyPatch,
    tmp_path: Path,
) -> None:
    def fake_find_firmware(
        project_dir: str,
        environment: str,
        firmware_name: str | None = None,
    ) -> None:
        return None

    monkeypatch.setattr("fbuild.find_firmware", fake_find_firmware)

    assert FbuildDriver().firmware_path(tmp_path, "rp2350w") is None


def test_autoresearch_parse_args_warns_for_deprecated_fbuild_flags(
    capsys: pytest.CaptureFixture[str],
) -> None:
    """Args.parse_args should warn when deprecated fbuild flags are used."""
    parsed = Args.parse_args(["--no-fbuild"])
    captured = capsys.readouterr()

    assert parsed.no_fbuild is True
    assert "--no-fbuild is deprecated and has no effect" in captured.err


def test_autoresearch_parse_args_supports_pin_free_rpc_smoke() -> None:
    parsed = Args.parse_args(["rp2040", "--rpc-smoke"])

    assert parsed.environment_positional == "rp2040"
    assert parsed.rpc_smoke is True
    assert parsed.auto_discover_pins is True


def test_debug_attached_parse_args_warns_for_deprecated_fbuild_flags(
    monkeypatch: pytest.MonkeyPatch, capsys: pytest.CaptureFixture[str]
) -> None:
    """debug_attached.parse_args should warn when deprecated fbuild flags are used."""
    monkeypatch.setattr("sys.argv", ["debug_attached", "--use-fbuild"])
    parsed = parse_debug_attached_args()
    captured = capsys.readouterr()

    assert parsed.use_fbuild is True
    assert "--use-fbuild is deprecated and has no effect" in captured.err
