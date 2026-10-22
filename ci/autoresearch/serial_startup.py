"""Verify the optional Arduino UART startup hook in canonical AutoResearch."""

import asyncio
import hashlib
import json
import sys
from collections.abc import AsyncIterator
from pathlib import Path

from running_process import RunningProcess

from ci.autoresearch.args import Args
from ci.autoresearch.staging import synthesise_autoresearch_project
from ci.boards import ESP32_ARDUINO_3_3_11_SERIAL_FRAMEWORK, create_board
from ci.compiler.build_config import generate_build_info_json_from_existing_build
from ci.rpc_client import RpcClient
from ci.util.fbuild_runner import run_fbuild_compile, run_fbuild_deploy
from ci.util.serial_interface import SerialInterface, create_serial_interface
from ci.util.sketch_resolver import parse_timeout


_BINDING = "void AutoResearchRemoteControl::bindSystemMethods(fl::Remote& remote) {"
_OTHER_MODES = (
    "parlio",
    "rmt",
    "spi",
    "uart",
    "bitbang",
    "lcd",
    "lcd_spi",
    "lcd_rgb",
    "object_fled",
    "flex_io",
    "lpuart",
    "all",
    "simd",
    "coroutine",
    "ieee754",
    "math",
    "fft",
    "mp3",
    "net",
    "net_server",
    "net_client",
    "net_peer",
    "ota",
    "ble",
    "decode",
    "watchdog_soak",
    "rpc_smoke",
    "perf_wave2d",
    "pwm_dma_cl",
    "dma_spi",
    "dma_uart",
    "rp_spi_loopback",
    "rp_spi_public_api",
    "rp_pio_both",
    "test_fault_emit",
    "embedded_fs",
    "embedded_fs_provider",
    "embedded_fs_format_fixture",
    "spi_provider",
    "spi_provider_native",
    "pin_toggle_rx",
    "ws2812_loopback",
    "tight_timing",
    "parallel",
    "legacy",
    "legacy_mixed_timings",
    "legacy_rgbw_small_counts",
)


def inject_serial_startup_fixture(
    project: Path, fixture: Path, native_only: bool
) -> None:
    """Instrument only the generated sketch, preserving the repository example."""
    sketch = project / "src/sketch"
    main = project / "src/main.cpp"
    main_text = main.read_text()
    include = '#include "sketch/AutoResearch.ino"'
    rpc = sketch / "AutoResearchRemoteSystemMethods.cpp"
    rpc_text = rpc.read_text()
    if main_text.count(include) != 1 or rpc_text.count(_BINDING) != 1:
        raise ValueError("Canonical AutoResearch startup/binding layout changed")
    if "serialStartupProbe" in rpc_text:
        raise ValueError("AutoResearch serial fixture is already installed")
    (sketch / "SerialStartupHil.hpp").write_bytes(fixture.read_bytes())
    main.write_text(
        main_text.replace(
            include,
            (
                "#define setup canonicalAutoResearchSetup\n"
                + include
                + '\n#undef setup\n#include "sketch/SerialStartupHil.hpp"\n'
                "void setup() { serial_startup_hil::begin(); canonicalAutoResearchSetup(); }"
            ),
        )
    )
    rpc.write_text(
        '#include "SerialStartupHil.hpp"\n'
        + rpc_text.replace(
            _BINDING,
            _BINDING + '\n remote.bind("serialStartupProbe", '
            "[](const fl::json& args) -> fl::json { return serial_startup_hil::probe(args); });",
        )
    )
    if native_only:
        # These are explicit application Serial users. Remove them only in the
        # no-Serial fixture; the ELF check below proves no other owner slipped in.
        ino = sketch / "AutoResearch.ino"
        lines = ino.read_text().splitlines(keepends=True)
        owners = [
            line
            for line in lines
            if line.strip().startswith(
                ("Serial.setRxBufferSize(", "Serial.setTxBufferSize(")
            )
        ]
        if len(owners) != 2:
            raise ValueError("Audit native-only AutoResearch Serial buffer setup")
        ino.write_text("".join(line for line in lines if line not in owners))


def verify_serial_startup_symbols(report: Path, native_only: bool) -> None:
    """Verify actual linked owners; Stream may belong to OTA's filesystem."""
    document = json.loads(report.read_text())
    names = {symbol["demangled"] for symbol in document["symbols"] if symbol["size"]}
    hardware = any(name.startswith("HardwareSerial::") for name in names)
    serial0 = "Serial0" in names
    hook = "arduinoSerial0SetDefaultPins" in names
    if native_only and (hardware or serial0 or hook):
        raise ValueError("Native-only image retained the HardwareSerial startup owner")
    if not native_only and not (hardware and serial0 and hook):
        raise ValueError("Application Serial image omitted its real startup provider")


def verify_serial_records(lines: list[str], native_only: bool) -> None:
    """Require complete records in the order emitted by the device fixture."""
    markers = (
        ["NATIVE_4892_RECORD"]
        if native_only
        else ["ARDUINO_4892_RECORD", "NATIVE_4892_RECORD"]
    )
    received = [
        line.strip()
        for line in lines
        if line.strip() in ("ARDUINO_4892_RECORD", "NATIVE_4892_RECORD")
    ]
    if received != markers * 8:
        raise ValueError("Serial records were missing, unexpected or out of order")


class _RecordingSerial:
    """Observe complete lines through fbuild's existing serial interface."""

    def __init__(self, serial: SerialInterface) -> None:
        self.serial = serial
        self.lines: list[str] = []

    async def connect(self) -> None:
        await self.serial.connect()

    async def close(self) -> None:
        await self.serial.close()

    async def write(self, data: str) -> None:
        await self.serial.write(data)

    async def reset_device(self, board: str | None) -> bool:
        return await self.serial.reset_device(board)

    async def read_lines(self, timeout: float) -> AsyncIterator[str]:
        async for line in self.serial.read_lines(timeout):
            self.lines.append(line)
            yield line


async def run_serial_startup(args: Args) -> int:
    """Build, inspect the ELF, deploy and check both UART APIs and warm heap."""
    environment = args.environment_positional or args.environment
    if not environment or not args.upload_port:
        print(
            "--serial-startup requires an explicit environment and --upload-port",
            file=sys.stderr,
        )
        return 1
    if any(getattr(args, mode) for mode in _OTHER_MODES):
        print("Run --serial-startup separately from other test modes", file=sys.stderr)
        return 1
    board = create_board(environment)
    if board.platform_family != "esp32" or board.get_mcu_target() != "ESP32":
        print("This shared-UART output fixture requires classic ESP32", file=sys.stderr)
        return 1
    if board.platform_packages != ESP32_ARDUINO_3_3_11_SERIAL_FRAMEWORK:
        print(
            "Use esp32dev: this fixture verifies the patched Arduino 3.3.11 profile",
            file=sys.stderr,
        )
        return 1
    if args.tx_pin not in (None, 4) or args.rx_pin not in (None, 4):
        print(
            "The serial fixture uses GPIO4 loopback and GPIO16/17 UART pin changes",
            file=sys.stderr,
        )
        return 1
    root = args.project_dir.resolve()
    if not args.skip_lint:
        lint = await asyncio.to_thread(
            RunningProcess.run,
            ["bash", "lint"],
            cwd=str(root),
            capture_output=True,
            text=True,
            encoding="utf-8",
            errors="replace",
            timeout=900,
        )
        print(lint.stdout)
        if lint.returncode:
            return lint.returncode
    native_only = args.serial_startup == "native-only"
    project = synthesise_autoresearch_project(
        environment,
        root,
        args.verbose,
        extra_defines=[
            "FL_TEST_SERIAL_NATIVE_ONLY=" + ("1" if native_only else "0"),
            "FL_TEST_ARDUINO_FIRST="
            + ("1" if args.serial_startup == "arduino-first" else "0"),
        ],
    )
    fixture = "tests/fl/system/serial.hpp"
    inject_serial_startup_fixture(project, root / fixture, native_only)
    compiled = await asyncio.to_thread(
        run_fbuild_compile, project, environment=environment, timeout=1800
    )
    if not compiled.success:
        return 1
    if not generate_build_info_json_from_existing_build(project, board, "AutoResearch"):
        raise ValueError("Could not generate metadata for staged AutoResearch firmware")
    elf = project / ".fbuild/build/release/firmware.elf"
    elf_digest = hashlib.sha256(elf.read_bytes()).hexdigest()
    bloat = await asyncio.to_thread(
        RunningProcess.run,
        ["bash", "bloat", environment, "--example", "AutoResearch", "--no-summary"],
        cwd=str(root),
        capture_output=True,
        text=True,
        encoding="utf-8",
        errors="replace",
        timeout=300,
    )
    print(bloat.stdout)
    if bloat.returncode:
        return bloat.returncode
    provenance = json.loads(
        (root / ".build/symbols" / environment / "provenance.json").read_text()
    )
    reported_elf = Path(provenance["elf"])
    if not reported_elf.is_absolute():
        reported_elf = root / reported_elf
    if reported_elf.resolve() != elf.resolve():
        raise ValueError("Bloat analysed a different ELF from staged AutoResearch")
    verify_serial_startup_symbols(
        root / ".build/symbols" / environment / "report.json", native_only
    )
    if hashlib.sha256(elf.read_bytes()).hexdigest() != elf_digest:
        raise ValueError("Staged firmware changed during bloat analysis")
    deployed = await asyncio.to_thread(
        run_fbuild_deploy,
        project,
        environment=environment,
        upload_port=args.upload_port,
        timeout=1800,
    )
    if not deployed.success:
        return 1
    if hashlib.sha256(elf.read_bytes()).hexdigest() != elf_digest:
        raise ValueError(
            "Deployment rebuilt a different firmware from the analysed ELF"
        )
    serial = _RecordingSerial(create_serial_interface(args.upload_port))
    client = RpcClient(
        args.upload_port,
        serial_interface=serial,
        timeout=max(30.0, parse_timeout(args.timeout or "30")),
        verbose=args.verbose,
    )
    try:
        await client.connect()
        configured = await client.send("setPins", args=[4, 4])
        if not configured.success or not configured.get("success"):
            return 1
        for phase in ("cold", "settle1", "settle2", "warm1", "warm2"):
            start = len(serial.lines)
            response = await client.send("serialStartupProbe", args={})
            print(
                f"serial_startup {args.serial_startup} {phase}: {json.dumps(response.data, sort_keys=True)}",
                flush=True,
            )
            if not response.success or not response.get("pass"):
                return 1
            if (
                response.get("coreVersion") != "3.3.11"
                or response.get("sdkMatches") is not True
            ):
                print("Device ran a different framework/SDK version", file=sys.stderr)
                return 1
            if response.get("nativeOnly") is not native_only:
                print("Device ran the wrong serial ownership fixture", file=sys.stderr)
                return 1
            if response.get("arduinoFirst") is not (
                args.serial_startup == "arduino-first"
            ):
                return 1
            verify_serial_records(serial.lines[start:], native_only)
            if phase.startswith("warm") and (
                response.get("heapBefore") is None
                or response.get("heapBefore") != response.get("heapAfter")
            ):
                print(
                    "Warm serial cycle retained heap or omitted measurements",
                    file=sys.stderr,
                )
                return 1
    finally:
        try:
            log = (
                root
                / ".build/autoresearch"
                / f"{environment}-serial-startup-{args.serial_startup}.json"
            )
            log.parent.mkdir(parents=True, exist_ok=True)
            log.write_text(json.dumps(serial.lines, indent=2))
        finally:
            await client.close()
    print(
        "Serial startup AutoResearch passed (ELF ownership, exact records and warm heap)"
    )
    return 0
