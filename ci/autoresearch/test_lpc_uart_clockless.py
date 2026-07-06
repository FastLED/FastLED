"""FastLED #3611 - LPC845 UART DMA clockless loopback via FastLED API.

This runner calls the device-side `uartDmaClocklessLoopbackSelf` RPC bound by
examples/AutoResearch/AutoResearchUartDma.h when FASTLED_LPC_UART_DMA=1. The
firmware drives LEDs through the normal FastLED.addLeds<WS2812, PIN, GRB>() +
FastLED.show() path, routes USART RXD to --rx-pin through the LPC845 switch
matrix, and verifies the USART RX-DMA bytes against the expected clockless
expansion.
"""

from __future__ import annotations

import argparse
import sys

from ci.autoresearch.rpc_bench import METHOD_NOT_FOUND, RpcBench  # noqa: E402
from ci.util.global_interrupt_handler import handle_keyboard_interrupt  # noqa: E402


DEFAULT_PORT = "COM10"
DEFAULT_BAUD = 115200
DEFAULT_LED_COUNT = 8


def send_rpc(
    s: "RpcBench",
    method: str,
    args=None,
    timeout: float = 10.0,
):
    """Call through RpcBench so serial stays on fbuild's monitor path."""
    result = s.call(method, args=args, timeout=timeout)
    if result is None:
        return None
    if result is METHOD_NOT_FOUND:
        return {"error": {"code": -32601, "message": "method not found"}}
    return {"result": result}


def send_rpc_flat(
    s: "RpcBench",
    method: str,
    args=None,
    timeout: float = 10.0,
):
    """Call low-memory LPC positional RPC methods."""
    result = s.call_flat(method, args=args, timeout=timeout)
    if result is None:
        return None
    if result is METHOD_NOT_FOUND:
        return {"error": {"code": -32601, "message": "method not found"}}
    return {"result": result}


def expected_grb_bytes(led_count: int, rgb: int) -> list[int]:
    r = (rgb >> 16) & 0xFF
    g = (rgb >> 8) & 0xFF
    b = rgb & 0xFF
    return [g, r, b] * led_count


def print_device_failure(csv: str, data_pin: int, rx_pin: int) -> None:
    parts = csv.split(",")
    if len(parts) >= 2 and parts[0] == "0" and parts[1] == "loopback":
        rx_count = parts[2] if len(parts) > 2 else "?"
        expected_count = parts[3] if len(parts) > 3 else "?"
        stream_done = parts[4] if len(parts) > 4 else "?"
        print(
            "    FAIL - UART RX-DMA mismatch: "
            f"received={rx_count}, expected={expected_count}, "
            f"uart_stream_done={stream_done}"
        )
        if len(parts) > 5:
            print(f"      diagnostics: {parts[5:]}")
        return

    print(f"    FAIL - device reported failure: {csv!r}")


def run_capture_self(
    s: "RpcBench",
    led_count: int,
    rgb: int,
    data_pin: int,
    rx_pin: int,
    capture_ms: int,
) -> bool:
    print()
    print(
        f"  RPC uartDmaClocklessLoopbackSelf(led_count={led_count}, "
        f"rgb=0x{rgb:06X}, data_pin={data_pin}, rx_pin={rx_pin}, "
        f"capture_ms={capture_ms})"
    )
    reply = send_rpc(
        s,
        "uartDmaClocklessLoopbackSelf",
        args=[led_count, rgb, data_pin, rx_pin, capture_ms],
    )
    if reply is None or "result" not in reply:
        print("    FAIL - no reply or error result")
        return False

    csv = reply["result"]
    if not isinstance(csv, str) or not csv:
        print(f"    FAIL - malformed result: {csv!r}")
        return False

    parts = csv.split(",")
    if not parts or parts[0] != "1":
        print_device_failure(csv, data_pin, rx_pin)
        return False

    if len(parts) < 2:
        print(f"    FAIL - CSV missing length field: {csv!r}")
        return False

    try:
        n_decoded = int(parts[1])
        decoded_bytes = [int(x) for x in parts[2 : 2 + n_decoded]]
    except (ValueError, IndexError) as e:
        print(f"    FAIL - CSV parse error: {e} ({csv!r})")
        return False

    expected = expected_grb_bytes(led_count, rgb)
    if decoded_bytes != expected:
        print("    FAIL - byte mismatch")
        print(f"      expected: {expected}")
        print(f"      decoded:  {decoded_bytes}")
        return False

    print(f"    PASS - {n_decoded} bytes match expected pattern")
    return True


def main() -> int:
    ap = argparse.ArgumentParser(
        description="LPC845 UART DMA clockless FastLED API loopback (#3611)"
    )
    ap.add_argument("--port", default=DEFAULT_PORT)
    ap.add_argument("--tx-pin", type=int, default=10)
    ap.add_argument("--rx-pin", type=int, default=11)
    args = ap.parse_args()

    print("LPC UART DMA clockless RX-DMA loopback - FastLED #3611")
    print(f"  Port: {args.port} @ {DEFAULT_BAUD}")
    print(
        f"  UART loopback: bridge U1_TXD P0_{args.tx_pin} -> "
        f"U1_RXD P0_{args.rx_pin}"
    )

    try:
        s = RpcBench(args.port)
    except KeyboardInterrupt as ki:
        handle_keyboard_interrupt(ki)
        raise
    except Exception as e:  # noqa: BLE001
        print(f"Could not connect to {args.port}: {e}")
        return 1

    with s:
        s.reset_and_drain()
        echo = send_rpc_flat(s, "echo", args=[42], timeout=5.0)
        if not echo or echo.get("result") != 42:
            print(
                "ERROR: echo failed; is AutoResearch firmware flashed and "
                f"running? Got: {echo}",
                file=sys.stderr,
            )
            return 2
        print("[lpc-uart-clockless] echo ok")

        for step in (0, 7):
            probe = send_rpc(
                s,
                "uartDmaClocklessProbe",
                args=[step, args.tx_pin, args.rx_pin],
                timeout=5.0,
            )
            print(f"[lpc-uart-clockless] probe step {step}: {probe}")

        all_pass = True
        all_pass &= run_capture_self(
            s, DEFAULT_LED_COUNT, 0xFF0000, args.tx_pin, args.rx_pin, 40
        )
        all_pass &= run_capture_self(
            s, DEFAULT_LED_COUNT, 0x00FF00, args.tx_pin, args.rx_pin, 40
        )
        all_pass &= run_capture_self(
            s, DEFAULT_LED_COUNT, 0x55AA33, args.tx_pin, args.rx_pin, 40
        )

    print()
    print("=" * 60)
    if all_pass:
        print("PASS - #3611 UART DMA FastLED API loopback confirmed.")
        return 0
    print("FAIL - one or more UART DMA loopback cases mismatched.")
    return 1


if __name__ == "__main__":
    sys.exit(main())
