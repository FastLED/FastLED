"""Direct serial monitor for OBJECT_FLED runSingleTest on Teensy 4.

Bypasses the autoresearch wrapper's serial filtering so we see every
FL_WARN line the device emits during the test. Sends runSingleTest
synchronously and dumps everything to stdout for 30 seconds.

Usage:
    uv run python ci/autoresearch/debug_object_fled_capture.py --port COM20

Goal: characterize where runSingleTest stalls on Teensy 4 after the
dual-circuit FlexPWM RX fix from PR #3222 (issue #3219).
"""

from __future__ import annotations

import argparse
import json
import sys
import time

import serial


def main() -> int:
    p = argparse.ArgumentParser()
    p.add_argument("--port", default="COM20")
    p.add_argument("--baud", default=115200, type=int)
    p.add_argument(
        "--wait-s", default=30.0, type=float, help="seconds to log after sending RPC"
    )
    p.add_argument(
        "--tx-pin", default=3, type=int, help="TX pin (must match jumper wire)"
    )
    p.add_argument(
        "--rx-pin", default=4, type=int, help="RX pin (must match jumper wire)"
    )
    args = p.parse_args()

    print(f"[direct] opening {args.port} @ {args.baud}", flush=True)
    s = serial.Serial(args.port, args.baud, timeout=0.05)
    with s:
        s.dtr = True
        s.rts = True
        # Boot drain.
        print("[direct] sleeping 3s for boot drain ...", flush=True)
        time.sleep(3.0)
        s.reset_input_buffer()

        # Set pins explicitly so we don't rely on prior findConnectedPins.
        set_pins_req = (
            '{"jsonrpc":"2.0","method":"setPins","params":[['
            + str(args.tx_pin)
            + ","
            + str(args.rx_pin)
            + ']],"id":1}\n'
        )
        print(f"[direct] TX: {set_pins_req.strip()}", flush=True)
        s.write(set_pins_req.encode())
        s.flush()
        time.sleep(0.5)

        # Drain any setPins response so it doesn't pollute the runSingleTest trace.
        while True:
            chunk = s.read(256)
            if not chunk:
                break

        # Now send runSingleTest for OBJECT_FLED.
        rpc = {
            "jsonrpc": "2.0",
            "method": "runSingleTest",
            "params": [
                {
                    "driver": "OBJECT_FLED",
                    "laneSizes": [100],
                    "pattern": "MSB_LSB_A",
                    "iterations": 1,
                    "timing": "WS2812B-V5",
                }
            ],
            "id": 2,
        }
        req = json.dumps(rpc, separators=(",", ":")) + "\n"
        print(f"[direct] TX: {req.strip()}", flush=True)
        s.write(req.encode())
        s.flush()

        # Now firehose-log everything for wait_s seconds.
        deadline = time.time() + args.wait_s
        last_print = time.time()
        accum = b""
        line_count = 0
        while time.time() < deadline:
            chunk = s.read(s.in_waiting or 1)
            if chunk:
                accum += chunk
                while b"\n" in accum:
                    line, _, accum = accum.partition(b"\n")
                    text = line.decode("ascii", errors="replace").rstrip()
                    elapsed = time.time() - last_print
                    print(f"[{elapsed:6.2f}] {text}", flush=True)
                    line_count += 1
                    last_print = time.time()
            time.sleep(0.005)

        # Flush any residual bytes.
        if accum:
            text = accum.decode("ascii", errors="replace").rstrip()
            print(f"[final-partial] {text}", flush=True)

        print(
            f"\n[direct] window closed after {args.wait_s:.1f}s, captured {line_count} lines",
            flush=True,
        )
        return 0


if __name__ == "__main__":
    sys.exit(main())
