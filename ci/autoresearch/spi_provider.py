"""Stage and run the classic ESP32 optional SPI provider hardware fixtures."""

import asyncio
import json
import sys
from pathlib import Path

from running_process import RunningProcess

from ci.autoresearch.args import Args
from ci.autoresearch.staging import synthesise_autoresearch_project
from ci.boards import create_board
from ci.compiler.project_ini import ProjectIni
from ci.rpc_client import RpcClient
from ci.util.fbuild_runner import run_fbuild_deploy
from ci.util.sketch_resolver import parse_timeout


BINDING = "void AutoResearchRemoteControl::bindDriverMethods(fl::Remote& remote) {"


def inject_spi_provider_fixture(sketch: Path, fixture: Path, faults: Path) -> None:
    """Bind the selected fixture in a staged copy of canonical AutoResearch."""
    rpc = sketch / "AutoResearchRemoteDriverMethods.cpp"
    text = rpc.read_text()
    if text.count(BINDING) != 1 or "spiProviderProbe" in text:
        raise ValueError(
            "AutoResearch driver binding changed or is already instrumented"
        )
    (sketch / "SpiProviderHil.hpp").write_bytes(fixture.read_bytes())
    (sketch / "SpiProviderFaults.hpp").write_bytes(faults.read_bytes())
    text = '#include "SpiProviderHil.hpp"\n' + text
    text = text.replace(
        BINDING,
        BINDING + '\n    remote.bind("spiProviderProbe", '
        "[](const fl::json& args) -> fl::json { return spi_provider_probe::probe(args); });",
        1,
    )
    rpc.write_text(text)


def configure_spi_provider_dependencies(ini: Path, environment: str) -> None:
    """Add bundled SPI headers without replacing board dependencies."""
    project = ProjectIni.parseFile(ini)
    section = f"env:{environment}"
    existing = project.get_option(section, "lib_deps", "") or ""
    names = {item.strip() for item in existing.replace(",", "\n").splitlines()}
    if "SPI" not in names:
        project.set_option(section, "lib_deps", "\n".join([existing, "SPI"]).strip())
        project.dump(ini)


def spi_provider_defines(native: bool) -> list[str]:
    """Alias SDK fault seams in all compiled units, including SDK call sites."""
    defines = [
        "heap_caps_malloc=fl_test_heap_caps_malloc",
        "spi_device_get_trans_result=fl_test_spi_get_trans_result"
        if native
        else "xQueueCreateMutex=fl_test_xQueueCreateMutex",
    ]
    if not native:
        defines.append("xQueueSemaphoreTake=fl_test_xQueueSemaphoreTake")
    return defines


async def run_spi_provider(args: Args) -> int:
    """Deploy the selected provider fixture and check cold and warm RPC results."""
    environment = args.environment_positional or args.environment
    if not environment or not args.upload_port:
        print(
            "--spi-provider requires an explicit classic ESP32 environment and --upload-port",
            file=sys.stderr,
        )
        return 1
    board = create_board(environment)
    if board.platform_family != "esp32" or board.get_mcu_target() != "ESP32":
        print("The SPI provider fixture requires classic ESP32", file=sys.stderr)
        return 1
    separate_modes = (
        args.parlio,
        args.rmt,
        args.spi,
        args.uart,
        args.bitbang,
        args.lcd,
        args.lcd_spi,
        args.lcd_rgb,
        args.object_fled,
        args.flex_io,
        args.lpuart,
        args.all,
        args.simd,
        args.coroutine,
        args.ieee754,
        args.math,
        args.fft,
        args.mp3,
        args.net,
        args.net_server,
        args.net_client,
        args.net_peer,
        args.ota,
        args.ble,
        args.decode,
        args.watchdog_soak,
        args.rpc_smoke,
        args.perf_wave2d,
        args.pwm_dma_cl,
        args.dma_spi,
        args.dma_uart,
        args.rp_spi_loopback,
        args.rp_spi_public_api,
        args.rp_pio_both,
        args.test_fault_emit,
        args.embedded_fs,
        args.embedded_fs_provider,
        args.embedded_fs_format_fixture,
        args.pin_toggle_rx,
        args.ws2812_loopback,
        args.tight_timing,
        args.parallel,
        args.legacy,
        args.legacy_mixed_timings,
        args.legacy_rgbw_small_counts,
    )
    if any(separate_modes):
        print(
            "Run SPI provider checks separately from other test modes", file=sys.stderr
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
    project = synthesise_autoresearch_project(
        environment,
        root,
        args.verbose,
        extra_defines=spi_provider_defines(args.spi_provider_native),
    )
    fixture = (
        "tests/platforms/esp/32/drivers/spi/spi_hw_1_esp32.hpp"
        if args.spi_provider_native
        else "tests/platforms/arduino/spi/spi_arduino.hpp"
    )
    inject_spi_provider_fixture(
        project / "src/sketch",
        root / fixture,
        root / "tests/platforms/esp/32/drivers/spi/spi_provider.hpp",
    )
    configure_spi_provider_dependencies(project / "platformio.ini", environment)
    deployed = await asyncio.to_thread(
        run_fbuild_deploy,
        project,
        environment=environment,
        upload_port=args.upload_port,
        timeout=1800,
    )
    if not deployed.success:
        print(deployed.output)
        return 1
    client = RpcClient(
        args.upload_port,
        timeout=max(30.0, parse_timeout(args.timeout or "30")),
        verbose=args.verbose,
    )
    await client.connect()
    try:
        # SDK/task allocation settles over the first three hardware lifecycles.
        # Require exact heap parity on two subsequent cycles; no leak tolerance.
        for phase in ("cold", "settle1", "settle2", "warm1", "warm2"):
            result = await client.send("spiProviderProbe", args={})
            print(
                f"spi_provider {phase}: {json.dumps(result.data, sort_keys=True)}",
                flush=True,
            )
            if not result.success or not result.get("success"):
                return 1
            provider = not args.spi_provider_native
            if result.get("provider") is not provider:
                print("SPI fixture used the wrong backend", file=sys.stderr)
                return 1
            provider_calls = result.get("providerCalls", 0)
            if provider and (
                not isinstance(provider_calls, int) or provider_calls <= 0
            ):
                print("Arduino SPI provider was not called", file=sys.stderr)
                return 1
            if phase.startswith("warm") and (
                result.get("heapStart") is None
                or result.get("heapEnd") != result.get("heapStart")
            ):
                print(
                    "Warm SPI cycles retained heap or omitted heap measurements",
                    file=sys.stderr,
                )
                return 1
    finally:
        await client.close()
    print("SPI provider AutoResearch passed (cold + warm)")
    return 0
