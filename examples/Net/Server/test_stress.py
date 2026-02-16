#!/usr/bin/env python3
"""
Phase 3: HTTP Server Stress Test for FastLED Network Example

Tests server stability under various stress conditions:
- Rapid connection/disconnection cycles
- Multiple concurrent connections
- Request flooding
- Server start/stop cycles

Usage:
    uv run python examples/Network/test_stress.py
    uv run python examples/Network/test_stress.py --connections 50 --requests 100
"""

import _thread
import argparse
import subprocess
import sys
import threading
import time
from dataclasses import dataclass
from pathlib import Path

import httpx
from rich.console import Console
from rich.progress import (
    BarColumn,
    Progress,
    SpinnerColumn,
    TaskProgressColumn,
    TextColumn,
)
from rich.table import Table


console = Console()


@dataclass
class StressTestConfig:
    host: str = "localhost"
    port: int = 8080
    num_connections: int = 20
    num_requests: int = 50
    timeout: float = 5.0
    server_start_delay: float = 2.0


class StressTestResults:
    def __init__(self) -> None:
        self.total_requests: int = 0
        self.successful_requests: int = 0
        self.failed_requests: int = 0
        self.connection_errors: int = 0
        self.timeout_errors: int = 0
        self.other_errors: int = 0
        self.response_times: list[float] = []
        self.lock: threading.Lock = threading.Lock()

    def record_success(self, response_time_ms: float):
        with self.lock:
            self.total_requests += 1
            self.successful_requests += 1
            self.response_times.append(response_time_ms)

    def record_failure(self, error_type: str):
        with self.lock:
            self.total_requests += 1
            self.failed_requests += 1
            if error_type == "connection":
                self.connection_errors += 1
            elif error_type == "timeout":
                self.timeout_errors += 1
            else:
                self.other_errors += 1


def start_server(config: StressTestConfig) -> subprocess.Popen[str]:
    """Start HTTP server in background."""
    runner_path = Path(".build/meson-quick/examples/example_runner.exe").absolute()
    dll_path = Path(".build/meson-quick/examples/example-Network.dll").absolute()

    if not runner_path.exists() or not dll_path.exists():
        console.print("[red]✗ Server executable not found - compile first:[/red]")
        console.print("  bash test Network --examples --build")
        sys.exit(1)

    proc = subprocess.Popen(
        [str(runner_path), str(dll_path)],
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
        text=True,
    )

    # Wait for server to start
    time.sleep(config.server_start_delay)

    # Verify server is responsive
    try:
        with httpx.Client(timeout=config.timeout) as client:
            response = client.get(f"http://{config.host}:{config.port}/ping")
            if response.status_code == 200:
                return proc
    except KeyboardInterrupt:
        proc.terminate()
        proc.wait()
        _thread.interrupt_main()
    except Exception as e:
        proc.terminate()
        proc.wait()
        console.print(f"[red]✗ Server not responsive: {e}[/red]")
        sys.exit(1)

    proc.terminate()
    proc.wait()
    console.print("[red]✗ Server failed to start[/red]")
    sys.exit(1)


def make_request(
    config: StressTestConfig, results: StressTestResults, client: httpx.Client
):
    """Make a single HTTP request and record results."""
    url = f"http://{config.host}:{config.port}/"

    try:
        start_time = time.perf_counter()
        response = client.get(url, timeout=config.timeout)
        elapsed_ms = (time.perf_counter() - start_time) * 1000

        if response.status_code == 200:
            results.record_success(elapsed_ms)
        else:
            results.record_failure("http_error")

    except httpx.ConnectError:
        results.record_failure("connection")
    except httpx.TimeoutException:
        results.record_failure("timeout")
    except KeyboardInterrupt:
        _thread.interrupt_main()
    except Exception:
        results.record_failure("other")


def stress_test_concurrent(
    config: StressTestConfig, results: StressTestResults
) -> None:
    """Test: Multiple concurrent connections."""
    console.print("\n[bold cyan]Test 1: Concurrent Connections[/bold cyan]")
    console.print(f"Making {config.num_connections} concurrent requests...")

    with Progress(
        SpinnerColumn(),
        TextColumn("[progress.description]{task.description}"),
        BarColumn(),
        TaskProgressColumn(),
        console=console,
    ) as progress:
        task = progress.add_task("Requesting...", total=config.num_connections)

        threads: list[threading.Thread] = []
        with httpx.Client() as client:
            for _ in range(config.num_connections):
                thread: threading.Thread = threading.Thread(
                    target=make_request, args=(config, results, client)
                )
                thread.start()
                threads.append(thread)

            for thread in threads:
                thread.join()
                progress.update(task, advance=1)

    success_rate = (
        (results.successful_requests / results.total_requests * 100)
        if results.total_requests > 0
        else 0
    )
    console.print(
        f"  Success rate: {success_rate:.1f}% ({results.successful_requests}/{results.total_requests})"
    )


def stress_test_rapid(config: StressTestConfig, results: StressTestResults) -> None:
    """Test: Rapid sequential requests."""
    console.print("\n[bold cyan]Test 2: Rapid Sequential Requests[/bold cyan]")
    console.print(f"Making {config.num_requests} rapid sequential requests...")

    initial_count = results.total_requests

    with Progress(
        SpinnerColumn(),
        TextColumn("[progress.description]{task.description}"),
        BarColumn(),
        TaskProgressColumn(),
        console=console,
    ) as progress:
        task = progress.add_task("Requesting...", total=config.num_requests)

        with httpx.Client() as client:
            for _ in range(config.num_requests):
                make_request(config, results, client)
                progress.update(task, advance=1)

    test_requests = results.total_requests - initial_count
    test_success = results.successful_requests - (
        initial_count - (initial_count - results.successful_requests)
    )
    success_rate = (test_success / test_requests * 100) if test_requests > 0 else 0
    console.print(
        f"  Success rate: {success_rate:.1f}% ({test_success}/{test_requests})"
    )


def display_results(results: StressTestResults):
    """Display test results in a formatted table."""
    console.print("\n[bold]Stress Test Results[/bold]")

    table = Table(title="Summary")
    table.add_column("Metric", style="cyan")
    table.add_column("Value", style="magenta")

    table.add_row("Total Requests", str(results.total_requests))
    table.add_row(
        "Successful",
        f"{results.successful_requests} ({results.successful_requests / results.total_requests * 100:.1f}%)",
    )
    table.add_row(
        "Failed",
        f"{results.failed_requests} ({results.failed_requests / results.total_requests * 100:.1f}%)",
    )
    table.add_row("Connection Errors", str(results.connection_errors))
    table.add_row("Timeout Errors", str(results.timeout_errors))
    table.add_row("Other Errors", str(results.other_errors))

    if results.response_times:
        table.add_row("Min Response Time", f"{min(results.response_times):.1f} ms")
        table.add_row("Max Response Time", f"{max(results.response_times):.1f} ms")
        table.add_row(
            "Avg Response Time",
            f"{sum(results.response_times) / len(results.response_times):.1f} ms",
        )

    console.print()
    console.print(table)

    success_rate = (
        (results.successful_requests / results.total_requests * 100)
        if results.total_requests > 0
        else 0
    )

    if success_rate >= 95:
        console.print(
            "\n[green bold]✓ Phase 3 PASSED - Server handled stress test successfully (≥95% success)[/green bold]"
        )
        return 0
    elif success_rate >= 80:
        console.print(
            f"\n[yellow bold]⚠ Phase 3 MARGINAL - Server mostly stable ({success_rate:.1f}% success, threshold: 95%)[/yellow bold]"
        )
        return 1
    else:
        console.print(
            f"\n[red bold]✗ Phase 3 FAILED - Server unstable under stress ({success_rate:.1f}% success)[/red bold]"
        )
        return 1


def main() -> int:
    parser = argparse.ArgumentParser(
        description="Stress test FastLED Network HTTP server"
    )
    parser.add_argument("--host", default="localhost", help="Server host")
    parser.add_argument("--port", type=int, default=8080, help="Server port")
    parser.add_argument(
        "--connections", type=int, default=20, help="Number of concurrent connections"
    )
    parser.add_argument(
        "--requests", type=int, default=50, help="Number of sequential requests"
    )
    args = parser.parse_args()

    config = StressTestConfig(
        host=args.host,
        port=args.port,
        num_connections=args.connections,
        num_requests=args.requests,
    )

    console.print("[bold]FastLED Network HTTP Server - Phase 3 Stress Test[/bold]")
    console.print(f"Server: http://{config.host}:{config.port}\n")

    console.print("[cyan]Starting server...[/cyan]")
    server_proc = start_server(config)
    console.print("[green]✓ Server started and responding[/green]")

    results = StressTestResults()

    try:
        # Test 1: Concurrent connections
        stress_test_concurrent(config, results)

        # Test 2: Rapid sequential requests
        stress_test_rapid(config, results)

        # Display results
        exit_code = display_results(results)

        return exit_code

    except KeyboardInterrupt:
        console.print("\n[yellow]Test interrupted by user[/yellow]")
        _thread.interrupt_main()
        return 130

    finally:
        console.print("\n[cyan]Stopping server...[/cyan]")
        server_proc.terminate()
        try:
            server_proc.wait(timeout=5)
        except subprocess.TimeoutExpired:
            server_proc.kill()
        console.print("[green]✓ Server stopped[/green]")


if __name__ == "__main__":
    sys.exit(main())
