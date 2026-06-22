"""Same as debug_object_fled_capture.py but with configurable lane size.

Used to isolate buffer-position-dependent decode failures in #3219:
- If laneSizes=[5] passes byte-level, the bug accumulates with frame
  length (DMA buffer overflow, sustained TX/RX skew, etc.).
- If laneSizes=[5] still fails, the bug fires every frame regardless
  of position.
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
    p.add_argument("--lane-size", default=5, type=int)
    p.add_argument("--wait-s", default=15.0, type=float)
    p.add_argument("--tx-pin", default=3, type=int)
    p.add_argument("--rx-pin", default=4, type=int)
    args = p.parse_args()

    print(
        f"[direct-small] {args.port} @ {args.baud}, laneSize={args.lane_size}",
        flush=True,
    )
    s = serial.Serial(args.port, args.baud, timeout=0.05)
    with s:
        s.dtr = True
        s.rts = True
        time.sleep(3.0)
        s.reset_input_buffer()

        set_pins = (
            '{"jsonrpc":"2.0","method":"setPins","params":[['
            + str(args.tx_pin)
            + ","
            + str(args.rx_pin)
            + ']],"id":1}\n'
        )
        s.write(set_pins.encode())
        s.flush()
        time.sleep(0.5)
        while s.read(256):
            pass

        rpc = {
            "jsonrpc": "2.0",
            "method": "runSingleTest",
            "params": [
                {
                    "driver": "OBJECT_FLED",
                    "laneSizes": [args.lane_size],
                    "pattern": "MSB_LSB_A",
                    "iterations": 1,
                    "timing": "WS2812B-V5",
                }
            ],
            "id": 2,
        }
        req = json.dumps(rpc, separators=(",", ":")) + "\n"
        print(f"[direct-small] TX: {req.strip()}", flush=True)
        s.write(req.encode())
        s.flush()

        deadline = time.time() + args.wait_s
        accum = b""
        while time.time() < deadline:
            chunk = s.read(s.in_waiting or 1)
            if chunk:
                accum += chunk
                while b"\n" in accum:
                    line, _, accum = accum.partition(b"\n")
                    print(line.decode("ascii", errors="replace").rstrip(), flush=True)
            time.sleep(0.005)

        if accum:
            print(accum.decode("ascii", errors="replace").rstrip(), flush=True)
        return 0


if __name__ == "__main__":
    sys.exit(main())
