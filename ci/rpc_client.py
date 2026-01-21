"""
Reusable JSON-RPC client for serial communication with embedded devices.

This module provides a stateful RPC client that handles:
- Serial connection management with context manager support
- JSON-RPC command serialization and response parsing
- Timeout and retry logic
- Proper KeyboardInterrupt handling
- Boot output draining

Usage:
    with RpcClient("/dev/ttyUSB0") as client:
        response = client.send("ping")
        print(response)  # {"timestamp": ..., "uptimeMs": ...}

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
from typing import Any

import serial

from ci.util.global_interrupt_handler import is_interrupted, notify_main_thread


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

    Attributes:
        port: Serial port path
        baudrate: Serial baud rate (default: 115200)
        timeout: Default RPC response timeout in seconds
        response_prefix: Prefix for RPC responses (default: "REMOTE: ")
    """

    RESPONSE_PREFIX = "REMOTE: "

    def __init__(
        self,
        port: str,
        baudrate: int = 115200,
        timeout: float = 10.0,
        read_timeout: float = 0.5,
    ):
        """Initialize RPC client.

        Args:
            port: Serial port path (e.g., "/dev/ttyUSB0", "COM3")
            baudrate: Serial baud rate (default: 115200)
            timeout: Default timeout for RPC operations in seconds
            read_timeout: Timeout for individual serial reads
        """
        self.port = port
        self.baudrate = baudrate
        self.timeout = timeout
        self.read_timeout = read_timeout
        self._serial: serial.Serial | None = None

    @property
    def is_connected(self) -> bool:
        """Check if serial connection is open."""
        return self._serial is not None and self._serial.is_open

    def connect(self, boot_wait: float = 3.0, drain_boot: bool = True) -> None:
        """Open serial connection.

        Args:
            boot_wait: Time to wait for device boot (seconds)
            drain_boot: Whether to drain boot output after connecting
        """
        if self.is_connected:
            return

        self._serial = serial.Serial(
            self.port, self.baudrate, timeout=self.read_timeout
        )

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
        """Close serial connection."""
        if self._serial is not None:
            self._serial.close()
            self._serial = None

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

        assert self._serial is not None

        lines_drained = 0
        empty_checks = 0
        max_empty_checks = 3  # Exit after 3 consecutive empty checks

        while lines_drained < max_lines:
            self._check_interrupt()
            try:
                if self._serial.in_waiting == 0:
                    if lines_drained > 0:
                        empty_checks += 1
                        if empty_checks >= max_empty_checks:
                            break
                    # Give a small window for more data
                    time.sleep(0.05)
                    self._check_interrupt()
                    continue

                empty_checks = 0  # Reset on data received
                line = self._serial.readline().decode("utf-8", errors="replace").strip()
                if not line:
                    break

                lines_drained += 1
                if verbose:
                    if lines_drained <= 3:
                        print(f"  [boot] {line[:80]}")
                    elif lines_drained == 4:
                        print("  [boot] ... (draining boot output)")
            except KeyboardInterrupt:
                notify_main_thread()
                raise
            except Exception:
                break

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

        assert self._serial is not None

        cmd = {"function": function, "args": args or []}
        cmd_str = json.dumps(cmd, separators=(",", ":"))

        effective_timeout = timeout if timeout is not None else self.timeout
        attempt_timeout = effective_timeout / max(retries, 1)

        last_error: Exception | None = None

        for _attempt in range(retries):
            try:
                self._serial.reset_input_buffer()
                self._serial.write((cmd_str + "\n").encode())
                self._serial.flush()

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

        assert self._serial is not None

        cmd = {"function": function, "args": args or []}
        cmd_str = json.dumps(cmd, separators=(",", ":"))

        effective_timeout = timeout if timeout is not None else self.timeout
        attempt_timeout = effective_timeout / max(retries, 1)

        for _attempt in range(retries):
            self._check_interrupt()  # Check before each attempt
            self._serial.reset_input_buffer()
            self._serial.write((cmd_str + "\n").encode())
            self._serial.flush()

            start = time.time()
            while time.time() - start < attempt_timeout:
                self._check_interrupt()

                # Use non-blocking approach: check for data first, then read
                # This allows interrupt checking on Windows where blocking I/O
                # may not respond to Ctrl+C signals properly
                try:
                    if self._serial.in_waiting == 0:
                        # No data available, sleep briefly and check interrupt again
                        time.sleep(0.05)
                        self._check_interrupt()
                        continue

                    line = (
                        self._serial.readline()
                        .decode("utf-8", errors="replace")
                        .strip()
                    )
                except KeyboardInterrupt:
                    notify_main_thread()
                    raise
                except Exception:
                    continue

                # Check interrupt after read completes
                self._check_interrupt()

                if not line:
                    continue

                if line.startswith(self.RESPONSE_PREFIX):
                    json_str = line[len(self.RESPONSE_PREFIX) :]
                    try:
                        data = json.loads(json_str)
                        if match_key is None or match_key in data:
                            return RpcResponse(
                                success=data.get("success", True),
                                data=data,
                                raw_line=line,
                            )
                    except json.JSONDecodeError:
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
        assert self._serial is not None

        start = time.time()

        while time.time() - start < timeout:
            self._check_interrupt()

            # Use non-blocking approach for Windows Ctrl+C compatibility
            try:
                if self._serial.in_waiting == 0:
                    time.sleep(0.05)
                    self._check_interrupt()
                    continue

                line = self._serial.readline().decode("utf-8", errors="replace").strip()
            except KeyboardInterrupt:
                notify_main_thread()
                raise
            except Exception:
                continue

            self._check_interrupt()

            if not line:
                continue

            if line.startswith(self.RESPONSE_PREFIX):
                json_str = line[len(self.RESPONSE_PREFIX) :]
                try:
                    data = json.loads(json_str)
                    return RpcResponse(
                        success=data.get("success", True),
                        data=data,
                        raw_line=line,
                    )
                except json.JSONDecodeError:
                    continue

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
