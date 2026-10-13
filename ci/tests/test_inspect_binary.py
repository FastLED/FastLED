"""Optional legacy conversion tools must not prevent final ELF diagnostics."""

import json
import sys
from pathlib import Path

import pytest
from running_process import CompletedProcess

from ci import inspect_binary
from ci.util import symbol_analysis


@pytest.mark.parametrize(
    ("unavailable_tool", "null_alias"),
    [(tool, null) for tool in ("ld", "as", "objcopy") for null in (False, True)]
    + [(None, False)],
)
def test_inspect_binary_keeps_elf_and_map_reports(
    tmp_path: Path,
    monkeypatch: pytest.MonkeyPatch,
    capsys: pytest.CaptureFixture[str],
    unavailable_tool: str | None,
    null_alias: bool,
) -> None:
    board_dir = tmp_path / ".build" / "fbuild" / "teensy41"
    output_dir = board_dir / ".fbuild" / "build" / "release"
    output_dir.mkdir(parents=True)
    elf = output_dir / "firmware.elf"
    elf.write_bytes(b"\x7fELF")
    elf.with_suffix(".hex").write_text(":00000001FF\n")
    map_file = elf.with_suffix(".map")
    map_file.write_text("diagnostic map content\n")
    aliases: dict[str, str | None] = {
        tool: str(tmp_path / f"arm-none-eabi-{tool}")
        for tool in ("nm", "readelf", "c++filt", "ld", "as", "objcopy")
    }
    if unavailable_tool is not None:
        if null_alias:
            aliases[unavailable_tool] = None
        else:
            del aliases[unavailable_tool]
    (board_dir / "build_info_teensy41.json").write_text(
        json.dumps({"teensy41": {"prog_path": str(elf), "aliases": aliases}})
    )
    monkeypatch.setattr(
        sys, "argv", ["inspect_binary", "--first", "--cwd", str(tmp_path)]
    )

    # Preserve the production symbol parser and report formatter; supply only
    # the output of the cross-toolchain executables unavailable in unit tests.
    def symbol_tool_output(command: str) -> str:
        if "--print-size" in command:
            return "4096 32 T diagnostic_function\n"
        return ""

    monkeypatch.setattr(symbol_analysis, "run_command", symbol_tool_output)
    monkeypatch.setattr(symbol_analysis, "demangle_symbol", lambda name, _: name)
    commands: list[list[str]] = []

    def run_tool(command: list[str], **_kwargs: object) -> CompletedProcess[str]:
        commands.append(command)
        return CompletedProcess(
            command, 0, "00001000 00000020 T diagnostic_function\n", ""
        )

    monkeypatch.setattr(inspect_binary.RunningProcess, "run", run_tool)
    conversions: list[tuple[Path, Path, Path]] = []

    def convert_binary(
        _binary: Path,
        _map: Path,
        assembler: Path,
        linker: Path,
        objcopy: Path,
        destination: Path,
    ) -> Path:
        conversions.append((assembler, linker, objcopy))
        return destination

    monkeypatch.setattr(inspect_binary, "bin_to_elf", convert_binary)

    assert inspect_binary.main() == 0
    output = capsys.readouterr().out
    assert "PER-FUNCTION CODE SIZE ANALYSIS - TEENSY41" in output
    assert "Total code size: 32 bytes" in output
    assert "diagnostic_function" in output
    assert "ARCHIVE-LEVEL SIZE BREAKDOWN" in output
    assert "diagnostic map content" in output
    assert ["uv", "run", "fpvgcc", str(map_file), "--sar"] in commands
    if unavailable_tool is None:
        assert conversions == [
            tuple(
                tmp_path / f"arm-none-eabi-{tool}" for tool in ("as", "ld", "objcopy")
            )
        ]
        assert "LEGACY ANALYSIS (bin-to-elf conversion)" in output
    else:
        assert not conversions
        assert "Error while converting binary to ELF" in output
