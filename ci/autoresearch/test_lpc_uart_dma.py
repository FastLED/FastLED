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

# Compile-time default in AutoResearchUartDma.h
# (FASTLED_LPC_UART_DMA_HARNESS_BAUD).
DEFAULT_WIRE_BAUD = 1_000_000


def send_rpc(
    s: "serial.Serial",
    method: str,
    args: list[Any] | None = None,
    request_id: int = 1,
    timeout: float = 15.0,
) -> dict[str, Any] | None:
    """JSON-RPC send/receive helper (mirrors test_lpc_dma_spi.py)."""
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


def wire_time_us(byte_count: int, wire_baud: int) -> int:
    # 8N1 = 10 bit-times per byte on the wire.
    return int(1_000_000 * 10 * byte_count / max(1, wire_baud))


def run_stream_once(
    s: "serial.Serial", byte_count: int, byte_pattern: int, wire_baud: int
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
    s: "serial.Serial", byte_count: int, byte_pattern: int, wire_baud: int
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

    if serial is None:
        print("FAIL — pyserial not available")
        return 1

    print("LPC async UART TX bench — FastLED #3453 follow-up")
    print(f"  Port: {args.port} @ {args.baud} (console on USART0)")
    print(f"  Wire: USART1 @ {args.baud_wire} baud via DMA0 ch3 + ISR chain")

    try:
        s = serial.Serial(args.port, args.baud, timeout=0.25)
    except Exception as e:
        print(f"FAIL — cannot open {args.port}: {e}")
        return 1

    time.sleep(0.5)
    try:
        s.reset_input_buffer()
    except Exception:
        pass

    ok = True
    # 2048 bytes = two full descriptors → exercises the ISR chunk chain.
    ok &= run_stream_once(s, 2048, 0xA5, args.baud_wire)
    ok &= run_stream_overlap(s, 2048, 0x5A, args.baud_wire)

    s.close()
    print()
    print("=" * 60)
    if ok:
        print("PASS — #3453 async UART TX bench green.")
        return 0
    print("FAIL — one or more cases did not meet expected bounds.")
    return 1


if __name__ == "__main__":
    sys.exit(main())
