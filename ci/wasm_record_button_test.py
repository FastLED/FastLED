from ci.util.global_interrupt_handler import handle_keyboard_interrupt_properly


#!/usr/bin/env python3
"""
Playwright test for FastLED WASM Record Button functionality.

Tests that the record button works correctly after worker initialization.
This test is designed to reproduce the "Worker not active, cannot record" bug.

Usage:
    uv run python ci/wasm_record_button_test.py Blink
    uv run python ci/wasm_record_button_test.py AnimartrixRing --viewport mobile
    uv run python ci/wasm_record_button_test.py DemoReel100 --timeout 30
"""

import argparse
import asyncio
import os
import sys
import time
from pathlib import Path
from typing import Any, TypedDict

from playwright.async_api import ConsoleMessage, Page, async_playwright
from rich.console import Console
from rich.panel import Panel
from rich.table import Table


HERE = Path(__file__).parent
PROJECT_ROOT = HERE.parent

console = Console()


class ViewportSize(TypedDict):
    """Type definition for viewport size matching playwright's ViewportSize."""

    width: int
    height: int


# Viewport configurations
VIEWPORTS: dict[str, ViewportSize] = {
    "mobile": {"width": 375, "height": 667},
    "tablet": {"width": 768, "height": 1024},
    "desktop": {"width": 1920, "height": 1080},
    "ultrawide": {"width": 3440, "height": 1440},
}


def parse_args():
    """Parse command-line arguments"""
    parser = argparse.ArgumentParser(
        description="Test WASM record button functionality for FastLED examples"
    )
    parser.add_argument(
        "example",
        help="Example name to test (e.g., 'Blink', 'AnimartrixRing')",
    )
    parser.add_argument(
        "--viewport",
        default="desktop",
        choices=list(VIEWPORTS.keys()),
        help="Viewport size to test (default: desktop)",
    )
    parser.add_argument(
        "--timeout",
        type=int,
        default=30,
        help="Timeout in seconds for worker initialization (default: 30)",
    )
    parser.add_argument(
        "--headless",
        action="store_true",
        default=True,
        help="Run browser in headless mode (default: True)",
    )
    parser.add_argument(
        "--no-headless",
        action="store_false",
        dest="headless",
        help="Run browser with visible UI for debugging",
    )
    return parser.parse_args()


def install_playwright_browsers():
    """Ensure Playwright browsers are installed"""
    console.print("[dim]Checking Playwright browsers...[/dim]")
    try:
        os.system(f"{sys.executable} -m playwright install chromium")
        console.print("[dim]Playwright browsers ready.[/dim]")
    except KeyboardInterrupt:
        handle_keyboard_interrupt_properly()
        raise
    except Exception as e:
        console.print(
            f"[bold red]Failed to install Playwright browsers:[/bold red] {e}"
        )
        sys.exit(1)


def start_http_server(port: int, directory: Path):
    """Start HTTP server for serving WASM files"""
    from fastled import Test  # type: ignore

    server_process = Test.spawn_http_server(
        directory=directory, port=port, open_browser=False
    )
    return server_process


class ConsoleLogCapture:
    """Captures and analyzes console logs from the browser"""

    def __init__(self):
        self.logs: list[dict[str, str]] = []
        self.worker_logs: list[str] = []
        self.errors: list[str] = []

    def handle_message(self, msg: ConsoleMessage) -> None:
        """Handle console message from browser"""
        text = msg.text
        msg_type = msg.type

        # Store all logs
        self.logs.append({"type": msg_type, "text": text})

        # Track worker initialization logs (🔧 prefix)
        if "🔧" in text:
            self.worker_logs.append(text)

        # Track errors
        if msg_type == "error" or "ERROR" in text or "Error:" in text:
            self.errors.append(text)

    def get_worker_logs(self) -> list[str]:
        """Get all worker initialization debug logs"""
        return self.worker_logs

    def get_errors(self) -> list[str]:
        """Get all error messages"""
        return self.errors

    def has_record_error(self) -> bool:
        """Check if 'Worker not active' error was logged"""
        return any("Worker not active" in log for log in self.errors)

    def print_summary(self):
        """Print summary of captured logs"""
        console.print("\n[bold cyan]Console Log Summary:[/bold cyan]")
        console.print(f"  Total logs: {len(self.logs)}")
        console.print(f"  Worker logs (🔧): {len(self.worker_logs)}")
        console.print(f"  Errors: {len(self.errors)}")

        if self.worker_logs:
            console.print("\n[bold yellow]Worker Initialization Logs:[/bold yellow]")
            for log in self.worker_logs:
                console.print(f"  {log}")

        if self.errors:
            console.print("\n[bold red]Error Logs:[/bold red]")
            for error in self.errors[:10]:  # Show first 10 errors
                console.print(f"  {error}")
            if len(self.errors) > 10:
                console.print(f"  ... and {len(self.errors) - 10} more errors")


async def check_worker_state(page: Page) -> dict[str, bool | None]:
    """Check the worker state flags in the browser"""
    try:
        state = await page.evaluate(
            """
            () => {
                return {
                    workerManagerExists: typeof window.fastLEDWorkerManager !== 'undefined',
                    isWorkerActive: window.fastLEDWorkerManager?.isWorkerActive ?? null,
                    controllerExists: typeof window.fastLEDController !== 'undefined',
                    controllerWorkerMode: window.fastLEDController?.workerMode ?? null,
                    asyncControllerExists: typeof window.fastLEDAsyncController !== 'undefined',
                    asyncControllerWorkerMode: window.fastLEDAsyncController?.workerMode ?? null,
                };
            }
        """
        )
        return state
    except KeyboardInterrupt:
        handle_keyboard_interrupt_properly()
        raise
    except Exception as e:
        console.print(f"[yellow]Warning: Could not check worker state: {e}[/yellow]")
        return {}


async def check_record_button_state(page: Page) -> dict[str, bool | None]:
    """Check the record button state in the browser"""
    try:
        state = await page.evaluate(
            """
            () => {
                const btn = document.getElementById('record-btn');
                if (!btn) return { exists: false };

                const computedStyle = window.getComputedStyle(btn);
                return {
                    exists: true,
                    visible: computedStyle.display !== 'none' &&
                             computedStyle.visibility !== 'hidden' &&
                             computedStyle.opacity !== '0',
                    enabled: !btn.disabled,
                    clickable: btn.offsetParent !== null,
                    text: btn.textContent?.trim() ?? '',
                    classList: Array.from(btn.classList),
                };
            }
        """
        )
        return state
    except KeyboardInterrupt:
        handle_keyboard_interrupt_properly()
        raise
    except Exception as e:
        console.print(f"[yellow]Warning: Could not check button state: {e}[/yellow]")
        return {"exists": False}


def print_state_table(worker_state: dict[str, Any], button_state: dict[str, Any]):
    """Print a formatted table of worker and button states"""
    table = Table(title="Worker & Button State", show_header=True, header_style="bold")
    table.add_column("Component", style="cyan")
    table.add_column("Property", style="yellow")
    table.add_column("Value", style="green")

    # Worker state
    table.add_row(
        "WorkerManager",
        "exists",
        str(worker_state.get("workerManagerExists", "unknown")),  # pyright: ignore[reportUnknownMemberType, reportUnknownArgumentType]
    )
    table.add_row(
        "WorkerManager",
        "isWorkerActive",
        str(worker_state.get("isWorkerActive", "unknown")),  # pyright: ignore[reportUnknownMemberType, reportUnknownArgumentType]
    )
    table.add_row(
        "Controller",
        "exists",
        str(worker_state.get("controllerExists", "unknown")),  # pyright: ignore[reportUnknownMemberType, reportUnknownArgumentType]
    )
    table.add_row(
        "Controller",
        "workerMode",
        str(worker_state.get("controllerWorkerMode", "unknown")),  # pyright: ignore[reportUnknownMemberType, reportUnknownArgumentType]
    )
    table.add_row(
        "AsyncController",
        "exists",
        str(worker_state.get("asyncControllerExists", "unknown")),  # pyright: ignore[reportUnknownMemberType, reportUnknownArgumentType]
    )
    table.add_row(
        "AsyncController",
        "workerMode",
        str(worker_state.get("asyncControllerWorkerMode", "unknown")),  # pyright: ignore[reportUnknownMemberType, reportUnknownArgumentType]
    )

    # Button state
    table.add_row("RecordButton", "exists", str(button_state.get("exists", "unknown")))  # pyright: ignore[reportUnknownMemberType, reportUnknownArgumentType]
    if button_state.get("exists"):
        table.add_row(
            "RecordButton",
            "visible",
            str(button_state.get("visible", "unknown")),  # pyright: ignore[reportUnknownMemberType, reportUnknownArgumentType]
        )
        table.add_row(
            "RecordButton",
            "enabled",
            str(button_state.get("enabled", "unknown")),  # pyright: ignore[reportUnknownMemberType, reportUnknownArgumentType]
        )
        table.add_row(
            "RecordButton",
            "clickable",
            str(button_state.get("clickable", "unknown")),  # pyright: ignore[reportUnknownMemberType, reportUnknownArgumentType]
        )
        table.add_row("RecordButton", "text", str(button_state.get("text", "")))  # pyright: ignore[reportUnknownMemberType, reportUnknownArgumentType]

    console.print()
    console.print(table)
    console.print()


async def test_record_button(page: Page, log_capture: ConsoleLogCapture) -> bool:
    """
    Test the record button functionality.

    Returns:
        True if test passes, False if bug is reproduced
    """
    console.print("[bold cyan]Testing record button...[/bold cyan]")

    # Check initial worker state
    console.print("[dim]Checking worker state before button click...[/dim]")
    worker_state = await check_worker_state(page)
    button_state = await check_record_button_state(page)

    print_state_table(worker_state, button_state)

    # Check if button exists
    if not button_state.get("exists"):
        console.print("[bold red]✗ Record button not found in DOM[/bold red]")
        return False

    # Check if button is visible and enabled
    if not button_state.get("visible"):
        console.print("[bold red]✗ Record button is not visible[/bold red]")
        return False

    if not button_state.get("enabled"):
        console.print("[bold yellow]⚠ Record button is disabled[/bold yellow]")

    # Try to click the button
    console.print("[dim]Clicking record button...[/dim]")
    try:
        await page.click("#record-btn", timeout=5000)
        console.print("[green]✓ Button clicked successfully[/green]")
    except KeyboardInterrupt:
        handle_keyboard_interrupt_properly()
        raise
    except Exception as e:
        console.print(f"[bold red]✗ Failed to click button: {e}[/bold red]")
        return False

    # Wait a moment for any error to appear
    await page.wait_for_timeout(1000)

    # Check for the "Worker not active" error
    if log_capture.has_record_error():
        console.print(
            "[bold red]✗ BUG REPRODUCED: 'Worker not active, cannot record' error detected[/bold red]"
        )
        return False

    # Check if recording state changed
    try:
        recording_state = await page.evaluate(
            """
            () => {
                return {
                    isRecording: window.fastLEDController?.isRecording ?? null,
                    asyncIsRecording: window.fastLEDAsyncController?.isRecording ?? null,
                };
            }
        """
        )

        if recording_state.get("isRecording") or recording_state.get(
            "asyncIsRecording"
        ):
            console.print("[bold green]✓ Recording started successfully![/bold green]")
            return True
        else:
            console.print(
                "[bold yellow]⚠ Button clicked but recording state did not change[/bold yellow]"
            )
            console.print(f"[dim]Recording state: {recording_state}[/dim]")
            # Check worker state again after click
            worker_state_after = await check_worker_state(page)
            print_state_table(worker_state_after, button_state)
            return False
    except KeyboardInterrupt:
        handle_keyboard_interrupt_properly()
        raise
    except Exception as e:
        console.print(f"[yellow]Warning: Could not check recording state: {e}[/yellow]")
        return False


async def main() -> int:
    args = parse_args()

    # Print test plan
    viewport_config = VIEWPORTS[args.viewport]
    console.print()
    console.print(
        Panel.fit(
            f"[bold cyan]FastLED WASM Record Button Test[/bold cyan]\n\n"
            f"[yellow]Will:[/yellow]\n"
            f"  [green]1.[/green] Launch browser at [bold]{args.viewport}[/bold] resolution ({viewport_config['width']}x{viewport_config['height']})\n"
            f"  [green]2.[/green] Load [bold]{args.example}[/bold] WASM example\n"
            f"  [green]3.[/green] Wait for worker initialization (timeout: {args.timeout}s)\n"
            f"  [green]4.[/green] Click record button and verify functionality\n"
            f"  [green]5.[/green] Report worker state and debug logs",
            title="[bold magenta]Test Plan[/bold magenta]",
            border_style="cyan",
        )
    )
    console.print()

    install_playwright_browsers()

    # Setup server
    port = 8080
    os.chdir(str(PROJECT_ROOT))
    directory = Path(f"examples/{args.example}/fastled_js")

    if not directory.exists():
        console.print(f"[bold red]✗ Error:[/bold red] Directory not found: {directory}")
        console.print(
            f"[yellow]Hint:[/yellow] Run: uv run ci/wasm_compile.py examples/{args.example} --just-compile"
        )
        return 1

    console.print(f"[cyan]Testing example:[/cyan] [bold]{args.example}[/bold]")
    console.print(f"[dim]Server directory: {directory}[/dim]")

    server_process = start_http_server(port=port, directory=directory)

    try:
        # Give server time to start
        time.sleep(2)

        # Launch browser
        async with async_playwright() as p:
            browser = await p.chromium.launch(headless=args.headless)
            page = await browser.new_page(viewport=viewport_config)

            # Setup console log capture
            log_capture = ConsoleLogCapture()
            page.on("console", log_capture.handle_message)

            try:
                test_url = f"http://localhost:{port}"
                console.print(f"[dim]Navigating to: {test_url}[/dim]")
                await page.goto(test_url, timeout=30000)

                # Wait for worker initialization
                console.print(
                    f"[dim]Waiting for worker initialization (timeout: {args.timeout}s)...[/dim]"
                )

                try:
                    # Wait for isWorkerActive to become true
                    await page.wait_for_function(
                        """
                        () => {
                            return window.fastLEDWorkerManager?.isWorkerActive === true;
                        }
                    """,
                        timeout=args.timeout * 1000,
                    )
                    console.print("[green]✓ Worker initialized successfully[/green]")
                except KeyboardInterrupt:
                    handle_keyboard_interrupt_properly()
                    raise
                except Exception as e:
                    console.print(
                        f"[bold yellow]⚠ Worker initialization timeout:[/bold yellow] {e}"
                    )
                    console.print("[dim]Proceeding with test to capture state...[/dim]")

                # Additional wait for UI to stabilize
                await page.wait_for_timeout(2000)

                # Run the record button test
                test_passed = await test_record_button(page, log_capture)

                # Print console log summary
                log_capture.print_summary()

                # Final result
                console.print()
                if test_passed:
                    console.print(
                        Panel.fit(
                            "[bold green]✓ TEST PASSED[/bold green]\n\n"
                            "Record button works correctly!",
                            border_style="green",
                        )
                    )
                    return 0
                else:
                    console.print(
                        Panel.fit(
                            "[bold red]✗ TEST FAILED[/bold red]\n\n"
                            "Record button bug reproduced.\n"
                            "See diagnostics above for details.",
                            border_style="red",
                        )
                    )
                    return 1

            except KeyboardInterrupt:
                handle_keyboard_interrupt_properly()
                console.print("\n[yellow]Test interrupted by user[/yellow]")
                raise
            except Exception as e:
                console.print(f"[bold red]✗ An error occurred:[/bold red] {e}")
                log_capture.print_summary()
                raise

            finally:
                await browser.close()

    except KeyboardInterrupt:
        handle_keyboard_interrupt_properly()
        console.print("\n[yellow]Test interrupted by user[/yellow]")
        raise
    finally:
        # Terminate server
        server_process.terminate()


if __name__ == "__main__":
    try:
        exit_code = asyncio.run(main())
        sys.exit(exit_code)
    except KeyboardInterrupt:
        handle_keyboard_interrupt_properly()
        raise
        console.print("\n[yellow]Interrupted[/yellow]")
        sys.exit(130)
