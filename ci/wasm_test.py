import argparse
import asyncio
import http.server
import multiprocessing
import os
import socketserver
import sys
import time
from pathlib import Path

from playwright.async_api import ConsoleMessage, async_playwright  # type: ignore
from rich.console import Console
from rich.panel import Panel

from ci.util.global_interrupt_handler import handle_keyboard_interrupt_properly


HERE = Path(__file__).parent
PROJECT_ROOT = HERE.parent

console = Console()


def parse_args():
    """Parse command-line arguments"""
    parser = argparse.ArgumentParser(
        description="Run WASM Playwright tests for FastLED examples"
    )
    parser.add_argument(
        "example",
        nargs="?",
        default="wasm",
        help="Example name to test (e.g., 'Blink', 'wasm')",
    )
    parser.add_argument(
        "--gfx",
        default="0",
        help="Graphics mode parameter (default: 0)",
    )
    return parser.parse_args()


# Ensure Playwright browsers are installed
def install_playwright_browsers():
    console.print("[dim]Installing Playwright browsers...[/dim]")
    try:
        # Simulate the `playwright install` command
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


def _run_http_server(port: int, directory: str) -> None:
    """Run HTTP server in a separate process."""
    os.chdir(directory)
    handler = http.server.SimpleHTTPRequestHandler
    with socketserver.TCPServer(("", port), handler) as httpd:
        httpd.serve_forever()


# Start an HTTP server on the dynamic port
def start_http_server(port: int, directory: Path) -> multiprocessing.Process:
    """Start HTTP server using Python's built-in http.server."""
    server_process = multiprocessing.Process(
        target=_run_http_server,
        args=(port, str(directory)),
        daemon=True,
    )
    server_process.start()
    return server_process


async def main() -> None:
    args = parse_args()

    # Print colorful status message
    console.print()
    console.print(
        Panel.fit(
            f"[bold cyan]FastLED WASM Test Runner[/bold cyan]\n\n"
            f"[yellow]Will:[/yellow]\n"
            f"  [green]✓[/green] Launch headless browser demo for [bold]{args.example}[/bold]\n"
            f"  [green]✓[/green] Run for 5 seconds\n"
            f"  [green]✓[/green] Verify FastLED initialization & rendering\n"
            f"  [green]✓[/green] Exit automatically",
            title="[bold magenta]Test Plan[/bold magenta]",
            border_style="cyan",
        )
    )
    console.print()

    install_playwright_browsers()
    # Find an available port
    port = (
        8080  # Todo, figure out why the http server ignores any port other than 8080.
    )
    console.print(f"[dim]Using port: {port}[/dim]")

    # Start the HTTP server
    os.chdir(str(PROJECT_ROOT))
    directory = Path(f"examples/{args.example}/fastled_js")
    console.print(f"[cyan]Testing example:[/cyan] [bold]{args.example}[/bold]")
    console.print(f"[dim]Server directory: {directory}[/dim]")
    server_process = start_http_server(port=port, directory=directory)

    try:
        # Give the server some time to start
        time.sleep(2)

        # Use Playwright to test the server
        async with async_playwright() as p:
            browser = await p.chromium.launch()
            page = await browser.new_page()

            try:
                test_url = f"http://localhost:{port}?gfx={args.gfx}"
                console.print(f"[dim]Navigating to: {test_url}[/dim]")
                await page.goto(test_url, timeout=30000)

                # Listen for console messages
                def console_log_handler(msg: ConsoleMessage) -> None:
                    if "INVALID_OPERATION" in msg.text:
                        console.print(
                            "[bold red]INVALID_OPERATION detected in console log[/bold red]"
                        )
                        raise Exception("INVALID_OPERATION detected in console log")

                page.on("console", console_log_handler)

                # Evaluate and monitor window.frameCallCount
                await page.evaluate(
                    """
                    window.frameCallCount = 0;
                    globalThis.FastLED_onFrame = (jsonStr) => {
                        console.log('FastLED_onFrame called with:', jsonStr);
                        window.frameCallCount++;
                    };
                """
                )
                await page.wait_for_timeout(5000)

                call_count = await page.evaluate("window.frameCallCount")
                if call_count > 0:
                    console.print()
                    console.print(
                        f"[bold green]✓ Success:[/bold green] FastLED.js initialized and rendered [bold]{call_count}[/bold] frames"
                    )
                    console.print()
                else:
                    console.print()
                    console.print(
                        "[bold red]✗ Error:[/bold red] FastLED.js failed to initialize within 5 seconds",
                    )
                    raise Exception("FastLED.js failed to initialize")

                handle_keyboard_interrupt_properly()
            except KeyboardInterrupt:
                handle_keyboard_interrupt_properly()
                raise
            except Exception as e:
                console.print(f"[bold red]✗ An error occurred:[/bold red] {e}")
                raise Exception(f"An error occurred: {e}") from e

            finally:
                await browser.close()

    finally:
        # Terminate the server process
        server_process.terminate()


# Run the main function
if __name__ == "__main__":
    sys.exit(asyncio.run(main()))
