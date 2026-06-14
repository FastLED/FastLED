"""Phase 1 of FastLED #3021 — bench-validate the LPC845 SCT-RX backend.

Drives a square wave on the TX pin via a bit-bang loop in the LPC845
bring-up sketch (`examples/AutoResearchLpc/AutoResearchLpc.ino`),
captures inter-edge timings via the new SCT-input-capture
`LpcSctRxChannel`, and asserts that mean / σ of the recovered period
fall within tolerance.

Three rates, mirroring FastLED #3021's Phase 1 acceptance table:

    | Frequency | Period      | Tolerance (σ)  | Tolerance (mean) |
    |-----------|-------------|----------------|------------------|
    | 1   kHz   | 1 000 000ns | <  1 000 ns    | ±2 %             |
    | 10  kHz   |   100 000ns | <    500 ns    | ±2 %             |
    | 100 kHz   |    10 000ns | <    500 ns    | ±2 %             |

(Tolerances are slightly looser than the Teensy 4 FlexIO bench because
the M0+ polling loop adds 1–2 µs of capture jitter per edge — see
`src/platforms/arm/lpc/rx_sct_capture.cpp.hpp::pollOnce` for the
timing budget.)

Usage:
    uv run python ci/autoresearch/test_lpc_pin_toggle_rx.py [--port COM10]

Talks to the AutoResearchLpc firmware over the same raw-serial
JSON-RPC protocol as the bring-up echo test (`phases.py
::_run_bring_up_tests`), but the LPC `pinToggleRx` RPC takes 4
positional integer args and returns a CSV-encoded string (smaller
flash footprint than the JSON-object response shape FlexIO uses).

Exits 0 on success, 1 on any failed assertion / RPC error.
"""

from __future__ import annotations

import argparse
import json
import sys
import time
from dataclasses import dataclass
from typing import Any


try:
    import serial  # type: ignore
except ImportError:
    serial = None  # type: ignore


DEFAULT_PORT = "COM10"
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
    BenchCase("1.1 / 1 kHz", 1_000, 200, 1_000_000, 1_000, 2.0),
    BenchCase("1.2 / 10 kHz", 10_000, 100, 100_000, 500, 2.0),
    BenchCase("1.3 / 100 kHz", 100_000, 100, 10_000, 500, 2.0),
]


def send_rpc(
    s: "serial.Serial",
    method: str,
    args: list[Any] | None = None,
    request_id: int = 1,
    timeout: float = 10.0,
) -> dict[str, Any] | None:
    """Send a JSON-RPC request and return the first matching response.

    `args` is a *list of positional args* (matching the AutoResearchLpc
    sketch's typed-binding signature like `pinToggleRx(int, int, int, int)`),
    NOT a dict — the LPC bring-up sketch doesn't use the FlexIO-style
    `[{"key": value, ...}]` wrapper because the typed binding gives a
    smaller flash footprint on the LPC845's 64 KB budget.
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


def parse_csv_result(csv: str) -> dict[str, int]:
    """Parse the AutoResearchLpc pinToggleRx CSV reply.

    Format (defined in examples/AutoResearchLpc/AutoResearchLpc.ino):
        success,edges,periods,mean_ns,sigma_ns,min_ns,max_ns
    """
    fields = ("success", "edges", "periods", "mean_ns", "sigma_ns", "min_ns", "max_ns")
    parts = csv.split(",")
    if len(parts) != len(fields):
        return {}
    try:
        return {k: int(v) for k, v in zip(fields, parts)}
    except ValueError:
        return {}


def evaluate(case: BenchCase, parsed: dict[str, int]) -> tuple[bool, str]:
    edges = parsed.get("edges", 0)
    periods = parsed.get("periods", 0)
    mean_ns = parsed.get("mean_ns", 0)
    sigma_ns = parsed.get("sigma_ns", 0)
    min_ns = parsed.get("min_ns", 0)
    max_ns = parsed.get("max_ns", 0)
    success = parsed.get("success", 0)
    if not success:
        return False, (
            f"firmware reported success=0 (edges={edges}, periods={periods}) — "
            "SCT capture likely returned no edges; verify TX↔RX jumper"
        )
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
        f"edges={edges}, periods={periods}, mean={mean_ns} ns, "
        f"sigma={sigma_ns} ns, min={min_ns} ns, max={max_ns} ns"
    )


def main() -> int:
    p = argparse.ArgumentParser()
    p.add_argument("--port", default=DEFAULT_PORT)
    p.add_argument("--baud", default=DEFAULT_BAUD, type=int)
    p.add_argument(
        "--tx-pin",
        default=10,
        type=int,
        help="LPC845 GPIO PIO0_n driven by bit-bang TX (default: P0_10)",
    )
    p.add_argument(
        "--rx-pin",
        default=11,
        type=int,
        help="LPC845 GPIO PIO0_n routed to SCT input 0 for capture "
        "(default: P0_11; any PIO0_n works via SWM)",
    )
    args = p.parse_args()

    if serial is None:
        print(
            "ERROR: pyserial not installed. Run: uv pip install pyserial",
            file=sys.stderr,
        )
        return 2

    print(f"[lpc-pin-toggle-rx] opening {args.port} @ {args.baud}")
    try:
        s = serial.Serial(args.port, args.baud, timeout=0.1)
    except serial.SerialException as e:
        print(f"ERROR: could not open {args.port}: {e}", file=sys.stderr)
        return 2

    with s:
        # The LPC845-BRK uses a CMSIS-DAP USB-CDC bridge; DTR/RTS toggles
        # reset the target. Hold them de-asserted and let the boot banner
        # drain before sending any RPC. This matches the bring-up echo
        # test's open sequence (phases.py::_run_bring_up_tests).
        s.dtr = False
        s.rts = False
        time.sleep(0.5)
        s.reset_input_buffer()
        time.sleep(2.0)
        s.reset_input_buffer()

        # Health check via the bring-up sketch's echo RPC. Confirms the
        # serial pipe + JSON-RPC parser are alive before we trust the
        # pinToggleRx result.
        echo = send_rpc(s, "echo", args=[42], request_id=1, timeout=5.0)
        if not echo or echo.get("result") != 42:
            print(
                "ERROR: echo failed; is AutoResearchLpc flashed and "
                f"running? Got: {echo}",
                file=sys.stderr,
            )
            return 2
        print("[lpc-pin-toggle-rx] echo ok")

        passes = 0
        fails = 0
        for i, case in enumerate(CASES, start=10):
            print(
                f"\n--- {case.label} "
                f"({case.frequency_hz} Hz / {case.duration_ms} ms) ---"
            )
            resp = send_rpc(
                s,
                "pinToggleRx",
                args=[args.tx_pin, args.rx_pin, case.frequency_hz, case.duration_ms],
                request_id=i,
                # SCT capture runs synchronously inside the sketch loop;
                # allow 2× the bit-bang window plus 5 s headroom.
                timeout=(case.duration_ms / 1000.0) * 2.0 + 5.0,
            )
            if not resp or "result" not in resp:
                print(f"  FAIL: no response or RPC error: {resp}")
                fails += 1
                continue
            csv = resp["result"]
            if not isinstance(csv, str):
                print(f"  FAIL: expected string CSV, got {type(csv).__name__}: {csv}")
                fails += 1
                continue
            parsed = parse_csv_result(csv)
            if not parsed:
                print(f"  FAIL: malformed CSV: {csv!r}")
                fails += 1
                continue
            ok, msg = evaluate(case, parsed)
            if ok:
                print(f"  PASS: {msg}")
                passes += 1
            else:
                print(f"  FAIL: {msg}")
                print(f"        raw csv: {csv!r}")
                fails += 1

        print(
            f"\n[lpc-pin-toggle-rx] summary: {passes} pass / {fails} fail "
            f"out of {len(CASES)} cases"
        )
        return 0 if fails == 0 else 1


if __name__ == "__main__":
    sys.exit(main())
