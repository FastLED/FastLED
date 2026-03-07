"""BLE validation helpers for FastLED hardware-in-the-loop testing.

Provides BLE GATT communication validation flows for --ble mode.
Tests bidirectional JSON-RPC over BLE by:
  - Sending startBle RPC over serial to start GATT server
  - Connecting to device via Bleak (BLE)
  - Sending ping over BLE and verifying pong response
  - Sending stopBle RPC over serial to clean up
"""

from __future__ import annotations

import asyncio
import json
from typing import TYPE_CHECKING

from colorama import Fore, Style

from ci.rpc_client import RpcClient, RpcTimeoutError
from ci.util.ble_interface import BLE_CHAR_TX_UUID, BleInterface
from ci.util.global_interrupt_handler import handle_keyboard_interrupt


if TYPE_CHECKING:
    from ci.util.serial_interface import SerialInterface


async def run_ble_validation(
    upload_port: str,
    serial_iface: "SerialInterface | None",
    timeout: float = 60.0,
) -> int:
    """Run BLE validation (--ble).

    1. Send startBle RPC over serial -> device starts BLE GATT server + advertising
    2. Scan for "FastLED-C6" via Bleak, connect
    3. Send ping JSON-RPC over BLE (write to RX characteristic)
    4. Receive pong via NOTIFY or READ on TX characteristic -> PASS
    5. Send stopBle RPC over serial -> device stops BLE

    Args:
        upload_port: Serial port for RPC communication
        serial_iface: Pre-created serial interface
        timeout: RPC timeout in seconds

    Returns:
        Exit code (0 = success, 1 = failure)
    """
    print()
    print("=" * 60)
    print("BLE VALIDATION MODE")
    print("=" * 60)
    print()

    serial_client: RpcClient | None = None
    ble_iface: BleInterface | None = None

    try:
        # Step 1: Connect to device via serial RPC
        print(f"--- Step 1: Connect to device on {upload_port} (serial) ---")
        serial_client = RpcClient(
            upload_port, timeout=timeout, serial_interface=serial_iface
        )
        await serial_client.connect(boot_wait=3.0, drain_boot=True)
        print(f"  {Fore.GREEN}Connected to device (serial){Style.RESET_ALL}")

        # Step 2: Start BLE on device via serial RPC
        print("\n--- Step 2: Start BLE GATT server on ESP32 ---")
        response = await serial_client.send("startBle", timeout=30.0)
        ble_info = response.data

        if not isinstance(ble_info, dict) or not ble_info.get("success"):
            error = (
                ble_info.get("error", "Unknown error")
                if isinstance(ble_info, dict)
                else str(ble_info)
            )
            print(f"  {Fore.RED}Failed to start BLE: {error}{Style.RESET_ALL}")
            return 1

        device_name = ble_info.get("device_name", "FastLED-C6")
        print(
            f"  {Fore.GREEN}BLE started: name={device_name}, "
            f"service={ble_info.get('service_uuid', '?')}{Style.RESET_ALL}"
        )

        # Brief delay to let BLE advertising stabilize
        await asyncio.sleep(3.0)

        # Step 3: Connect to device via BLE
        print(f"\n--- Step 3: Connect to '{device_name}' via BLE ---")
        ble_iface = BleInterface(device_name=device_name, scan_timeout=30.0)
        await ble_iface.connect()
        print(f"  {Fore.GREEN}Connected via BLE{Style.RESET_ALL}")

        # Brief delay to let BLE connection stabilize
        await asyncio.sleep(1.0)

        # Step 3b: Query BLE status over serial (diagnostics)
        print("\n--- Step 3b: Query BLE status (serial) ---")
        try:
            status_response = await serial_client.send("bleStatus", timeout=10.0)
            ble_status = status_response.data
            print(f"  BLE status: {json.dumps(ble_status, indent=2)}")
        except KeyboardInterrupt as ki:
            handle_keyboard_interrupt(ki)
            raise
        except Exception as e:
            print(f"  Warning: bleStatus query failed: {e}")

        # Step 4: Send ping over BLE
        print("\n--- Step 4: Send ping over BLE ---")
        # RPC functions expect signature: (const fl::json& args)
        # So params must be [{}] (array with one empty object arg)
        ping_request = json.dumps(
            {"jsonrpc": "2.0", "method": "ping", "params": [{}], "id": 1}
        )
        print(f"  Sending: {ping_request}")
        await ble_iface.write(ping_request + "\n")

        # Give device time to process the request
        await asyncio.sleep(0.5)

        # Step 5: Read response
        print("\n--- Step 5: Wait for pong response ---")
        tests_passed = 0
        tests_failed = 0
        pong_received = False

        # Try notifications first (preferred mechanism)
        async for line in ble_iface.read_lines(timeout=3.0):
            payload = line
            if payload.startswith("REMOTE: "):
                payload = payload[8:]
            try:
                parsed = json.loads(payload)
                result = parsed.get("result", {})
                if isinstance(result, dict) and result.get("message") == "pong":
                    print(
                        f"  {Fore.GREEN}PASS{Style.RESET_ALL} - "
                        f"Received pong via NOTIFY (transport={result.get('transport', '?')}, "
                        f"uptimeMs={result.get('uptimeMs', '?')})"
                    )
                    tests_passed += 1
                    pong_received = True
                    break
            except json.JSONDecodeError:
                continue

        # Fallback: read TX characteristic value directly
        if not pong_received and ble_iface._client:
            try:
                tx_val = await ble_iface._client.read_gatt_char(BLE_CHAR_TX_UUID)
                text = tx_val.decode("utf-8", errors="replace").strip()
                payload = text
                if payload.startswith("REMOTE: "):
                    payload = payload[8:]
                parsed = json.loads(payload)
                result = parsed.get("result", {})
                if isinstance(result, dict) and result.get("message") == "pong":
                    print(
                        f"  {Fore.GREEN}PASS{Style.RESET_ALL} - "
                        f"Received pong via READ (transport={result.get('transport', '?')}, "
                        f"uptimeMs={result.get('uptimeMs', '?')})"
                    )
                    tests_passed += 1
                    pong_received = True
            except KeyboardInterrupt as ki:
                handle_keyboard_interrupt(ki)
                raise
            except Exception:
                pass

        if not pong_received:
            print(
                f"  {Fore.RED}FAIL{Style.RESET_ALL} - No pong received within timeout"
            )
            tests_failed += 1

        # Step 6: Clean up — stop BLE via serial
        print("\n--- Step 6: Stop BLE on device ---")
        try:
            await ble_iface.close()
        except KeyboardInterrupt as ki:
            handle_keyboard_interrupt(ki)
            raise
        except Exception:
            pass
        ble_iface = None

        try:
            stop_response = await serial_client.send("stopBle", timeout=10.0)
            print(f"  stopBle response: {stop_response.data}")
        except KeyboardInterrupt as ki:
            handle_keyboard_interrupt(ki)
            raise
        except Exception as e:
            print(f"  Warning: stopBle failed: {e}")

        # Summary
        total = tests_passed + tests_failed
        print()
        print("=" * 60)
        if tests_failed == 0:
            print(
                f"{Fore.GREEN}BLE VALIDATION PASSED ({tests_passed}/{total} tests){Style.RESET_ALL}"
            )
            return 0
        else:
            print(
                f"{Fore.RED}BLE VALIDATION FAILED ({tests_passed}/{total} passed, "
                f"{tests_failed} failed){Style.RESET_ALL}"
            )
            return 1

    except KeyboardInterrupt as ki:
        print("\n\n  Interrupted by user")
        handle_keyboard_interrupt(ki)
        return 130
    except RpcTimeoutError:
        print(f"\n  {Fore.RED}Timeout waiting for BLE response{Style.RESET_ALL}")
        return 1
    except Exception as e:
        print(f"\n  {Fore.RED}BLE validation error: {e}{Style.RESET_ALL}")
        import traceback

        traceback.print_exc()
        return 1
    finally:
        # Cleanup
        if ble_iface:
            try:
                await ble_iface.close()
            except KeyboardInterrupt as ki:
                handle_keyboard_interrupt(ki)
            except Exception:
                pass
        if serial_client:
            try:
                await serial_client.send("stopBle", timeout=5.0)
            except KeyboardInterrupt as ki:
                handle_keyboard_interrupt(ki)
            except Exception:
                pass
            await serial_client.close()
