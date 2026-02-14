"""
Bidirectional JSON-RPC agent for LED validation testing.

This module provides a Python agent that communicates with the Validation.ino
sketch via JSON-RPC over serial. It enables programmatic control of hardware
validation tests with variable lane sizes and comprehensive result reporting.

Usage:
    agent = ValidationAgent("/dev/ttyUSB0")
    config = TestConfig.uniform("PARLIO", led_count=100, lane_count=2, pattern="MSB_LSB_A")
    agent.configure(config)
    result = agent.run_test()
    print(f"Test {'PASSED' if result.passed else 'FAILED'}")
"""

import json
import time
from dataclasses import dataclass
from enum import Enum
from typing import Any

from fbuild.api import SerialMonitor


class TestPhase(Enum):
    """Test execution phase."""

    IDLE = "idle"
    CONFIGURING = "configuring"
    RUNNING = "running"
    COMPLETE = "complete"
    ERROR = "error"


@dataclass
class TestConfig:
    """Test configuration supporting variable lane sizes.

    Use lane_sizes for heterogeneous configurations (different LED count per lane).
    For uniform configurations, you can either:
      - Set lane_sizes = [100, 100, 100] (explicit)
      - Use TestConfig.uniform() factory method
    """

    driver: str
    lane_sizes: list[int]  # Per-lane LED counts, e.g., [300, 200, 100]
    pattern: str
    iterations: int = 1

    # Runtime strip size configuration (fully dynamic)
    short_strip_size: int | None = None  # Short strip LED count (default: 100)
    long_strip_size: int | None = None  # Long strip LED count (default: 300)
    test_small_strips: bool = True  # Enable small strip testing
    test_large_strips: bool = False  # Enable large strip testing

    # Lane range configuration (NEW - Phase 6)
    min_lanes: int | None = None  # Minimum lane count for test matrix
    max_lanes: int | None = None  # Maximum lane count for test matrix

    @property
    def lane_count(self) -> int:
        """Number of lanes (derived from lane_sizes length)."""
        return len(self.lane_sizes)

    @property
    def total_leds(self) -> int:
        """Total LED count across all lanes."""
        return sum(self.lane_sizes)

    @classmethod
    def uniform(
        cls,
        driver: str,
        led_count: int,
        lane_count: int,
        pattern: str,
        iterations: int = 1,
    ) -> "TestConfig":
        """Create config with uniform lane sizes (all lanes same size)."""
        return cls(
            driver=driver,
            lane_sizes=[led_count] * lane_count,
            pattern=pattern,
            iterations=iterations,
        )


@dataclass
class LaneResult:
    """Result for a single lane."""

    lane: int
    led_count: int  # LED count for this specific lane
    pin: int
    passed: bool
    bit_errors: int
    timing: str  # "ok", "slow", "fast", "unstable"
    expected_bytes: int | None = None
    received_bytes: int | None = None


@dataclass
class TestResult:
    """Complete test result with per-lane details."""

    passed: bool
    total_tests: int
    passed_tests: int
    failed_tests: int
    duration_ms: int
    lane_results: list[LaneResult]  # Per-lane details with sizes


class ValidationAgent:
    """Bidirectional JSON-RPC agent for LED validation testing."""

    REMOTE_PREFIX = "REMOTE: "

    def __init__(self, port: str, baudrate: int = 115200, timeout: float = 30.0):
        """Initialize validation agent.

        Args:
            port: Serial port path (e.g., "/dev/ttyUSB0", "COM3")
            baudrate: Serial baud rate (default: 115200)
            timeout: RPC response timeout in seconds (default: 30.0)
        """
        self._monitor = SerialMonitor(
            port=port,
            baud_rate=baudrate,
            auto_reconnect=True,
            verbose=False,
        )
        self._monitor.__enter__()  # Manual context manager entry
        self.timeout = timeout

    def send_rpc(self, function: str, args: list[Any] | None = None) -> dict[str, Any]:
        """Send JSON-RPC command and wait for response.

        Args:
            function: RPC function name (e.g., "configure", "runTest")
            args: Function arguments as list (default: [])

        Returns:
            JSON response dict from device

        Raises:
            TimeoutError: If no response received within timeout
        """
        cmd = {"method": function, "params": args or []}
        cmd_str = json.dumps(cmd, separators=(",", ":"))

        # Send command (no manual buffer clearing needed with SerialMonitor)
        self._monitor.write(cmd_str + "\n")

        # Wait for REMOTE: prefixed response
        return self._wait_for_response()

    def _wait_for_response(self) -> dict[str, Any]:
        """Wait for and parse JSON-RPC response.

        Returns:
            Parsed JSON response dict

        Raises:
            TimeoutError: If no response received within timeout
        """
        start = time.time()

        while time.time() - start < self.timeout:
            # Use read_lines() iterator with short timeout
            for line in self._monitor.read_lines(timeout=0.05):
                if line.startswith(self.REMOTE_PREFIX):
                    json_str = line[len(self.REMOTE_PREFIX) :]
                    try:
                        return json.loads(json_str)
                    except json.JSONDecodeError as e:
                        print(f"Warning: Failed to parse JSON: {e}")
                        print(f"  Raw line: {line}")
                        continue
                break  # Process one line at a time

        raise TimeoutError(f"No response within {self.timeout}s")

    def configure(self, config: TestConfig) -> dict[str, Any]:
        """Configure test parameters with per-lane sizes and strip size settings.

        Args:
            config: Test configuration object

        Returns:
            Configuration confirmation dict from device
        """
        # Build config dict with mandatory fields
        config_dict = {
            "driver": config.driver,
            "laneSizes": config.lane_sizes,  # Array of per-lane LED counts
            "pattern": config.pattern,
            "iterations": config.iterations,
        }

        # Add optional strip size configuration (NEW - Phase 6)
        if config.short_strip_size is not None:
            config_dict["shortStripSize"] = config.short_strip_size
        if config.long_strip_size is not None:
            config_dict["longStripSize"] = config.long_strip_size
        if config.test_small_strips is not None:
            config_dict["testSmallStrips"] = config.test_small_strips
        if config.test_large_strips is not None:
            config_dict["testLargeStrips"] = config.test_large_strips

        # Add optional lane range configuration (NEW - Phase 6)
        if config.min_lanes is not None:
            config_dict["minLanes"] = config.min_lanes
        if config.max_lanes is not None:
            config_dict["maxLanes"] = config.max_lanes

        return self.send_rpc("configure", [config_dict])

    def set_lane_sizes(self, sizes: list[int]) -> dict[str, Any]:
        """Set per-lane LED counts directly.

        Args:
            sizes: List of LED counts per lane, e.g., [300, 200, 100, 50]

        Returns:
            Response dict with laneSizes, laneCount, totalLeds
        """
        return self.send_rpc("setLaneSizes", [sizes])

    def set_led_count(self, count: int) -> dict[str, Any]:
        """Set uniform LED count for all lanes.

        Args:
            count: LED count to apply to all lanes

        Returns:
            Response dict with ledCount, laneSizes, testCases
        """
        return self.send_rpc("setLedCount", [count])

    def set_pattern(self, pattern: str) -> dict[str, Any]:
        """Set test pattern.

        Args:
            pattern: Pattern name ("MSB_LSB_A", "SOLID_RGB", etc.)

        Returns:
            Response dict with pattern, description
        """
        return self.send_rpc("setPattern", [pattern])

    def set_short_strip_size(self, size: int) -> dict[str, Any]:
        """Set the short strip size (default: 100 LEDs).

        Args:
            size: LED count for short strips

        Returns:
            Response dict with shortStripSize, testCases
        """
        return self.send_rpc("setShortStripSize", [size])

    def set_long_strip_size(self, size: int) -> dict[str, Any]:
        """Set the long strip size (default: 3000 LEDs).

        Args:
            size: LED count for long strips

        Returns:
            Response dict with longStripSize, testCases
        """
        return self.send_rpc("setLongStripSize", [size])

    def set_strip_size_values(self, short: int, long: int) -> dict[str, Any]:
        """Set both short and long strip sizes in one call.

        Args:
            short: LED count for short strips
            long: LED count for long strips

        Returns:
            Response dict with shortStripSize, longStripSize, testCases
        """
        return self.send_rpc("setStripSizeValues", [short, long])

    def set_lane_range(self, min_lanes: int, max_lanes: int) -> dict[str, Any]:
        """Set the lane count range for test matrix generation.

        Args:
            min_lanes: Minimum number of lanes to test
            max_lanes: Maximum number of lanes to test

        Returns:
            Response dict with minLanes, maxLanes, testCases
        """
        return self.send_rpc("setLaneRange", [min_lanes, max_lanes])

    def set_strip_sizes_enabled(
        self, small: bool = True, large: bool = False
    ) -> dict[str, Any]:
        """Enable/disable testing of small and large strip sizes.

        Args:
            small: Enable small strip testing (default: True)
            large: Enable large strip testing (default: False)

        Returns:
            Response dict with testSmallStrips, testLargeStrips, testCases
        """
        return self.send_rpc("setStripSizes", [{"small": small, "large": large}])

    def run_test(self) -> TestResult:
        """Run configured test and return results.

        This function handles the streaming JSONL response from the device.
        The device sends:
        1. REMOTE: {"success": true, "streamMode": true} - RPC acknowledgement
        2. RESULT: {"type": "test_start", ...} - Test start event
        3. RESULT: {"type": "test_complete", ...} - Test complete event

        Returns:
            TestResult object with per-lane details

        Raises:
            RuntimeError: If test execution fails
            TimeoutError: If test does not complete within timeout
        """
        # Send RPC command
        cmd: dict[str, str | list[Any]] = {"method": "runTest", "params": []}
        cmd_str = json.dumps(cmd, separators=(",", ":"))

        # Send command
        self._monitor.write(cmd_str + "\n")

        # Wait for REMOTE: response (RPC acknowledgement)
        response = self._wait_for_response()

        if not response.get("success"):
            raise RuntimeError(f"Test failed: {response.get('error')}")

        # Check if using streaming mode
        if not response.get("streamMode"):
            # Legacy mode - response contains testState directly
            # This path is for backward compatibility if needed
            state = response.get("testState", {})
            results = state.get("results", {})

            lane_results: list[LaneResult] = []
            for detail in results.get("details", []):
                lane_idx = detail["lane"]
                lane_results.append(
                    LaneResult(
                        lane=lane_idx,
                        led_count=detail.get("ledCount", 0),
                        pin=detail.get("pin", lane_idx),
                        passed=detail["passed"],
                        bit_errors=detail["bitErrors"],
                        timing=detail["timing"],
                        expected_bytes=detail.get("expectedBytes"),
                        received_bytes=detail.get("receivedBytes"),
                    )
                )

            return TestResult(
                passed=results["passed"],
                total_tests=results["totalTests"],
                passed_tests=results["passedTests"],
                failed_tests=results["failedTests"],
                duration_ms=state.get("timing", {}).get("durationMs", 0),
                lane_results=lane_results,
            )

        # Streaming mode - parse RESULT: prefixed JSONL events
        STREAM_PREFIX = "RESULT: "
        start = time.time()
        test_complete_data = None

        while time.time() - start < self.timeout:
            # Use read_lines() iterator with short timeout
            for line in self._monitor.read_lines(timeout=0.05):
                if line.startswith(STREAM_PREFIX):
                    json_str = line[len(STREAM_PREFIX) :]
                    try:
                        event = json.loads(json_str)
                        event_type = event.get("type")

                        if event_type == "test_complete":
                            test_complete_data = event
                            break  # Test finished

                    except json.JSONDecodeError as e:
                        print(f"Warning: Failed to parse JSONL: {e}")
                        print(f"  Line: {line}")
                        continue
                break  # Process one line at a time

            if test_complete_data:
                break

        if test_complete_data is None:
            raise TimeoutError(f"Test did not complete within {self.timeout}s")

        # Build TestResult from streaming data
        # Note: Streaming mode does not include per-lane details in test_complete event
        # For full per-lane details, use getState() or wait for future enhancement
        return TestResult(
            passed=test_complete_data["passed"],
            total_tests=test_complete_data["totalTests"],
            passed_tests=test_complete_data["passedTests"],
            failed_tests=test_complete_data["totalTests"]
            - test_complete_data["passedTests"],
            duration_ms=test_complete_data["durationMs"],
            lane_results=[],  # Per-lane details not included in streaming mode yet
        )

    def get_state(self) -> dict[str, Any]:
        """Query current device state.

        Returns:
            State dict with phase, config, lastResults
        """
        return self.send_rpc("getState")

    def get_drivers(self) -> list[str]:
        """Get list of available drivers.

        Returns:
            List of enabled driver names
        """
        response: list[Any] = self.send_rpc("drivers")  # type: ignore[assignment]
        # response is a list of driver dicts with 'name' and 'enabled' keys
        return [str(d["name"]) for d in response if d.get("enabled")]

    def ping(self) -> dict[str, Any]:
        """Health check.

        Returns:
            Response dict with timestamp, uptimeMs, frameCounter
        """
        return self.send_rpc("ping")

    def reset(self) -> dict[str, Any]:
        """Reset device state.

        Returns:
            Response dict with success status
        """
        return self.send_rpc("reset")

    def close(self):
        """Close serial connection."""
        if self._monitor:
            self._monitor.__exit__(None, None, None)

    def __enter__(self):
        """Context manager entry."""
        return self

    def __exit__(self, _exc_type: object, _exc_val: object, _exc_tb: object) -> bool:
        """Context manager exit - ensures serial connection is closed."""
        self.close()
        return False
