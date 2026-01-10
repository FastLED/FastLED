#!/usr/bin/env python3
"""Serial monitoring infrastructure for embedded device development.

This module provides a composable, handler-based architecture for monitoring
serial output from embedded devices. It supports both direct serial connections
and PlatformIO device monitor integration.

Architecture:
    - LineHandler: Abstract base class for processing serial output lines
    - Concrete handlers: OutputCollector, ExpectHandler, FailHandler, InputTriggerHandler
    - Monitor implementations: SerialMonitor (direct), PioDeviceMonitorHandler (PIO)

Usage:
    # Create a monitor
    monitor = SerialMonitor(port="COM3", timeout_seconds=30)

    # Add handlers
    monitor.add_handler(OutputCollector())
    monitor.add_handler(ExpectHandler(["READY", "SUCCESS"]))
    monitor.add_handler(FailHandler(["ERROR", "PANIC"]))

    # Run and check success
    success = monitor.run()
"""

import re
import time
from pathlib import Path

import serial
from colorama import Fore, Style
from running_process import RunningProcess
from running_process.process_output_reader import EndOfStream

from ci.compiler.build_utils import get_utf8_env
from ci.util.global_interrupt_handler import handle_keyboard_interrupt_properly
from ci.util.output_formatter import TimestampFormatter


class LineHandler:
    """Abstract interface for handling serial output lines.

    Handlers are invoked for each line of serial output. They can:
    - Track patterns and state
    - Request immediate termination (return False)
    - Report success/failure status
    """

    def handle_line(self, line: str) -> bool:
        """Process a line of serial output.

        Args:
            line: The line to process

        Returns:
            True to continue monitoring, False to stop immediately
        """
        raise NotImplementedError

    def is_successful(self) -> bool:
        """Check if this handler's criteria for success are met.

        Returns:
            True if successful, False otherwise
        """
        return True

    def get_failure_message(self) -> str | None:
        """Get failure message if handler failed.

        Returns:
            Error message string if failed, None if successful
        """
        return None


class OutputCollector(LineHandler):
    """Collects all output lines for later retrieval."""

    def __init__(self) -> None:
        self.mLines: list[str] = []

    def handle_line(self, line: str) -> bool:
        self.mLines.append(line)
        return True

    def get_lines(self) -> list[str]:
        """Get all collected output lines."""
        return self.mLines


class ExpectHandler(LineHandler):
    """Monitors for expected patterns that must all be found.

    This handler tracks regex patterns and considers success when ALL patterns
    have been matched at least once during monitoring.
    """

    def __init__(self, patterns: list[str]) -> None:
        """Initialize handler with patterns to expect.

        Args:
            patterns: List of regex pattern strings that must all match
        """
        self.mPatterns: list[tuple[str, re.Pattern[str]]] = []
        self.mFoundPatterns: set[str] = set()
        self.mMatchedLines: list[tuple[str, str]] = []

        for pattern_str in patterns:
            try:
                self.mPatterns.append((pattern_str, re.compile(pattern_str)))
            except re.error as e:
                print(f"⚠️  Warning: Invalid expect regex pattern '{pattern_str}': {e}")
                self.mPatterns.append((pattern_str, re.compile(re.escape(pattern_str))))

    def handle_line(self, line: str) -> bool:
        for pattern_str, pattern_re in self.mPatterns:
            if pattern_re.search(line):
                if pattern_str not in self.mFoundPatterns:
                    self.mFoundPatterns.add(pattern_str)
                    self.mMatchedLines.append((pattern_str, line))
                    print(f"\n✅ EXPECT PATTERN DETECTED: /{pattern_str}/")
                    print(f"   Matched line: {line}")
                    print(
                        f"   (Continuing to monitor - need all {len(self.mPatterns)} patterns)\n"
                    )
        return True

    def is_successful(self) -> bool:
        return len(self.mFoundPatterns) == len(self.mPatterns)

    def get_failure_message(self) -> str | None:
        if self.is_successful():
            return None

        missing = set(p for p, _ in self.mPatterns) - self.mFoundPatterns
        if len(missing) == 1:
            return f"Missing expected pattern: /{list(missing)[0]}/"
        else:
            patterns_str = "\n".join(f"     - /{p}/" for p in sorted(missing))
            return f"Missing expected patterns:\n{patterns_str}"

    def get_matched_lines(self) -> list[tuple[str, str]]:
        """Get all (pattern, line) tuples that matched."""
        return self.mMatchedLines


class FailHandler(LineHandler):
    """Monitors for failure patterns that trigger immediate termination.

    This handler stops monitoring immediately when any pattern matches,
    indicating a critical failure condition.
    """

    def __init__(self, patterns: list[str]) -> None:
        """Initialize handler with failure patterns.

        Args:
            patterns: List of regex pattern strings that indicate failure
        """
        self.mPatterns: list[tuple[str, re.Pattern[str]]] = []
        self.mFailureFound: bool = False
        self.mMatchedPattern: str | None = None
        self.mMatchedLine: str | None = None
        self.mAllMatches: list[tuple[str, str]] = []

        for pattern_str in patterns:
            try:
                self.mPatterns.append((pattern_str, re.compile(pattern_str)))
            except re.error as e:
                print(f"⚠️  Warning: Invalid fail regex pattern '{pattern_str}': {e}")
                self.mPatterns.append((pattern_str, re.compile(re.escape(pattern_str))))

    def handle_line(self, line: str) -> bool:
        for pattern_str, pattern_re in self.mPatterns:
            if pattern_re.search(line):
                self.mAllMatches.append((pattern_str, line))
                if not self.mFailureFound:
                    self.mFailureFound = True
                    self.mMatchedPattern = pattern_str
                    self.mMatchedLine = line
                    print(f"\n🚨 FAIL PATTERN DETECTED: /{pattern_str}/")
                    print(f"   Matched line: {line}")
                    print("   Terminating monitor immediately...\n")
                    return False  # Stop monitoring immediately
        return True

    def is_successful(self) -> bool:
        return not self.mFailureFound

    def get_failure_message(self) -> str | None:
        if not self.mFailureFound:
            return None
        return f"Fail pattern /{self.mMatchedPattern}/ detected\n   Matched line: {self.mMatchedLine}"

    def get_all_matches(self) -> list[tuple[str, str]]:
        """Get all (pattern, line) tuples that matched failure patterns."""
        return self.mAllMatches


class InputTriggerHandler(LineHandler):
    """Sends input to serial port when trigger pattern is detected.

    This handler monitors for a specific pattern and sends configured text
    to the serial port when the pattern matches. Useful for interactive
    test scenarios.
    """

    def __init__(self, trigger_pattern: str, input_text: str) -> None:
        """Initialize handler with trigger pattern and response text.

        Args:
            trigger_pattern: Regex pattern that triggers input
            input_text: Text to send (without newline - added automatically)
        """
        self.mTriggerPattern: str = trigger_pattern
        self.mInputText: str = input_text
        self.mSerialPort: serial.Serial | None = None
        self.mInputSent: bool = False

        try:
            self.mTriggerRegex: re.Pattern[str] = re.compile(trigger_pattern)
        except re.error as e:
            print(f"⚠️  Warning: Invalid trigger regex pattern '{trigger_pattern}': {e}")
            self.mTriggerRegex = re.compile(re.escape(trigger_pattern))

    def set_serial_port(self, serial_port: serial.Serial) -> None:
        """Set the serial port for sending input.

        Must be called before monitoring starts. SerialMonitor does this automatically.

        Args:
            serial_port: Open serial.Serial object
        """
        self.mSerialPort = serial_port

    def handle_line(self, line: str) -> bool:
        if not self.mInputSent and self.mSerialPort and self.mTriggerRegex.search(line):
            time.sleep(0.1)  # Small delay before sending
            print(
                f"\n{Fore.GREEN}✅ TRIGGER MATCHED: /{self.mTriggerPattern}/{Style.RESET_ALL}"
            )
            print(
                f"{Fore.GREEN}>>> Sending input: {repr(self.mInputText)}{Style.RESET_ALL}\n"
            )
            self.mSerialPort.write((self.mInputText + "\n").encode("utf-8"))
            self.mSerialPort.flush()
            self.mInputSent = True
        return True


class SerialMonitor:
    """Monitors serial output via direct pyserial connection.

    This monitor opens a direct serial connection and dispatches each line
    to registered handlers. Use this when you need programmatic serial input
    (InputTriggerHandler) or direct control over the serial port.

    For PlatformIO projects, prefer PioDeviceMonitorHandler instead.
    """

    def __init__(
        self,
        port: str,
        baud_rate: int = 115200,
        timeout_seconds: int = 20,
        stream: bool = False,
    ) -> None:
        """Initialize serial monitor.

        Args:
            port: Serial port name (e.g., "COM3", "/dev/ttyUSB0")
            baud_rate: Serial baud rate (default: 115200)
            timeout_seconds: Maximum monitoring time in seconds
            stream: If True, monitor runs indefinitely until Ctrl+C
        """
        self.mPort: str = port
        self.mBaudRate: int = baud_rate
        self.mTimeoutSeconds: int = timeout_seconds
        self.mStream: bool = stream
        self.mHandlers: list[LineHandler] = []
        self.mSerialPort: serial.Serial | None = None

    def add_handler(self, handler: LineHandler) -> None:
        """Add a line handler to the monitor.

        Args:
            handler: LineHandler instance to register
        """
        self.mHandlers.append(handler)

    def run(self) -> bool:
        """Run the serial monitor until timeout or handler requests stop.

        Returns:
            True if all handlers succeeded, False otherwise
        """
        # Open serial connection
        try:
            self.mSerialPort = serial.Serial(self.mPort, self.mBaudRate, timeout=1)
            time.sleep(0.5)  # Let serial connection stabilize
            print(f"{Fore.GREEN}✅ Serial connection established{Style.RESET_ALL}\n")
        except serial.SerialException as e:
            print(f"{Fore.RED}❌ Failed to open serial port: {e}{Style.RESET_ALL}")
            return False

        # Initialize handlers that need the serial port
        for handler in self.mHandlers:
            if isinstance(handler, InputTriggerHandler):
                handler.set_serial_port(self.mSerialPort)

        start_time = time.time()
        should_continue = True

        try:
            while should_continue:
                # Check timeout (unless in streaming mode)
                if not self.mStream:
                    elapsed = time.time() - start_time
                    if elapsed >= self.mTimeoutSeconds:
                        print(
                            f"\n⏱️  Timeout reached ({self.mTimeoutSeconds}s), stopping monitor..."
                        )
                        break

                # Read available serial data
                if self.mSerialPort.in_waiting:
                    try:
                        line = (
                            self.mSerialPort.readline()
                            .decode("utf-8", errors="ignore")
                            .strip()
                        )
                        if line:
                            print(line)  # Display line

                            # Dispatch to all handlers
                            for handler in self.mHandlers:
                                if not handler.handle_line(line):
                                    # Handler requested stop
                                    should_continue = False
                                    break

                    except UnicodeDecodeError:
                        # Skip lines with decode errors
                        pass

                time.sleep(0.01)  # Small delay to prevent busy-waiting

        except KeyboardInterrupt:
            print(f"\n{Fore.YELLOW}⚠️  Interrupted by user{Style.RESET_ALL}")
            handle_keyboard_interrupt_properly()
            raise
        finally:
            if self.mSerialPort:
                self.mSerialPort.close()

        # Check if all handlers succeeded
        return all(h.is_successful() for h in self.mHandlers)

    def get_serial_port(self) -> serial.Serial | None:
        """Get the underlying serial port object.

        Returns:
            serial.Serial object or None if not connected
        """
        return self.mSerialPort


class PioDeviceMonitorHandler:
    """Runs PlatformIO device monitor and dispatches lines to handlers.

    This monitor uses PlatformIO's `pio device monitor` command and dispatches
    each output line to registered handlers. This is the preferred approach
    for PlatformIO projects as it leverages PIO's serial port handling.

    For direct serial control or InputTriggerHandler, use SerialMonitor instead.
    """

    def __init__(
        self,
        build_dir: Path,
        environment: str | None = None,
        monitor_port: str | None = None,
        verbose: bool = False,
        timeout_seconds: int = 20,
        stream: bool = False,
    ) -> None:
        """Initialize PIO device monitor.

        Args:
            build_dir: Project directory containing platformio.ini
            environment: PlatformIO environment name (None = default)
            monitor_port: Serial port to monitor (None = auto-detect)
            verbose: Enable verbose PlatformIO output
            timeout_seconds: Maximum monitoring time in seconds
            stream: If True, monitor runs indefinitely until Ctrl+C
        """
        self.mBuildDir: Path = build_dir
        self.mEnvironment: str | None = environment
        self.mMonitorPort: str | None = monitor_port
        self.mVerbose: bool = verbose
        self.mTimeoutSeconds: int = timeout_seconds
        self.mStream: bool = stream
        self.mHandlers: list[LineHandler] = []
        self.mDeviceStuck: bool = False

    def add_handler(self, handler: LineHandler) -> None:
        """Add a line handler to the monitor.

        Args:
            handler: LineHandler instance to register
        """
        self.mHandlers.append(handler)

    def run(self) -> bool:
        """Run the PIO device monitor until timeout or handler requests stop.

        Returns:
            True if all handlers succeeded, False otherwise
        """
        cmd = [
            "pio",
            "device",
            "monitor",
            "--project-dir",
            str(self.mBuildDir),
        ]
        if self.mEnvironment:
            cmd.extend(["--environment", self.mEnvironment])
        if self.mMonitorPort:
            cmd.extend(["--port", self.mMonitorPort])
        if self.mVerbose:
            cmd.append("--verbose")

        formatter = TimestampFormatter()
        proc = RunningProcess(
            cmd,
            cwd=self.mBuildDir,
            auto_run=True,
            output_formatter=formatter,
            env=get_utf8_env(),
        )

        start_time = time.time()
        timeout_reached = False
        should_continue = True

        try:
            while should_continue:
                # Check timeout (unless in streaming mode)
                if not self.mStream:
                    elapsed = time.time() - start_time
                    if elapsed >= self.mTimeoutSeconds:
                        print(
                            f"\n⏱️  Timeout reached ({self.mTimeoutSeconds}s), stopping monitor..."
                        )
                        timeout_reached = True
                        proc.terminate()
                        break

                # Read next line with 30 second read timeout
                try:
                    line = proc.get_next_line(timeout=30)
                    if isinstance(line, EndOfStream):
                        break
                    if line:
                        print(line)  # Real-time output

                        # Dispatch to all handlers
                        for handler in self.mHandlers:
                            if not handler.handle_line(line):
                                # Handler requested stop
                                should_continue = False
                                proc.terminate()
                                break

                except TimeoutError:
                    # No output within read timeout - continue waiting
                    continue
                except KeyboardInterrupt:
                    handle_keyboard_interrupt_properly()
                    raise
                except Exception as e:
                    # Check for specific serial port errors indicating device is stuck
                    error_str = str(e)
                    if "ClearCommError" in error_str and "PermissionError" in error_str:
                        self.mDeviceStuck = True
                        print(
                            "\n🚨 CRITICAL ERROR: Device appears stuck and no longer responding"
                        )
                        print(
                            "   Serial port failure: ClearCommError (device not recognizing commands)"
                        )
                        print("   Possible causes:")
                        print("     - ISR (Interrupt Service Routine) hogging CPU time")
                        print("     - Device crashed or entered non-responsive state")
                        print("     - Hardware watchdog triggered")
                        print(f"   Technical details: {error_str}")
                        print("\n📋 Recovery Instructions:")
                        print("   - Physically unplug and replug the USB cable")
                        print("   - Press reset button on the device if available")
                        proc.terminate()
                        break
                    # Re-raise other exceptions
                    raise

        except KeyboardInterrupt:
            print("\nKeyboardInterrupt: Stopping monitor")
            proc.terminate()
            handle_keyboard_interrupt_properly()
            raise

        proc.wait()

        # Determine success based on handlers and exit conditions
        if self.mDeviceStuck:
            return False
        elif timeout_reached and not self.mHandlers:
            # Normal timeout with no handlers is success
            return True
        else:
            # Check if all handlers succeeded
            return all(h.is_successful() for h in self.mHandlers)

    def is_device_stuck(self) -> bool:
        """Check if device stuck error occurred.

        Returns:
            True if ClearCommError was detected (device unresponsive)
        """
        return self.mDeviceStuck
