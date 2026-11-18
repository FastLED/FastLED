"""FastLED #3468 - bench-validate the channels-API SCT+DMA clockless engine.

Sibling of `test_lpc_ws2812_loopback.py` for the channels-API path.
Drives `ChannelEngineLpcSctDma` on the LPC845-BRK (via
`BusTraits<Bus::BIT_BANG>::instance()`) and measures every bit's high time
on the jumpered RX pin with CTIMER0 + DMA (the transmitter owns the SCT,
so the SCT capture can't be used) for a byte-match self-loopback.

Three RPC handlers are called in sequence:

    pwmDmaClFrameOnce(led_count, rgb, data_pin)
        Drive one frame; report timing.
    pwmDmaClFrameBurst(led_count, rgb, n_frames, data_pin)
        Drive N frames back-to-back; report fps.
    pwmDmaClCaptureSelf(led_count, rgb, data_pin, rx_pin, hold_us, step_rgb)
        Drive one frame AND capture each bit's high time. LED i is
        `(rgb + i * step_rgb) & 0xFFFFFF`, so a nonzero step gives every
        transmitter chunk different bytes. `hold_us` > 0
        skips poll() for that long after show(), like FastLED.show(). A
        rise more than `gap_guard` ns after the previous one (the line
        idling mid-frame) stops the device-side capture, so such a frame
        decodes short. Returns the decoded bytes plus the measured T0H/T1H
        ranges. Host-side byte-compares against a GRB stream it builds
        itself from rgb and step_rgb.

The three RPCs are bound automatically when the LPC845 firmware is
compiled with `-DFASTLED_LPC_PWM_DMA=1`; the `bash autoresearch
--pwm-dma-cl` wrapper injects the flag via `build_flags`.

The `hold_us=2000` cases also need the DMA ISR hub (`FASTLED_LPC_DMA_ISR`,
on by default): with nothing calling poll() during the hold, only the DMA
interrupt can hand over chunks. A build with `-DFASTLED_LPC_DMA_ISR=0`
halts those frames after two chunks by design, and the device reports
`tx_short`; that is not a driver regression.

**Silicon status.** This is the on-device version of the host-side
TX->RX byte-flow readback in
`tests/fl/channels/lpc_sct_dma_engine.cpp` (#3464/#3468). Same
byte-compare contract; the host test proves the engine correctly hands
bytes to the transmitter, this runner proves the transmitter puts
those bytes on the wire in the WS2812 protocol.

Usage:
    uv run python -m ci.autoresearch.test_lpc_pwm_dma_cl \\
        [--port COM10] [--tx-pin 10] [--rx-pin 11]

Exits 0 on success, 1 on any failed assertion / RPC error.
"""

from __future__ import annotations

import argparse
import json
import sys
import time
from pathlib import Path
from typing import Any

from ci.autoresearch.lpc_pin_scan import resolve_pins
from ci.autoresearch.rpc_bench import METHOD_NOT_FOUND, RpcBench  # noqa: E402,F401
from ci.util.global_interrupt_handler import handle_keyboard_interrupt  # noqa: E402


def send_rpc(
    s,
    method,
    args=None,
    timeout: float = 10.0,
):
    """Shim over RpcBench (`s`) so run_* keep their reply["result"] shape.

    Device serial goes through fbuild's Rust monitor (never raw pyserial);
    see agents/docs/hardware-autoresearch.md -> "Device serial".
    """
    result = s.call_flat(method, args=args, timeout=timeout)
    if result is None:
        return None
    if result is METHOD_NOT_FOUND:
        return {"error": {"code": -32601, "message": "method not found"}}
    return {"result": result}


DEFAULT_PORT = "COM10"
DEFAULT_BAUD = 115200


def expected_grb_bytes(led_count: int, rgb: int, step: int = 0) -> list[int]:
    """Return the WS2812 GRB byte stream; LED i is (rgb + i * step) & 0xFFFFFF."""
    out: list[int] = []
    for i in range(led_count):
        c = (rgb + i * step) & 0xFFFFFF
        out += [(c >> 8) & 0xFF, (c >> 16) & 0xFF, c & 0xFF]
    return out


def run_capture_self(
    s: "RpcBench",
    led_count: int,
    rgb: int,
    data_pin: int,
    rx_pin: int,
    hold_us: int,
    step: int = 0,
) -> bool:
    """Drive one frame + capture; byte-compare against expected."""
    print()
    print(
        f"  RPC pwmDmaClCaptureSelf(led_count={led_count}, "
        f"rgb=0x{rgb:06X}, data_pin={data_pin}, "
        f"rx_pin={rx_pin}, hold_us={hold_us}, step_rgb=0x{step:06X})"
    )
    reply = send_rpc(
        s,
        "pwmDmaClCaptureSelf",
        args=[led_count, rgb, data_pin, rx_pin, hold_us, step],
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
        print(f"    FAIL - device reported failure: {csv!r}")
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

    timing = " ".join(parts[2 + n_decoded :])
    if "gap_guard=" not in timing:
        print(f"    FAIL - reply lacks the mid-frame gap guard: {csv!r}")
        return False
    expected = expected_grb_bytes(led_count, rgb, step)
    if decoded_bytes != expected:
        print("    FAIL - byte mismatch")
        print(f"      expected: {expected}")
        print(f"      decoded:  {decoded_bytes}")
        print(f"      measured: {timing}")
        if len(decoded_bytes) < len(expected):
            print(
                f"      (short: captured {len(decoded_bytes)} of "
                f"{len(expected)} bytes; possible causes: a mid-frame idle "
                "past gap_guard, or no signal on the RX pin)"
            )
        return False

    print(f"    PASS - {n_decoded} bytes match expected pattern ({timing} ns)")
    return True


def main() -> int:
    ap = argparse.ArgumentParser(
        description="LPC845 SCT+DMA channels-API bench (#3468)"
    )
    ap.add_argument("--port", default=DEFAULT_PORT)
    # Default: the findConnectedPins jumper scan picks the pair.
    ap.add_argument("--tx-pin", type=int, default=None)
    ap.add_argument("--rx-pin", type=int, default=None)
    args = ap.parse_args()

    print("LPC SCT+DMA channels-API self-loopback - FastLED #3468")
    print(f"  Port: {args.port} @ {DEFAULT_BAUD}")

    try:
        s = RpcBench(args.port)
    except KeyboardInterrupt as ki:
        handle_keyboard_interrupt(ki)
        raise
    except Exception as e:  # noqa: BLE001
        print(f"Could not connect to {args.port}: {e}")
        return 1

    pins = resolve_pins(s, args.tx_pin, args.rx_pin, "lpc-pwm-dma-cl")
    if pins is None:
        s.close()
        return 3
    args.tx_pin, args.rx_pin = pins
    print(f"  Wiring: jumper P0_{args.tx_pin} -> P0_{args.rx_pin}")

    all_pass = True

    # #5276 regressions: frames ending in bits '1,0' (B = 0x02, 0xFE).
    for b_byte in (0x02, 0xFE):
        all_pass &= run_capture_self(
            s,
            led_count=1,
            rgb=b_byte,
            data_pin=args.tx_pin,
            rx_pin=args.rx_pin,
            hold_us=0,
        )

    # Case 1: single red LED. Simplest possible byte-match.
    all_pass &= run_capture_self(
        s,
        led_count=1,
        rgb=0xFF0000,
        data_pin=args.tx_pin,
        rx_pin=args.rx_pin,
        hold_us=0,
    )

    # Case 2: three LEDs, R/G/B chain. Exercises byte ordering + LED offset.
    all_pass &= run_capture_self(
        s,
        led_count=3,
        rgb=0x00FF00,
        data_pin=args.tx_pin,
        rx_pin=args.rx_pin,
        hold_us=0,
    )

    # Case 3: alternating bit pattern within bytes. Exercises T0/T1 fidelity.
    all_pass &= run_capture_self(
        s,
        led_count=1,
        rgb=0x55AA33,
        data_pin=args.tx_pin,
        rx_pin=args.rx_pin,
        hold_us=0,
    )

    # Case 4: ten LEDs = 240 bits, three transmitter chunks (96 bits each),
    # so the chunk hand-over is on the wire.
    all_pass &= run_capture_self(
        s,
        led_count=10,
        rgb=0xA5C30F,
        data_pin=args.tx_pin,
        rx_pin=args.rx_pin,
        hold_us=0,
    )

    # Case 5: same frame, but nothing calls poll() for 2 ms after show(),
    # as with FastLED.show(): chunk hand-over must come from the DMA ISR.
    # LED-indexed colors, so each chunk carries different bytes.
    all_pass &= run_capture_self(
        s,
        led_count=10,
        rgb=0xA5C30F,
        data_pin=args.tx_pin,
        rx_pin=args.rx_pin,
        hold_us=2000,
        step=0x1F3B57,
    )

    # Case 6: 20 LEDs = 480 bits = 5 chunks, so both descriptor slots are
    # reused (chunks 3 and 4), with no poll() for 2 ms after show(). Every
    # LED differs, so a stale chunk replayed in place of a later one shows
    # up as a byte mismatch even with no gap on the wire.
    all_pass &= run_capture_self(
        s,
        led_count=20,
        rgb=0x3CA5F0,
        data_pin=args.tx_pin,
        rx_pin=args.rx_pin,
        hold_us=2000,
        step=0x1F3B57,
    )

    # Case 7: multiple frames, timing regression. Just checks the burst
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
        print("    FAIL - no reply from frameBurst")
        all_pass = False
    else:
        burst_csv = str(burst_reply["result"])
        burst_parts = burst_csv.split(",")
        if burst_parts and burst_parts[0] == "1":
            print(f"    PASS - burst completed ({burst_csv})")
        else:
            print(f"    FAIL - burst reported failure: {burst_csv!r}")
            all_pass = False

    print()
    print("=" * 60)
    if all_pass:
        print("PASS - #3468 TX->RX readback confirmed on silicon.")
        return 0
    else:
        print("FAIL - one or more cases did not match expected bytes.")
        return 1


if __name__ == "__main__":
    sys.exit(main())
