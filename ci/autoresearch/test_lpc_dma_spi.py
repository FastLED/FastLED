"""FastLED #3456 — bench-validate the LPC845 SPI+DMA async driver.

Sibling of `test_lpc_pwm_dma_cl.py` for the SPI+DMA async path.
Drives `ARMHardwareSPIOutputDMA<>` on the LPC845-BRK and checks:

    1. `dmaSpiTransferOnce(byte_count, byte_pattern)`
        Single-shot DMA transfer with a CPU-poll wait. Passes if the
        RPC returns success and the wall-clock duration is within a
        loose upper bound (transfer + RPC overhead).

    2. `dmaSpiTransferOverlap(byte_count, byte_pattern)`
        The affirmative async proof. Kicks the DMA stream, then runs a
        tight `volatile` counter until `done()` returns true. A
        non-zero counter under DMA load proves the CPU was free while
        the DMA0 channel drove SPI TX.

    3. `dmaSpiMeasureSck(divider_hint)`
        Measures the effective SCK rate via wall-clock over a fixed
        512-byte burst. Compared against the compile-time driver
        divider (default 6 → 4 MHz on the 24 MHz LPC845 FRO).

The three RPCs are bound automatically when the LPC845 firmware is
compiled with `-DFASTLED_LPC_SPI_DMA=1`; the `bash autoresearch
--dma-spi` wrapper expects that flag to already be present in
`build_flags` (add it to `platformio.ini` or pass it via the fbuild
config for `lpc845`).

**Silicon status.** This is the on-device Phase-1 bench for #3453 —
the driver header self-gates and compiles clean, but every timing
band below is only meaningful when run against real LPC845 silicon.
Host-only CI cannot execute this runner.

Usage:
    uv run python -m ci.autoresearch.test_lpc_dma_spi [--port COM10]

Exits 0 on success, 1 on any failed assertion / RPC error.
"""

from __future__ import annotations

import argparse
import asyncio
import json
import sys
from pathlib import Path
from typing import Any

from ci.rpc_client import RpcClient, RpcError, RpcTimeoutError  # noqa: E402
from ci.util.serial_interface import create_serial_interface  # noqa: E402


DEFAULT_PORT = "COM10"
DEFAULT_BAUD = 115200

# Sentinel: the firmware doesn't have this RPC bound (non-ISR build).
_METHOD_NOT_FOUND = object()

# Compile-time defaults in AutoResearchSpiDma.h. If the host overrides
# via -DFASTLED_LPC_SPI_DMA_HARNESS_DIVIDER=<N>, adjust here or pass
# --divider on the CLI.
DEFAULT_HARNESS_DIVIDER = 6
DEFAULT_HARNESS_CORE_HZ = 24_000_000


class SpiBench:
    """Synchronous facade over the async fbuild-backed RpcClient.

    Device serial goes through fbuild's native (Rust) serial monitor —
    NEVER raw pyserial (see agents/docs/hardware-autoresearch.md "Device
    serial"). The raw-pyserial version of this runner dropped replies
    ~one-per-session on Windows; RpcClient gives mandatory JSON-RPC id
    correlation and correct framing over that transport.
    """

    def __init__(self, port: str, timeout: float = 10.0) -> None:
        self._loop = asyncio.new_event_loop()
        iface = create_serial_interface(port)
        self._client = RpcClient(port, timeout=timeout, serial_interface=iface)
        self._loop.run_until_complete(
            self._client.connect(boot_wait=3.0, drain_boot=True)
        )

    def call(
        self, method: str, args: list[Any] | None = None, timeout: float | None = None
    ) -> Any:
        """Return the RPC's `result` (a CSV string for these handlers),
        None on timeout, or _METHOD_NOT_FOUND when the device doesn't
        have the method bound (non-ISR build)."""
        try:
            resp = self._loop.run_until_complete(
                self._client.send(method, args=args, timeout=timeout)
            )
        except RpcTimeoutError:
            return None
        except RpcError as e:
            msg = str(e).lower()
            if "not found" in msg or "unknown method" in msg or "method" in msg:
                return _METHOD_NOT_FOUND
            return None
        # These handlers return a CSV *string* result. RpcClient coerces
        # non-dict results to {} (it targets JSON-object handlers), but
        # preserves the full line in raw_line — recover the string there.
        line = resp.raw_line or ""
        for prefix in ("REMOTE: ", "RESULT: "):
            if line.startswith(prefix):
                line = line[len(prefix) :]
                break
        try:
            obj = json.loads(line)
        except (json.JSONDecodeError, ValueError):
            return resp.data
        return obj.get("result", resp.data)

    def close(self) -> None:
        try:
            self._loop.run_until_complete(self._client.close())
        finally:
            self._loop.close()


def send_rpc(
    bench: SpiBench,
    method: str,
    args: list[Any] | None = None,
    timeout: float = 10.0,
) -> dict[str, Any] | None:
    """Thin shim so the run_* helpers keep their `reply["result"]` shape.

    Returns {"result": <csv>} on success, {"error": ...} when the method
    isn't bound, or None on timeout.
    """
    result = bench.call(method, args=args, timeout=timeout)
    if result is None:
        return None
    if result is _METHOD_NOT_FOUND:
        return {"error": {"code": -32601, "message": "method not found"}}
    return {"result": result}


def parse_csv_result(result: Any) -> list[str] | None:
    if not isinstance(result, str) or not result:
        return None
    parts = result.split(",")
    if not parts or parts[0] != "1":
        return None
    return parts


def run_transfer_once(
    s: "SpiBench",
    byte_count: int,
    byte_pattern: int,
    divider: int,
    core_hz: int,
) -> bool:
    print()
    print(
        f"  RPC dmaSpiTransferOnce(byte_count={byte_count}, byte_pattern=0x{byte_pattern:02X})"
    )
    reply = send_rpc(s, "dmaSpiTransferOnce", args=[byte_count, byte_pattern])
    if reply is None or "result" not in reply:
        print("    FAIL — no reply")
        return False
    parts = parse_csv_result(reply["result"])
    if parts is None or len(parts) < 4:
        print(f"    FAIL — malformed CSV: {reply['result']!r}")
        return False

    try:
        started_us = int(parts[1])
        done_us = int(parts[2])
    except ValueError as e:
        print(f"    FAIL — CSV parse error: {e}")
        return False

    duration_us = done_us - started_us
    # Loose upper bound: 8 bits/byte * byte_count / SCK_hz + 3ms slack
    # for RPC framing and TX-idle drain. Effective SCK = core / divider.
    sck_hz = max(1, core_hz // max(1, divider))
    theoretical_us = int(1_000_000 * 8 * byte_count / sck_hz)
    upper_bound_us = theoretical_us + 3_000
    ok = 0 < duration_us < upper_bound_us
    verdict = "PASS" if ok else "FAIL"
    print(
        f"    {verdict} — duration={duration_us} us "
        f"(theoretical={theoretical_us} us, upper_bound={upper_bound_us} us)"
    )
    return ok


def run_transfer_overlap(
    s: "SpiBench",
    byte_count: int,
    byte_pattern: int,
) -> bool:
    print()
    print(
        f"  RPC dmaSpiTransferOverlap(byte_count={byte_count}, byte_pattern=0x{byte_pattern:02X})"
    )
    reply = send_rpc(s, "dmaSpiTransferOverlap", args=[byte_count, byte_pattern])
    if reply is None or "result" not in reply:
        print("    FAIL — no reply")
        return False
    parts = parse_csv_result(reply["result"])
    if parts is None or len(parts) < 3:
        print(f"    FAIL — malformed CSV: {reply['result']!r}")
        return False

    try:
        total_us = int(parts[1])
        toggle_count = int(parts[2])
    except ValueError as e:
        print(f"    FAIL — CSV parse error: {e}")
        return False

    # The affirmative async claim: non-zero toggle count under DMA load.
    # Any positive count proves the CPU was free to run the beacon loop
    # while the DMA channel drove the SPI TX pipeline. The absolute
    # count depends on core clock + compiler codegen for the volatile
    # counter loop — we just require > 0.
    ok = toggle_count > 0 and total_us > 0
    verdict = "PASS" if ok else "FAIL"
    print(
        f"    {verdict} — total={total_us} us, toggles={toggle_count} "
        f"({toggle_count / max(1, total_us):.1f}/us)"
    )
    if not ok:
        print("           toggle_count == 0 means DMA blocked the CPU")
        print("           (or the volatile counter got optimized out).")
    return ok


def run_stream_overlap(s: "SpiBench") -> bool:
    """ISR-refilled streaming async proof (#3453 follow-up, #3585).

    Streams a multi-chunk frame (default 512 B = 2 kHalfCap chunks + 1 ISR
    refill) via ``dmaSpiStreamOverlap`` while the device beacon-toggles on
    the main thread. Runs the RPC three times back-to-back — the repeat is
    the point: the ISR ping-pong must survive re-arming without corrupting
    memory (the #3585 stack/heap-collision regression). Only bound on
    ``-DFASTLED_LPC_DMA_ISR=1`` firmware; a missing handler is a SKIP.
    """
    print()
    print("  RPC dmaSpiStreamOverlap ×3 (ISR ping-pong refill, #3453/#3585)")
    for attempt in range(1, 4):
        reply = send_rpc(
            s,
            "dmaSpiStreamOverlap",
            args=[512, 0x40 + attempt],
        )
        if reply is None:
            print("    FAIL — no reply")
            return False
        if "error" in reply and "result" not in reply:
            # method-not-found ⇒ non-ISR build ⇒ skip (not a failure).
            print("    SKIP — dmaSpiStreamOverlap not bound (non-ISR build)")
            return True
        parts = parse_csv_result(reply.get("result"))
        if parts is None or len(parts) < 3:
            print(f"    FAIL — run {attempt} malformed CSV: {reply.get('result')!r}")
            return False
        try:
            total_us = int(parts[1])
            toggles = int(parts[2])
        except ValueError as e:
            print(f"    FAIL — run {attempt} CSV parse error: {e}")
            return False
        if toggles <= 0 or total_us <= 0:
            print(f"    FAIL — run {attempt}: toggles={toggles}, total_us={total_us}")
            return False
        print(f"    run {attempt}: PASS — total={total_us} us, toggles={toggles}")
    return True


def run_measure_sck(
    s: "SpiBench",
    divider: int,
    core_hz: int,
) -> bool:
    print()
    print(f"  RPC dmaSpiMeasureSck(divider_hint={divider})")
    reply = send_rpc(s, "dmaSpiMeasureSck", args=[divider])
    if reply is None or "result" not in reply:
        print("    FAIL — no reply")
        return False
    parts = parse_csv_result(reply["result"])
    if parts is None or len(parts) < 3:
        print(f"    FAIL — malformed CSV: {reply['result']!r}")
        return False

    try:
        total_us = int(parts[1])
        byte_count = int(parts[2])
    except ValueError as e:
        print(f"    FAIL — CSV parse error: {e}")
        return False

    if total_us <= 0 or byte_count <= 0:
        print(f"    FAIL — zero total_us or byte_count ({total_us=}, {byte_count=})")
        return False

    measured_sck_hz = int(8 * byte_count * 1_000_000 / total_us)
    wire_sck_hz = max(1, core_hz // max(1, divider))
    # Effective-throughput model, NOT raw wire SCK. The LPC845 SPI is
    # FIFO-less (single-buffered): each byte costs 8/SCK of shifting
    # PLUS one DMA request→TXDAT-refill round trip, because TXRDY only
    # re-asserts after the shift completes (no holding register to
    # overlap the refill with). Silicon measurement 2026-07-02
    # (LPC845-BRK, core=24 MHz, divider=6 → wire SCK 4 MHz):
    # 512 bytes in ~1414 us → 2.90 MHz effective, i.e. ~0.76 us of DMA
    # refill latency per byte on top of the 2.0 us shift time. Model
    # the expectation accordingly and keep a ±30% band around it for
    # the WDOSC/clock-corner spread.
    dma_refill_us_per_byte = 0.9  # measured 0.76 us + margin for slower corners
    shift_us_per_byte = 8 * 1_000_000 / wire_sck_hz  # 2.0 us at 4 MHz wire SCK
    expected_eff_hz = int(8 * 1_000_000 / (shift_us_per_byte + dma_refill_us_per_byte))
    lo = int(expected_eff_hz * 0.70)
    hi = int(wire_sck_hz * 1.25)  # can't beat the wire rate (+tolerance)
    ok = lo <= measured_sck_hz <= hi
    verdict = "PASS" if ok else "FAIL"
    print(
        f"    {verdict} — measured effective SCK={measured_sck_hz} Hz "
        f"(wire SCK {wire_sck_hz} Hz, FIFO-less+DMA model ~{expected_eff_hz} Hz, "
        f"band [{lo}, {hi}])"
    )
    return ok


def main() -> int:
    ap = argparse.ArgumentParser(
        description="LPC845 SPI+DMA async driver bench (#3456)"
    )
    ap.add_argument("--port", default=DEFAULT_PORT)
    ap.add_argument(
        "--divider",
        type=int,
        default=DEFAULT_HARNESS_DIVIDER,
        help="Compile-time SPI divider baked into the harness "
        "(FASTLED_LPC_SPI_DMA_HARNESS_DIVIDER; default 6 → 4 MHz).",
    )
    ap.add_argument(
        "--core-hz",
        type=int,
        default=DEFAULT_HARNESS_CORE_HZ,
        help="LPC845 core clock in Hz (default 24_000_000).",
    )
    args = ap.parse_args()

    print("LPC SPI+DMA async driver bench — FastLED #3456 (Phase 1 of #3453)")
    print(f"  Port: {args.port} @ {DEFAULT_BAUD} (fbuild Rust serial monitor)")
    print(
        f"  Expected SCK: {args.core_hz // max(1, args.divider)} Hz "
        f"(core={args.core_hz}, divider={args.divider})"
    )

    try:
        bench = SpiBench(args.port)
    except (RpcError, OSError) as e:
        print(f"Could not connect to {args.port}: {e}")
        return 1

    try:
        all_pass = True

        # Case 1: small single-shot transfer.
        all_pass &= run_transfer_once(bench, 32, 0xA5, args.divider, args.core_hz)

        # Case 2: larger single-shot transfer — exercises the DMA descriptor
        # path proper (bytes > 16 → DMA engages per the driver's fast-path).
        all_pass &= run_transfer_once(bench, 256, 0x5A, args.divider, args.core_hz)

        # Case 3: async proof.
        all_pass &= run_transfer_overlap(bench, 512, 0xFF)

        # Case 4: SCK rate measurement.
        all_pass &= run_measure_sck(bench, args.divider, args.core_hz)

        # Case 5: ISR-refilled streaming (#3453 follow-up). Only bound when the
        # firmware was built with -DFASTLED_LPC_DMA_ISR=1; a build without it
        # returns "method not found", which we treat as skip-not-fail.
        all_pass &= run_stream_overlap(bench)
    finally:
        bench.close()

    print()
    print("=" * 60)
    if all_pass:
        print("PASS — #3456 SPI+DMA async bench green.")
        return 0
    else:
        print("FAIL — one or more cases did not meet expected bounds.")
        return 1


if __name__ == "__main__":
    sys.exit(main())
