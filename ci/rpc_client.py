"""
Async JSON-RPC client with automatic request ID correlation for serial communication.

This module provides an async/await RPC client that automatically manages request IDs:
- Serial connection management via fbuild daemon (eliminates port lock conflicts)
- Or direct pyserial connection (when --no-fbuild is specified)
- JSON-RPC 2.0 command serialization with automatic request ID generation
- Internal response matching by ID (transparent to users)
- ID wraparound handling for uint32 range (0 to 4,294,967,295)
- Async/await API (mandatory)
- Timeout and retry logic
- Proper KeyboardInterrupt handling
- Boot output draining

Usage (IDs are managed internally, async/await required):
    async with RpcClient("/dev/ttyUSB0") as client:
        response = await client.send("ping")
        print(response.data)  # {"timestamp": ..., "uptimeMs": ...}
        print(response.success)  # True/False
        # Note: response._id exists but is internal - don't use it

    # With pyserial (no fbuild daemon):
    async with RpcClient("/dev/ttyUSB0", use_pyserial=True) as client:
        response = await client.send("ping")

    # Or with explicit connection management:
    client = RpcClient("/dev/ttyUSB0")
    await client.connect()
    await client.drain_boot_output()
    response = await client.send("testGpioConnection", [16, 17])
    await client.close()
"""

from __future__ import annotations

import asyncio
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
    """Structured RPC response with internal request ID correlation.

    The request ID is managed internally for correlation and is not exposed
    in the public API. Users should only interact with success, data, and raw_line.
    """

    success: bool
    data: dict[str, Any]
    raw_line: str
    _id: int = 0  # Internal: Request ID from JSON-RPC 2.0 (always present, hidden from public API)

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
        self._next_id: int = (
            1  # Request ID counter for JSON-RPC 2.0 correlation (uint32 range)
        )

    @property
    def is_connected(self) -> bool:
        """Check if serial connection is open."""
        return self._monitor is not None

    async def connect(self, boot_wait: float = 3.0, drain_boot: bool = True) -> None:
        """Open serial connection (async).

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
            # Use async sleep to allow other tasks to run
            # Poll in small increments for interrupt checking
            start = time.time()
            while time.time() - start < boot_wait:
                self._check_interrupt()
                await asyncio.sleep(0.05)  # Async sleep, check interrupt every 50ms

        if drain_boot:
            await self.drain_boot_output()

    async def close(self) -> None:
        """Close serial connection (async)."""
        if self._monitor is not None:
            self._monitor.__exit__(None, None, None)
            self._monitor = None
            await asyncio.sleep(0)  # Yield control

    async def drain_boot_output(
        self, max_lines: int = 100, verbose: bool = False
    ) -> int:
        """Drain pending boot output from serial buffer (async).

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
                await asyncio.sleep(0)  # Allow other async tasks to run
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

    async def send(
        self,
        function: str,
        args: list[Any] | None = None,
        timeout: float | None = None,
        retries: int = 1,
    ) -> RpcResponse:
        """Send JSON-RPC command and wait for response with mandatory ID correlation (async).

        Args:
            function: RPC function name
            args: Function arguments as list (default: [])
            timeout: Override default timeout for this call
            retries: Number of retry attempts (default: 1 = no retries)

        Returns:
            RpcResponse with parsed data and matching request ID (ID always present)

        Raises:
            RpcTimeoutError: If no response within timeout
            RpcError: If not connected
        """
        if not self.is_connected:
            raise RpcError("Not connected")

        assert self._monitor is not None

        # Generate unique request ID for JSON-RPC 2.0 correlation (mandatory)
        request_id = self._next_id
        self._next_id = (self._next_id + 1) & 0xFFFFFFFF  # Wrap at uint32 max

        # Wrap args in array to pass as single Json parameter to RPC functions
        # RPC functions expect signature: (const fl::Json& args)
        # So we need to pass the entire args as one parameter
        wrapped_args: list[Any] = [args] if args is not None else [{}]
        cmd: dict[str, Any] = {
            "method": function,
            "params": wrapped_args,
            "id": request_id,  # ID is mandatory
        }
        cmd_str = json.dumps(cmd, separators=(",", ":"))

        effective_timeout = timeout if timeout is not None else self.timeout
        attempt_timeout = effective_timeout / max(retries, 1)

        last_error: Exception | None = None

        for _attempt in range(retries):
            try:
                # Write command via serial monitor
                self._monitor.write(cmd_str + "\n")

                # Await response with ID matching (mandatory)
                response = await self._wait_for_response(
                    attempt_timeout, expected_id=request_id
                )
                return response

            except RpcTimeoutError as e:
                last_error = e
                continue

        raise last_error or RpcTimeoutError(
            f"No response after {retries} attempts (total timeout: {effective_timeout}s)"
        )

    async def send_and_match(
        self,
        function: str,
        args: list[Any] | None = None,
        match_key: str | None = None,
        timeout: float | None = None,
        retries: int = 3,
    ) -> RpcResponse:
        """Send RPC and wait for response containing specific key with mandatory ID correlation (async).

        Useful when device may send multiple response types and you need
        to match a specific one.

        Args:
            function: RPC function name
            args: Function arguments
            match_key: Key that must be present in response (e.g., "connected")
            timeout: Override default timeout
            retries: Number of retry attempts

        Returns:
            RpcResponse containing the match_key and matching request ID (always includes ID)

        Raises:
            RpcTimeoutError: If matching response not received
        """
        if not self.is_connected:
            raise RpcError("Not connected")

        assert self._monitor is not None

        # Generate unique request ID for JSON-RPC 2.0 correlation (mandatory)
        request_id = self._next_id
        self._next_id = (self._next_id + 1) & 0xFFFFFFFF  # Wrap at uint32 max

        # Wrap args in array to pass as single Json parameter to RPC functions
        # RPC functions expect signature: (const fl::Json& args)
        # So we need to pass the entire args as one parameter
        wrapped_args: list[Any] = [args] if args is not None else [{}]
        cmd: dict[str, Any] = {
            "method": function,
            "params": wrapped_args,
            "id": request_id,  # ID is mandatory
        }
        cmd_str = json.dumps(cmd, separators=(",", ":"))

        effective_timeout = timeout if timeout is not None else self.timeout
        attempt_timeout = effective_timeout / max(retries, 1)

        for _attempt in range(retries):
            self._check_interrupt()  # Check before each attempt

            # Write command via serial monitor
            self._monitor.write(cmd_str + "\n")

            start = time.time()
            try:
                for line in self._monitor.read_lines(timeout=attempt_timeout):
                    self._check_interrupt()
                    await asyncio.sleep(0)  # Allow other async tasks to run

                    if line.startswith(self.RESPONSE_PREFIX):
                        json_str = line[len(self.RESPONSE_PREFIX) :]
                        try:
                            data = json.loads(json_str)

                            # Check if response ID matches request ID (JSON-RPC 2.0 correlation)
                            # ID is mandatory - must match
                            response_id = data.get("id")
                            if response_id is None or response_id != request_id:
                                # Skip responses without ID or non-matching IDs
                                continue

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
                                # Extract result/error field for JSON-RPC 2.0 responses
                                # Priority: result > error > empty dict (no protocol fields)
                                if "result" in data:
                                    response_data = data["result"]
                                elif "error" in data:
                                    response_data = data["error"]
                                else:
                                    # No result or error - empty response
                                    response_data = {}

                                # Determine success: void functions return null, treat as success
                                if "success" in data:
                                    success = data.get("success", True)
                                elif isinstance(response_data, dict):
                                    success = response_data.get("success", True)
                                else:
                                    # Void functions return null - treat as successful execution
                                    success = True

                                # Ensure response_data is always a dict for consistent API
                                if response_data is None or not isinstance(
                                    response_data, dict
                                ):
                                    response_data = {}

                                return RpcResponse(
                                    success=success,
                                    data=response_data,
                                    raw_line=line,
                                    _id=response_id,  # ID is always present
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

    async def _wait_for_response(self, timeout: float, expected_id: int) -> RpcResponse:
        """Wait for and parse JSON-RPC response with mandatory ID matching (async).

        Args:
            timeout: Maximum time to wait in seconds
            expected_id: Expected request ID for JSON-RPC 2.0 correlation (mandatory)

        Returns:
            RpcResponse with parsed data and matching request ID (ID always present)

        Raises:
            RpcTimeoutError: If no matching response within timeout
        """
        assert self._monitor is not None

        start = time.time()

        try:
            for line in self._monitor.read_lines(timeout=timeout):
                self._check_interrupt()
                await asyncio.sleep(0)  # Allow other async tasks to run

                if line.startswith(self.RESPONSE_PREFIX):
                    json_str = line[len(self.RESPONSE_PREFIX) :]
                    try:
                        data = json.loads(json_str)

                        # Check if response ID matches expected ID (JSON-RPC 2.0 correlation)
                        # ID is mandatory - must match exactly
                        response_id = data.get("id")
                        if response_id is None:
                            # Responses without ID are invalid - skip them
                            continue
                        if response_id != expected_id:
                            # Skip responses that don't match our expected request ID
                            continue

                        # Extract result/error field for JSON-RPC 2.0 responses
                        # Priority: result > error > empty dict (no protocol fields)
                        if "result" in data:
                            response_data = data["result"]
                        elif "error" in data:
                            response_data = data["error"]
                        else:
                            # No result or error - empty response
                            response_data = {}

                        # Determine success: void functions return null, treat as success
                        if "success" in data:
                            success = data.get("success", True)
                        elif isinstance(response_data, dict):
                            success = response_data.get("success", True)
                        else:
                            # Void functions return null - treat as successful execution
                            success = True

                        # Ensure response_data is always a dict for consistent API
                        if response_data is None or not isinstance(response_data, dict):
                            response_data = {}

                        return RpcResponse(
                            success=success,
                            data=response_data,
                            raw_line=line,
                            _id=response_id,  # ID is always present
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

        raise RpcTimeoutError(f"No response with ID {expected_id} within {timeout}s")

    def _check_interrupt(self) -> None:
        """Check for KeyboardInterrupt signal."""
        if is_interrupted():
            raise KeyboardInterrupt()

    async def __aenter__(self) -> "RpcClient":
        """Async context manager entry - connects to device."""
        await self.connect()
        return self

    async def __aexit__(
        self, _exc_type: type | None, _exc_val: Exception | None, _exc_tb: object
    ) -> None:
        """Async context manager exit - closes connection."""
        await self.close()
