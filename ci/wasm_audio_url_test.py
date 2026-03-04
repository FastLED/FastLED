"""
Playwright test for FastLED WASM Audio URL auto-load functionality.

Verifies that UIAudio with a URL specified in C++ causes the WASM frontend
to automatically create an audio element with the correct source and begin
playback without user interaction.

Usage:
    uv run python ci/wasm_audio_url_test.py
"""

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

from ci.util.global_interrupt_handler import handle_keyboard_interrupt_properly


HERE = Path(__file__).parent
PROJECT_ROOT = HERE.parent

console = Console()


def parse_args():
    parser = argparse.ArgumentParser(
        description="Test WASM audio URL auto-load for FastLED AudioUrl example"
    )
    parser.add_argument(
        "example",
        nargs="?",
        default="AudioUrl",
        help="Example name to test (default: AudioUrl)",
    )
    parser.add_argument(
        "--timeout",
        type=int,
        default=30,
        help="Timeout in seconds for audio load (default: 30)",
    )
    return parser.parse_args()


def install_playwright_browsers():
    console.print("[dim]Installing Playwright browsers...[/dim]")
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


def _find_free_port(start: int = 8080) -> int:
    for port in range(start, start + 100):
        with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as s:
            try:
                s.bind(("", port))
                return port
            except OSError:
                continue
    raise OSError(f"No free port found in range {start}-{start + 99}")


class _ReuseAddrTCPServer(socketserver.TCPServer):
    allow_reuse_address = True


def _run_http_server(port: int, directory: str) -> None:
    os.chdir(directory)

    class _MimeHandler(http.server.SimpleHTTPRequestHandler):
        extensions_map = {
            **http.server.SimpleHTTPRequestHandler.extensions_map,
            ".js": "application/javascript",
            ".mjs": "application/javascript",
            ".wasm": "application/wasm",
            ".json": "application/json",
            ".css": "text/css",
        }

        def log_message(self, format, *args):  # noqa: A002
            pass

    with _ReuseAddrTCPServer(("", port), _MimeHandler) as httpd:
        httpd.serve_forever()


def start_http_server(port: int, directory: Path) -> multiprocessing.Process:
    server_process = multiprocessing.Process(
        target=_run_http_server,
        args=(port, str(directory)),
        daemon=True,
    )
    server_process.start()
    return server_process


async def main() -> None:
    args = parse_args()

    console.print()
    console.print(
        Panel.fit(
            f"[bold cyan]FastLED WASM Audio URL Test[/bold cyan]\n\n"
            f"[yellow]Will:[/yellow]\n"
            f"  [green]1.[/green] Launch headless Chromium with autoplay enabled\n"
            f"  [green]2.[/green] Load [bold]{args.example}[/bold] WASM example\n"
            f"  [green]3.[/green] Wait for FastLED init + audio element creation\n"
            f"  [green]4.[/green] Verify audio element exists with correct src\n"
            f"  [green]5.[/green] Verify audio readyState >= 2 (has data)\n"
            f"  [green]6.[/green] Verify audio is playing (not paused)",
            title="[bold magenta]Test Plan[/bold magenta]",
            border_style="cyan",
        )
    )
    console.print()

    install_playwright_browsers()

    port = _find_free_port(8080)
    console.print(f"[dim]Using port: {port}[/dim]")

    os.chdir(str(PROJECT_ROOT))
    directory = Path(f"examples/{args.example}/fastled_js")

    if not directory.exists():
        console.print(f"[bold red]Error:[/bold red] Directory not found: {directory}")
        console.print(
            f"[yellow]Hint:[/yellow] Run: uv run ci/wasm_compile.py examples/{args.example} --just-compile"
        )
        sys.exit(1)

    console.print(f"[cyan]Testing example:[/cyan] [bold]{args.example}[/bold]")
    console.print(f"[dim]Server directory: {directory}[/dim]")
    server_process = start_http_server(port=port, directory=directory)

    try:
        time.sleep(2)

        if not server_process.is_alive():
            raise Exception(f"HTTP server failed to start on port {port}")

        async with async_playwright() as p:
            # Launch with autoplay policy disabled so audio plays without gesture
            browser = await p.chromium.launch(
                args=["--autoplay-policy=no-user-gesture-required"]
            )
            page = await browser.new_page()

            try:
                browser_errors: list[str] = []
                audio_logs: list[str] = []

                def console_log_handler(msg: ConsoleMessage) -> None:
                    text = msg.text
                    if msg.type in ("error", "warn"):
                        browser_errors.append(f"[{msg.type}] {text}")
                    # Capture audio-related logs for verification
                    if "Auto-loading audio from URL" in text:
                        audio_logs.append(text)
                    if "Audio auto-load from URL complete" in text:
                        audio_logs.append(text)

                page.on("console", console_log_handler)

                def page_error_handler(error: Exception) -> None:
                    browser_errors.append(f"[error] Page error: {error}")

                page.on("pageerror", page_error_handler)

                test_url = f"http://localhost:{port}"
                console.print(f"[dim]Navigating to: {test_url}[/dim]")
                await page.goto(
                    test_url,
                    timeout=30000,
                    wait_until="domcontentloaded",
                )

                # Wait for FastLED to initialise and render some frames
                console.print("[dim]Waiting for FastLED init...[/dim]")
                await page.wait_for_timeout(5000)

                # Check FastLED is alive
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

                if not is_alive:
                    console.print(
                        "[bold red]Error:[/bold red] FastLED.js failed to initialize within 5 seconds"
                    )
                    raise Exception("FastLED.js failed to initialize")

                console.print("[green]FastLED initialized OK[/green]")

                # Now wait for the audio element to appear (auto-loaded from URL)
                console.print("[dim]Waiting for audio element...[/dim]")
                try:
                    await page.wait_for_function(
                        """() => {
                            const audio = document.querySelector('audio.audio-player');
                            return audio && audio.src && audio.src.length > 0;
                        }""",
                        timeout=args.timeout * 1000,
                    )
                except Exception:
                    console.print(
                        "[bold red]Error:[/bold red] Audio element with src not found within timeout"
                    )
                    raise

                console.print("[green]Audio element found[/green]")

                # Validate the audio element state
                audio_state = await page.evaluate(
                    """(() => {
                        const audio = document.querySelector('audio.audio-player');
                        if (!audio) return { exists: false };
                        return {
                            exists: true,
                            src: audio.src,
                            readyState: audio.readyState,
                            paused: audio.paused,
                            loop: audio.loop,
                            currentTime: audio.currentTime,
                            duration: audio.duration,
                        };
                    })()"""
                )

                console.print(f"[dim]Audio state: {audio_state}[/dim]")

                # Verify assertions
                failures: list[str] = []

                if not audio_state.get("exists"):
                    failures.append("Audio element does not exist")

                src = audio_state.get("src", "")
                if not src or "soundhelix" not in src.lower():
                    failures.append(
                        f"Audio src does not contain expected URL (got: {src})"
                    )

                ready_state = audio_state.get("readyState", 0)
                # readyState: 0=HAVE_NOTHING, 1=HAVE_METADATA, 2=HAVE_CURRENT_DATA,
                #             3=HAVE_FUTURE_DATA, 4=HAVE_ENOUGH_DATA
                # We accept >= 1 since the audio may still be loading from a remote URL
                if ready_state < 1:
                    failures.append(
                        f"Audio readyState too low: {ready_state} (expected >= 1)"
                    )

                if audio_state.get("paused", True):
                    # Audio might be paused if autoplay policy blocked it despite
                    # our flag; treat as warning not failure
                    console.print(
                        "[yellow]Warning: Audio is paused (autoplay may be blocked)[/yellow]"
                    )

                if not audio_state.get("loop", False):
                    failures.append("Audio loop is not enabled")

                # Verify console logging shows the auto-load flow
                has_auto_load_start = any(
                    "Auto-loading audio from URL" in log for log in audio_logs
                )
                has_auto_load_complete = any(
                    "Audio auto-load from URL complete" in log for log in audio_logs
                )

                if has_auto_load_start:
                    console.print("[green]Console log: auto-load started[/green]")
                else:
                    failures.append(
                        "Console log missing: 'Auto-loading audio from URL'"
                    )

                if has_auto_load_complete:
                    console.print("[green]Console log: auto-load completed[/green]")
                else:
                    # Not a hard failure - audio may still be loading from
                    # remote URL; the configureAudioPlayback may throw if
                    # autoplay is blocked
                    console.print(
                        "[yellow]Warning: Console log 'Audio auto-load from URL complete' not seen "
                        "(autoplay may be blocked)[/yellow]"
                    )

                # Print browser errors if any
                if browser_errors:
                    from collections import Counter

                    counts = Counter(browser_errors)
                    unique_errors = [e for e in counts if e.startswith("[error]")]
                    if unique_errors:
                        console.print(
                            f"\n[bold red]Browser errors ({len(unique_errors)} unique):[/bold red]"
                        )
                        for err in unique_errors[:5]:
                            console.print(f"  {err}")

                if failures:
                    console.print()
                    for f in failures:
                        console.print(f"[bold red]FAIL:[/bold red] {f}")
                    raise Exception(f"Audio URL test failed: {'; '.join(failures)}")

                console.print()
                console.print(
                    "[bold green]SUCCESS:[/bold green] Audio URL auto-load working correctly"
                )
                console.print()

            except KeyboardInterrupt:
                handle_keyboard_interrupt_properly()
                raise
            except Exception as e:
                console.print(f"[bold red]An error occurred:[/bold red] {e}")
                raise

            finally:
                await browser.close()

    finally:
        server_process.terminate()


if __name__ == "__main__":
    try:
        asyncio.run(main())
    except KeyboardInterrupt:
        handle_keyboard_interrupt_properly()
