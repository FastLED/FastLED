#!/usr/bin/env python3
"""
HTTP Server Test Client for FastLED Network Example

Tests the Network.ino HTTP server using httpx.

Usage:
    uv run python examples/Network/test_client.py
    uv run python examples/Network/test_client.py --host 192.168.1.100 --port 80
    uv run pytest examples/Network/test_client.py
"""

import _thread
import argparse
import sys
import time
from dataclasses import dataclass

import httpx
from rich.console import Console
from rich.table import Table


console = Console()


@dataclass
class TestConfig:
    host: str = "localhost"
    port: int = 8080
    num_requests: int = 5
    max_retries: int = 10
    retry_delay: float = 0.5
    timeout: float = 5.0


@dataclass
class RequestResult:
    success: bool
    status_code: int | None
    response_text: str | None
    response_time_ms: float
    error_message: str | None = None


def wait_for_server(config: TestConfig) -> bool:
    """Wait for server to become available with retry logic."""
    url = f"http://{config.host}:{config.port}/ping"

    console.print(f"Waiting for server at {url}...")

    for attempt in range(1, config.max_retries + 1):
        try:
            with httpx.Client(timeout=config.timeout) as client:
                response = client.get(url)
                if response.status_code == 200:
                    console.print("✓ Server is ready!", style="green")
                    return True
        except (httpx.ConnectError, httpx.TimeoutException):
            delay = config.retry_delay * (2 ** (attempt - 1))
            console.print(
                f"  Attempt {attempt}/{config.max_retries}: Not ready (retry in {delay:.1f}s)"
            )
            if attempt < config.max_retries:
                time.sleep(delay)
        except KeyboardInterrupt:
            console.print("\nInterrupted by user", style="yellow")
            _thread.interrupt_main()
            raise

    console.print("✗ Server not reachable", style="red")
    return False


def send_request(client: httpx.Client, url: str, config: TestConfig) -> RequestResult:
    """Send HTTP GET request and measure response time."""
    try:
        start_time = time.perf_counter()
        response = client.get(url, timeout=config.timeout)
        elapsed_ms = (time.perf_counter() - start_time) * 1000

        return RequestResult(
            success=response.status_code == 200,
            status_code=response.status_code,
            response_text=response.text.strip(),
            response_time_ms=elapsed_ms,
        )
    except httpx.ConnectError:
        return RequestResult(False, None, None, 0.0, "Connection refused")
    except httpx.TimeoutException:
        return RequestResult(False, None, None, 0.0, "Request timeout")
    except KeyboardInterrupt:
        _thread.interrupt_main()
        raise
    except Exception as e:
        return RequestResult(False, None, None, 0.0, str(e))


def run_tests(config: TestConfig) -> list[RequestResult]:
    """Run test sequence with multiple requests."""
    url = f"http://{config.host}:{config.port}/"
    results: list[RequestResult] = []

    console.print(f"\nSending {config.num_requests} requests to {url}...\n")

    with httpx.Client() as client:
        for i in range(1, config.num_requests + 1):
            result = send_request(client, url, config)
            results.append(result)

            if result.success:
                console.print(
                    f"Request {i}: ✓ {result.status_code} ({result.response_time_ms:.1f} ms)",
                    style="green",
                )
                console.print(f"  Response: {result.response_text!r}")
            else:
                console.print(f"Request {i}: ✗ {result.error_message}", style="red")

            time.sleep(0.1)  # Small delay between requests

    return results


def display_statistics(results: list[RequestResult]) -> None:
    """Display test statistics."""
    successful = [r for r in results if r.success]
    failed = [r for r in results if not r.success]

    if not successful:
        console.print("\n✗ All requests failed", style="red")
        return

    times = [r.response_time_ms for r in successful]

    table = Table(title="Test Statistics")
    table.add_column("Metric", style="cyan")
    table.add_column("Value", style="magenta")

    table.add_row("Total Requests", str(len(results)))
    table.add_row(
        "Successful", f"{len(successful)} ({len(successful) / len(results) * 100:.0f}%)"
    )
    table.add_row("Failed", f"{len(failed)} ({len(failed) / len(results) * 100:.0f}%)")
    table.add_row("Min Response Time", f"{min(times):.1f} ms")
    table.add_row("Max Response Time", f"{max(times):.1f} ms")
    table.add_row("Avg Response Time", f"{sum(times) / len(times):.1f} ms")

    console.print()
    console.print(table)

    if len(successful) == len(results):
        console.print("\n✓ All tests passed!", style="green bold")
    else:
        console.print(f"\n⚠ {len(failed)} test(s) failed", style="yellow bold")


def main() -> int:
    """Main entry point."""
    parser = argparse.ArgumentParser(description="Test FastLED Network example")
    parser.add_argument("--host", default="localhost", help="Server host")
    parser.add_argument("--port", type=int, default=8080, help="Server port")
    parser.add_argument("--requests", type=int, default=5, help="Number of requests")
    args = parser.parse_args()

    config = TestConfig(host=args.host, port=args.port, num_requests=args.requests)

    console.print("[bold]FastLED Network Example Test Client[/bold]")
    console.print(f"Server: http://{config.host}:{config.port}\n")

    # Wait for server
    if not wait_for_server(config):
        console.print("\nERROR: Server not available", style="red bold")
        console.print("\nTroubleshooting:")
        console.print("1. Compile: bash compile posix --examples Network")
        console.print("2. Run: .build/meson-quick/examples/Network.exe")
        return 1

    # Run tests
    results = run_tests(config)

    # Display statistics
    display_statistics(results)

    return 0 if all(r.success for r in results) else 1


def test_network_example() -> None:
    """Pytest test for Network example."""
    config = TestConfig(num_requests=3, max_retries=5)

    if not wait_for_server(config):
        raise RuntimeError("Server not available")

    url = f"http://{config.host}:{config.port}/"
    with httpx.Client() as client:
        result = send_request(client, url, config)
        assert result.success, f"Request failed: {result.error_message}"
        assert result.status_code == 200
        assert result.response_time_ms < 1000


if __name__ == "__main__":
    try:
        sys.exit(main())
    except KeyboardInterrupt:
        console.print("\nTest interrupted", style="yellow")
        _thread.interrupt_main()
        sys.exit(130)
