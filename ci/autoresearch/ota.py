"""OTA autoresearch helpers for FastLED hardware-in-the-loop testing.

Provides WiFi management and HTTP autoresearch flows for --ota mode.
Tests the fl::OTA web-based firmware update interface by verifying:
  - Authentication (Basic Auth: admin:<password>)
  - Unauthenticated access is rejected (401)
  - OTA upload endpoint exists and handles errors
"""

from __future__ import annotations

import asyncio
import base64
import hashlib
import time
from pathlib import Path
from typing import TYPE_CHECKING, Any

import httpx
from colorama import Fore, Style

from ci.autoresearch.net import create_wifi_manager
from ci.rpc_client import RpcClient, RpcTimeoutError
from ci.util.global_interrupt_handler import handle_keyboard_interrupt


if TYPE_CHECKING:
    from ci.util.serial_interface import SerialInterface


def _served_request_count(status: dict[str, Any]) -> int:
    """Read `servedRequests` from an untrusted device status payload.

    `int(status.get("servedRequests", 0))` raised ValueError/TypeError on a
    non-numeric or null value, and neither is caught by the handler around
    this flow — so a malformed device reply aborted the run with a traceback
    instead of the normal failure message (FastLED#3956).

    A value that is not a plain non-negative integer counts as zero served
    requests, which surfaces as the ordinary "did not serve" RuntimeError.
    """
    value = status.get("servedRequests", 0)
    # bool is an int subclass; True must not read as "1 request served".
    if isinstance(value, bool) or not isinstance(value, int):
        return 0
    return value if value >= 0 else 0


async def run_ota_peer_autoresearch(
    upload_port: str,
    peer_upload_port: str,
    serial_iface: "SerialInterface | None",
    firmware_path: Path | None,
    timeout: float = 360.0,
) -> int:
    """Update RP2350W from an ESP32-C6 fixture without host WiFi access.

    Both boards are deployed by fbuild before this function runs. The host uses
    the normal fbuild-backed RPC transport only to stage the RP image onto the
    C6. The RP then fetches that verified artifact over their private WiFi link.
    """
    from ci.util.serial_interface import create_serial_interface

    print("\nOTA PEER AUTORESEARCH: RP2350W <- ESP32-C6")
    print("  Host WiFi is not used or changed by this mode.")
    if firmware_path is None or not firmware_path.is_file():
        print(
            f"  {Fore.RED}RP2350W firmware is missing: {firmware_path}{Style.RESET_ALL}"
        )
        return 1

    artifact = firmware_path.read_bytes()
    sha256 = hashlib.sha256(artifact).hexdigest()
    deadline = time.monotonic() + timeout
    primary: RpcClient | None = None
    peer: RpcClient | None = None

    def rpc_timeout() -> float:
        remaining = deadline - time.monotonic()
        if remaining <= 0:
            raise RpcTimeoutError("--net-peer --ota deadline expired")
        return min(20.0, remaining)

    async def rpc_data(
        client: RpcClient,
        method: str,
        params: list[Any] | dict[str, Any] | str | None = None,
    ) -> dict[str, Any]:
        response = await client.send(method, params or {}, timeout=rpc_timeout())
        if not isinstance(response.data, dict):
            raise RuntimeError(f"{method} returned a non-object response")
        return response.data

    try:
        primary = RpcClient(
            upload_port, timeout=rpc_timeout(), serial_interface=serial_iface
        )
        peer = RpcClient(
            peer_upload_port,
            timeout=rpc_timeout(),
            serial_interface=create_serial_interface(peer_upload_port),
        )
        await primary.connect(boot_wait=3.0, drain_boot=True)
        await peer.connect(boot_wait=3.0, drain_boot=True)

        primary_status = await rpc_data(primary, "status")
        peer_status = await rpc_data(peer, "status")
        if "rp2350" not in str(primary_status.get("platform", "")).lower():
            raise RuntimeError(f"Primary board is not RP2350W: {primary_status}")
        if "esp32-c6" not in str(peer_status.get("platform", "")).lower():
            raise RuntimeError(f"Companion board is not ESP32-C6: {peer_status}")

        begin = await rpc_data(
            peer, "beginOtaArtifact", {"size": len(artifact), "sha256": sha256}
        )
        if not begin.get("success"):
            raise RuntimeError(f"C6 refused OTA artifact: {begin}")
        for offset in range(0, len(artifact), 512):
            encoded = base64.b64encode(artifact[offset : offset + 512]).decode("ascii")
            written = await rpc_data(peer, "writeOtaArtifact", encoded)
            if not written.get("success"):
                raise RuntimeError(
                    f"C6 artifact write failed at byte {offset}: {written}"
                )
        finished = await rpc_data(peer, "finishOtaArtifact")
        if not finished.get("success") or finished.get("sha256") != sha256:
            raise RuntimeError(f"C6 artifact verification failed: {finished}")

        c6_server = await rpc_data(peer, "startNetServer")
        if not c6_server.get("success"):
            raise RuntimeError(f"C6 AP start failed: {c6_server}")
        artifact_server = await rpc_data(peer, "startOtaArtifactServer")
        host = artifact_server.get("ip")
        port = artifact_server.get("port")
        # `isinstance(port, int)` alone admits 0, negatives and values past
        # 65535, which the device would then narrow and silently target a
        # different endpoint (FastLED#3956). bool is an int subclass, so
        # exclude it explicitly.
        if (
            not artifact_server.get("success")
            or not isinstance(host, str)
            or not isinstance(port, int)
            or isinstance(port, bool)
            or not 1 <= port <= 65535
        ):
            raise RuntimeError(f"C6 artifact server failed: {artifact_server}")

        joined = await rpc_data(
            primary,
            "wifiConnect",
            {"ssid": c6_server.get("ssid"), "password": c6_server.get("password")},
        )
        if not joined.get("success"):
            raise RuntimeError(f"RP2350W WiFi join failed: {joined}")
        for _ in range(20):
            wifi = await rpc_data(primary, "wifiStatus")
            if wifi.get("connected"):
                break
            await asyncio.sleep(min(0.5, rpc_timeout()))
        else:
            raise RpcTimeoutError("RP2350W did not join the C6 AP")

        accepted = await rpc_data(
            primary, "applyOtaArtifact", {"host": host, "port": port}
        )
        if not accepted.get("success"):
            raise RuntimeError(f"RP2350W rejected OTA artifact: {accepted}")
        await primary.close()
        primary = None
        await asyncio.sleep(min(8.0, rpc_timeout()))

        primary = RpcClient(
            upload_port,
            timeout=rpc_timeout(),
            serial_interface=create_serial_interface(upload_port),
        )
        await primary.connect(boot_wait=8.0, drain_boot=True)
        await rpc_data(primary, "ping")
        served = await rpc_data(peer, "otaArtifactStatus")
        if not served.get("success") or _served_request_count(served) < 1:
            raise RuntimeError(f"C6 did not serve the RP2350W artifact: {served}")
        print(f"{Fore.GREEN}OTA PEER AUTORESEARCH PASSED{Style.RESET_ALL}")
        return 0
    except KeyboardInterrupt as ki:
        handle_keyboard_interrupt(ki)
        return 130
    except (RpcTimeoutError, RuntimeError, OSError) as error:
        print(f"{Fore.RED}OTA peer autoresearch failed: {error}{Style.RESET_ALL}")
        return 1
    finally:
        for client, stop_method in ((primary, "stopNet"), (peer, "stopNet")):
            if client is None:
                continue
            try:
                await client.send(stop_method, timeout=2.0)
            except KeyboardInterrupt as ki:
                handle_keyboard_interrupt(ki)
            except Exception:
                pass
            await client.close()


async def run_ota_autoresearch(
    upload_port: str,
    serial_iface: "SerialInterface | None",
    timeout: float = 60.0,
    firmware_path: Path | None = None,
) -> int:
    """Run OTA autoresearch (--ota).

    1. Send startOta RPC to ESP32 (starts WiFi AP + OTA HTTP server)
    2. Connect host to ESP32's WiFi AP
    3. Run HTTP tests against OTA endpoints with auth checks
    4. (Optional) Flash firmware via OTA upload and verify reboot
    5. Send stopOta RPC to cleanup
    6. Restore host WiFi

    Args:
        upload_port: Serial port for RPC communication
        serial_iface: Pre-created serial interface
        timeout: RPC timeout in seconds
        firmware_path: Path to firmware.bin for OTA flash test (None = skip)

    Returns:
        Exit code (0 = success, 1 = failure)
    """
    wifi = create_wifi_manager()

    print()
    print("=" * 60)
    print("OTA AUTORESEARCH MODE")
    print("=" * 60)
    print()

    # Save current WiFi SSID for restore
    original_ssid = wifi.get_current_ssid()
    if original_ssid:
        print(f"  Current WiFi: '{original_ssid}' (will restore after test)")
    else:
        print("  No current WiFi connection detected")

    client: RpcClient | None = None

    try:
        # Connect to device via RPC
        print(f"\n  Connecting to device on {upload_port}...")
        client = RpcClient(upload_port, timeout=timeout, serial_interface=serial_iface)
        await client.connect(boot_wait=3.0, drain_boot=True)
        print(f"  {Fore.GREEN}Connected to device{Style.RESET_ALL}")

        # Step 1: Start OTA on device
        print("\n--- Step 1: Start WiFi AP + OTA Server on ESP32 ---")
        response = await client.send("startOta", timeout=30.0)
        ota_info = response.data

        if not isinstance(ota_info, dict) or not ota_info.get("success"):
            error = (
                ota_info.get("error", "Unknown error")
                if isinstance(ota_info, dict)
                else str(ota_info)
            )
            print(f"  {Fore.RED}Failed to start OTA: {error}{Style.RESET_ALL}")
            return 1

        ssid = ota_info.get("ssid", "")
        password = ota_info.get("password", "")
        ip = ota_info.get("ip", "192.168.4.1")
        port = ota_info.get("port", 80)
        ota_password = ota_info.get("ota_password", "")
        hostname = ota_info.get("hostname", "")
        print(
            f"  {Fore.GREEN}OTA started: SSID={ssid}, IP={ip}:{port}, hostname={hostname}{Style.RESET_ALL}"
        )

        # Step 2: Connect host to ESP32 WiFi AP
        print("\n--- Step 2: Connect Host to ESP32 WiFi AP ---")
        if not wifi.connect(ssid, password):
            print(f"  {Fore.RED}Failed to connect host to ESP32 AP{Style.RESET_ALL}")
            return 1

        # Step 3: Run HTTP tests
        print(f"\n--- Step 3: Validate OTA HTTP Endpoints on {ip}:{port} ---")
        base_url = f"http://{ip}:{port}"
        tests_passed = 0
        tests_failed = 0

        # Build Basic Auth header
        auth_value = base64.b64encode(f"admin:{ota_password}".encode()).decode()
        auth_headers = {"Authorization": f"Basic {auth_value}"}
        bad_auth_value = base64.b64encode(b"admin:wrongpassword").decode()
        bad_auth_headers = {"Authorization": f"Basic {bad_auth_value}"}

        # Test 1: GET / with valid auth -> 200 + HTML
        print("\n  Test 1: GET / (valid auth)")
        try:
            r = httpx.get(
                f"{base_url}/",
                headers=auth_headers,
                timeout=10.0,
                follow_redirects=True,
            )
            if r.status_code == 200:
                print(
                    f"    {Fore.GREEN}PASS{Style.RESET_ALL} - status={r.status_code}, content_length={len(r.content)}"
                )
                tests_passed += 1
            else:
                print(
                    f"    {Fore.RED}FAIL{Style.RESET_ALL} - status={r.status_code} (expected 200)"
                )
                tests_failed += 1
        except httpx.HTTPError as e:
            print(f"    {Fore.RED}FAIL{Style.RESET_ALL} - {e}")
            tests_failed += 1

        # Test 2: GET / without auth -> 401
        print("\n  Test 2: GET / (no auth)")
        try:
            r = httpx.get(f"{base_url}/", timeout=10.0, follow_redirects=True)
            if r.status_code == 401:
                print(
                    f"    {Fore.GREEN}PASS{Style.RESET_ALL} - status={r.status_code} (correctly rejected)"
                )
                tests_passed += 1
            else:
                print(
                    f"    {Fore.RED}FAIL{Style.RESET_ALL} - status={r.status_code} (expected 401)"
                )
                tests_failed += 1
        except httpx.HTTPError as e:
            print(f"    {Fore.RED}FAIL{Style.RESET_ALL} - {e}")
            tests_failed += 1

        # Test 3: GET / with wrong password -> 401
        print("\n  Test 3: GET / (wrong password)")
        try:
            r = httpx.get(
                f"{base_url}/",
                headers=bad_auth_headers,
                timeout=10.0,
                follow_redirects=True,
            )
            if r.status_code == 401:
                print(
                    f"    {Fore.GREEN}PASS{Style.RESET_ALL} - status={r.status_code} (correctly rejected)"
                )
                tests_passed += 1
            else:
                print(
                    f"    {Fore.RED}FAIL{Style.RESET_ALL} - status={r.status_code} (expected 401)"
                )
                tests_failed += 1
        except httpx.HTTPError as e:
            print(f"    {Fore.RED}FAIL{Style.RESET_ALL} - {e}")
            tests_failed += 1

        # Test 4: POST /update with auth, no body -> error response (endpoint exists)
        print("\n  Test 4: POST /update (auth, no body)")
        try:
            r = httpx.post(f"{base_url}/update", headers=auth_headers, timeout=10.0)
            # We expect an error response (400 or 500) because no firmware data was sent,
            # but the key check is that the endpoint exists (not 404)
            if r.status_code != 404:
                print(
                    f"    {Fore.GREEN}PASS{Style.RESET_ALL} - status={r.status_code} (endpoint exists)"
                )
                tests_passed += 1
            else:
                print(
                    f"    {Fore.RED}FAIL{Style.RESET_ALL} - status=404 (endpoint not found)"
                )
                tests_failed += 1
        except httpx.HTTPError as e:
            print(f"    {Fore.RED}FAIL{Style.RESET_ALL} - {e}")
            tests_failed += 1

        # Test 5: POST /update with auth, invalid firmware (4 zero bytes) -> error
        print("\n  Test 5: POST /update (auth, invalid firmware data)")
        try:
            r = httpx.post(
                f"{base_url}/update",
                headers=auth_headers,
                content=b"\x00\x00\x00\x00",
                timeout=10.0,
            )
            # We expect an error response (bad firmware header), but not 404
            if r.status_code != 404:
                print(
                    f"    {Fore.GREEN}PASS{Style.RESET_ALL} - status={r.status_code} (handled invalid firmware)"
                )
                tests_passed += 1
            else:
                print(
                    f"    {Fore.RED}FAIL{Style.RESET_ALL} - status=404 (endpoint not found)"
                )
                tests_failed += 1
        except httpx.HTTPError as e:
            print(f"    {Fore.RED}FAIL{Style.RESET_ALL} - {e}")
            tests_failed += 1

        # Test 6: OTA Firmware Upload + Reboot Verification
        if firmware_path is None:
            print(
                "\n  Test 6: OTA firmware flash (SKIPPED - no firmware path provided)"
            )
        elif not firmware_path.exists():
            print(
                f"\n  Test 6: OTA firmware flash (SKIPPED - {firmware_path} not found)"
            )
        else:
            print("\n  Test 6: OTA firmware flash + reboot verification")
            try:
                firmware_data = firmware_path.read_bytes()
                firmware_size = len(firmware_data)
                print(
                    f"    Uploading firmware ({firmware_size} bytes) via POST /update..."
                )

                # Upload firmware with longer timeout (flash can take a while)
                r = httpx.post(
                    f"{base_url}/update",
                    headers=auth_headers,
                    content=firmware_data,
                    timeout=60.0,
                )

                if r.status_code != 200:
                    print(
                        f"    {Fore.RED}FAIL{Style.RESET_ALL} - upload returned status={r.status_code}, body={r.text!r}"
                    )
                    tests_failed += 1
                else:
                    print(
                        f"    Upload succeeded (status={r.status_code}, body={r.text!r})"
                    )

                    # Device will reboot after ~1s. Close the current RPC client
                    # since serial connection will drop.
                    await client.close()
                    client = None

                    # Restore WiFi back to original network - the ESP32 AP is gone
                    # after reboot, so we need our normal network for nothing but
                    # we need to drop the AP connection to avoid network issues.
                    wifi.restore(original_ssid)

                    # Wait for serial port to reappear after reboot. Use
                    # OS-level port enumeration (never a raw pyserial open
                    # of the device — see the PYS001 ban).
                    print("    Waiting for device to reboot...")
                    from ci.util.port_utils import port_exists

                    port_ready = False
                    max_wait = 20.0
                    start_time = time.time()

                    while time.time() - start_time < max_wait:
                        if port_exists(upload_port):
                            port_ready = True
                            elapsed = time.time() - start_time
                            print(f"    Serial port available after {elapsed:.1f}s")
                            break
                        await asyncio.sleep(0.5)

                    if not port_ready:
                        print(
                            f"    {Fore.RED}FAIL{Style.RESET_ALL} - serial port did not reappear after {max_wait}s"
                        )
                        tests_failed += 1
                    else:
                        # Reconnect via RPC and verify device is alive
                        print("    Reconnecting via RPC...")
                        client = RpcClient(
                            upload_port,
                            timeout=timeout,
                            serial_interface=serial_iface,
                        )
                        await client.connect(boot_wait=3.0, drain_boot=True)

                        status_resp = await client.send("status", timeout=10.0)
                        status_data = status_resp.data

                        if isinstance(status_data, dict) and status_data.get("ready"):
                            print(
                                f"    {Fore.GREEN}PASS{Style.RESET_ALL} - device rebooted and responding (status={status_data})"
                            )
                            tests_passed += 1
                        else:
                            print(
                                f"    {Fore.RED}FAIL{Style.RESET_ALL} - unexpected status response: {status_data}"
                            )
                            tests_failed += 1

            except KeyboardInterrupt as ki:
                handle_keyboard_interrupt(ki)
                raise
            except httpx.HTTPError as e:
                print(f"    {Fore.RED}FAIL{Style.RESET_ALL} - HTTP error: {e}")
                tests_failed += 1
            except RpcTimeoutError:
                print(f"    {Fore.RED}FAIL{Style.RESET_ALL} - RPC timeout after reboot")
                tests_failed += 1
            except Exception as e:
                print(f"    {Fore.RED}FAIL{Style.RESET_ALL} - {e}")
                tests_failed += 1

        # Summary
        total = tests_passed + tests_failed
        print()
        print("=" * 60)
        if tests_failed == 0:
            print(
                f"{Fore.GREEN}OTA AUTORESEARCH PASSED ({tests_passed}/{total} tests){Style.RESET_ALL}"
            )
            return 0
        else:
            print(
                f"{Fore.RED}OTA AUTORESEARCH FAILED ({tests_passed}/{total} passed, {tests_failed} failed){Style.RESET_ALL}"
            )
            return 1

    except KeyboardInterrupt as ki:
        print("\n\n  Interrupted by user")
        handle_keyboard_interrupt(ki)
        return 130
    except RpcTimeoutError:
        print(f"\n  {Fore.RED}Timeout waiting for OTA response{Style.RESET_ALL}")
        return 1
    except Exception as e:
        print(f"\n  {Fore.RED}OTA autoresearch error: {e}{Style.RESET_ALL}")
        return 1
    finally:
        # Cleanup: stop OTA on device (only if client is still connected
        # and we haven't already rebooted via OTA flash)
        if client:
            try:
                await client.send("stopOta", timeout=10.0)
            except KeyboardInterrupt as ki:
                handle_keyboard_interrupt(ki)
            except Exception:
                pass
            await client.close()
        # Restore original WiFi
        wifi.restore(original_ssid)
