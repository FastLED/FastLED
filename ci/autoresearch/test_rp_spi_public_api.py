"""Validate RP2040 SPI1 APA102/SK9822 output through FastLED's public API."""

from __future__ import annotations

import argparse
import sys

from ci.autoresearch.rpc_bench import METHOD_NOT_FOUND, RpcBench
from ci.util.global_interrupt_handler import handle_keyboard_interrupt


PATTERNS = ("black", "red", "green", "blue", "white", "walking")


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--port", required=True)
    parser.add_argument("--chipset", choices=("apa102", "sk9822"), required=True)
    args = parser.parse_args()
    print(f"RP2040 SPI1 {args.chipset.upper()} public API loopback — FastLED #3660")
    print("  Wiring: GPIO11 (MOSI) -> GPIO8 (MISO); GPIO10 is SCK output only")
    try:
        with RpcBench(args.port) as bench:
            for pattern, pattern_index in zip(PATTERNS, range(len(PATTERNS))):
                result = bench.call(
                    "rpSpiPublicApiLoopback",
                    args={"sk9822": args.chipset == "sk9822", "pattern": pattern_index},
                    timeout=10.0,
                )
                if result is METHOD_NOT_FOUND or not isinstance(result, dict):
                    print(f"FAIL — rpSpiPublicApiLoopback dispatch result: {result!r}")
                    return 1
                data = result.get("data")
                if not result.get("success") or not isinstance(data, dict):
                    print(f"FAIL — {pattern}: {result}")
                    return 1
                if data.get("frameBytes") != 1068 or data.get("wireIdle") is not True:
                    print(f"FAIL — {pattern}: malformed frame evidence {data}")
                    return 1
                print(
                    f"  PASS — {pattern}: {data['frameBytes']} bytes, "
                    f"actual={data['actualClockHz']:,} Hz, wire idle"
                )
    except KeyboardInterrupt as interrupt:
        handle_keyboard_interrupt(interrupt)
        raise
    except Exception as error:  # noqa: BLE001
        print(f"FAIL — unable to run RP SPI public API loopback: {error}")
        return 1
    print(f"PASS — {args.chipset.upper()} public API framing is byte-exact on SPI1.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
