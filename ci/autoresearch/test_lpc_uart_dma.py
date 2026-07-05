"""FastLED #3453 follow-up — bench-validate the LPC845 async UART TX driver.

Sibling of `test_lpc_dma_spi.py` for the ISR-refillable UART path.
Drives `ARMHardwareUARTOutputDMA<>` (USART1 TX + DMA0 channel 3, chunks
chained from the DMA0 completion ISR) on the LPC845-BRK and checks:

    1. `uartDmaStreamOnce(byte_count, byte_pattern)`
        Full async stream with a CPU-poll wait. 2048 bytes = two
        1024-transfer descriptors, so completion REQUIRES the ISR
        chunk-chain to fire — a silent chain stall times out into a
        register-snapshot CSV on the device.

    2. `uartDmaStreamOverlap(byte_count, byte_pattern)`
        The affirmative async proof: beacon-toggle counter runs on the
        main thread while the DMA/ISR machinery drives USART1 TX.
        Non-zero toggles across a multi-chunk stream proves both the
        CPU-free property and the ISR refill.

Throughout, USART0 keeps serving this very RPC conversation — the
"drive a data lane and keep printing" story is validated implicitly by
every reply that arrives.

The RPCs are bound when the LPC845 firmware is compiled with
`-DFASTLED_LPC_UART_DMA=1` (plus `-DFASTLED_LPC_DMA_ISR=1` for the ISR
hub); `bash autoresearch --dma-uart` injects both.

Requires the ACLPC core with named weak IRQ vector slots
(framework-arduino-lpc8xx#38) — on older cores the ISR never fires and
case 1 reports the device-side timeout snapshot.

Usage:
    uv run python ci/autoresearch/test_lpc_uart_dma.py [--port COM10]

Exits 0 on success, 1 on any failed assertion / RPC error.
"""

from __future__ import annotations

import argparse
import asyncio
import json
import sys
from pathlib import Path
from typing import Any


# Repo root on sys.path so `ci.*` imports resolve when run directly.
_REPO_ROOT = Path(__file__).resolve().parents[2]
if str(_REPO_ROOT) not in sys.path:
    sys.path.insert(0, str(_REPO_ROOT))

from ci.rpc_client import RpcClient, RpcError, RpcTimeoutError  # noqa: E402
from ci.util.serial_interface import create_serial_interface  # noqa: E402


DEFAULT_PORT = "COM10"
DEFAULT_BAUD = 115200

# Compile-time default in AutoResearchUartDma.h
# (FASTLED_LPC_UART_DMA_HARNESS_BAUD).
DEFAULT_WIRE_BAUD = 1_000_000


class UartBench:
    """Synchronous facade over the async fbuild-backed RpcClient.

    Device serial goes through fbuild's native (Rust) serial monitor,
    NEVER raw pyserial — see agents/docs/hardware-autoresearch.md
    "Device serial". Same pattern as test_lpc_dma_spi.py::SpiBench.
    """

    def __init__(self, port: str, timeout: float = 15.0) -> None:
        self._loop = asyncio.new_event_loop()
        iface = create_serial_interface(port)
        self._client = RpcClient(port, timeout=timeout, serial_interface=iface)
        self._loop.run_until_complete(
            self._client.connect(boot_wait=3.0, drain_boot=True)
        )

    def call(
        self, method: str, args: list[Any] | None = None, timeout: float | None = None
    ) -> Any:
        try:
            resp = self._loop.run_until_complete(
                self._client.send(method, args=args, timeout=timeout)
            )
        except (RpcTimeoutError, RpcError):
            return None
        # CSV-string result: RpcClient coerces non-dict results to {} but
        # keeps the full line in raw_line — recover the string there.
        line = resp.raw_line or ""
        for prefix in ("REMOTE: ", "RESULT: "):
            if line.startswith(prefix):
                line = line[len(prefix) :]
                break
        try:
            obj = json.loads(line)
        except (json.JSONDecodeError, ValueError):
            return None
        return obj.get("result")

    def close(self) -> None:
        try:
            self._loop.run_until_complete(self._client.close())
        finally:
            self._loop.close()


def send_rpc(
    bench: "UartBench",
    method: str,
    args: list[Any] | None = None,
    timeout: float = 15.0,
) -> dict[str, Any] | None:
    """Shim so run_* helpers keep their `reply["result"]` shape."""
    result = bench.call(method, args=args, timeout=timeout)
    if result is None:
        return None
    return {"result": result}


def parse_csv_result(result: Any) -> list[str] | None:
    if not isinstance(result, str) or not result:
        return None
    parts = result.split(",")
    if not parts or parts[0] != "1":
        return None
    return parts


def wire_time_us(byte_count: int, wire_baud: int) -> int:
    # 8N1 = 10 bit-times per byte on the wire.
    return int(1_000_000 * 10 * byte_count / max(1, wire_baud))


def run_stream_once(
    s: "UartBench", byte_count: int, byte_pattern: int, wire_baud: int
) -> bool:
    print()
    print(
        f"  RPC uartDmaStreamOnce(byte_count={byte_count}, "
        f"byte_pattern=0x{byte_pattern:02X})"
    )
    reply = send_rpc(s, "uartDmaStreamOnce", args=[byte_count, byte_pattern])
    if reply is None or "result" not in reply:
        print("    FAIL — no reply")
        return False
    parts = parse_csv_result(reply["result"])
    if parts is None or len(parts) < 3:
        print(f"    FAIL — device reported: {reply['result']!r}")
        return False
    try:
        total_us = int(parts[1])
        sent = int(parts[2])
    except ValueError as e:
        print(f"    FAIL — CSV parse error: {e}")
        return False

    theoretical_us = wire_time_us(sent, wire_baud)
    # Lower bound guards against a stream that "completed" without
    # clocking the wire; upper bound = theoretical + 20 ms slack for
    # chunk-chain latency and RPC framing.
    lo = int(theoretical_us * 0.8)
    hi = theoretical_us + 20_000
    ok = lo <= total_us <= hi
    verdict = "PASS" if ok else "FAIL"
    print(
        f"    {verdict} — total={total_us} us for {sent} bytes "
        f"(wire-theoretical={theoretical_us} us, band [{lo}, {hi}])"
    )
    return ok


def run_stream_overlap(
    s: "UartBench", byte_count: int, byte_pattern: int, wire_baud: int
) -> bool:
    print()
    print(
        f"  RPC uartDmaStreamOverlap(byte_count={byte_count}, "
        f"byte_pattern=0x{byte_pattern:02X})"
    )
    reply = send_rpc(s, "uartDmaStreamOverlap", args=[byte_count, byte_pattern])
    if reply is None or "result" not in reply:
        print("    FAIL — no reply")
        return False
    parts = parse_csv_result(reply["result"])
    if parts is None or len(parts) < 3:
        print(f"    FAIL — device reported: {reply['result']!r}")
        return False
    try:
        total_us = int(parts[1])
        toggles = int(parts[2])
    except ValueError as e:
        print(f"    FAIL — CSV parse error: {e}")
        return False

    theoretical_us = wire_time_us(byte_count, wire_baud)
    ok = toggles > 0 and total_us >= int(theoretical_us * 0.8)
    verdict = "PASS" if ok else "FAIL"
    rate = toggles / total_us if total_us else 0.0
    print(
        f"    {verdict} — total={total_us} us, toggles={toggles} "
        f"({rate:.1f}/us) across a multi-chunk stream"
    )
    return ok


def main() -> int:
    ap = argparse.ArgumentParser(
        description="LPC845 async UART TX bench (#3453 follow-up)"
    )
    ap.add_argument("--port", default=DEFAULT_PORT)
    ap.add_argument("--baud", type=int, default=DEFAULT_BAUD, help="console baud")
    ap.add_argument(
        "--baud-wire",
        type=int,
        default=DEFAULT_WIRE_BAUD,
        help="USART1 wire baud the firmware was built with "
        "(FASTLED_LPC_UART_DMA_HARNESS_BAUD)",
    )
    args = ap.parse_args()

    print("LPC async UART TX bench — FastLED #3453 follow-up")
    print(f"  Port: {args.port} @ {args.baud} (console on USART0, Rust serial)")
    print(f"  Wire: USART1 @ {args.baud_wire} baud via DMA0 ch3 + ISR chain")

    try:
        bench = UartBench(args.port)
    except (RpcError, OSError) as e:
        print(f"FAIL — cannot connect to {args.port}: {e}")
        return 1

    try:
        ok = True
        # 2048 bytes = two full descriptors → exercises the ISR chunk chain.
        ok &= run_stream_once(bench, 2048, 0xA5, args.baud_wire)
        ok &= run_stream_overlap(bench, 2048, 0x5A, args.baud_wire)
    finally:
        bench.close()

    print()
    print("=" * 60)
    if ok:
        print("PASS — #3453 async UART TX bench green.")
        return 0
    print("FAIL — one or more cases did not meet expected bounds.")
    return 1


if __name__ == "__main__":
    sys.exit(main())
