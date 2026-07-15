"""Validate RP2040 fixed-SPI DMA loopback through ChannelEngineRpSpi.

The companion AutoResearch RPC sends a fixed 32-byte pattern through
``BusTraits<Bus::SPI, N>``. RX DMA captures MISO while the engine waits for
the PL022 wire-idle state. SPI0 uses GPIO3 (MOSI) -> GPIO0 (MISO), GPIO2
(SCK); SPI1 uses GPIO11 (MOSI) -> GPIO8 (MISO), GPIO10 (SCK).
"""

from __future__ import annotations

import argparse
import sys
from typing import Any

from ci.autoresearch.rpc_bench import METHOD_NOT_FOUND, RpcBench
from ci.util.global_interrupt_handler import handle_keyboard_interrupt


DEFAULT_PORT = "COM10"
RATES_HZ = (1_000_000, 8_000_000, 24_000_000)


def pins_for_spi(spi_index: int) -> tuple[int, int, int]:
    """Return the canonical MOSI, MISO, and SCK pins for an RP SPI instance."""
    return (3, 0, 2) if spi_index == 0 else (11, 8, 10)


def run_case(
    bench: RpcBench,
    spi_index: int,
    mosi_pin: int,
    miso_pin: int,
    sck_pin: int,
    clock_hz: int,
) -> bool:
    """Run one rate and require byte-exact capture plus engine wire-idle."""
    print(f"  rpSpiLoopback(SPI{spi_index}, {clock_hz:,} Hz)")
    result: Any = bench.call(
        "rpSpiLoopback",
        args={
            "spi_index": spi_index,
            "mosi_pin": mosi_pin,
            "miso_pin": miso_pin,
            "sck_pin": sck_pin,
            "clock_hz": clock_hz,
        },
        timeout=10.0,
    )
    if result is METHOD_NOT_FOUND:
        print("    FAIL — firmware does not expose rpSpiLoopback")
        return False
    if not isinstance(result, dict):
        print(f"    FAIL — malformed RPC result: {result!r}")
        return False
    data = result.get("data")
    if not result.get("success") or not isinstance(data, dict):
        print(f"    FAIL — {result.get('error', result.get('message', result))}")
        if isinstance(data, dict):
            print(f"      capture: {data}")
        return False
    expected = data.get("expected")
    received = data.get("received")
    actual_clock = data.get("actualClockHz")
    wire_idle = data.get("wireIdle")
    valid = (
        expected == received
        and wire_idle is True
        and isinstance(actual_clock, int)
        and actual_clock > 0
    )
    if not valid:
        print(f"    FAIL — {data}")
        return False
    print(
        f"    PASS — {len(expected)} bytes exact, actual={actual_clock:,} Hz, "
        "PL022 wire idle"
    )
    return True


def main() -> int:
    parser = argparse.ArgumentParser(
        description="RP2040 fixed-SPI DMA byte-loopback through ChannelEngineRpSpi"
    )
    parser.add_argument("--port", default=DEFAULT_PORT)
    parser.add_argument("--spi-index", type=int, choices=(0, 1), default=0)
    parser.add_argument("--mosi-pin", type=int)
    parser.add_argument("--miso-pin", type=int)
    args = parser.parse_args()
    default_mosi, default_miso, sck_pin = pins_for_spi(args.spi_index)
    mosi_pin = args.mosi_pin if args.mosi_pin is not None else default_mosi
    miso_pin = args.miso_pin if args.miso_pin is not None else default_miso

    print(f"RP2040 SPI{args.spi_index} DMA byte-loopback — FastLED #3659")
    print(f"  Port: {args.port}")
    print(f"  Wiring: GPIO{mosi_pin} (MOSI) -> GPIO{miso_pin} (MISO)")
    print(f"  SCK: GPIO{sck_pin} (output only)")
    try:
        with RpcBench(args.port) as bench:
            schema = bench.call_flat("rpc.discover", args=[])
            methods = schema.get("schema", []) if isinstance(schema, dict) else []
            method_names = {
                entry[0]
                for entry in methods
                if isinstance(entry, list) and entry and isinstance(entry[0], str)
            }
            if "rpSpiLoopback" not in method_names:
                print("FAIL — deployed firmware schema does not contain rpSpiLoopback")
                return 1
            passed = all(
                run_case(bench, args.spi_index, mosi_pin, miso_pin, sck_pin, clock_hz)
                for clock_hz in RATES_HZ
            )
    except KeyboardInterrupt as interrupt:
        handle_keyboard_interrupt(interrupt)
        raise
    except Exception as error:  # noqa: BLE001
        print(f"FAIL — unable to run RP SPI loopback: {error}")
        return 1
    if passed:
        print(f"PASS — SPI{args.spi_index} DMA loopback is byte-exact at all requested rates.")
        return 0
    print("FAIL — RP SPI loopback did not meet the byte-exact contract.")
    return 1


if __name__ == "__main__":
    sys.exit(main())
