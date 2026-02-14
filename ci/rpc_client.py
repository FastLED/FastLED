"""
Reusable JSON-RPC client for serial communication with embedded devices.

This module provides a stateful RPC client that handles:
- Serial connection management via fbuild daemon (eliminates port lock conflicts)
- Or direct pyserial connection (when --no-fbuild is specified)
- JSON-RPC command serialization and response parsing
- Timeout and retry logic
- Proper KeyboardInterrupt handling
- Boot output draining

Usage:
    with RpcClient("/dev/ttyUSB0") as client:
        response = client.send("ping")
        print(response)  # {"timestamp": ..., "uptimeMs": ...}

    # With pyserial (no fbuild daemon):
    with RpcClient("/dev/ttyUSB0", use_pyserial=True) as client:
        response = client.send("ping")

    # Or with explicit connection management:
    client = RpcClient("/dev/ttyUSB0")
    client.connect()
    client.drain_boot_output()
    response = client.send("testGpioConnection", [16, 17])
    client.close()
"""

from __future__ import annotations

import json
import time
from dataclasses import dataclass
from typing import TYPE_CHECKING, Any

from ci.util.global_interrupt_handler import is_interrupted, notify_main_thread


if TYPE_CHECKING:
    from fbuild.api import SerialMonitor as FbuildSerialMonitorType

    from ci.util.pyserial_monitor import SerialMonitor as PySerialMonitorType

    SerialMonitorType = PySerialMonitorType | FbuildSerialMonitorType


@dataclass
class RpcResponse:
    """Structured RPC response."""

    success: bool
    data: dict[str, Any]
    raw_line: str

    def get(self, key: str, default: Any = None) -> Any:
        """Get value from response data."""
        return self.data.get(key, default)

    def __getitem__(self, key: str) -> Any:
        """Allow dict-like access to response data."""
        return self.data[key]

    def __contains__(self, key: str) -> bool:
        """Check if key exists in response data."""
        return key in self.data


class RpcTimeoutError(TimeoutError):
    """RPC operation timed out."""

    pass


class RpcError(Exception):
    """RPC operation failed."""

    pass


class RpcClient:
    """Stateful JSON-RPC client for serial communication.

    Supports two serial backends:
    - fbuild daemon (default): Better for concurrent access and deploy coordination
    - pyserial direct (use_pyserial=True): For --no-fbuild mode, avoids asyncio issues

    Attributes:
        port: Serial port path
        baudrate: Serial baud rate (default: 115200)
        timeout: Default RPC response timeout in seconds
        response_prefix: Prefix for RPC responses (default: "REMOTE: ")
        use_pyserial: If True, use pyserial directly instead of fbuild daemon
    """

    RESPONSE_PREFIX = "REMOTE: "

    def __init__(
        self,
        port: str,
        baudrate: int = 115200,
        timeout: float = 10.0,
        read_timeout: float = 0.5,
        use_pyserial: bool = False,
    ):
        """Initialize RPC client.

        Args:
            port: Serial port path (e.g., "/dev/ttyUSB0", "COM3")
            baudrate: Serial baud rate (default: 115200)
            timeout: Default timeout for RPC operations in seconds
            read_timeout: Timeout for individual serial reads (unused with fbuild)
            use_pyserial: If True, use pyserial directly instead of fbuild (for --no-fbuild)
        """
        self.port = port
        self.baudrate = baudrate
        self.timeout = timeout
        self.read_timeout = read_timeout
        self.use_pyserial = use_pyserial
        self._monitor: Any = None  # SerialMonitor (fbuild or pyserial)

    @property
    def is_connected(self) -> bool:
        """Check if serial connection is open."""
        return self._monitor is not None

    def connect(self, boot_wait: float = 3.0, drain_boot: bool = True) -> None:
        """Open serial connection.

        Uses either fbuild daemon or direct pyserial based on use_pyserial setting.

        Args:
            boot_wait: Time to wait for device boot (seconds)
            drain_boot: Whether to drain boot output after connecting
        """
        if self.is_connected:
            return

        # Select appropriate SerialMonitor implementation
        if self.use_pyserial:
            from ci.util.pyserial_monitor import SerialMonitor as PySerialMonitor

            self._monitor = PySerialMonitor(
                port=self.port,
                baud_rate=self.baudrate,
                auto_reconnect=True,
                verbose=False,
            )
        else:
            from fbuild.api import SerialMonitor as FbuildSerialMonitor

            self._monitor = FbuildSerialMonitor(
                port=self.port,
                baud_rate=self.baudrate,
                auto_reconnect=True,  # Handle deploy preemption gracefully
                verbose=False,
            )

        # Attach to serial session (works for both implementations)
        self._monitor.__enter__()

        if boot_wait > 0:
            # Use interruptible sleep loop for Windows Ctrl+C compatibility.
            # On Windows, time.sleep() blocks signal delivery, so poll.
            start = time.time()
            while time.time() - start < boot_wait:
                self._check_interrupt()
                time.sleep(0.05)  # Check interrupt every 50ms

        if drain_boot:
            self.drain_boot_output()

    def close(self) -> None:
        """Close serial connection via fbuild daemon."""
        if self._monitor is not None:
            self._monitor.__exit__(None, None, None)
            self._monitor = None

    def drain_boot_output(self, max_lines: int = 100, verbose: bool = False) -> int:
        """Drain pending boot output from serial buffer.

        Args:
            max_lines: Maximum lines to drain
            verbose: Print drained lines

        Returns:
            Number of lines drained
        """
        if not self.is_connected:
            raise RpcError("Not connected")

        assert self._monitor is not None

        lines_drained = 0

        # Poll for boot output with short timeout (wait for buffer to drain)
        # Use read_lines with a short timeout to drain existing buffer
        try:
            for line in self._monitor.read_lines(timeout=1.0):
                self._check_interrupt()
                lines_drained += 1

                if verbose:
                    if lines_drained <= 3:
                        print(f"  [boot] {line[:80]}")
                    elif lines_drained == 4:
                        print("  [boot] ... (draining boot output)")

                if lines_drained >= max_lines:
                    break

        except KeyboardInterrupt:
            notify_main_thread()
            raise
        except Exception:
            pass  # Timeout is normal when buffer is drained

        return lines_drained

    def send(
        self,
        function: str,
        args: list[Any] | None = None,
        timeout: float | None = None,
        retries: int = 1,
    ) -> RpcResponse:
        """Send JSON-RPC command and wait for response.

        Args:
            function: RPC function name
            args: Function arguments as list (default: [])
            timeout: Override default timeout for this call
            retries: Number of retry attempts (default: 1 = no retries)

        Returns:
            RpcResponse with parsed data

        Raises:
            RpcTimeoutError: If no response within timeout
            RpcError: If not connected
        """
        if not self.is_connected:
            raise RpcError("Not connected")

        assert self._monitor is not None

        # Wrap args in array to pass as single Json parameter to RPC functions
        # RPC functions expect signature: (const fl::Json& args)
        # So we need to pass the entire args as one parameter
        wrapped_args: list[Any] = [args] if args is not None else [{}]
        cmd: dict[str, Any] = {"function": function, "args": wrapped_args}
        cmd_str = json.dumps(cmd, separators=(",", ":"))

        effective_timeout = timeout if timeout is not None else self.timeout
        attempt_timeout = effective_timeout / max(retries, 1)

        last_error: Exception | None = None

        for _attempt in range(retries):
            try:
                # Write command via fbuild daemon
                self._monitor.write(cmd_str + "\n")

                response = self._wait_for_response(attempt_timeout)
                return response

            except RpcTimeoutError as e:
                last_error = e
                continue

        raise last_error or RpcTimeoutError(
            f"No response after {retries} attempts (total timeout: {effective_timeout}s)"
        )

    def send_and_match(
        self,
        function: str,
        args: list[Any] | None = None,
        match_key: str | None = None,
        timeout: float | None = None,
        retries: int = 3,
    ) -> RpcResponse:
        """Send RPC and wait for response containing specific key.

        Useful when device may send multiple response types and you need
        to match a specific one.

        Args:
            function: RPC function name
            args: Function arguments
            match_key: Key that must be present in response (e.g., "connected")
            timeout: Override default timeout
            retries: Number of retry attempts

        Returns:
            RpcResponse containing the match_key

        Raises:
            RpcTimeoutError: If matching response not received
        """
        if not self.is_connected:
            raise RpcError("Not connected")

        assert self._monitor is not None

        # Wrap args in array to pass as single Json parameter to RPC functions
        # RPC functions expect signature: (const fl::Json& args)
        # So we need to pass the entire args as one parameter
        wrapped_args: list[Any] = [args] if args is not None else [{}]
        cmd: dict[str, Any] = {"function": function, "args": wrapped_args}
        cmd_str = json.dumps(cmd, separators=(",", ":"))

        effective_timeout = timeout if timeout is not None else self.timeout
        attempt_timeout = effective_timeout / max(retries, 1)

        for _attempt in range(retries):
            self._check_interrupt()  # Check before each attempt

            # Write command via fbuild daemon
            self._monitor.write(cmd_str + "\n")

            start = time.time()
            try:
                for line in self._monitor.read_lines(timeout=attempt_timeout):
                    self._check_interrupt()

                    if line.startswith(self.RESPONSE_PREFIX):
                        json_str = line[len(self.RESPONSE_PREFIX) :]
                        try:
                            data = json.loads(json_str)
                            # Check if match_key is present
                            # For JSON-RPC 2.0 responses, check inside "result" field
                            match_found = False
                            if match_key is None:
                                match_found = True
                            elif match_key in data:
                                match_found = True
                            elif (
                                "result" in data
                                and isinstance(data["result"], dict)
                                and match_key in data["result"]
                            ):
                                match_found = True

                            if match_found:
                                # Extract result field for JSON-RPC 2.0 responses
                                # This allows validation code to use response.get("key") directly
                                response_data = (
                                    data.get("result", data)
                                    if "result" in data
                                    else data
                                )
                                return RpcResponse(
                                    success=data.get("success", True)
                                    if "success" in data
                                    else response_data.get("success", True),
                                    data=response_data,
                                    raw_line=line,
                                )
                        except json.JSONDecodeError:
                            continue

                    # Check timeout
                    if time.time() - start >= attempt_timeout:
                        break

            except KeyboardInterrupt:
                notify_main_thread()
                raise
            except Exception:
                continue

        raise RpcTimeoutError(
            f"No response with key '{match_key}' after {retries} attempts"
        )

    def _wait_for_response(self, timeout: float) -> RpcResponse:
        """Wait for and parse JSON-RPC response.

        Args:
            timeout: Maximum time to wait in seconds

        Returns:
            RpcResponse with parsed data

        Raises:
            RpcTimeoutError: If no response within timeout
        """
        assert self._monitor is not None

        start = time.time()

        try:
            for line in self._monitor.read_lines(timeout=timeout):
                self._check_interrupt()

                if line.startswith(self.RESPONSE_PREFIX):
                    json_str = line[len(self.RESPONSE_PREFIX) :]
                    try:
                        data = json.loads(json_str)
                        # Extract result field for JSON-RPC 2.0 responses
                        # This allows validation code to use response.get("key") directly
                        response_data = (
                            data.get("result", data) if "result" in data else data
                        )
                        return RpcResponse(
                            success=data.get("success", True)
                            if "success" in data
                            else response_data.get("success", True),
                            data=response_data,
                            raw_line=line,
                        )
                    except json.JSONDecodeError:
                        continue

                # Check timeout
                if time.time() - start >= timeout:
                    break

        except KeyboardInterrupt:
            notify_main_thread()
            raise
        except Exception:
            pass  # Timeout or other error

        raise RpcTimeoutError(f"No response within {timeout}s")

    def _check_interrupt(self) -> None:
        """Check for KeyboardInterrupt signal."""
        if is_interrupted():
            raise KeyboardInterrupt()

    def __enter__(self) -> "RpcClient":
        """Context manager entry - connects to device."""
        self.connect()
        return self

    def __exit__(
        self, _exc_type: type | None, _exc_val: Exception | None, _exc_tb: object
    ) -> None:
        """Context manager exit - closes connection."""
        self.close()
