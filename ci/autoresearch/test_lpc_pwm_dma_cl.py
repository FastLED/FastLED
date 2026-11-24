"""FastLED #3468 - bench-validate the channels-API SCT+DMA clockless engine.

Sibling of `test_lpc_ws2812_loopback.py` for the channels-API path.
Drives `ChannelEngineLpcSctDma` on the LPC845-BRK (via
`BusTraits<Bus::BIT_BANG>::instance()`) and measures every bit's high time
on the jumpered RX pin with CTIMER0 + DMA (the transmitter owns the SCT,
so the SCT capture can't be used) for a byte-match self-loopback.

These RPC handlers are called in sequence:

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
    pwmDmaClDmaError(led_count, rgb, data_pin, rx_pin, hold_us)
        #5278: corrupt the second chunk's descriptor so its reload raises a
        DMA error, then report whether the frame ended (and how many bits
        reached the wire).
    pwmDmaClCaptureStress(led_count, rgb, data_pin, rx_pin, step_rgb,
                          real_timing, stress_priority, paced)
        #5278: pwmDmaClCaptureSelf with a second DMA0 channel running for
        the whole frame; compares bytes on the device and reports T0H/T1H
        overall and at the chunk reload bits.

The RPCs are bound automatically when the LPC845 firmware is
compiled with `-DFASTLED_LPC_PWM_DMA=1`; the `bash autoresearch
--pwm-dma-cl` wrapper injects the flag via `build_flags`.

Frames over two chunks need the DMA ISR hub (`FASTLED_LPC_DMA_ISR`, on by
default): with nothing calling poll(), only the DMA interrupt can hand over
chunks. A `--define FASTLED_LPC_DMA_ISR=0` build refuses such frames
(#5278). The bench reads the build's hub setting from the device first
(`hub=` in the pwmDmaClDmaError reply, an 8-LED frame that fits either
way), then requires byte-exact frames where they fit and, in a hub-off
build only, `0,hub_off_refused` from pwmDmaClCaptureSelf and
pwmDmaClCaptureStress for longer ones.

**Silicon status.** This is the on-device version of the host-side
TX->RX byte-flow readback in
`tests/unit/fl/channels/lpc_sct_dma_engine.cpp` (#3464/#3468). Same
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
    hub_on: bool = True,
) -> bool:
    """Drive one frame + capture; byte-compare against expected.

    A hub-off build must refuse a frame over two chunks; any other frame
    must arrive byte-exact.
    """
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
    if expects_refusal(led_count, hub_on):
        if csv == "0,hub_off_refused":
            print("    PASS - refused: over two chunks without the DMA ISR hub")
            return True
        print(f"    FAIL - a hub-off build must refuse this frame: {csv!r}")
        return False
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


def parse_fields(csv: str) -> dict[str, str]:
    """Parse the `key=value` fields of a device CSV reply."""
    return dict(f.split("=", 1) for f in csv.split(",") if "=" in f)


# Without the DMA ISR hub the engine sends at most two 96-bit chunks.
HUB_OFF_MAX_BITS = 2 * 96


def expects_refusal(led_count: int, hub_on: bool) -> bool:
    """Whether the build must refuse this frame (#5278)."""
    return not hub_on and led_count * 24 > HUB_OFF_MAX_BITS


def probe_hub(s: "RpcBench", data_pin: int, rx_pin: int) -> bool | None:
    """The deployed build's FASTLED_LPC_DMA_ISR, from the device itself.

    An 8-LED forced-error frame (two chunks) runs in either build and its
    reply carries `hub=`. None if the reply is unusable.
    """
    reply = send_rpc(s, "pwmDmaClDmaError", args=[8, 0xA5C30F, data_pin, rx_pin, 0])
    csv = str(reply["result"]) if reply and "result" in reply else ""
    hub = parse_fields(csv).get("hub") if csv.startswith("1,") else None
    if hub not in ("0", "1"):
        print(f"  FAIL - cannot read the hub setting: {csv!r}")
        return None
    print(f"  DMA ISR hub: {'on' if hub == '1' else 'off'} (FASTLED_LPC_DMA_ISR={hub})")
    return hub == "1"


# Stale bits allowed past the first chunk after a forced DMA error: the
# error is taken one bit after the failed reload, and the ISR then needs a
# few bit times to halt the SCT (5 on silicon).
DMA_ERROR_SLACK_BITS = 8


def run_dma_error(s: "RpcBench", data_pin: int, rx_pin: int, hold_us: int) -> bool:
    """#5278: force a DMA error at the first reload; the frame must end."""
    print()
    print(
        f"  RPC pwmDmaClDmaError(led_count=8, data_pin={data_pin}, "
        f"rx_pin={rx_pin}, hold_us={hold_us})"
    )
    reply = send_rpc(
        s, "pwmDmaClDmaError", args=[8, 0xA5C30F, data_pin, rx_pin, hold_us]
    )
    csv = str(reply["result"]) if reply and "result" in reply else ""
    if not csv.startswith("1,"):
        print(f"    FAIL - no reply or device failure: {csv!r}")
        return False
    f = parse_fields(csv)
    try:
        bits, chunk = int(f["bits"]), int(f["chunk_bits"])
        ended = f["ready"] == "1" and f["complete"] == "0" and f["line"] == "0"
        hub = f["hub"]
    except (KeyError, ValueError):
        print(f"    FAIL - malformed reply: {csv!r}")
        return False
    if hub == "1":
        ok = ended and chunk <= bits <= chunk + DMA_ERROR_SLACK_BITS
    elif hold_us == 0:
        # Without the hub only poll() sees the error, here in a busy wait;
        # 114-119 bits on silicon, well inside the next chunk.
        ok = ended and chunk <= bits <= 2 * chunk
    else:
        # Without the hub nothing sees the error during the hold, so stale
        # bits run on through it: a documented limit, not bounded above.
        ok = ended and bits >= chunk
    print(f"    {'PASS' if ok else 'FAIL'} - {csv}")
    return ok


# Contention cases: (label, stress_priority, paced, must_pass). paced 0 = unpaced
# (always requesting), n >= 1 = one 2^(n-1)-word burst per bit on the
# transmitter's own DMA trigger event. `must_pass` cases are asserted; the
# others are measured and printed, as documented limits.
CONTENTION_CASES = [
    ("no stressor", -1, 0, True),
    ("paced 1 word, priority 0", 0, 1, True),
    ("paced 4 words, priority 0", 0, 3, False),
    ("paced 4 words, priority 1", 1, 3, True),
    ("unpaced, priority 0", 0, 0, False),
    ("unpaced, priority 1", 1, 0, True),
    ("unpaced, priority 7", 7, 0, True),
]


def run_contention(s: "RpcBench", data_pin: int, rx_pin: int, hub_on: bool) -> bool:
    """#5278: T0H/T1H, overall and at reload bits, under DMA0 contention."""
    all_ok = True
    for real_timing in (1, 0):
        name = "WS2812 1250 ns" if real_timing else "capture 2250 ns"
        for label, prio, paced, must_pass in CONTENTION_CASES:
            reply = send_rpc(
                s,
                "pwmDmaClCaptureStress",
                args=[
                    17,
                    0xA5C30C,
                    data_pin,
                    rx_pin,
                    0x1F3B57,
                    real_timing,
                    prio,
                    paced,
                ],
            )
            csv = str(reply["result"]) if reply and "result" in reply else ""
            if expects_refusal(17, hub_on):
                refused = csv == "0,hub_off_refused"
                print(
                    f"  [{name}] {label}: {'PASS - refused' if refused else 'FAIL'}"
                    f" (no DMA ISR hub) {csv if not refused else ''}"
                )
                all_ok &= refused
                continue
            f = parse_fields(csv)
            ok = csv.startswith("1,") and f.get("bad") == "0" and f.get("bits") == "408"
            # A documented limit is a frame that went out wrong or stalled;
            # no reply, a rejected call or a crash is a failure either way.
            measured = csv.startswith(("1,", "0,tx_stalled", "0,tx_short"))
            verdict = (
                "PASS" if ok else ("LIMIT" if not must_pass and measured else "FAIL")
            )
            print(f"  [{name}] {label}: {verdict} - {csv}")
            if verdict == "FAIL":
                all_ok = False
    return all_ok


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

    hub_on = probe_hub(s, args.tx_pin, args.rx_pin)
    if hub_on is None:
        s.close()
        return 1

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
            hub_on=hub_on,
        )

    # Case 1: single red LED. Simplest possible byte-match.
    all_pass &= run_capture_self(
        s,
        led_count=1,
        rgb=0xFF0000,
        data_pin=args.tx_pin,
        rx_pin=args.rx_pin,
        hold_us=0,
        hub_on=hub_on,
    )

    # Case 2: three LEDs, R/G/B chain. Exercises byte ordering + LED offset.
    all_pass &= run_capture_self(
        s,
        led_count=3,
        rgb=0x00FF00,
        data_pin=args.tx_pin,
        rx_pin=args.rx_pin,
        hold_us=0,
        hub_on=hub_on,
    )

    # Case 3: alternating bit pattern within bytes. Exercises T0/T1 fidelity.
    all_pass &= run_capture_self(
        s,
        led_count=1,
        rgb=0x55AA33,
        data_pin=args.tx_pin,
        rx_pin=args.rx_pin,
        hold_us=0,
        hub_on=hub_on,
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
        hub_on=hub_on,
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
        hub_on=hub_on,
    )

    # Case 6: 17 LEDs = 408 bits = 5 chunks, so both descriptor slots are
    # reused (chunks 3 and 4), with no poll() for 2 ms after show(). Every
    # LED differs, so a stale chunk replayed in place of a later one shows
    # up as a byte mismatch even with no gap on the wire.
    all_pass &= run_capture_self(
        s,
        led_count=17,
        rgb=0x3CA5F0,
        data_pin=args.tx_pin,
        rx_pin=args.rx_pin,
        hold_us=2000,
        step=0x1F3B57,
        hub_on=hub_on,
    )

    # Case 7 (#5278): a forced DMA error at the first chunk reload ends the
    # frame within a few bits, with and without poll() during the frame.
    for hold_us in (0, 2000):
        all_pass &= run_dma_error(s, args.tx_pin, args.rx_pin, hold_us)
    # The next ordinary frame is byte-exact again.
    all_pass &= run_capture_self(
        s,
        led_count=8,
        rgb=0xA5C30F,
        data_pin=args.tx_pin,
        rx_pin=args.rx_pin,
        hold_us=0,
        step=0x1F3B57,
        hub_on=hub_on,
    )

    # Case 8 (#5278): a second DMA0 channel running during the frame.
    print()
    print("  RPC pwmDmaClCaptureStress (17 LEDs, 5 chunks)")
    all_pass &= run_contention(s, args.tx_pin, args.rx_pin, hub_on)

    # Case 9: multiple frames, timing regression. 8 LEDs (two chunks), so a
    # FASTLED_LPC_DMA_ISR=0 build runs it too. Just checks the burst
    # RPC completes cleanly (no wire byte-compare here).
    print()
    print(
        f"  RPC pwmDmaClFrameBurst(led_count=8, rgb=0x00FF00, n_frames=8, data_pin={args.tx_pin})"
    )
    burst_reply = send_rpc(
        s,
        "pwmDmaClFrameBurst",
        args=[8, 0x00FF00, 8, args.tx_pin],
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
