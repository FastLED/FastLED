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
    uv run python ci/autoresearch/test_lpc_dma_spi.py [--port COM10]

Exits 0 on success, 1 on any failed assertion / RPC error.
"""

from __future__ import annotations

import argparse
import json
import sys
import time
from typing import Any


try:
    import serial  # type: ignore
except ImportError:
    serial = None  # type: ignore


DEFAULT_PORT = "COM10"
DEFAULT_BAUD = 115200

# Compile-time defaults in AutoResearchSpiDma.h. If the host overrides
# via -DFASTLED_LPC_SPI_DMA_HARNESS_DIVIDER=<N>, adjust here or pass
# --divider on the CLI.
DEFAULT_HARNESS_DIVIDER = 6
DEFAULT_HARNESS_CORE_HZ = 24_000_000


def send_rpc(
    s: "serial.Serial",
    method: str,
    args: list[Any] | None = None,
    request_id: int = 1,
    timeout: float = 10.0,
) -> dict[str, Any] | None:
    """JSON-RPC send/receive helper.

    Mirrors `test_lpc_pwm_dma_cl.py::send_rpc` — kept inline so the
    runner has no cross-file dependencies beyond stdlib + pyserial.
    """
    params = args if args is not None else []
    req = {"jsonrpc": "2.0", "method": method, "params": params, "id": request_id}
    s.write((json.dumps(req, separators=(",", ":")) + "\n").encode("ascii"))
    s.flush()

    deadline = time.time() + timeout
    buf = b""
    while time.time() < deadline:
        chunk = s.read(s.in_waiting or 1)
        if not chunk:
            continue
        buf += chunk
        while b"\n" in buf:
            line, _, buf = buf.partition(b"\n")
            text = line.decode("ascii", errors="replace").strip()
            for prefix in ("REMOTE: ", "RESULT: "):
                if text.startswith(prefix):
                    text = text[len(prefix) :]
                    break
            if not (text.startswith("{") and text.endswith("}")):
                continue
            try:
                msg = json.loads(text)
            except json.JSONDecodeError:
                continue
            if isinstance(msg, dict) and msg.get("id") == request_id:
                return msg
    return None


def parse_csv_result(result: Any) -> list[str] | None:
    if not isinstance(result, str) or not result:
        return None
    parts = result.split(",")
    if not parts or parts[0] != "1":
        return None
    return parts


def run_transfer_once(
    s: "serial.Serial",
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
    s: "serial.Serial",
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


def run_measure_sck(
    s: "serial.Serial",
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
    expected_sck_hz = max(1, core_hz // max(1, divider))
    # Loose ±25% band — the total_us includes the SPI master
    # shift-register drain, which adds a fixed ~2 byte-times of
    # overhead. On a 512-byte burst that's ~0.5% at 4 MHz, but on
    # slower dividers it grows.
    lo = int(expected_sck_hz * 0.75)
    hi = int(expected_sck_hz * 1.25)
    ok = lo <= measured_sck_hz <= hi
    verdict = "PASS" if ok else "FAIL"
    print(
        f"    {verdict} — measured SCK={measured_sck_hz} Hz "
        f"(expected ~{expected_sck_hz} Hz, band [{lo}, {hi}])"
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

    if serial is None:
        print("pyserial not available; install with `uv sync`")
        return 1

    print("LPC SPI+DMA async driver bench — FastLED #3456 (Phase 1 of #3453)")
    print(f"  Port: {args.port} @ {DEFAULT_BAUD}")
    print(
        f"  Expected SCK: {args.core_hz // max(1, args.divider)} Hz "
        f"(core={args.core_hz}, divider={args.divider})"
    )

    try:
        s = serial.Serial(args.port, DEFAULT_BAUD, timeout=0.1)
    except serial.SerialException as e:
        print(f"Could not open {args.port}: {e}")
        return 1

    # Give the LPC845 a moment to boot after opening the port.
    time.sleep(0.5)
    s.reset_input_buffer()

    all_pass = True

    # Case 1: small single-shot transfer.
    all_pass &= run_transfer_once(s, 32, 0xA5, args.divider, args.core_hz)

    # Case 2: larger single-shot transfer — exercises the DMA descriptor
    # path proper (bytes > 16 → DMA engages per the driver's fast-path).
    all_pass &= run_transfer_once(s, 256, 0x5A, args.divider, args.core_hz)

    # Case 3: async proof.
    all_pass &= run_transfer_overlap(s, 512, 0xFF)

    # Case 4: SCK rate measurement.
    all_pass &= run_measure_sck(s, args.divider, args.core_hz)

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
