"""GPIO connectivity pre-test and pin auto-discovery for autoresearch."""

from __future__ import annotations

from dataclasses import dataclass
from typing import TYPE_CHECKING, Iterable

from colorama import Fore, Style

from ci.autoresearch.context import PIN_RX, PIN_TX
from ci.rpc_client import RpcClient, RpcTimeoutError
from ci.util.global_interrupt_handler import handle_keyboard_interrupt


if TYPE_CHECKING:
    from ci.util.serial_interface import SerialInterface


async def run_gpio_pretest(
    port: str,
    tx_pin: int = PIN_TX,
    rx_pin: int = PIN_RX,
    timeout: float = 15.0,
    serial_interface: SerialInterface | None = None,
) -> bool:
    """Test GPIO connectivity between TX and RX pins before running autoresearch.

    Uses a simple pullup/drive-low pattern to verify that the TX and RX pins
    are physically connected via a jumper wire.

    Returns:
        True if pins are connected, False otherwise
    """
    print()
    print("=" * 60)
    print("GPIO CONNECTIVITY PRE-TEST")
    print("=" * 60)
    print(f"Testing connection: TX (GPIO {tx_pin}) \u2192 RX (GPIO {rx_pin})")
    print()

    try:
        print("  Waiting for device to boot...")
        print("\n" + "=" * 60)
        print("RPC CLIENT DEBUG OUTPUT")
        print("=" * 60)
        client = RpcClient(
            port, timeout=timeout, serial_interface=serial_interface, verbose=True
        )
        # Wait longer than the nominal boot time: FbuildSerialAdapter's
        # connect triggers a DTR reset, and ESP32-S3 AutoResearch setup
        # takes ~5-7s before the RPC dispatcher is ready. A 3s boot_wait
        # raced the first ping against setup() and caused intermittent
        # timeouts.
        await client.connect(boot_wait=8.0, drain_boot=True)
        try:
            print()
            print("=" * 60)
            print("PING TEST (verify basic RPC works)")
            print("=" * 60)
            try:
                ping_response = await client.send("ping", timeout=15.0, retries=3)
                print(f"\u2705 Ping successful: {ping_response.data}")
            except KeyboardInterrupt as ki:
                handle_keyboard_interrupt(ki)
                raise
            except Exception as e:
                # Try DTR reset and retry — device may be booted but idle
                print(f"\u26a0\ufe0f  Ping failed ({e}), attempting DTR reset...")
                try:
                    # Use SerialInterface.reset_device(wait_for_output=True)
                    # to block until the device starts producing serial output
                    reset_ok = False
                    if serial_interface is not None:
                        print(
                            "  Resetting device via SerialInterface.reset_device(wait_for_output=True)..."
                        )
                        reset_ok = await serial_interface.reset_device(board=None)
                    else:
                        from ci.util.serial_interface import _pyserial_dtr_reset

                        reset_ok = _pyserial_dtr_reset(port)

                    # Use longer boot_wait if reset reported failure
                    boot_wait = 1.0 if reset_ok else 3.0

                    if serial_interface is None:
                        await client.close()
                        client = None  # Prevent double-close in outer finally
                        client = RpcClient(
                            port,
                            timeout=timeout,
                            serial_interface=serial_interface,
                            verbose=True,
                        )
                        await client.connect(boot_wait=boot_wait, drain_boot=True)
                    else:
                        await client.drain_boot_output(verbose=True)
                    ping_response = await client.send("ping", retries=3)
                    print(
                        f"\u2705 Ping successful after DTR reset: {ping_response.data}"
                    )
                except KeyboardInterrupt as ki:
                    handle_keyboard_interrupt(ki)
                    raise
                except Exception as e2:
                    print(f"\u274c Ping failed after DTR reset: {e2}")
                    print()
                    print(
                        f"   {Fore.RED}DIAGNOSIS: RPC communication failure{Style.RESET_ALL}"
                    )
                    print("   The device is not responding to JSON-RPC commands.")
                    print("   This is NOT a jumper wire issue.")
                    print()
                    print("   Possible causes:")
                    print("   1. Firmware not flashed or outdated")
                    print("   2. Device crashed during boot")
                    print("   3. Serial port conflict with another process")
                    print("   4. Wrong baud rate or USB-CDC driver issue")
                    return False

            print()
            print("=" * 60)
            print("GPIO CONNECTIVITY TEST")
            print("=" * 60)
            print("  Sending GPIO test command...")

            response = await client.send_and_match(
                "testGpioConnection",
                args=[tx_pin, rx_pin],
                match_key="connected",
                retries=3,
            )

            if response.get("connected", False):
                print()
                print(f"{Fore.GREEN}\u2705 GPIO PRE-TEST PASSED{Style.RESET_ALL}")
                print(f"   TX (GPIO {tx_pin}) and RX (GPIO {rx_pin}) are connected")
                print()
                return True
            else:
                rx_low = response.get("rxWhenTxLow", "?")
                rx_high = response.get("rxWhenTxHigh", "?")
                msg = response.get("message", "Unknown error")
                print()
                print(f"{Fore.RED}\u274c GPIO PRE-TEST FAILED{Style.RESET_ALL}")
                print()
                print(f"   {Fore.RED}Error: {msg}{Style.RESET_ALL}")
                print(f"   Failure class: electrical_issue")
                print()
                # Provide specific diagnosis based on pin readings
                if rx_low == 1 and rx_high == 1:
                    print(
                        f"   {Fore.YELLOW}DIAGNOSIS: RX pin (GPIO {rx_pin}) is stuck HIGH (floating/pullup).{Style.RESET_ALL}"
                    )
                    print(
                        f"   The jumper wire is likely not connected to GPIO {rx_pin}."
                    )
                    print(
                        f"   Check that the wire connects TX (GPIO {tx_pin}) to RX (GPIO {rx_pin})."
                    )
                elif rx_low == 0 and rx_high == 0:
                    print(
                        f"   {Fore.YELLOW}DIAGNOSIS: RX pin (GPIO {rx_pin}) is stuck LOW.{Style.RESET_ALL}"
                    )
                    print(
                        "   Possible short to ground or GPIO conflict with a peripheral."
                    )
                else:
                    print("   The TX and RX pins are NOT electrically connected.")
                print()
                print("   Debug info:")
                print(f"     RX when TX=LOW:  {rx_low}")
                print(f"     RX when TX=HIGH: {rx_high}")
                print(f"     Tested: TX=GPIO {tx_pin}, RX=GPIO {rx_pin}")
                print()
                return False
        finally:
            if client is not None:
                await client.close()

    except RpcTimeoutError:
        print()
        print(f"{Fore.RED}\u274c GPIO PRE-TEST TIMEOUT{Style.RESET_ALL}")
        print(f"   No response from device within {timeout} seconds")
        print()
        print("   Possible causes:")
        print("   1. Firmware is outdated (recompile with latest code)")
        print("   2. Device is stuck in setup (check serial output above)")
        print("   3. Serial communication issue")
        print()
        return False
    except KeyboardInterrupt as ki:
        handle_keyboard_interrupt(ki)
        raise
    except (RuntimeError, OSError) as e:
        print()
        print(f"{Fore.RED}\u274c GPIO PRE-TEST ERROR{Style.RESET_ALL}")
        print(f"   Serial connection error: {e}")
        print()
        return False
    except Exception as e:
        print()
        print(f"{Fore.RED}\u274c GPIO PRE-TEST ERROR{Style.RESET_ALL}")
        print(f"   Unexpected error: {e}")
        print()
        return False


# ============================================================
# Pin Discovery (Auto-Detect Connected Pins)
# ============================================================


@dataclass(slots=True)
class PinDiscoveryResult:
    """Result of auto-discovering connected pin pairs."""

    success: bool
    tx_pin: int | None
    rx_pin: int | None
    client: RpcClient | None


async def run_pin_discovery(
    port: str,
    start_pin: int = 0,
    end_pin: int = 8,
    timeout: float = 15.0,
    serial_interface: SerialInterface | None = None,
) -> PinDiscoveryResult:
    """Auto-discover connected pin pairs by probing adjacent GPIO pins
    in a single window.

    Calls the `findConnectedPins` RPC with the given range. Defaults to
    the conservative 0-8 window — for full-chip sweeps that find shorts
    on higher GPIOs use :func:`run_pin_discovery_segmented`, which
    iterates 8-pin overlapping windows so a hang in one segment doesn't
    take out the rest of the scan.

    Returns:
        PinDiscoveryResult with success, tx_pin, rx_pin, and client (kept open for reuse)
    """
    print()
    print("=" * 60)
    print("PIN DISCOVERY (Auto-Detect Connected Pins)")
    print("=" * 60)
    print(f"Searching for jumper wire connection in GPIO range {start_pin}-{end_pin}")
    print()

    client: RpcClient | None = None
    try:
        print("  Waiting for device to boot...")
        print("\n" + "=" * 60)
        print("RPC CLIENT DEBUG OUTPUT")
        print("=" * 60)
        client = RpcClient(
            port, timeout=timeout, serial_interface=serial_interface, verbose=True
        )
        await client.connect(boot_wait=8.0, drain_boot=True)

        print()
        print("=" * 60)
        print("PING TEST (verify basic RPC works)")
        print("=" * 60)
        try:
            ping_response = await client.send("ping", timeout=30.0, retries=3)
            print(f"\u2705 Ping successful: {ping_response.data}")
        except KeyboardInterrupt as ki:
            handle_keyboard_interrupt(ki)
            raise
        except Exception as e:
            # Try DTR reset and retry — device may be booted but idle
            print(f"\u26a0\ufe0f  Ping failed ({e}), attempting DTR reset...")
            try:
                # Use SerialInterface.reset_device(wait_for_output=True)
                reset_ok = False
                if serial_interface is not None:
                    print(
                        "  Resetting device via SerialInterface.reset_device(wait_for_output=True)..."
                    )
                    reset_ok = await serial_interface.reset_device(board=None)
                else:
                    from ci.util.serial_interface import _pyserial_dtr_reset

                    reset_ok = _pyserial_dtr_reset(port)

                boot_wait = 1.0 if reset_ok else 3.0

                if serial_interface is None:
                    if client:
                        await client.close()
                    client = None
                    client = RpcClient(
                        port,
                        timeout=timeout,
                        serial_interface=serial_interface,
                        verbose=True,
                    )
                    await client.connect(boot_wait=boot_wait, drain_boot=True)
                else:
                    await client.drain_boot_output(verbose=True)
                ping_response = await client.send("ping", timeout=30.0, retries=3)
                print(f"\u2705 Ping successful after DTR reset: {ping_response.data}")
            except KeyboardInterrupt as ki:
                handle_keyboard_interrupt(ki)
                raise
            except Exception as e2:
                print(f"\u274c Ping failed after DTR reset: {e2}")
                print()
                print(
                    "   DIAGNOSIS: RPC communication failure — device is not responding."
                )
                print("   This is NOT a jumper wire issue.")
                print("   Check: firmware flashed? device crashed? port conflict?")
                if client:
                    await client.close()
                return PinDiscoveryResult(
                    success=False, tx_pin=None, rx_pin=None, client=None
                )

        print()
        print("=" * 60)
        print("PIN DISCOVERY")
        print("=" * 60)
        print("  Probing adjacent pin pairs for jumper wire connection...")

        response = await client.send_and_match(
            "findConnectedPins",
            args=[{"startPin": start_pin, "endPin": end_pin, "autoApply": True}],
            match_key="found",
            retries=3,
        )

        # FastLED #3446: render the skippedPins payload (if any) so the
        # human-facing log says exactly which pins the firmware refused
        # to probe \u2014 turning a silent "no jumper" into "GPIO 6-11 were
        # flash pads, 20 USB-JTAG, 34-39 input-only".
        skipped = response.get("skippedPins") or []
        if skipped:
            unique_pins: dict[int, str] = {}
            for entry in skipped:
                if isinstance(entry, dict):
                    pin_val = entry.get("pin")
                    reason = entry.get("reason", "unknown")
                    if isinstance(pin_val, int) and pin_val not in unique_pins:
                        unique_pins[pin_val] = str(reason)
            if unique_pins:
                summary = ", ".join(
                    f"GPIO {p} ({reason})" for p, reason in sorted(unique_pins.items())
                )
                print(
                    f"   {Fore.CYAN}Skipped per FastLED pin-validity map: "
                    f"{summary}{Style.RESET_ALL}"
                )

        if response.get("found", False):
            tx_pin = response.get("txPin")
            rx_pin = response.get("rxPin")
            auto_applied = response.get("autoApplied", False)

            print()
            print(f"{Fore.GREEN}\u2705 PIN DISCOVERY SUCCESSFUL{Style.RESET_ALL}")
            print(
                f"   Found connected pins: TX (GPIO {tx_pin}) \u2192 RX (GPIO {rx_pin})"
            )
            if auto_applied:
                print(f"   {Fore.CYAN}Pins auto-applied to firmware{Style.RESET_ALL}")
            print()
            return PinDiscoveryResult(
                success=True, tx_pin=tx_pin, rx_pin=rx_pin, client=client
            )
        else:
            print()
            print(
                f"{Fore.YELLOW}\u26a0\ufe0f  PIN DISCOVERY: No connection found{Style.RESET_ALL}"
            )
            print()
            print(f"   {response.get('message', 'No connected pin pairs detected')}")
            print()
            print(f"   {Fore.YELLOW}Falling back to default pins{Style.RESET_ALL}")
            print()
            return PinDiscoveryResult(
                success=False, tx_pin=None, rx_pin=None, client=client
            )

    except RpcTimeoutError:
        print()
        print(f"{Fore.YELLOW}\u26a0\ufe0f  PIN DISCOVERY TIMEOUT{Style.RESET_ALL}")
        print(f"   No response within {timeout} seconds, falling back to default pins")
        print()
        if client:
            await client.close()
        return PinDiscoveryResult(success=False, tx_pin=None, rx_pin=None, client=None)
    except KeyboardInterrupt as ki:
        if client:
            await client.close()
        handle_keyboard_interrupt(ki)
        raise
    except (RuntimeError, OSError) as e:
        print()
        print(f"{Fore.YELLOW}\u26a0\ufe0f  PIN DISCOVERY ERROR{Style.RESET_ALL}")
        print(f"   Serial error: {e}")
        print("   Falling back to default pins")
        print()
        if client:
            await client.close()
        return PinDiscoveryResult(success=False, tx_pin=None, rx_pin=None, client=None)
    except Exception as e:
        print()
        print(f"{Fore.YELLOW}\u26a0\ufe0f  PIN DISCOVERY ERROR{Style.RESET_ALL}")
        print(f"   Unexpected error: {e}")
        print("   Falling back to default pins")
        print()
        if client:
            await client.close()
        return PinDiscoveryResult(success=False, tx_pin=None, rx_pin=None, client=None)


# ============================================================
# Segmented Pin Discovery (FastLED #3446 \u2014 full-chip sweep)
# ============================================================
#
# The single-window `run_pin_discovery` defaults to GPIO 0-8 because
# that's the historically-safe window \u2014 driving anything higher on
# classic-ESP32 can hit boot strapping pins (12, 15), the SPI flash
# pads (6-11), or the USB JTAG pads (20) and either reset the chip or
# corrupt the live flash. But the user-reported (33, 34) short \u2014 and
# more generally any short on ADC2 / IO_MUX-only pins \u2014 is completely
# invisible to a 0-8 sweep.
#
# Segmented discovery splits the full 0-39 range into overlapping
# 8-pin windows and probes them one at a time. Each segment gets its
# own RPC round-trip, so:
#
#   * a hang or watchdog reset in segment N is isolated \u2014 the host
#     re-establishes RPC, logs which segment knocked the chip over,
#     and moves on to N+1 instead of giving up on the whole sweep;
#   * the human-visible scan log says exactly which segment found the
#     pair (or which segment hung), turning "I don't know why it
#     failed" into "GPIO 32-40 caused a reset, try a clean power
#     cycle and re-run from segment 5".
#
# Overlap on the segment-boundary pin (e.g. both 0-8 and 8-16 include
# pin 8) means the (n-1, n) and (n, n+1) pairs straddling the boundary
# are still tested.

# Default segment plan for classic-ESP32: five overlapping 8-pin
# windows covering 0..40. Each pair `(start, end)` matches
# `findConnectedPins` semantics \u2014 `start` inclusive, `end` exclusive
# upper bound on the *pin* (so the loop tests pairs (start, start+1)
# through (end-1, end)).
DEFAULT_PIN_DISCOVERY_SEGMENTS: tuple[tuple[int, int], ...] = (
    (0, 8),
    (8, 16),
    (16, 24),
    (24, 32),
    (32, 40),
)


async def run_pin_discovery_segmented(
    port: str,
    segments: Iterable[tuple[int, int]] = DEFAULT_PIN_DISCOVERY_SEGMENTS,
    per_segment_timeout: float = 15.0,
    serial_interface: SerialInterface | None = None,
) -> PinDiscoveryResult:
    """Sweep adjacent-pair pin discovery across multiple GPIO segments.

    Iterates ``segments`` in order, issuing one ``findConnectedPins``
    RPC per (start, end) window. The first segment that returns
    ``found=true`` wins. If a segment hangs the firmware (no response
    within ``per_segment_timeout``), the host logs which window
    misbehaved, attempts a DTR reset + reconnect, and continues with
    the next segment \u2014 so an ADC2 / strap-pin hazard in one window
    doesn't abort the whole sweep.

    Args:
        port: Serial port (e.g. ``"COM11"``).
        segments: Iterable of ``(start_pin, end_pin)`` windows.
            Defaults to :data:`DEFAULT_PIN_DISCOVERY_SEGMENTS` which
            covers the full classic-ESP32 GPIO range in five
            overlapping 8-pin windows.
        per_segment_timeout: Wall-clock budget per segment. The single
            window scan typically completes in <2 s, so 15 s leaves
            generous headroom for reset + handshake.
        serial_interface: Optional shared serial interface for reuse.

    Returns:
        PinDiscoveryResult \u2014 ``success=True`` with the discovered
        ``tx_pin/rx_pin`` and an open ``client`` on the first hit;
        ``success=False`` with ``client=None`` if every segment came
        back empty (or hung).
    """
    print()
    print("=" * 60)
    print("PIN DISCOVERY \u2014 SEGMENTED SWEEP (FastLED #3446)")
    print("=" * 60)
    segments_list = list(segments)
    print(f"Probing {len(segments_list)} overlapping GPIO window(s):")
    for s in segments_list:
        print(f"  \u2022 GPIO {s[0]}-{s[1]}")
    print()

    crashed_segments: list[tuple[int, int]] = []
    for idx, (start_pin, end_pin) in enumerate(segments_list, start=1):
        print(
            f"{Fore.CYAN}\u2500\u2500 Segment {idx}/{len(segments_list)}: "
            f"GPIO {start_pin}-{end_pin} \u2500\u2500{Style.RESET_ALL}"
        )
        result = await run_pin_discovery(
            port=port,
            start_pin=start_pin,
            end_pin=end_pin,
            timeout=per_segment_timeout,
            serial_interface=serial_interface,
        )
        if result.success:
            print()
            print(
                f"{Fore.GREEN}\u2705 SEGMENTED PIN DISCOVERY: HIT in "
                f"GPIO {start_pin}-{end_pin}{Style.RESET_ALL}"
            )
            if crashed_segments:
                print(
                    f"   (segments that hung mid-sweep: "
                    f"{', '.join(f'{a}-{b}' for a, b in crashed_segments)})"
                )
            return result

        # Empty result is fine \u2014 try next segment. We only treat a
        # missing client as a true hang signal, since
        # `run_pin_discovery` closes the client when the RPC times out
        # or the serial layer raises.
        if result.client is None:
            crashed_segments.append((start_pin, end_pin))
            print(
                f"{Fore.YELLOW}\u26a0\ufe0f  Segment GPIO {start_pin}-{end_pin} "
                f"did not respond \u2014 skipping and continuing.{Style.RESET_ALL}"
            )

    print()
    print(
        f"{Fore.YELLOW}\u26a0\ufe0f  PIN DISCOVERY: no connection in any of the "
        f"{len(segments_list)} segment(s){Style.RESET_ALL}"
    )
    if crashed_segments:
        print(
            f"   crashed segments: {', '.join(f'{a}-{b}' for a, b in crashed_segments)}"
        )
    print()
    return PinDiscoveryResult(success=False, tx_pin=None, rx_pin=None, client=None)
