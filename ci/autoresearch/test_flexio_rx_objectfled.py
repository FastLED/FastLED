"""Phase 3 of FastLED#2764 — ObjectFLED TX -> FlexIO RX loopback verification.

Drives five fixed WS2812 patterns through the `Bus::OBJECT_FLED` clockless TX
driver, captures the wire signal through the new `RxBackend::FLEXIO` RX
backend, decodes the bit stream against `TIMING_WS2812B_V5`, and asserts
byte-for-byte equality with the transmitted pattern.

Five test cases (parent issue Phase 3 table):

    | # | Pattern                          | Bytes (GRB wire order) |
    |---|----------------------------------|------------------------|
    | 0 | Red single LED                   | 3                      |
    | 1 | RGB three-LED chain              | 9                      |
    | 2 | All zeros (1 LED)                | 3                      |
    | 3 | All ones  (1 LED)                | 3                      |
    | 4 | 100-LED alternating R/G/B        | 300                    |

Usage:
    uv run python ci/autoresearch/test_flexio_rx_objectfled.py [--port COM20]

Exits 0 on all pass, 1 on any failure.
"""

from __future__ import annotations

import argparse
import json
import sys
import time
from dataclasses import dataclass
from typing import Any

from typeguard import typechecked

from ci.util.global_interrupt_handler import handle_keyboard_interrupt


# `pyserial` is checked at `main()` entry, not at import — see the matching
# rationale in test_flexio_rx_squarewave.py.
try:
    import serial  # type: ignore
except ImportError:  # pragma: no cover — handled at main() entry
    serial = None  # type: ignore


DEFAULT_PORT = "COM20"
DEFAULT_BAUD = 115200


@typechecked
@dataclass
class TestCase:
    index: int
    label: str
    expected_bytes: int


@typechecked
@dataclass
class EvaluationResult:
    """Per-case result returned from `evaluate()`. Replaces the older
    `(bool, str)` tuple with a typed struct so downstream code can introspect
    individual fields without positional-index brittleness."""

    ok: bool
    message: str


CASES: list[TestCase] = [
    TestCase(0, "3.1 / Red single LED", 3),
    TestCase(1, "3.2 / RGB three-LED chain", 9),
    TestCase(2, "3.3 / All zeros", 3),
    TestCase(3, "3.4 / All ones", 3),
    TestCase(4, "3.5 / 100-LED alternating", 300),
]


def send_rpc(
    s: Any,
    method: str,
    args: dict[str, Any] | None = None,
    request_id: int = 1,
    timeout: float = 15.0,
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


@typechecked
def evaluate(case: TestCase, result: dict[str, Any]) -> EvaluationResult:
    expected = result.get("expected_bytes", 0)
    decoded = result.get("decoded_bytes", 0)
    matched = result.get("matched", 0)
    mismatched = result.get("mismatched", 0)
    edges = result.get("edges_captured", 0)
    if expected != case.expected_bytes:
        return EvaluationResult(
            ok=False,
            message=(
                f"firmware reported expected_bytes={expected} but test case "
                f"expects {case.expected_bytes}"
            ),
        )
    if mismatched != 0 or decoded != expected:
        return EvaluationResult(
            ok=False,
            message=(
                f"byte mismatch: decoded={decoded}/{expected}, "
                f"matched={matched}, mismatched={mismatched}, edges={edges}"
            ),
        )
    return EvaluationResult(
        ok=True,
        message=f"decoded={decoded}/{expected} all matched, edges={edges}",
    )


def main(argv: list[str] | None = None) -> int:
    # `argv` accepted so that unit tests can inject argument lists without
    # touching the real `sys.argv`. Defaults to `None`, which argparse
    # interprets as "use sys.argv[1:]".
    p = argparse.ArgumentParser()
    p.add_argument("--port", default=DEFAULT_PORT)
    p.add_argument("--baud", default=DEFAULT_BAUD, type=int)
    p.add_argument(
        "--tx-pin", default=3, type=int, help="TX pin driven by ObjectFLED (default: 3)"
    )
    p.add_argument(
        "--rx-pin", default=4, type=int, help="RX pin captured by FLEXIO1 (default: 4)"
    )
    p.add_argument(
        "--capture-ms",
        default=50,
        type=int,
        help="Capture window per test case in ms (default: 50)",
    )
    args = p.parse_args(argv)

    if serial is None:
        print(
            "ERROR: pyserial not installed. Run: uv pip install pyserial",
            file=sys.stderr,
        )
        return 2

    print(f"[flexio-objectfled] opening {args.port} @ {args.baud}")
    try:
        s = serial.Serial(args.port, args.baud, timeout=0.1)
    except KeyboardInterrupt as ki:
        # Catch Ctrl-C before the bare `Exception`/`SerialException` block so
        # interactive users can abort cleanly. `handle_keyboard_interrupt`
        # routes the signal through the project-wide interrupt handler
        # (required by the KBI002 lint).
        handle_keyboard_interrupt(ki)
        raise
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
        print(
            f"[flexio-objectfled] ping ok: uptime={ping['result'].get('uptimeMs')} ms"
        )

        passes = 0
        fails = 0
        for i, case in enumerate(CASES, start=20):
            print(f"\n--- {case.label} ---")
            resp = send_rpc(
                s,
                "flexioObjectFledTest",
                args={
                    "test_case": case.index,
                    "tx_pin": args.tx_pin,
                    "rx_pin": args.rx_pin,
                    "capture_ms": args.capture_ms,
                },
                request_id=i,
                timeout=args.capture_ms / 1000.0 + 10.0,
            )
            if not resp or "result" not in resp:
                print(f"  FAIL: no response or RPC error: {resp}")
                fails += 1
                continue
            result = resp["result"]
            if not result.get("success", False) and result.get("error"):
                print(f"  FAIL: firmware reported error: {result}")
                fails += 1
                continue
            eval_result = evaluate(case, result)
            if eval_result.ok:
                print(f"  PASS: {eval_result.message}")
                passes += 1
            else:
                print(f"  FAIL: {eval_result.message}")
                print(f"        raw result: {result}")
                fails += 1

        print(
            f"\n[flexio-objectfled] summary: {passes} pass / {fails} fail "
            f"out of {len(CASES)} cases"
        )
        return 0 if fails == 0 else 1


if __name__ == "__main__":
    sys.exit(main())
