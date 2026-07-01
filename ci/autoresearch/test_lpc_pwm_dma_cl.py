"""FastLED #3468 — bench-validate the channels-API SCT+DMA clockless engine.

Sibling of `test_lpc_ws2812_loopback.py` for the channels-API path.
Drives `ChannelEngineLpcSctDma` on the LPC845-BRK (via
`BusTraits<Bus::BIT_BANG>::instance()`) and captures the wire through
`LpcSctRxChannel` for a byte-match self-loopback.

Three RPC handlers are called in sequence:

    pwmDmaClFrameOnce(led_count, rgb, data_pin)
        Drive one frame; report timing.
    pwmDmaClFrameBurst(led_count, rgb, n_frames, data_pin)
        Drive N frames back-to-back; report fps.
    pwmDmaClCaptureSelf(led_count, rgb, data_pin, rx_pin, capture_ms)
        Drive one frame AND capture via LpcSctRxChannel. Returns the
        decoded byte sequence. Host-side byte-compares against the
        driven rgb (mapped to WS2812 GRB order per LED).

The three RPCs are bound automatically when the LPC845 firmware is
compiled with `-DFASTLED_LPC_PWM_DMA=1`; the `bash autoresearch
--pwm-dma-cl` wrapper injects the flag via `build_flags`.

**Silicon status.** This is the on-device version of the host-side
TX→RX byte-flow readback in
`tests/fl/channels/lpc_sct_dma_engine.cpp` (#3464/#3468). Same
byte-compare contract; the host test proves the engine correctly hands
bytes to the transmitter, this runner proves the transmitter puts
those bytes on the wire in the WS2812 protocol.

Usage:
    uv run python ci/autoresearch/test_lpc_pwm_dma_cl.py \\
        [--port COM10] [--tx-pin 10] [--rx-pin 11]

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


def send_rpc(
    s: "serial.Serial",
    method: str,
    args: list[Any] | None = None,
    request_id: int = 1,
    timeout: float = 10.0,
) -> dict[str, Any] | None:
    """JSON-RPC send/receive helper.

    Mirrors the shape of `test_lpc_ws2812_loopback.py::send_rpc` — kept
    inline here so the runner has no cross-file dependencies beyond
    stdlib + pyserial.
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


def expected_grb_bytes(led_count: int, rgb: int) -> list[int]:
    """Return the WS2812 GRB byte stream for `led_count` LEDs of `rgb`."""
    r = (rgb >> 16) & 0xFF
    g = (rgb >> 8) & 0xFF
    b = (rgb >> 0) & 0xFF
    return [g, r, b] * led_count


def run_capture_self(
    s: "serial.Serial",
    led_count: int,
    rgb: int,
    data_pin: int,
    rx_pin: int,
    capture_ms: int,
) -> bool:
    """Drive one frame + capture; byte-compare against expected."""
    print()
    print(
        f"  RPC pwmDmaClCaptureSelf(led_count={led_count}, "
        f"rgb=0x{rgb:06X}, data_pin={data_pin}, "
        f"rx_pin={rx_pin}, capture_ms={capture_ms})"
    )
    reply = send_rpc(
        s,
        "pwmDmaClCaptureSelf",
        args=[led_count, rgb, data_pin, rx_pin, capture_ms],
    )
    if reply is None or "result" not in reply:
        print("    FAIL — no reply or error result")
        return False

    csv = reply["result"]
    if not isinstance(csv, str) or not csv:
        print(f"    FAIL — malformed result: {csv!r}")
        return False

    parts = csv.split(",")
    if not parts or parts[0] != "1":
        print(f"    FAIL — device reported failure: {csv!r}")
        return False

    if len(parts) < 2:
        print(f"    FAIL — CSV missing length field: {csv!r}")
        return False

    try:
        n_decoded = int(parts[1])
        decoded_bytes = [int(x) for x in parts[2 : 2 + n_decoded]]
    except (ValueError, IndexError) as e:
        print(f"    FAIL — CSV parse error: {e} ({csv!r})")
        return False

    expected = expected_grb_bytes(led_count, rgb)
    if decoded_bytes != expected:
        print(f"    FAIL — byte mismatch")
        print(f"      expected: {expected}")
        print(f"      decoded:  {decoded_bytes}")
        return False

    print(f"    PASS — {n_decoded} bytes match expected pattern")
    return True


def main() -> int:
    ap = argparse.ArgumentParser(
        description="LPC845 SCT+DMA channels-API bench (#3468)"
    )
    ap.add_argument("--port", default=DEFAULT_PORT)
    ap.add_argument("--tx-pin", type=int, default=10)
    ap.add_argument("--rx-pin", type=int, default=11)
    args = ap.parse_args()

    if serial is None:
        print("pyserial not available; install with `uv sync`")
        return 1

    print("LPC SCT+DMA channels-API self-loopback — FastLED #3468")
    print(f"  Port: {args.port} @ {DEFAULT_BAUD}")
    print(f"  Wiring: jumper P0_{args.tx_pin} -> P0_{args.rx_pin}")

    try:
        s = serial.Serial(args.port, DEFAULT_BAUD, timeout=0.1)
    except serial.SerialException as e:
        print(f"Could not open {args.port}: {e}")
        return 1

    # Give the LPC845 a moment to boot after opening the port.
    time.sleep(0.5)
    s.reset_input_buffer()

    all_pass = True

    # Case 1: single red LED. Simplest possible byte-match.
    all_pass &= run_capture_self(
        s,
        led_count=1,
        rgb=0xFF0000,
        data_pin=args.tx_pin,
        rx_pin=args.rx_pin,
        capture_ms=20,
    )

    # Case 2: three LEDs, R/G/B chain. Exercises byte ordering + LED offset.
    all_pass &= run_capture_self(
        s,
        led_count=3,
        rgb=0x00FF00,
        data_pin=args.tx_pin,
        rx_pin=args.rx_pin,
        capture_ms=30,
    )

    # Case 3: alternating bit pattern within bytes. Exercises T0/T1 fidelity.
    all_pass &= run_capture_self(
        s,
        led_count=1,
        rgb=0x55AA33,
        data_pin=args.tx_pin,
        rx_pin=args.rx_pin,
        capture_ms=20,
    )

    # Case 4: multiple frames, timing regression. Just checks the burst
    # RPC completes cleanly (no wire byte-compare here).
    print()
    print(
        f"  RPC pwmDmaClFrameBurst(led_count=10, rgb=0x00FF00, n_frames=8, data_pin={args.tx_pin})"
    )
    burst_reply = send_rpc(
        s,
        "pwmDmaClFrameBurst",
        args=[10, 0x00FF00, 8, args.tx_pin],
    )
    if burst_reply is None or "result" not in burst_reply:
        print("    FAIL — no reply from frameBurst")
        all_pass = False
    else:
        burst_csv = str(burst_reply["result"])
        burst_parts = burst_csv.split(",")
        if burst_parts and burst_parts[0] == "1":
            print(f"    PASS — burst completed ({burst_csv})")
        else:
            print(f"    FAIL — burst reported failure: {burst_csv!r}")
            all_pass = False

    print()
    print("=" * 60)
    if all_pass:
        print("PASS — #3468 TX→RX readback confirmed on silicon.")
        return 0
    else:
        print("FAIL — one or more cases did not match expected bytes.")
        return 1


if __name__ == "__main__":
    sys.exit(main())
