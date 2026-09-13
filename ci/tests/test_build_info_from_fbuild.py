"""build_info.json is derived from fbuild outputs, never from an external tool.

Covers ci/compiler/build_config.py:
- synthesis from ``.fbuild/build/release/compile_commands.json`` + firmware.elf
  (the ESP32 family, where fbuild does not emit build_info_<env>.json)
- preference for fbuild's own ``build_info_<env>.json`` when present
- ``insert_tool_aliases`` deriving gcc-prefixed and llvm-* tool names
"""

import json
from pathlib import Path

from ci.boards import create_board
from ci.compiler.build_config import (
    generate_build_info_json_from_existing_build,
    insert_tool_aliases,
)


def _make_fbuild_tree(tmp_path: Path, compiler: str) -> Path:
    out = tmp_path / ".fbuild" / "build" / "release"
    out.mkdir(parents=True)
    (out / "firmware.elf").write_bytes(b"\x7fELF")
    (out / "firmware.bin").write_bytes(b"\x00" * 1234)
    entries = [
        {
            "directory": str(tmp_path),
            "file": str(tmp_path / "src" / "main.cpp"),
            "command": (
                f"{compiler} -Os -DARDUINO=10808 -DF_CPU=240000000L -DESP32 "
                f"-I{tmp_path}/lib/FastLED/src -I{tmp_path}/src -std=gnu++17 "
                f"-MMD -o main.o -c {tmp_path}/src/main.cpp"
            ),
        },
        {
            "directory": str(tmp_path),
            "file": str(tmp_path / "core" / "esp32-hal.c"),
            "command": f"{compiler} -Os -DARDUINO=10808 -DCORE_ONLY -std=gnu17 -c core/esp32-hal.c",
        },
    ]
    (out / "compile_commands.json").write_text(json.dumps(entries))
    return out


def test_synthesizes_build_info_from_compile_commands(tmp_path: Path) -> None:
    out = _make_fbuild_tree(tmp_path, "clang")
    board = create_board("esp32s3")

    assert generate_build_info_json_from_existing_build(tmp_path, board, "Blink")

    data = json.loads((tmp_path / "build_info_Blink.json").read_text())
    env = data["esp32s3"]
    assert env["prog_path"] == str((out / "firmware.elf").resolve())
    assert env["prog_size"] == 1234
    assert "ARDUINO=10808" in env["defines"]
    assert "ESP32" in env["defines"]
    assert "CORE_ONLY" in env["defines"]
    assert f"{tmp_path}/lib/FastLED/src" in env["includes"]
    assert "-Os" in env["cxx_flags"]
    assert "-std=gnu++17" in env["cxx_flags"]
    assert "-std=gnu17" in env["cc_flags"]
    # -c, -o main.o and the source file are not flags
    assert "-c" not in env["cxx_flags"]
    assert "main.o" not in env["cxx_flags"]
    assert "aliases" in env
    assert set(env["aliases"]) >= {"nm", "objdump", "c++filt", "size", "ld"}


def test_prefers_fbuild_emitted_build_info(tmp_path: Path) -> None:
    out = _make_fbuild_tree(tmp_path, "avr-gcc")
    board = create_board("uno")
    (tmp_path / "build_info_uno.json").write_text(
        json.dumps(
            {
                "uno": {
                    "board": "uno",
                    "cc_path": "avr-gcc",
                    "cxx_path": "avr-g++",
                    "cc_flags": ["-mmcu=atmega328p"],
                    "cxx_flags": ["-mmcu=atmega328p"],
                    "defines": ["F_CPU=16000000L"],
                    "includes": [],
                    "prog_path": str(out / "firmware.hex"),
                    "aliases": {"nm": "/toolchain/avr-nm"},
                }
            }
        )
    )

    assert generate_build_info_json_from_existing_build(tmp_path, board)

    data = json.loads((tmp_path / "build_info.json").read_text())
    env = data["uno"]
    assert env["cc_flags"] == ["-mmcu=atmega328p"]
    # ELF wins over fbuild's hex so the symbol tools can read it
    assert env["prog_path"] == str((out / "firmware.elf").resolve())
    # fbuild's own alias is kept; the missing ones are filled in
    assert env["aliases"]["nm"] == "/toolchain/avr-nm"
    assert "objdump" in env["aliases"]


def test_returns_false_without_fbuild_output(tmp_path: Path) -> None:
    assert not generate_build_info_json_from_existing_build(
        tmp_path, create_board("uno"), "Blink"
    )
    assert not (tmp_path / "build_info_Blink.json").exists()


def test_insert_tool_aliases_gcc_prefix(tmp_path: Path) -> None:
    bin_dir = tmp_path / "bin"
    bin_dir.mkdir()
    for name in ("arm-none-eabi-gcc", "arm-none-eabi-nm", "arm-none-eabi-objdump"):
        (bin_dir / name).write_text("")
    data = {"teensy41": {"cc_path": str(bin_dir / "arm-none-eabi-gcc")}}

    insert_tool_aliases(data)

    aliases = data["teensy41"]["aliases"]
    assert aliases["nm"] == str(bin_dir / "arm-none-eabi-nm")
    assert aliases["objdump"] == str(bin_dir / "arm-none-eabi-objdump")
    # not present in the fake toolchain, so resolved via PATH or None
    assert "size" in aliases


def test_insert_tool_aliases_llvm_names(tmp_path: Path) -> None:
    bin_dir = tmp_path / "bin"
    bin_dir.mkdir()
    for name in ("clang", "llvm-nm", "llvm-cxxfilt", "ld.lld"):
        (bin_dir / name).write_text("")
    data = {"esp32s3": {"cc_path": str(bin_dir / "clang")}}

    insert_tool_aliases(data)

    aliases = data["esp32s3"]["aliases"]
    assert aliases["nm"] == str(bin_dir / "llvm-nm")
    assert aliases["c++filt"] == str(bin_dir / "llvm-cxxfilt")
    assert aliases["ld"] == str(bin_dir / "ld.lld")
