#!/usr/bin/env python3
"""PySerial-based SerialMonitor - Direct serial I/O without fbuild daemon.

This module provides a SerialMonitor class with an API compatible with
fbuild's SerialMonitor, but using pyserial directly. This is used when
--no-fbuild is specified to avoid daemon dependencies.

Example Usage:
    >>> from ci.util.pyserial_monitor import SerialMonitor
    >>>
    >>> # Basic monitoring
    >>> with SerialMonitor(port='COM13', baud_rate=115200) as mon:
    ...     for line in mon.read_lines(timeout=30.0):
    ...         print(line)
    ...         if 'ERROR' in line:
    ...             raise RuntimeError('Device error detected')
    >>>
    >>> # With write capability
    >>> with SerialMonitor(port='COM13') as mon:
    ...     mon.write('{"function":"ping"}\\n')
    ...     for line in mon.read_lines(timeout=10.0):
    ...         if 'REMOTE:' in line:
    ...             print(f"Got response: {line}")
    ...             break
"""

import _thread
import time
from collections.abc import Iterator
from types import TracebackType

import serial


class SerialMonitor:
    """Context manager for direct pyserial-based serial monitoring.

    This class provides an API compatible with fbuild's SerialMonitor,
    but uses pyserial directly without daemon dependencies. Use this
    when --no-fbuild is specified.

    Example:
        >>> with SerialMonitor(port='COM13', baud_rate=115200) as mon:
        ...     for line in mon.read_lines(timeout=30.0):
        ...         if 'READY' in line:
        ...             break

    Attributes:
        port: Serial port to monitor
        baud_rate: Baud rate for serial connection
        auto_reconnect: Placeholder for API compatibility (not used)
        verbose: Whether to log verbose debug information
    """

    def __init__(
        self,
        port: str,
        baud_rate: int = 115200,
        auto_reconnect: bool = True,
        verbose: bool = False,
    ):
        """Initialize SerialMonitor.

        Args:
            port: Serial port to monitor (e.g., "COM13", "/dev/ttyUSB0")
            baud_rate: Baud rate for serial connection (default: 115200)
            auto_reconnect: Placeholder for API compatibility (ignored)
            verbose: Enable verbose debug output
        """
        self.port = port
        self.baud_rate = baud_rate
        self.auto_reconnect = auto_reconnect
        self.verbose = verbose
        self._ser: serial.Serial | None = None
        self._attached = False

    def __enter__(self) -> "SerialMonitor":
        """Open serial port (context manager entry).

        Returns:
            Self for context manager pattern

        Raises:
            serial.SerialException: If port cannot be opened
        """
        try:
            self._ser = serial.Serial(
                self.port,
                self.baud_rate,
                timeout=0.1,  # Short timeout for non-blocking reads
                write_timeout=1.0,
            )
            # Flush any stale data from previous sessions (crash output, etc.)
            # This prevents false positives from old crash messages in the buffer
            self._ser.reset_input_buffer()
            self._ser.reset_output_buffer()
            self._attached = True
            if self.verbose:
                print(f"✓ Serial port opened: {self.port} @ {self.baud_rate} baud")
            return self
        except serial.SerialException as e:
            raise RuntimeError(f"Failed to open serial port {self.port}: {e}") from e

    def __exit__(
        self,
        exc_type: type[BaseException] | None,
        exc_val: BaseException | None,
        exc_tb: TracebackType | None,
    ) -> None:
        """Close serial port (context manager exit).

        Args:
            exc_type: Exception type (if any)
            exc_val: Exception value (if any)
            exc_tb: Exception traceback (if any)
        """
        if self._ser and self._ser.is_open:
            self._ser.close()
            self._attached = False
            if self.verbose:
                print(f"✓ Serial port closed: {self.port}")

    def read_lines(self, timeout: float | None = None) -> Iterator[str]:
        """Stream lines from serial port (blocking iterator).

        This method reads lines from the serial port and yields them as strings.
        It blocks until a full line is received or timeout is reached.

        Args:
            timeout: Maximum time to wait for lines (None = infinite)
                     Note: Timeout starts from first call, not from last line received

        Yields:
            Lines from serial output (as strings, without newlines)

        Raises:
            RuntimeError: If not attached or serial error occurs
        """
        if not self._attached or not self._ser:
            raise RuntimeError("Cannot read lines: not attached")

        start_time = time.time()
        buffer = b""

        while True:
            # Check timeout
            if timeout is not None:
                elapsed = time.time() - start_time
                if elapsed >= timeout:
                    # Timeout reached - yield any remaining buffered data
                    if buffer:
                        try:
                            line = buffer.decode("utf-8", errors="ignore").strip()
                            if line:
                                yield line
                        except KeyboardInterrupt:
                            # Thread-aware interrupt: notify main thread
                            _thread.interrupt_main()
                            raise
                        except Exception:
                            pass
                    return

            # Read available bytes
            try:
                if self._ser.in_waiting:
                    data = self._ser.read(self._ser.in_waiting)
                    buffer += data

                    # Process complete lines
                    while b"\n" in buffer:
                        line_bytes, buffer = buffer.split(b"\n", 1)
                        line = line_bytes.decode("utf-8", errors="ignore").strip()
                        if line:
                            yield line
                else:
                    # No data available - sleep briefly to avoid busy-waiting
                    time.sleep(0.01)

            except serial.SerialException as e:
                raise RuntimeError(f"Serial read error: {e}") from e
            except KeyboardInterrupt:
                # Thread-aware interrupt: notify main thread
                _thread.interrupt_main()
                raise

    def write(self, data: str | bytes) -> int:
        """Write data to serial port.

        Args:
            data: String or bytes to write to serial port

        Returns:
            Number of bytes written

        Raises:
            RuntimeError: If not attached or write fails
        """
        if not self._attached or not self._ser:
            raise RuntimeError("Cannot write: not attached")

        try:
            # Convert string to bytes if needed
            if isinstance(data, str):
                data_bytes = data.encode("utf-8")
            else:
                data_bytes = data

            bytes_written = self._ser.write(data_bytes)
            self._ser.flush()
            return bytes_written or 0
        except serial.SerialException as e:
            raise RuntimeError(f"Serial write error: {e}") from e
