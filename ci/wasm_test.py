import argparse
import asyncio
import http.server
import multiprocessing
import os
import socket
import socketserver
import sys
import time
from pathlib import Path

from playwright.async_api import ConsoleMessage, async_playwright
from rich.console import Console
from rich.panel import Panel

from ci.util.global_interrupt_handler import handle_keyboard_interrupt


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
    except KeyboardInterrupt as ki:
        handle_keyboard_interrupt(ki)
        raise
    except Exception as e:
        console.print(
            f"[bold red]Failed to install Playwright browsers:[/bold red] {e}"
        )
        sys.exit(1)


def _find_free_port(start: int = 8080) -> int:
    """Find a free TCP port starting from `start`."""
    for port in range(start, start + 100):
        with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as s:
            try:
                s.bind(("", port))
                return port
            except OSError:
                continue
    raise OSError(f"No free port found in range {start}–{start + 99}")


class _ReuseAddrTCPServer(socketserver.TCPServer):
    allow_reuse_address = True


def _run_http_server(port: int, directory: str) -> None:
    """Run HTTP server in a separate process."""
    os.chdir(directory)

    class _MimeHandler(http.server.SimpleHTTPRequestHandler):
        """Handler with correct MIME types for ES module scripts."""

        extensions_map = {
            **http.server.SimpleHTTPRequestHandler.extensions_map,
            ".js": "application/javascript",
            ".mjs": "application/javascript",
            ".wasm": "application/wasm",
            ".json": "application/json",
            ".css": "text/css",
        }

        def log_message(self, format, *args):  # noqa: A002
            """Suppress noisy request logs during tests."""
            pass

    with _ReuseAddrTCPServer(("", port), _MimeHandler) as httpd:
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
    # Find a free port starting from 8080
    port = _find_free_port(8080)
    console.print(f"[dim]Using port: {port}[/dim]")

    # Start the HTTP server
    os.chdir(str(PROJECT_ROOT))
    directory = Path(f"examples/{args.example}/fastled_js")
    console.print(f"[cyan]Testing example:[/cyan] [bold]{args.example}[/bold]")
    console.print(f"[dim]Server directory: {directory}[/dim]")
    server_process = start_http_server(port=port, directory=directory)

    try:
        # Wait for server to be ready by attempting to connect
        max_retries = 20
        retry_delay = 0.5
        server_ready = False

        for attempt in range(max_retries):
            if not server_process.is_alive():
                raise Exception(f"HTTP server failed to start on port {port}")

            with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as s:
                s.settimeout(1)
                try:
                    s.connect(("localhost", port))
                    server_ready = True
                    break
                except (ConnectionRefusedError, socket.timeout):
                    if attempt < max_retries - 1:
                        time.sleep(retry_delay)

        if not server_ready:
            raise Exception(
                f"HTTP server on port {port} did not become ready within {max_retries * retry_delay}s"
            )

        # Use Playwright to test the server
        async with async_playwright() as p:
            browser = await p.chromium.launch()
            page = await browser.new_page()

            try:
                # Listen for console messages BEFORE navigation
                browser_errors: list[str] = []

                def console_log_handler(msg: ConsoleMessage) -> None:
                    text = msg.text
                    if msg.type in ("error", "warn"):
                        browser_errors.append(f"[{msg.type}] {text}")
                    if "INVALID_OPERATION" in text:
                        console.print(
                            "[bold red]INVALID_OPERATION detected in console log[/bold red]"
                        )

                page.on("console", console_log_handler)

                # Also capture page errors (uncaught exceptions)
                def page_error_handler(error: Exception) -> None:
                    browser_errors.append(f"[error] Page error: {error}")

                page.on("pageerror", page_error_handler)

                test_url = f"http://localhost:{port}/?gfx={args.gfx}"
                console.print(f"[dim]Navigating to: {test_url}[/dim]")
                await page.goto(
                    test_url,
                    timeout=30000,
                    wait_until="domcontentloaded",
                )

                # Wait for FastLED to initialize and render frames.
                # The Vite-bundled fastled_callbacks.ts increments
                # globalThis.fastLEDFrameCount on each frame automatically.
                await page.wait_for_timeout(5000)

                # Check frame count OR controller/worker state.
                # In worker mode, frames render in the worker thread so
                # fastLEDFrameCount may stay 0 on the main thread.
                result = await page.evaluate(
                    """(() => {
                        const frameCount = globalThis.fastLEDFrameCount || 0;
                        const controllerRunning = !!(
                            window.fastLEDController &&
                            window.fastLEDController.running
                        );
                        const workerActive = !!(
                            window.fastLEDWorkerManager &&
                            window.fastLEDWorkerManager.isWorkerActive
                        );
                        return { frameCount, controllerRunning, workerActive };
                    })()"""
                )
                call_count = result.get("frameCount", 0)
                controller_running = result.get("controllerRunning", False)
                worker_active = result.get("workerActive", False)
                is_alive = call_count > 0 or controller_running or worker_active

                # Always print browser errors/warnings
                if browser_errors:
                    from collections import Counter

                    counts = Counter(browser_errors)
                    unique_errors = [e for e in counts if e.startswith("[error]")]
                    unique_warnings = [e for e in counts if e.startswith("[warn]")]
                    if unique_errors:
                        console.print(
                            f"\n[bold red]Browser errors ({len(unique_errors)} unique, {sum(counts[e] for e in unique_errors)} total):[/bold red]"
                        )
                        for err in unique_errors:
                            c = counts[err]
                            prefix = f"  (x{c}) " if c > 1 else "  "
                            console.print(f"{prefix}{err}")
                    if unique_warnings:
                        console.print(
                            f"\n[bold yellow]Browser warnings ({len(unique_warnings)} unique, {sum(counts[e] for e in unique_warnings)} total):[/bold yellow]"
                        )
                        for warn in unique_warnings:
                            c = counts[warn]
                            prefix = f"  (x{c}) " if c > 1 else "  "
                            console.print(f"{prefix}{warn}")

                if is_alive:
                    status_parts = []
                    if call_count > 0:
                        status_parts.append(f"{call_count} frames")
                    if controller_running:
                        status_parts.append("controller running")
                    if worker_active:
                        status_parts.append("worker active")
                    console.print()
                    console.print(
                        f"[bold green]✓ Success:[/bold green] FastLED.js initialized ({', '.join(status_parts)})"
                    )
                    console.print()
                else:
                    console.print()
                    console.print(
                        "[bold red]✗ Error:[/bold red] FastLED.js failed to initialize within 5 seconds",
                    )
                    raise Exception("FastLED.js failed to initialize")

            except KeyboardInterrupt as ki:
                handle_keyboard_interrupt(ki)
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
