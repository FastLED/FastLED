"""GPIO connectivity pre-test and pin auto-discovery for autoresearch."""

from __future__ import annotations

from dataclasses import dataclass
from typing import TYPE_CHECKING

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
                    await client.close()
                    client = None  # Prevent double-close in outer finally

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

                    client = RpcClient(
                        port,
                        timeout=timeout,
                        serial_interface=serial_interface,
                        verbose=True,
                    )
                    await client.connect(boot_wait=boot_wait, drain_boot=True)
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
    """Auto-discover connected pin pairs by probing adjacent GPIO pins.

    Calls the findConnectedPins RPC to search for a jumper wire connection
    between adjacent pin pairs.

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
                if client:
                    await client.close()
                client = None

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

                client = RpcClient(
                    port,
                    timeout=timeout,
                    serial_interface=serial_interface,
                    verbose=True,
                )
                await client.connect(boot_wait=boot_wait, drain_boot=True)
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
