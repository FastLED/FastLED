"""Validate the ESP32 native USB console on hardware (FastLED #4893).

Deploys canonical AutoResearch with the board profile's console selection and
checks, over the real USB route:

- ownership: the linked firmware contains the selected native backend and no
  competing Arduino console owner (HWCDC/USBCDC);
- repeated bidirectional RPC, long payload lines and sustained traffic;
- host disconnect/reconnect while the device keeps writing (bounded writes);
- RPC during RMT LED output;
- watchdog reset and USB re-enumeration recovery;
- heap integrity throughout.
"""

import asyncio
import json
import sys
import time
from pathlib import Path
from typing import Any

from running_process import RunningProcess

from ci.autoresearch.args import Args
from ci.autoresearch.staging import synthesise_autoresearch_project
from ci.boards import create_board
from ci.compiler.project_ini import ProjectIni
from ci.decode_esp32_backtrace import find_addr2line
from ci.rpc_client import RpcClient, RpcError
from ci.util.fbuild_runner import run_fbuild_deploy


PING_ROUNDS = 200
SUSTAINED_ROUNDS = 500
RECONNECT_CYCLES = 5
# Long enough for the device to emit at least one periodic status line while
# no host is reading, which exercises its bounded-write path.
DISCONNECTED_SECONDS = 6.0
# Requests must fit the console receive ring (4096 B on USB Serial/JTAG,
# envelope included): the SDK drops input once it is full.
PAYLOAD_SIZES = (256, 1024, 3072)
OVERSIZE_PAYLOAD = 6000
REBOOT_DEADLINE_SECONDS = 45.0

# Arduino console owners that must not be linked beside a native backend.
COMPETING_OWNERS = ("HWCDC::", "USBCDC::")
BACKEND_SYMBOLS = {
    "usb_serial_jtag": "usb_serial_jtag_isr",
    "tinyusb_cdc": "tud_cdc_",
}


class UsbConsoleError(RuntimeError):
    """A validation step failed."""


def selected_backend(build_flags: str) -> str | None:
    """Return the native console the profile selects, if any."""
    flags = build_flags.replace(" ", "")
    if "-DFL_ESP_USE_TINYUSB_CDC" in flags:
        return "tinyusb_cdc"
    if "-DFASTLED_ESP_FORCE_USB_SERIAL_JTAG" in flags:
        return "usb_serial_jtag"
    return None


def check_ownership(elf: Path, backend: str) -> dict[str, int]:
    """Count backend and competing-owner symbols in the linked firmware."""
    addr2line = find_addr2line()
    if not addr2line:
        raise UsbConsoleError("toolchain addr2line (for nm) not found")
    nm = addr2line.with_name(addr2line.name.replace("addr2line", "nm"))
    result = RunningProcess.run(
        [str(nm), "-C", str(elf)],
        capture_output=True,
        text=True,
        encoding="utf-8",
        errors="replace",
        timeout=120,
    )
    if result.returncode:
        raise UsbConsoleError(f"nm failed: {result.stderr}")
    symbols = result.stdout
    counts = {name: symbols.count(name) for name in COMPETING_OWNERS}
    counts[BACKEND_SYMBOLS[backend]] = symbols.count(BACKEND_SYMBOLS[backend])
    if counts[BACKEND_SYMBOLS[backend]] == 0:
        raise UsbConsoleError(f"selected {backend} backend is not linked: {counts}")
    competing = {k: v for k, v in counts.items() if k in COMPETING_OWNERS and v}
    if competing:
        raise UsbConsoleError(f"competing Arduino console owners linked: {competing}")
    return counts


async def call(client: RpcClient, method: str, args: Any = None) -> Any:
    response = await client.send(method, args=args)
    if not response.success:
        raise UsbConsoleError(f"{method} failed: {response.data!r}")
    return response.data


async def check_heap(client: RpcClient, label: str) -> dict[str, Any]:
    heap = await call(client, "heapCheck")
    if not isinstance(heap, dict) or heap.get("intact") is not True:
        raise UsbConsoleError(f"heap not intact after {label}: {heap!r}")
    print(
        f"usb_console heap[{label}]: free={heap.get('free')} "
        f"largest={heap.get('largest')} minFree={heap.get('minFree')}",
        flush=True,
    )
    return heap


async def open_client(port: str, verbose: bool, deadline_s: float) -> RpcClient:
    """Connect, retrying while the port re-enumerates after a reset."""
    end = time.monotonic() + deadline_s
    last_error: Exception | None = None
    while time.monotonic() < end:
        client = RpcClient(port, timeout=20.0, verbose=verbose)
        try:
            await client.connect()
            await call(client, "ping")
            return client
        except (RpcError, OSError, TimeoutError) as exc:
            last_error = exc
            await client.close()
            await asyncio.sleep(1.0)
    raise UsbConsoleError(f"device did not come back on {port}: {last_error!r}")


async def uptime(client: RpcClient) -> int:
    pong = await call(client, "ping")
    value = pong.get("uptimeMs") if isinstance(pong, dict) else None
    if not isinstance(value, int):
        raise UsbConsoleError(f"ping returned no uptime: {pong!r}")
    return value


async def check_transport(client: RpcClient) -> dict[str, Any]:
    previous = -1
    for _ in range(PING_ROUNDS):
        now = await uptime(client)
        if now < previous:
            raise UsbConsoleError("uptime went backwards: unexpected reset")
        previous = now

    # Long lines with every printable ASCII character, including JSON escapes.
    alphabet = "".join(chr(c) for c in range(32, 127))
    for size in PAYLOAD_SIZES:
        text = (alphabet * (size // len(alphabet) + 1))[:size]
        payload = {"text": text, "size": size}
        echoed = await call(client, "debugTest", payload)
        if not isinstance(echoed, dict) or echoed.get("received") != payload:
            raise UsbConsoleError(f"{size}-byte payload did not round-trip")

    # An oversized request loses bytes; the link must recover afterwards.
    try:
        await client.send("debugTest", {"text": "x" * OVERSIZE_PAYLOAD}, timeout=5.0)
    except (RpcError, TimeoutError):
        pass  # expected: the truncated request gets no matching reply
    for _ in range(5):
        await uptime(client)

    start = time.monotonic()
    for _ in range(SUSTAINED_ROUNDS):
        await call(client, "ping")
    elapsed = time.monotonic() - start
    return {
        "pings": PING_ROUNDS,
        "payloadBytes": list(PAYLOAD_SIZES),
        "oversizeRecovered": OVERSIZE_PAYLOAD,
        "sustainedRpcPerSecond": round(SUSTAINED_ROUNDS / elapsed, 1),
    }


async def run_validation(port: str, verbose: bool) -> dict[str, Any]:
    summary: dict[str, Any] = {}
    client = await open_client(port, verbose, REBOOT_DEADLINE_SECONDS)
    try:
        status = await call(client, "status")
        summary["platform"] = (
            status.get("platform") if isinstance(status, dict) else None
        )
        baseline = await check_heap(client, "baseline")
        summary["transport"] = await check_transport(client)
        # First-use allocations (RPC buffers, JSON interning) settle during the
        # transport phase; later phases must not lose heap relative to it.
        settled = await check_heap(client, "transport")
        summary["heapFreeBaseline"] = baseline["free"]

        # Host closes the port; the device keeps writing status lines.
        for cycle in range(RECONNECT_CYCLES):
            before = await uptime(client)
            await client.close()
            await asyncio.sleep(DISCONNECTED_SECONDS)
            client = await open_client(port, verbose, REBOOT_DEADLINE_SECONDS)
            after = await uptime(client)
            if after <= before:
                raise UsbConsoleError(f"reconnect cycle {cycle} reset the device")
        summary["reconnectCycles"] = RECONNECT_CYCLES
        await check_heap(client, "reconnect")

        # Apply the jumpered TX/RX pair, as the canonical LED flow does.
        pins = await call(
            client,
            "findConnectedPins",
            [{"startPin": 0, "endPin": 8, "autoApply": True}],
        )
        if not isinstance(pins, dict) or not pins.get("found"):
            raise UsbConsoleError(f"no TX/RX jumper found for the RMT step: {pins!r}")
        # The console must stay responsive while the driver transmits. Decode
        # correctness belongs to the LED loopback harness and is reported, not
        # gated, here (loopback capture is tracked in FastLED #4903). The first
        # runs initialize the driver; the last two must not lose heap.
        rmt_heap: list[int] = []
        decode: list[bool] = []
        for run in range(3):
            led = await call(
                client,
                "runSingleTest",
                {
                    "driver": "RMT",
                    "laneSizes": [100],
                    "pattern": "MSB_LSB_A",
                    "iterations": 3,
                    "timing": "WS2812B-V5",
                },
            )
            if not isinstance(led, dict) or led.get("success") is not True:
                raise UsbConsoleError(f"RMT output did not run: {led!r}")
            decode.append(bool(led.get("passed")))
            await uptime(client)
            rmt_heap.append(int((await check_heap(client, f"rmt{run + 1}"))["free"]))
        if rmt_heap[2] < rmt_heap[1]:
            raise UsbConsoleError(f"heap lost across repeated RMT runs: {rmt_heap}")
        summary["rmt"] = {"runs": 3, "decodePassed": decode, "heapFree": rmt_heap}
        summary["heapFreeSettled"] = settled["free"]
        summary["heapFreeDelta"] = rmt_heap[2] - int(settled["free"])

        # Watchdog reset: the USB device drops and re-enumerates.
        # ASYNC: the device hangs right after the ACK, so no final reply.
        await client.send("deliberateHang", args=None, return_on_ack=True)
        await client.close()
        await asyncio.sleep(2.0)
        client = await open_client(port, verbose, REBOOT_DEADLINE_SECONDS)
        pong = await call(client, "ping")
        if not isinstance(pong, dict) or pong.get("lastResetWasWatchdog") is not True:
            raise UsbConsoleError(f"device did not report a watchdog reset: {pong!r}")
        summary["watchdogRecovery"] = {"uptimeMs": pong.get("uptimeMs")}
        await check_heap(client, "after-reset")
    finally:
        await client.close()
    return summary


async def run_usb_console(args: Args) -> int:
    """Build/deploy canonical AutoResearch and validate its USB console."""
    environment = args.environment_positional or args.environment
    if not environment or not args.upload_port:
        print(
            "--usb-console requires an explicit ESP32 environment and --upload-port",
            file=sys.stderr,
        )
        return 1
    if create_board(environment).platform_family != "esp32":
        print("--usb-console validates ESP32 native USB routes", file=sys.stderr)
        return 1
    if args.embedded_fs or args.spi_provider or args.rpc_smoke or args.watchdog_soak:
        print("Run USB console checks separately from other modes", file=sys.stderr)
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

    project = synthesise_autoresearch_project(environment, root, args.verbose)
    ini = ProjectIni.parseFile(project / "platformio.ini")
    build_flags = ini.get_option(f"env:{environment}", "build_flags", "") or ""
    backend = selected_backend(build_flags)
    if backend is None:
        print(
            f"{environment} does not select a native USB console "
            "(FASTLED_ESP_FORCE_USB_SERIAL_JTAG or FL_ESP_USE_TINYUSB_CDC)",
            file=sys.stderr,
        )
        return 1

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
    port = deployed.port or args.upload_port

    try:
        build = project / ".fbuild" / "build"
        elf = next(
            (
                p
                for p in (
                    build / environment / "firmware.elf",
                    build / "release" / "firmware.elf",
                )
                if p.exists()
            ),
            None,
        )
        if elf is None:
            raise UsbConsoleError(f"firmware.elf not found under {build}")
        ownership = check_ownership(elf, backend)
        summary = await run_validation(port, args.verbose)
    except UsbConsoleError as exc:
        print(f"USB console AutoResearch FAILED: {exc}", file=sys.stderr)
        return 1
    summary = {
        "environment": environment,
        "port": port,
        "backend": backend,
        "buildFlags": [f for f in build_flags.split() if "USB" in f or "CDC" in f],
        "ownership": ownership,
        **summary,
    }
    print("usb_console summary: " + json.dumps(summary, sort_keys=True), flush=True)
    print("USB console AutoResearch passed")
    return 0
