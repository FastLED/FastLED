"""Matched public-API memory benches in the canonical AutoResearch firmware."""

import asyncio
import hashlib
import json
import sys
from collections.abc import Mapping
from pathlib import Path

from running_process import RunningProcess

from ci.autoresearch.args import Args, memory_bench_conflicts
from ci.autoresearch.dual_spi_hosts import reset_board
from ci.autoresearch.staging import synthesise_autoresearch_project
from ci.boards import create_board
from ci.rpc_client import RpcClient
from ci.util.fbuild_runner import run_fbuild_deploy
from ci.util.serial_interface import create_serial_interface
from ci.util.sketch_resolver import parse_timeout


def _sha256(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def _source_provenance(project: Path) -> dict[str, str]:
    """Hash staged inputs, including any deliberately restored baseline source."""
    relative_paths = (
        "platformio.ini",
        "src/sketch/AutoResearchRemoteMemoryMethods.cpp",
        "lib/FastLED/FastLED.h",
        "lib/FastLED/fl/channels/adapters/spi_channel_adapter.cpp.hpp",
        "lib/FastLED/platforms/shared/spi_hw_base.h",
        "lib/FastLED/platforms/esp/32/drivers/spi/spi_hw_1_esp32.cpp.hpp",
    )
    return {name: _sha256(project / name) for name in relative_paths}


def _deployed_elf(project: Path) -> Path:
    """Select the release artifact used by run_fbuild_deploy, never newest mtime."""
    elf = project / ".fbuild" / "build" / "release" / "firmware.elf"
    if not elf.is_file():
        raise FileNotFoundError(f"Deployed release ELF is missing: {elf}")
    return elf


def spi_memory_within_budget(result: Mapping[str, object]) -> bool:
    """Require measured transient DMA storage at most one third of a full frame."""
    frame_bytes = result.get("frameBytes")
    peak_dma_bytes = result.get("peakDmaBytes")
    return (
        result.get("heapWatermarkMeasured") is True
        and isinstance(frame_bytes, int)
        and isinstance(peak_dma_bytes, int)
        and frame_bytes > 0
        and peak_dma_bytes >= 0
        and peak_dma_bytes * 3 <= frame_bytes
    )


async def run_memory_bench(args: Args) -> int:
    """Build one isolated mode, deploy it, and require its measured RPC verdict."""
    environment = args.environment_positional or args.environment
    if not environment or not args.upload_port:
        print(
            "Memory benches require an explicit environment and --upload-port",
            file=sys.stderr,
        )
        return 1
    board = create_board(environment)
    if board.platform_family != "esp32":
        print("Memory benches currently require an ESP32-family board", file=sys.stderr)
        return 1
    if args.spi_streaming and board.get_mcu_target() != "ESP32S3":
        print(
            "--spi-streaming requires ESP32-S3 (SPI2 master + SPI3 slave)",
            file=sys.stderr,
        )
        return 1
    if memory_bench_conflicts(args):
        print(
            "Run memory benches separately from driver and subsystem modes",
            file=sys.stderr,
        )
        return 1
    root = args.project_dir.resolve()
    if not args.skip_lint:
        lint = await asyncio.to_thread(
            RunningProcess.run,
            ["bash", "lint"],
            cwd=str(root),
            timeout=900,
            capture_output=True,
            encoding="utf-8",
            errors="replace",
        )
        print(lint.stdout)
        if lint.returncode:
            return lint.returncode
    mode = 3 if args.spi_streaming else (1 if args.driver_binding == "typed" else 2)
    project = synthesise_autoresearch_project(
        environment,
        root,
        args.verbose,
        extra_defines=[f"FL_AUTORESEARCH_MEMORY_BENCH={mode}"],
    )
    print(
        "SOURCES: " + json.dumps(_source_provenance(project), sort_keys=True),
        flush=True,
    )
    deployed = await asyncio.to_thread(
        run_fbuild_deploy,
        project,
        environment=environment,
        upload_port=args.upload_port,
        timeout=1800,
    )
    print(deployed.output)
    if not deployed.success:
        return 1
    port = deployed.port or args.upload_port
    try:
        elf = _deployed_elf(project)
    except FileNotFoundError as error:
        print(error, file=sys.stderr)
        return 1
    if not elf.is_relative_to(project):
        print(
            f"Refusing an ELF outside the staged benchmark project: {elf}",
            file=sys.stderr,
        )
        return 1
    print(
        "FIRMWARE: " + json.dumps({"elf": str(elf), "sha256": _sha256(elf)}), flush=True
    )
    # A controller remains registered for the firmware lifetime. Rebooting
    # between chipsets avoids a second controller contaminating either result.
    chipsets = ("apa102", "sk9822") if args.spi_streaming else ("clockless",)
    for index, chipset in enumerate(chipsets):
        if index:
            await reset_board(environment, port)
        async with RpcClient(
            port,
            serial_interface=create_serial_interface(port),
            timeout=max(30.0, parse_timeout(args.timeout or "30")),
            verbose=args.verbose,
        ) as client:
            result = await client.send(
                "memoryBench",
                args={"chipset": chipset, "pixels": 1000, "frames": 20}
                if args.spi_streaming
                else {"pin": 4},
                timeout=60,
            )
        print("RESULT: " + json.dumps(result.data, sort_keys=True), flush=True)
        if not result.success or result.get("success") is not True:
            return 1
        if result.get("mode") != mode:
            print("Memory benchmark firmware mode mismatch", file=sys.stderr)
            return 1
        if args.spi_streaming and not spi_memory_within_budget(result.data):
            print(
                "SPI DMA watermark must be measured and at most one third of frame bytes",
                file=sys.stderr,
            )
            return 1
    return 0
