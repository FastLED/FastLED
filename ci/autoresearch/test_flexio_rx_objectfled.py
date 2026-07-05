"""Phase 3 of FastLED#2764 — ObjectFLED TX -> FlexIO RX loopback verification.

Drives five fixed WS2812 patterns through the `Bus::FLEX_IO` slot 0 clockless TX
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
    uv run python -m ci.autoresearch.test_flexio_rx_objectfled [--port COM20]

Exits 0 on all pass, 1 on any failure.
"""

from __future__ import annotations

import argparse
import json
import sys
import time
from dataclasses import dataclass
from pathlib import Path
from typing import Any

from typeguard import typechecked

# `pyserial` is checked at `main()` entry, not at import — see the matching
# rationale in test_flexio_rx_squarewave.py.
from ci.autoresearch.rpc_bench import METHOD_NOT_FOUND, RpcBench  # noqa: E402,F401
from ci.util.global_interrupt_handler import handle_keyboard_interrupt


def send_rpc(
    s,
    method,
    args=None,
    request_id=None,
    timeout: float = 10.0,
):
    """Shim over RpcBench (`s`) so run_* keep their reply["result"] shape.

    Device serial goes through fbuild's Rust monitor (never raw pyserial);
    see agents/docs/hardware-autoresearch.md -> "Device serial". The
    request_id kwarg is accepted for call-site compatibility and ignored
    (RpcClient assigns its own correlation ids).
    """
    _ = request_id
    result = s.call(method, args=args, timeout=timeout)
    if result is None:
        return None
    if result is METHOD_NOT_FOUND:
        return {"error": {"code": -32601, "message": "method not found"}}
    return {"result": result}


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

    print(f"[flexio-objectfled] opening {args.port} @ {args.baud}")
    try:
        s = RpcBench(args.port)
    except KeyboardInterrupt as ki:
        handle_keyboard_interrupt(ki)
        raise
    except Exception as e:  # noqa: BLE001
        print(f"Could not connect to {args.port}: {e}")
        return 1
    with s:
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
