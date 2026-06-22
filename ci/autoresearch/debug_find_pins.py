"""Direct test of findConnectedPins RPC -- isolate wrapper vs firmware.

If this script gets a response in <2s, the firmware path is healthy
and the wrapper hang is wrapper-side. If it hangs too, the firmware
itself is the wedge.
"""

from __future__ import annotations

import json
import sys
import time

import serial


def main() -> int:
    port = sys.argv[1] if len(sys.argv) > 1 else "COM20"
    print(f"[direct] opening {port}", flush=True)
    s = serial.Serial(port, 115200, timeout=0.05)
    with s:
        s.dtr = True
        s.rts = True
        time.sleep(3.0)
        s.reset_input_buffer()

        # ping
        ping = '{"jsonrpc":"2.0","method":"ping","params":[{}],"id":1}\n'
        print(f"[direct] TX: {ping.strip()}", flush=True)
        s.write(ping.encode())
        s.flush()
        time.sleep(0.5)
        while s.read(256):
            pass

        # findConnectedPins
        find = (
            '{"jsonrpc":"2.0","method":"findConnectedPins",'
            '"params":[[{"startPin":0,"endPin":8,"autoApply":true}]],"id":2}\n'
        )
        t0 = time.time()
        print(f"\n[direct] TX: {find.strip()}", flush=True)
        s.write(find.encode())
        s.flush()

        deadline = time.time() + 10.0
        accum = b""
        seen_response = False
        while time.time() < deadline:
            chunk = s.read(s.in_waiting or 1)
            if chunk:
                accum += chunk
                while b"\n" in accum:
                    line, _, accum = accum.partition(b"\n")
                    text = line.decode("ascii", errors="replace").rstrip()
                    elapsed = time.time() - t0
                    print(f"[{elapsed:6.3f}s] {text}", flush=True)
                    if (
                        '"id":2' in text
                        and text.startswith(("REMOTE:", "{"))
                        and ("result" in text or "error" in text)
                    ):
                        seen_response = True
                        print(
                            f"\n[direct] === RESPONSE in {elapsed:.3f}s ===",
                            flush=True,
                        )
                        deadline = time.time() + 0.3  # drain briefly
            time.sleep(0.005)

        if not seen_response:
            print(
                f"\n[direct] NO RESPONSE in 10s -- firmware-side hang",
                flush=True,
            )
            return 1
        return 0


if __name__ == "__main__":
    sys.exit(main())
