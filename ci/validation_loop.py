"""
Validation loop orchestration for testing LED drivers with variable lane sizes.

This module provides functions to run comprehensive validation test matrices,
supporting both uniform and asymmetric lane configurations.

Usage:
    # Basic validation with default configs
    python validation_loop.py --port /dev/ttyUSB0

    # Custom lane sizes
    python validation_loop.py --port COM3 --lane-sizes '[[100,100],[300,200,100]]'

    # Use presets
    python validation_loop.py --port COM3 --presets uniform_medium decreasing --lane-count 4
"""

import argparse
import json
import sys
from typing import Any, Callable

from validation_agent import TestConfig, ValidationAgent

from ci.util.global_interrupt_handler import handle_keyboard_interrupt_properly


# Predefined lane size configurations for testing
LANE_SIZE_PRESETS: dict[str, Callable[[int], list[int]]] = {
    # Uniform configurations (all lanes same size)
    "uniform_small": lambda n: [10] * n,
    "uniform_medium": lambda n: [100] * n,
    "uniform_large": lambda n: [300] * n,
    # Asymmetric configurations (decreasing sizes)
    "decreasing": lambda n: [300 - i * 50 for i in range(n)],  # [300, 250, 200, ...]
    "half_each": lambda n: [300 // (2**i) for i in range(n)],  # [300, 150, 75, ...]
    # Edge cases
    "one_big_rest_small": lambda n: [300] + [10] * (n - 1),  # [300, 10, 10, ...]
    "one_small_rest_big": lambda n: [1] + [300] * (n - 1),  # [1, 300, 300, ...]
    "alternating": lambda n: [
        300 if i % 2 == 0 else 50 for i in range(n)
    ],  # [300, 50, 300, 50]
}


def generate_lane_configs(
    lane_count: int,
    presets: list[str] | None = None,
    custom_configs: list[list[int]] | None = None,
) -> list[list[int]]:
    """Generate lane size configurations for testing.

    Args:
        lane_count: Number of lanes
        presets: List of preset names from LANE_SIZE_PRESETS
        custom_configs: Custom lane size arrays to include

    Returns:
        List of lane size configurations to test
    """
    configs = []

    # Add preset configurations
    if presets:
        for preset_name in presets:
            if preset_name in LANE_SIZE_PRESETS:
                configs.append(LANE_SIZE_PRESETS[preset_name](lane_count))

    # Add custom configurations
    if custom_configs:
        for custom in custom_configs:
            if len(custom) == lane_count:
                configs.append(custom)

    # Default: uniform medium if nothing specified
    if not configs:
        configs.append([100] * lane_count)

    return configs


def run_validation_loop(
    port: str,
    drivers: list[str] | None = None,
    lane_size_configs: list[list[int]] | None = None,
    patterns: list[str] | None = None,
    iterations: int = 1,
) -> dict[str, Any]:
    """Run comprehensive validation loop across all configurations.

    Args:
        port: Serial port
        drivers: List of drivers to test (None = auto-detect)
        lane_size_configs: List of lane size arrays, e.g., [[100, 100], [300, 200, 100]]
                          Each inner array specifies per-lane LED counts
        patterns: List of patterns to test
        iterations: Number of test iterations per configuration

    Returns:
        Summary of all test results
    """
    agent = ValidationAgent(port)

    try:
        # Get available drivers if not specified
        if drivers is None:
            drivers = agent.get_drivers()
            if drivers:
                print(f"Auto-detected drivers: {', '.join(drivers)}")

        # Default lane configurations: test various asymmetric setups
        if lane_size_configs is None:
            lane_size_configs = [
                [100],  # 1 lane
                [100, 100],  # 2 lanes, uniform
                [300, 100],  # 2 lanes, asymmetric
                [100, 100, 100, 100],  # 4 lanes, uniform
                [300, 200, 100, 50],  # 4 lanes, decreasing
                [1, 300],  # Edge case: 1 LED + 300 LEDs
            ]

        patterns = patterns or ["MSB_LSB_A", "SOLID_RGB"]

        # Results accumulator
        all_results = []
        total_passed = 0
        total_failed = 0

        # Test matrix loop
        for driver in drivers or []:
            for lane_sizes in lane_size_configs or []:
                for pattern in patterns or []:
                    config = TestConfig(
                        driver=driver,
                        lane_sizes=lane_sizes,
                        pattern=pattern,
                        iterations=iterations,
                    )

                    # Pretty print the lane configuration
                    lane_str = ",".join(str(s) for s in lane_sizes)
                    print(f"\n{'=' * 60}")
                    print(
                        f"Testing: {driver} | lanes=[{lane_str}] ({config.total_leds} total) | {pattern}"
                    )
                    print(f"{'=' * 60}")

                    # Configure
                    config_response = agent.configure(config)
                    if not config_response.get("success"):
                        print(f"  Config failed: {config_response.get('error')}")
                        all_results.append(
                            {
                                "config": {
                                    "driver": config.driver,
                                    "laneSizes": config.lane_sizes,
                                    "pattern": config.pattern,
                                },
                                "error": config_response.get("error"),
                            }
                        )
                        total_failed += 1
                        continue

                    # Run test
                    try:
                        result = agent.run_test()

                        status = "PASS" if result.passed else "FAIL"
                        print(f"  Result: {status}")
                        print(f"  Tests: {result.passed_tests}/{result.total_tests}")
                        print(f"  Duration: {result.duration_ms}ms")

                        # Print per-lane results
                        for lr in result.lane_results:
                            lane_status = "✓" if lr.passed else "✗"
                            print(
                                f"    Lane {lr.lane} ({lr.led_count} LEDs): {lane_status}",
                                end="",
                            )
                            if not lr.passed:
                                print(f" - {lr.bit_errors} bit errors", end="")
                            print()

                        all_results.append(
                            {
                                "config": {
                                    "driver": config.driver,
                                    "laneSizes": config.lane_sizes,
                                    "pattern": config.pattern,
                                },
                                "result": {
                                    "passed": result.passed,
                                    "totalTests": result.total_tests,
                                    "passedTests": result.passed_tests,
                                    "failedTests": result.failed_tests,
                                    "durationMs": result.duration_ms,
                                    "laneResults": [
                                        {
                                            "lane": lr.lane,
                                            "ledCount": lr.led_count,
                                            "passed": lr.passed,
                                            "bitErrors": lr.bit_errors,
                                        }
                                        for lr in result.lane_results
                                    ],
                                },
                            }
                        )

                        if result.passed:
                            total_passed += 1
                        else:
                            total_failed += 1

                    except KeyboardInterrupt:
                        print("\n\nKeyboardInterrupt: Stopping validation loop")
                        handle_keyboard_interrupt_properly()
                        raise
                    except Exception as e:
                        print(f"  Error: {e}")
                        all_results.append(
                            {
                                "config": {
                                    "driver": config.driver,
                                    "laneSizes": config.lane_sizes,
                                    "pattern": config.pattern,
                                },
                                "error": str(e),
                            }
                        )
                        total_failed += 1

                    # Reset between tests
                    agent.reset()

        # Summary
        print(f"\n{'=' * 60}")
        print("VALIDATION COMPLETE")
        print(f"{'=' * 60}")
        print(f"Total tests: {total_passed + total_failed}")
        print(f"Passed: {total_passed}")
        print(f"Failed: {total_failed}")

        return {
            "summary": {
                "total": total_passed + total_failed,
                "passed": total_passed,
                "failed": total_failed,
            },
            "results": all_results,
        }

    except KeyboardInterrupt:
        print("\nKeyboardInterrupt: Cancelling validation loop")
        handle_keyboard_interrupt_properly()
        raise
    finally:
        agent.close()


# CLI entry point
if __name__ == "__main__":
    parser = argparse.ArgumentParser(
        description="FastLED Validation Agent Loop",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
Examples:
  # Test all drivers with default lane configurations
  %(prog)s -p COM3

  # Test PARLIO with custom lane sizes (JSON array)
  %(prog)s -p COM3 -d PARLIO --lane-sizes '[[100, 100], [300, 200, 100]]'

  # Test with preset configurations
  %(prog)s -p COM3 --presets uniform_medium decreasing --lane-count 4

  # Single asymmetric test
  %(prog)s -p COM3 -d RMT --lane-sizes '[[300, 100, 50]]'

Lane Size Presets:
  uniform_small    - All lanes 10 LEDs
  uniform_medium   - All lanes 100 LEDs
  uniform_large    - All lanes 300 LEDs
  decreasing       - [300, 250, 200, ...] decreasing by 50
  half_each        - [300, 150, 75, ...] each half previous
  one_big_rest_small - [300, 10, 10, ...]
  one_small_rest_big - [1, 300, 300, ...]
  alternating      - [300, 50, 300, 50, ...]
        """,
    )
    parser.add_argument("--port", "-p", required=True, help="Serial port")
    parser.add_argument("--drivers", "-d", nargs="+", help="Drivers to test")
    parser.add_argument(
        "--lane-sizes",
        type=str,
        help="JSON array of lane size configs, e.g., '[[100,100],[300,200,100]]'",
    )
    parser.add_argument(
        "--presets",
        nargs="+",
        choices=list(LANE_SIZE_PRESETS.keys()),
        help="Use preset lane configurations",
    )
    parser.add_argument(
        "--lane-count", type=int, default=4, help="Lane count for presets (default: 4)"
    )
    parser.add_argument("--patterns", nargs="+", help="Patterns to test")
    parser.add_argument(
        "--iterations", "-i", type=int, default=1, help="Test iterations"
    )

    args = parser.parse_args()

    # Build lane size configurations
    lane_size_configs = None

    if args.lane_sizes:
        # Parse JSON array of lane configs
        lane_size_configs = json.loads(args.lane_sizes)
    elif args.presets:
        # Generate from presets
        lane_size_configs = generate_lane_configs(
            lane_count=args.lane_count, presets=args.presets
        )

    try:
        results = run_validation_loop(
            port=args.port,
            drivers=args.drivers,
            lane_size_configs=lane_size_configs,
            patterns=args.patterns,
            iterations=args.iterations,
        )

        # Exit with error code if any tests failed
        sys.exit(0 if results["summary"]["failed"] == 0 else 1)
    except KeyboardInterrupt:
        print("\nValidation cancelled by user")
        handle_keyboard_interrupt_properly()
        sys.exit(130)  # Standard exit code for SIGINT
