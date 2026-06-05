"""Phase 2 of FastLED#2764 — bench-validate the FlexIO RX backend.

Drives a square wave on the TX pin via the Teensy core's
`analogWriteFrequency` PWM, captures inter-edge timings via the new
FlexIO1-based `RxBackend::FLEXIO`, and asserts that mean/σ of the
recovered period falls within tolerance.

Three rates, mirroring the parent issue's Phase 2 table:

    | Frequency | Period      | Tolerance (σ)  | Tolerance (mean) |
    |-----------|-------------|----------------|------------------|
    | 1   kHz   | 1 000 000ns | <  1 000 ns    | ±1 %             |
    | 10  kHz   |   100 000ns | <    500 ns    | ±1 %             |
    | 100 kHz   |    10 000ns | <    100 ns    | ±1 %             |

Usage:
    uv run python ci/autoresearch/test_flexio_rx_squarewave.py [--port COM20]

Talks to the AutoResearch firmware over the raw serial JSON-RPC protocol
(the same path the in-tree `RpcClient` uses, but without the fbuild
WebSocket daemon — keeps this script self-contained).

Exits 0 on success, 1 on any failed assertion / RPC error.
"""

from __future__ import annotations

import argparse
import json
import sys
import time
from dataclasses import dataclass
from typing import Any


# `pyserial` is checked at `main()` entry, not at import, so test discovery
# (e.g. pytest collection on a CI runner without the dep installed) does not
# abort before the caller can decide to skip the hardware bench.
try:
    import serial  # type: ignore
except ImportError:  # pragma: no cover — handled at main() entry
    serial = None  # type: ignore


DEFAULT_PORT = "COM20"
DEFAULT_BAUD = 115200


@dataclass
class BenchCase:
    label: str
    frequency_hz: int
    duration_ms: int
    expected_period_ns: int
    sigma_max_ns: int
    mean_tolerance_pct: float


CASES: list[BenchCase] = [
    BenchCase("2.1 / 1 kHz", 1_000, 200, 1_000_000, 1_000, 1.0),
    BenchCase("2.2 / 10 kHz", 10_000, 100, 100_000, 500, 1.0),
    BenchCase("2.3 / 100 kHz", 100_000, 100, 10_000, 100, 1.0),
]


def send_rpc(
    s: "serial.Serial",
    method: str,
    args: dict[str, Any] | None = None,
    request_id: int = 1,
    timeout: float = 10.0,
) -> dict[str, Any] | None:
    params = [args] if args is not None else [{}]
    req = {"method": method, "params": params, "id": request_id}
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


def evaluate(case: BenchCase, result: dict[str, Any]) -> tuple[bool, str]:
    edges = result.get("edges_captured", 0)
    periods = result.get("periods", 0)
    mean_ns = result.get("period_mean_ns", 0)
    sigma_ns = result.get("period_sigma_ns", 0)
    min_ns = result.get("period_min_ns", 0)
    max_ns = result.get("period_max_ns", 0)
    if periods < 5:
        return False, (
            f"only {periods} periods captured in {case.duration_ms} ms "
            f"(edges={edges}) — capture path likely not working yet"
        )
    mean_low = case.expected_period_ns * (1.0 - case.mean_tolerance_pct / 100.0)
    mean_high = case.expected_period_ns * (1.0 + case.mean_tolerance_pct / 100.0)
    if not (mean_low <= mean_ns <= mean_high):
        return False, (
            f"mean {mean_ns} ns outside ±{case.mean_tolerance_pct}% of "
            f"{case.expected_period_ns} ns (range {mean_low:.0f}..{mean_high:.0f})"
        )
    if sigma_ns > case.sigma_max_ns:
        return False, f"sigma {sigma_ns} ns exceeds {case.sigma_max_ns} ns"
    return True, (
        f"periods={periods}, mean={mean_ns} ns, sigma={sigma_ns} ns, "
        f"min={min_ns} ns, max={max_ns} ns"
    )


def main() -> int:
    p = argparse.ArgumentParser()
    p.add_argument("--port", default=DEFAULT_PORT)
    p.add_argument("--baud", default=DEFAULT_BAUD, type=int)
    p.add_argument(
        "--tx-pin",
        default=3,
        type=int,
        help="TX pin driven by analogWriteFrequency (default: 3)",
    )
    p.add_argument(
        "--rx-pin",
        default=4,
        type=int,
        help="RX pin captured by FLEXIO1 (default: 4; "
        "must be in kFlexIo1Pins[] in the firmware)",
    )
    args = p.parse_args()

    if serial is None:
        print(
            "ERROR: pyserial not installed. Run: uv pip install pyserial",
            file=sys.stderr,
        )
        return 2

    print(f"[flexio-rx-bench] opening {args.port} @ {args.baud}")
    try:
        s = serial.Serial(args.port, args.baud, timeout=0.1)
    except serial.SerialException as e:
        print(f"ERROR: could not open {args.port}: {e}", file=sys.stderr)
        return 2
    with s:
        s.dtr = True
        s.rts = True
        time.sleep(1.0)
        s.reset_input_buffer()

        ping = send_rpc(s, "ping", request_id=1, timeout=5.0)
        if not ping or "result" not in ping:
            print(
                "ERROR: ping failed; is AutoResearch flashed and running?",
                file=sys.stderr,
            )
            return 2
        print(f"[flexio-rx-bench] ping ok: uptime={ping['result'].get('uptimeMs')} ms")

        passes = 0
        fails = 0
        for i, case in enumerate(CASES, start=10):
            print(
                f"\n--- {case.label} ({case.frequency_hz} Hz / {case.duration_ms} ms) ---"
            )
            resp = send_rpc(
                s,
                "flexioRxBenchmark",
                args={
                    "frequency_hz": case.frequency_hz,
                    "duration_ms": case.duration_ms,
                    "tx_pin": args.tx_pin,
                    "rx_pin": args.rx_pin,
                },
                request_id=i,
                timeout=case.duration_ms / 1000.0 + 5.0,
            )
            if not resp or "result" not in resp:
                print(f"  FAIL: no response or RPC error: {resp}")
                fails += 1
                continue
            result = resp["result"]
            if not result.get("success", False):
                print(f"  FAIL: firmware reported failure: {result}")
                fails += 1
                continue
            ok, msg = evaluate(case, result)
            if ok:
                print(f"  PASS: {msg}")
                passes += 1
            else:
                print(f"  FAIL: {msg}")
                print(f"        raw result: {result}")
                fails += 1

        print(
            f"\n[flexio-rx-bench] summary: {passes} pass / {fails} fail "
            f"out of {len(CASES)} cases"
        )
        return 0 if fails == 0 else 1


if __name__ == "__main__":
    sys.exit(main())
