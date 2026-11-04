"""Arduino compiles every library .S, including other platforms' sources."""

from __future__ import annotations

import hashlib
import os
import platform
import shutil
import sys
import tarfile
import urllib.request
from pathlib import Path

import pytest
from running_process import RunningProcess


ROOT = Path(__file__).resolve().parents[2]


@pytest.fixture(scope="session")
def legacy_arm_gcc() -> str:
    """Pin the reporter's toolchain; newer linkers accept the broken object."""
    compiler = os.environ.get("FL_TEST_LEGACY_ARM_GCC")
    if compiler:
        return compiler
    if not os.environ.get("GITHUB_ACTIONS"):
        pytest.skip("Set FL_TEST_LEGACY_ARM_GCC to Seeed GCC 7-2017q4")
    if sys.platform != "linux" or platform.machine() != "x86_64":
        pytest.skip("The automatic legacy toolchain fixture targets Linux x86-64")
    cache = ROOT / ".cached" / "legacy-samd-gcc7"
    compiler_path = cache / "gcc-arm-none-eabi-7-2017-q4-major/bin/arm-none-eabi-gcc"
    if not compiler_path.exists():
        cache.mkdir(parents=True, exist_ok=True)
        archive = cache / "toolchain.tar.bz2"
        url = (
            "https://files.seeedstudio.com/arduino/tools/res/arm-none-eabi-gcc/"
            "7-2017q4/gcc-arm-none-eabi-7-2017-q4-major-linux64.tar.bz2"
        )
        with (
            urllib.request.urlopen(url, timeout=60) as response,
            archive.open("wb") as output,
        ):
            shutil.copyfileobj(response, output)
        expected = "96a029e2ae130a1210eaa69e309ea40463028eab18ba19c1086e4c2dafe69a6a"
        assert hashlib.sha256(archive.read_bytes()).hexdigest() == expected
        with tarfile.open(archive) as package:
            package.extractall(cache, filter="data")
    return str(compiler_path)


@pytest.mark.parametrize("cpu", ["cortex-m0plus", "cortex-m4"])
def test_legacy_samd_links_all_library_assembly(
    tmp_path: Path, legacy_arm_gcc: str, cpu: str
) -> None:
    """Exercise the Seeed 1.8.6 assembler and direct-object linker recipes."""
    compiler = legacy_arm_gcc
    main = tmp_path / "main.c"
    main.write_text("int main(void) { return 0; }\n")
    objects: list[str] = []
    for index, source in enumerate(sorted((ROOT / "src").rglob("*.S"))):
        obj = tmp_path / f"assembly-{index}.o"
        result = RunningProcess.run(
            [
                compiler,
                "-c",
                "-g",
                "-x",
                "assembler-with-cpp",
                "-DARDUINO_ARCH_SAMD",
                str(source),
                "-o",
                str(obj),
            ],
            capture_output=True,
            encoding="utf-8",
            errors="replace",
            check=False,
        )
        assert result.returncode == 0, result.stdout
        objects.append(str(obj))
    result = RunningProcess.run(
        [
            compiler,
            "-mcpu=" + cpu,
            "-mthumb",
            "-nostdlib",
            "-Wl,-e,main",
            str(main),
            *objects,
            "-o",
            str(tmp_path / "firmware.elf"),
        ],
        capture_output=True,
        encoding="utf-8",
        errors="replace",
        check=False,
    )
    assert result.returncode == 0, result.stdout


def test_riscv_isr_survives_lto_archive(tmp_path: Path) -> None:
    """An asm-only definition must still be discoverable by the LTO linker."""
    compiler = os.environ.get("FL_TEST_RISCV_GXX")
    if not compiler:
        pytest.skip("Set FL_TEST_RISCV_GXX to the ESP RISC-V compiler")
    archiver = str(Path(compiler).with_name(Path(compiler).name[:-3] + "gcc-ar"))
    assembler = str(Path(compiler).with_name(Path(compiler).name[:-3] + "gcc"))
    source = ROOT / "src/platforms/esp/32/drivers/gpio_isr_rx/fast_isr.S"
    obj = tmp_path / "isr.o"
    archive = tmp_path / "isr.a"
    main = tmp_path / "main.cpp"
    main.write_text(
        'extern "C" void gpio_fast_edge_isr(void*) noexcept;\n'
        "int main() { gpio_fast_edge_isr(nullptr); return 0; }\n"
    )
    commands = [
        [
            assembler,
            "-Os",
            "-x",
            "assembler-with-cpp",
            "-c",
            str(source),
            "-o",
            str(obj),
        ],
        [archiver, "rcs", str(archive), str(obj)],
        [
            compiler,
            "-flto",
            "-nostdlib",
            "-Wl,-e,main",
            str(main),
            str(archive),
            "-o",
            str(tmp_path / "firmware.elf"),
        ],
    ]
    for command in commands:
        result = RunningProcess.run(
            command,
            capture_output=True,
            encoding="utf-8",
            errors="replace",
            check=False,
            timeout=60,
        )
        assert result.returncode == 0, result.stdout
