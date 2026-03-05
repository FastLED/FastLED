"""Network validation helpers for FastLED hardware-in-the-loop testing.

Provides WiFi management, HTTP server, and validation flows for
--net-server and --net-client modes.
"""

import os
import platform as platform_mod
import subprocess
import tempfile
import time
from abc import ABC, abstractmethod
from typing import TYPE_CHECKING, Any

from colorama import Fore, Style

from ci.rpc_client import RpcClient, RpcTimeoutError
from ci.util.global_interrupt_handler import handle_keyboard_interrupt_properly


if TYPE_CHECKING:
    from ci.util.serial_interface import SerialInterface

# WiFi AP credentials (must match firmware constants)
NET_SSID = "FastLED-Validation"
NET_PASSWORD = "fastled123"
NET_AP_IP = "192.168.4.1"
NET_SERVER_PORT = 80
NET_CLIENT_PORT = 8080


class HostWifiManager(ABC):
    """Abstract base for platform-specific WiFi operations."""

    @property
    @abstractmethod
    def platform_name(self) -> str:
        """Human-readable platform name (for log messages)."""

    @abstractmethod
    def get_current_ssid(self) -> str | None:
        """Return the SSID the host is currently connected to, or None."""

    @abstractmethod
    def _run_connect(
        self, ssid: str, password: str
    ) -> subprocess.CompletedProcess[str]:
        """Run the platform-specific connect command. Subclasses implement this."""

    @abstractmethod
    def _run_restore(self, ssid: str) -> None:
        """Run the platform-specific restore command. Subclasses implement this."""

    # -- public helpers (shared logic) --

    def connect(self, ssid: str, password: str) -> bool:
        """Connect the host machine to a WiFi network."""
        print(f"  Connecting host to WiFi '{ssid}' ({self.platform_name})...")
        try:
            result = self._run_connect(ssid, password)
            if result.returncode != 0:
                print(
                    f"  {Fore.RED}Failed to connect: {result.stderr}{Style.RESET_ALL}"
                )
                return False
            print("  Waiting for WiFi connection to establish...")
            time.sleep(5)
            print(f"  {Fore.GREEN}Connected to '{ssid}'{Style.RESET_ALL}")
            return True
        except (subprocess.TimeoutExpired, FileNotFoundError, OSError) as e:
            print(f"  {Fore.RED}WiFi connection error: {e}{Style.RESET_ALL}")
            return False

    def restore(self, original_ssid: str | None) -> None:
        """Restore the host's original WiFi connection."""
        if not original_ssid:
            print("  No previous WiFi to restore.")
            return
        print(f"  Restoring WiFi to '{original_ssid}'...")
        try:
            self._run_restore(original_ssid)
            print(f"  WiFi restored to '{original_ssid}'")
        except (subprocess.TimeoutExpired, FileNotFoundError, OSError) as e:
            print(
                f"  {Fore.YELLOW}Warning: Failed to restore WiFi: {e}{Style.RESET_ALL}"
            )


class _WindowsWifiManager(HostWifiManager):
    @property
    def platform_name(self) -> str:
        return "windows"

    def get_current_ssid(self) -> str | None:
        try:
            result = subprocess.run(
                ["netsh", "wlan", "show", "interfaces"],
                capture_output=True,
                text=True,
                timeout=10,
            )
            if result.returncode == 0:
                for line in result.stdout.splitlines():
                    line = line.strip()
                    if line.startswith("SSID") and "BSSID" not in line:
                        parts = line.split(":", 1)
                        if len(parts) == 2:
                            ssid = parts[1].strip()
                            if ssid:
                                return ssid
        except (subprocess.TimeoutExpired, FileNotFoundError, OSError):
            pass
        return None

    def _run_connect(
        self, ssid: str, password: str
    ) -> subprocess.CompletedProcess[str]:
        # First try connecting if profile already exists
        result = subprocess.run(
            ["netsh", "wlan", "connect", f"ssid={ssid}", f"name={ssid}"],
            capture_output=True,
            text=True,
            timeout=15,
        )
        if result.returncode == 0:
            return result

        # Profile doesn't exist — create a temporary XML profile
        profile_xml = f"""<?xml version="1.0"?>
<WLANProfile xmlns="http://www.microsoft.com/networking/WLAN/profile/v1">
    <name>{ssid}</name>
    <SSIDConfig>
        <SSID>
            <name>{ssid}</name>
        </SSID>
    </SSIDConfig>
    <connectionType>ESS</connectionType>
    <connectionMode>manual</connectionMode>
    <MSM>
        <security>
            <authEncryption>
                <authentication>WPA2PSK</authentication>
                <encryption>AES</encryption>
                <useOneX>false</useOneX>
            </authEncryption>
            <sharedKey>
                <keyType>passPhrase</keyType>
                <protected>false</protected>
                <keyMaterial>{password}</keyMaterial>
            </sharedKey>
        </security>
    </MSM>
</WLANProfile>"""
        with tempfile.NamedTemporaryFile(mode="w", suffix=".xml", delete=False) as f:
            f.write(profile_xml)
            profile_path = f.name

        try:
            subprocess.run(
                ["netsh", "wlan", "add", "profile", f"filename={profile_path}"],
                capture_output=True,
                text=True,
                timeout=10,
            )
            result = subprocess.run(
                ["netsh", "wlan", "connect", f"ssid={ssid}", f"name={ssid}"],
                capture_output=True,
                text=True,
                timeout=15,
            )
        finally:
            os.unlink(profile_path)
        return result

    def _run_restore(self, ssid: str) -> None:
        subprocess.run(
            ["netsh", "wlan", "connect", f"ssid={ssid}", f"name={ssid}"],
            capture_output=True,
            text=True,
            timeout=15,
        )


class _MacOSWifiManager(HostWifiManager):
    @property
    def platform_name(self) -> str:
        return "macos"

    def get_current_ssid(self) -> str | None:
        try:
            result = subprocess.run(
                [
                    "/System/Library/PrivateFrameworks/Apple80211.framework/Versions/Current/Resources/airport",
                    "-I",
                ],
                capture_output=True,
                text=True,
                timeout=10,
            )
            if result.returncode == 0:
                for line in result.stdout.splitlines():
                    line = line.strip()
                    if line.startswith("SSID:"):
                        return line.split(":", 1)[1].strip()
        except (subprocess.TimeoutExpired, FileNotFoundError, OSError):
            pass
        return None

    def _run_connect(
        self, ssid: str, password: str
    ) -> subprocess.CompletedProcess[str]:
        return subprocess.run(
            ["networksetup", "-setairportnetwork", "en0", ssid, password],
            capture_output=True,
            text=True,
            timeout=30,
        )

    def _run_restore(self, ssid: str) -> None:
        subprocess.run(
            ["networksetup", "-setairportnetwork", "en0", ssid],
            capture_output=True,
            text=True,
            timeout=30,
        )


class _LinuxWifiManager(HostWifiManager):
    @property
    def platform_name(self) -> str:
        return "linux"

    def get_current_ssid(self) -> str | None:
        try:
            result = subprocess.run(
                ["nmcli", "-t", "-f", "active,ssid", "dev", "wifi"],
                capture_output=True,
                text=True,
                timeout=10,
            )
            if result.returncode == 0:
                for line in result.stdout.splitlines():
                    if line.startswith("yes:"):
                        return line.split(":", 1)[1]
        except (subprocess.TimeoutExpired, FileNotFoundError, OSError):
            pass
        return None

    def _run_connect(
        self, ssid: str, password: str
    ) -> subprocess.CompletedProcess[str]:
        return subprocess.run(
            ["nmcli", "device", "wifi", "connect", ssid, "password", password],
            capture_output=True,
            text=True,
            timeout=30,
        )

    def _run_restore(self, ssid: str) -> None:
        subprocess.run(
            ["nmcli", "device", "wifi", "connect", ssid],
            capture_output=True,
            text=True,
            timeout=30,
        )


def create_wifi_manager() -> HostWifiManager:
    """Factory — returns the right HostWifiManager for the current OS."""
    system = platform_mod.system().lower()
    if system == "windows" or "msys" in system or "mingw" in system:
        return _WindowsWifiManager()
    elif system == "darwin":
        return _MacOSWifiManager()
    else:
        return _LinuxWifiManager()


class _HostHttpServer:
    """Simple HTTP server for net-client validation.

    Runs in a background thread and provides test endpoints.
    """

    def __init__(self, port: int = NET_CLIENT_PORT) -> None:
        from http.server import BaseHTTPRequestHandler, HTTPServer

        self.port = port
        self._server: HTTPServer | None = None
        self._thread: Any = None

        class TestHandler(BaseHTTPRequestHandler):
            def do_GET(self) -> None:
                if self.path == "/ping":
                    self.send_response(200)
                    self.send_header("Content-Type", "text/plain")
                    self.end_headers()
                    self.wfile.write(b"pong")
                elif self.path == "/data":
                    self.send_response(200)
                    self.send_header("Content-Type", "application/json")
                    self.end_headers()
                    self.wfile.write(b'{"value": 42}')
                else:
                    self.send_response(404)
                    self.end_headers()

            def log_message(self, format: str, *log_args: Any) -> None:
                # Suppress default request logging
                pass

        self._handler_class = TestHandler

    def start(self) -> None:
        import threading
        from http.server import HTTPServer

        self._server = HTTPServer(("0.0.0.0", self.port), self._handler_class)
        self._thread = threading.Thread(target=self._server.serve_forever, daemon=True)
        self._thread.start()
        print(f"  Host HTTP server started on port {self.port}")

    def stop(self) -> None:
        if self._server:
            self._server.shutdown()
            self._server = None
        self._thread = None
        print("  Host HTTP server stopped")


async def run_net_loopback_validation(
    upload_port: str,
    serial_iface: "SerialInterface | None",
    timeout: float = 60.0,
) -> int:
    """Run self-contained loopback network validation (--net).

    The ESP32 starts an HTTP server and uses its own HTTP client to fetch
    from 127.0.0.1 (localhost). No WiFi, no host involvement.

    Args:
        upload_port: Serial port for RPC communication
        serial_iface: Pre-created serial interface
        timeout: RPC timeout in seconds

    Returns:
        Exit code (0 = success, 1 = failure)
    """
    print()
    print("=" * 60)
    print("NETWORK VALIDATION MODE: LOOPBACK (self-contained)")
    print("=" * 60)
    print()

    client: RpcClient | None = None

    try:
        # Connect to device via RPC
        print(f"  Connecting to device on {upload_port}...")
        client = RpcClient(upload_port, timeout=timeout, serial_interface=serial_iface)
        await client.connect(boot_wait=3.0, drain_boot=True)
        print(f"  {Fore.GREEN}Connected to device{Style.RESET_ALL}")

        # Send the loopback test command
        print("\n--- Running loopback HTTP test on ESP32 ---")
        print("  ESP32 will start HTTP server and GET its own endpoints via 127.0.0.1")
        print()

        response = await client.send("runNetLoopback", timeout=30.0)
        test_result = response.data

        if not isinstance(test_result, dict):
            print(f"  {Fore.RED}Unexpected response: {test_result}{Style.RESET_ALL}")
            return 1

        # Display results
        tests_passed = test_result.get("tests_passed", 0)
        tests_failed = test_result.get("tests_failed", 0)
        total = tests_passed + tests_failed
        results = test_result.get("results", [])

        if test_result.get("error"):
            print(f"  {Fore.RED}Error: {test_result['error']}{Style.RESET_ALL}")
            return 1

        for r in results:
            test_name = r.get("test", "?")
            passed = r.get("passed", False)
            status = (
                f"{Fore.GREEN}PASS{Style.RESET_ALL}"
                if passed
                else f"{Fore.RED}FAIL{Style.RESET_ALL}"
            )
            error = r.get("error", "")
            status_code = r.get("status_code", "")
            detail = f" (status={status_code})" if status_code else ""
            error_str = f" - {error}" if error else ""
            print(f"  {test_name}: {status}{detail}{error_str}")

        # Summary
        print()
        print("=" * 60)
        if test_result.get("success"):
            print(
                f"{Fore.GREEN}NET LOOPBACK VALIDATION PASSED ({tests_passed}/{total} tests){Style.RESET_ALL}"
            )
            return 0
        else:
            print(
                f"{Fore.RED}NET LOOPBACK VALIDATION FAILED ({tests_passed}/{total} passed, {tests_failed} failed){Style.RESET_ALL}"
            )
            return 1

    except KeyboardInterrupt:
        print("\n\n  Interrupted by user")
        handle_keyboard_interrupt_properly()
        return 130
    except RpcTimeoutError:
        print(
            f"\n  {Fore.RED}Timeout waiting for loopback test response{Style.RESET_ALL}"
        )
        return 1
    except Exception as e:
        print(f"\n  {Fore.RED}Network loopback validation error: {e}{Style.RESET_ALL}")
        return 1
    finally:
        if client:
            try:
                await client.send("stopNet", timeout=10.0)
            except KeyboardInterrupt:
                handle_keyboard_interrupt_properly()
            except Exception:
                pass
            await client.close()


async def run_net_validation(
    upload_port: str,
    serial_iface: "SerialInterface | None",
    net_server_mode: bool,
    net_client_mode: bool,
    timeout: float = 60.0,
) -> int:
    """Run network validation (--net-server or --net-client).

    Args:
        upload_port: Serial port for RPC communication
        serial_iface: Pre-created serial interface
        net_server_mode: True if running --net-server validation
        net_client_mode: True if running --net-client validation
        timeout: RPC timeout in seconds

    Returns:
        Exit code (0 = success, 1 = failure)
    """
    mode_name = "NET-SERVER" if net_server_mode else "NET-CLIENT"
    wifi = create_wifi_manager()

    print()
    print("=" * 60)
    print(f"NETWORK VALIDATION MODE: {mode_name}")
    print("=" * 60)
    print()

    # Save current WiFi SSID for restore
    original_ssid = wifi.get_current_ssid()
    if original_ssid:
        print(f"  Current WiFi: '{original_ssid}' (will restore after test)")
    else:
        print("  No current WiFi connection detected")

    client: RpcClient | None = None
    host_server: _HostHttpServer | None = None

    try:
        # Connect to device via RPC
        print(f"\n  Connecting to device on {upload_port}...")
        client = RpcClient(upload_port, timeout=timeout, serial_interface=serial_iface)
        await client.connect(boot_wait=3.0, drain_boot=True)
        print(f"  {Fore.GREEN}Connected to device{Style.RESET_ALL}")

        if net_server_mode:
            return await _run_net_server_validation(client, wifi)
        else:
            return await _run_net_client_validation(client, wifi)

    except KeyboardInterrupt:
        print("\n\n  Interrupted by user")
        handle_keyboard_interrupt_properly()
        return 130
    except Exception as e:
        print(f"\n  {Fore.RED}Network validation error: {e}{Style.RESET_ALL}")
        return 1
    finally:
        # Cleanup
        if host_server:
            host_server.stop()
        if client:
            # Send stopNet to clean up device resources
            try:
                await client.send("stopNet", timeout=10.0)
            except KeyboardInterrupt:
                handle_keyboard_interrupt_properly()
            except Exception:
                pass
            await client.close()
        # Restore original WiFi
        wifi.restore(original_ssid)


async def _run_net_server_validation(
    client: RpcClient,
    wifi: HostWifiManager,
) -> int:
    """Run --net-server validation flow.

    1. Send startNetServer RPC to ESP32
    2. Connect host to ESP32's WiFi AP
    3. Make HTTP requests to ESP32 and validate responses
    4. Cleanup
    """
    import httpx

    print("\n--- Step 1: Start WiFi AP + HTTP Server on ESP32 ---")
    response = await client.send("startNetServer", timeout=30.0)
    server_info = response.data

    if not isinstance(server_info, dict) or not server_info.get("success"):
        error = (
            server_info.get("error", "Unknown error")
            if isinstance(server_info, dict)
            else str(server_info)
        )
        print(f"  {Fore.RED}Failed to start net server: {error}{Style.RESET_ALL}")
        return 1

    ssid = server_info.get("ssid", NET_SSID)
    password = server_info.get("password", NET_PASSWORD)
    ip = server_info.get("ip", NET_AP_IP)
    port = server_info.get("port", NET_SERVER_PORT)
    print(
        f"  {Fore.GREEN}ESP32 WiFi AP started: SSID={ssid}, IP={ip}:{port}{Style.RESET_ALL}"
    )

    print("\n--- Step 2: Connect Host to ESP32 WiFi AP ---")
    if not wifi.connect(ssid, password):
        print(f"  {Fore.RED}Failed to connect host to ESP32 AP{Style.RESET_ALL}")
        return 1

    print(f"\n--- Step 3: Validate HTTP Endpoints on {ip}:{port} ---")
    base_url = f"http://{ip}:{port}"
    tests_passed = 0
    tests_failed = 0

    # Test 1: GET /ping
    print(f"\n  Test 1: GET /ping")
    try:
        r = httpx.get(f"{base_url}/ping", timeout=10.0)
        if r.status_code == 200 and r.text == "pong":
            print(
                f"    {Fore.GREEN}PASS{Style.RESET_ALL} - status={r.status_code}, body='{r.text}'"
            )
            tests_passed += 1
        else:
            print(
                f"    {Fore.RED}FAIL{Style.RESET_ALL} - status={r.status_code}, body='{r.text}' (expected 200, 'pong')"
            )
            tests_failed += 1
    except httpx.HTTPError as e:
        print(f"    {Fore.RED}FAIL{Style.RESET_ALL} - {e}")
        tests_failed += 1

    # Test 2: GET /status
    print(f"\n  Test 2: GET /status")
    try:
        r = httpx.get(f"{base_url}/status", timeout=10.0)
        if r.status_code == 200:
            data = r.json()
            has_uptime = "uptime_ms" in data
            has_heap = "free_heap" in data
            has_chip = "chip" in data
            if has_uptime and has_heap and has_chip:
                print(
                    f"    {Fore.GREEN}PASS{Style.RESET_ALL} - chip={data['chip']}, uptime={data['uptime_ms']}ms, heap={data['free_heap']}"
                )
                tests_passed += 1
            else:
                print(
                    f"    {Fore.RED}FAIL{Style.RESET_ALL} - missing fields: uptime={has_uptime}, heap={has_heap}, chip={has_chip}"
                )
                tests_failed += 1
        else:
            print(f"    {Fore.RED}FAIL{Style.RESET_ALL} - status={r.status_code}")
            tests_failed += 1
    except httpx.HTTPError as e:
        print(f"    {Fore.RED}FAIL{Style.RESET_ALL} - {e}")
        tests_failed += 1

    # Test 3: POST /echo
    print(f"\n  Test 3: POST /echo")
    echo_body = "hello from validation"
    try:
        r = httpx.post(f"{base_url}/echo", content=echo_body, timeout=10.0)
        if r.status_code == 200 and r.text == echo_body:
            print(f"    {Fore.GREEN}PASS{Style.RESET_ALL} - echoed correctly")
            tests_passed += 1
        else:
            print(
                f"    {Fore.RED}FAIL{Style.RESET_ALL} - status={r.status_code}, body='{r.text}' (expected '{echo_body}')"
            )
            tests_failed += 1
    except httpx.HTTPError as e:
        print(f"    {Fore.RED}FAIL{Style.RESET_ALL} - {e}")
        tests_failed += 1

    # Test 4: GET /leds
    print(f"\n  Test 4: GET /leds")
    try:
        r = httpx.get(f"{base_url}/leds", timeout=10.0)
        if r.status_code == 200:
            data = r.json()
            has_num = "num_leds" in data
            has_bright = "brightness" in data
            if has_num and has_bright:
                print(
                    f"    {Fore.GREEN}PASS{Style.RESET_ALL} - num_leds={data['num_leds']}, brightness={data['brightness']}"
                )
                tests_passed += 1
            else:
                print(
                    f"    {Fore.RED}FAIL{Style.RESET_ALL} - missing fields: num_leds={has_num}, brightness={has_bright}"
                )
                tests_failed += 1
        else:
            print(f"    {Fore.RED}FAIL{Style.RESET_ALL} - status={r.status_code}")
            tests_failed += 1
    except httpx.HTTPError as e:
        print(f"    {Fore.RED}FAIL{Style.RESET_ALL} - {e}")
        tests_failed += 1

    # Summary
    total = tests_passed + tests_failed
    print()
    print("=" * 60)
    if tests_failed == 0:
        print(
            f"{Fore.GREEN}NET-SERVER VALIDATION PASSED ({tests_passed}/{total} tests){Style.RESET_ALL}"
        )
        return 0
    else:
        print(
            f"{Fore.RED}NET-SERVER VALIDATION FAILED ({tests_passed}/{total} passed, {tests_failed} failed){Style.RESET_ALL}"
        )
        return 1


async def _run_net_client_validation(
    client: RpcClient,
    wifi: HostWifiManager,
) -> int:
    """Run --net-client validation flow.

    1. Send startNetClient RPC to ESP32 (starts WiFi AP)
    2. Connect host to ESP32's WiFi AP
    3. Start host HTTP server
    4. Send runNetClientTest RPC to ESP32
    5. Validate results
    6. Cleanup
    """
    print("\n--- Step 1: Start WiFi AP on ESP32 ---")
    response = await client.send("startNetClient", timeout=30.0)
    ap_info = response.data

    if not isinstance(ap_info, dict) or not ap_info.get("success"):
        error = (
            ap_info.get("error", "Unknown error")
            if isinstance(ap_info, dict)
            else str(ap_info)
        )
        print(f"  {Fore.RED}Failed to start WiFi AP: {error}{Style.RESET_ALL}")
        return 1

    ssid = ap_info.get("ssid", NET_SSID)
    password = ap_info.get("password", NET_PASSWORD)
    gateway_ip = ap_info.get("gateway_ip", NET_AP_IP)
    print(
        f"  {Fore.GREEN}ESP32 WiFi AP started: SSID={ssid}, gateway={gateway_ip}{Style.RESET_ALL}"
    )

    print("\n--- Step 2: Connect Host to ESP32 WiFi AP ---")
    if not wifi.connect(ssid, password):
        print(f"  {Fore.RED}Failed to connect host to ESP32 AP{Style.RESET_ALL}")
        return 1

    print("\n--- Step 3: Start Host HTTP Server ---")
    host_server = _HostHttpServer(port=NET_CLIENT_PORT)
    host_server.start()

    # The host IP on the ESP32's AP network is typically 192.168.4.2
    # (first DHCP client connected to the AP)
    host_ip = "192.168.4.2"
    print(f"  Host server at http://{host_ip}:{NET_CLIENT_PORT}")

    print("\n--- Step 4: ESP32 Fetches from Host ---")
    try:
        response = await client.send(
            "runNetClientTest",
            args=[{"host_ip": host_ip, "port": NET_CLIENT_PORT}],
            timeout=30.0,
        )
        test_result = response.data
    except RpcTimeoutError:
        print(f"  {Fore.RED}Timeout waiting for ESP32 client tests{Style.RESET_ALL}")
        host_server.stop()
        return 1

    host_server.stop()

    if not isinstance(test_result, dict):
        print(f"  {Fore.RED}Unexpected response: {test_result}{Style.RESET_ALL}")
        return 1

    # Display results
    tests_passed = test_result.get("tests_passed", 0)
    tests_failed = test_result.get("tests_failed", 0)
    total = tests_passed + tests_failed
    results = test_result.get("results", [])

    print(f"\n  Results from ESP32 HTTP client:")
    for r in results:
        test_name = r.get("test", "?")
        passed = r.get("passed", False)
        status = (
            f"{Fore.GREEN}PASS{Style.RESET_ALL}"
            if passed
            else f"{Fore.RED}FAIL{Style.RESET_ALL}"
        )
        error = r.get("error", "")
        error_str = f" - {error}" if error else ""
        print(f"    {test_name}: {status}{error_str}")

    # Summary
    print()
    print("=" * 60)
    if test_result.get("success"):
        print(
            f"{Fore.GREEN}NET-CLIENT VALIDATION PASSED ({tests_passed}/{total} tests){Style.RESET_ALL}"
        )
        return 0
    else:
        print(
            f"{Fore.RED}NET-CLIENT VALIDATION FAILED ({tests_passed}/{total} passed, {tests_failed} failed){Style.RESET_ALL}"
        )
        return 1
