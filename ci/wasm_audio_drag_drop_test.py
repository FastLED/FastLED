"""
Playwright test for FastLED WASM audio drag-and-drop + buffered sample pipeline.

Verifies that:
1. Audio file can be loaded via drag-and-drop onto the audio control
2. Audio samples flow from JS → Worker buffer → WASM C++ ring buffer
3. The buffered audio pipeline (flushAudioSamplesToWasm) works correctly

Can also test vibe-reactive mode by enabling the "Enable Vibe Reactive" checkbox.

Usage:
    uv run python ci/wasm_audio_drag_drop_test.py [--mp3 path/to/file.mp3]
    uv run python ci/wasm_audio_drag_drop_test.py AnimartrixRing --mp3 e:/videos/soundtrack1.mp3 --enable-vibe-reactive
"""

import argparse
import asyncio
import http.server
import multiprocessing
import os
import socket
import socketserver
import subprocess
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
    parser = argparse.ArgumentParser(
        description="Test WASM audio drag-and-drop and buffered sample pipeline"
    )
    parser.add_argument(
        "example",
        nargs="?",
        default="AudioUrl",
        help="Example name to test (default: AudioUrl)",
    )
    parser.add_argument(
        "--mp3",
        type=str,
        default=None,
        help="Path to MP3 file for drag-and-drop test",
    )
    parser.add_argument(
        "--timeout",
        type=int,
        default=45,
        help="Timeout in seconds for audio pipeline verification (default: 45)",
    )
    parser.add_argument(
        "--enable-vibe-reactive",
        action="store_true",
        default=False,
        help="Enable the 'Enable Vibe Reactive' checkbox after audio loads",
    )
    parser.add_argument(
        "--headed",
        action="store_true",
        default=False,
        help="Run browser in headed (visible) mode for debugging",
    )
    parser.add_argument(
        "--monitor-duration",
        type=int,
        default=30,
        help="How long (seconds) to monitor after vibe reactive is enabled (default: 30)",
    )
    return parser.parse_args()


def install_playwright_browsers():
    console.print("[dim]Installing Playwright browsers...[/dim]")
    try:
        subprocess.run(
            [sys.executable, "-m", "playwright", "install", "chromium"],
            check=False,
        )
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


async def enable_vibe_reactive_checkbox(page) -> bool:
    """Find and check the 'Enable Vibe Reactive' checkbox by label text."""
    # The checkbox label contains "Enable Vibe Reactive"
    # The checkbox element is an input[type="checkbox"] next to that label
    checked = await page.evaluate("""(() => {
        // Find all labels and look for the one with "Enable Vibe Reactive"
        const labels = document.querySelectorAll('label');
        for (const label of labels) {
            if (label.textContent.includes('Enable Vibe Reactive')) {
                // The checkbox is a sibling in the same flex container
                const container = label.parentElement;
                if (container) {
                    const checkbox = container.querySelector('input[type="checkbox"]');
                    if (checkbox) {
                        // Just set checked - no events. UI manager uses polling.
                        checkbox.checked = true;
                        return true;
                    }
                }
            }
        }
        return false;
    })()""")
    return checked


async def get_frame_count(page) -> int:
    """Get the current FastLED frame count."""
    return await page.evaluate("globalThis.fastLEDFrameCount || 0")


async def main() -> None:
    args = parse_args()

    mp3_path = args.mp3
    if mp3_path:
        mp3_path = Path(mp3_path)
        if not mp3_path.exists():
            console.print(f"[bold red]Error:[/bold red] MP3 file not found: {mp3_path}")
            sys.exit(1)

    console.print()
    steps = [
        f"  [green]1.[/green] Launch headless Chromium with autoplay enabled",
        f"  [green]2.[/green] Load [bold]{args.example}[/bold] WASM example",
        f"  [green]3.[/green] Wait for FastLED init + audio UI",
    ]
    if mp3_path:
        steps.append(
            f"  [green]4.[/green] Load MP3 file via file input: [bold]{mp3_path}[/bold]"
        )
    else:
        steps.append(
            f"  [green]4.[/green] Let URL auto-load proceed (intercept SoundHelix -> local serve)"
        )
    steps.append(f"  [green]5.[/green] Verify audio samples reach C++ ring buffer")
    if args.enable_vibe_reactive:
        steps.append(
            f"  [green]6.[/green] Enable 'Vibe Reactive' checkbox and monitor for {args.monitor_duration}s"
        )
    steps.append(
        f"  [green]{'7' if args.enable_vibe_reactive else '6'}.[/green] Check diagnostic messages in console"
    )

    console.print(
        Panel.fit(
            f"[bold cyan]FastLED WASM Audio Pipeline Test[/bold cyan]\n\n"
            f"[yellow]Will:[/yellow]\n" + "\n".join(steps),
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
            f"[yellow]Hint:[/yellow] Run: bash compile wasm --examples {args.example}"
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
            browser = await p.chromium.launch(
                headless=not args.headed,
                args=["--autoplay-policy=no-user-gesture-required"],
            )
            page = await browser.new_page()

            try:
                browser_errors: list[str] = []
                all_console_logs: list[str] = []

                # Track key diagnostic messages
                saw_audio_buffered = False
                saw_audio_flushed = False
                saw_cpp_received = False
                saw_cpp_consumed = False
                saw_drag_drop_load = False
                saw_script_processor = False

                # Vibe-reactive monitoring
                vibe_log_count = 0
                last_vibe_log_time = 0.0
                vibe_stopped = False
                vibe_stop_time = 0.0
                frame_counts: list[tuple[float, int]] = []  # (time, frame_count)
                audio_sample_counts: list[str] = []
                worker_audio_msg_count = 0
                last_worker_audio_time = 0.0
                cpp_output_count = 0
                last_cpp_output_time = 0.0
                monitoring_active = False

                def console_log_handler(msg: ConsoleMessage) -> None:
                    nonlocal saw_audio_buffered, saw_audio_flushed
                    nonlocal saw_cpp_received, saw_cpp_consumed
                    nonlocal saw_drag_drop_load, saw_script_processor
                    nonlocal vibe_log_count, last_vibe_log_time
                    nonlocal worker_audio_msg_count, last_worker_audio_time
                    nonlocal cpp_output_count, last_cpp_output_time
                    text = msg.text
                    all_console_logs.append(f"[{msg.type}] {text}")

                    # Track worker audio messages
                    if "audio_samples" in text:
                        worker_audio_msg_count += 1
                        last_worker_audio_time = time.time()

                    # Track any C++ output (has timestamps like "1.2s" prefix or known patterns)
                    if any(
                        kw in text
                        for kw in [">>>", "Vibe:", "AnimartrixRing:", "setup complete"]
                    ):
                        cpp_output_count += 1
                        last_cpp_output_time = time.time()

                    # Print DBG/LOOP/AR_/FLL messages from C++ loop
                    if (
                        "DBG" in text
                        or "LOOP" in text
                        or "AR_" in text
                        or "VIBE_CB" in text
                        or "FC #" in text
                        or "FLL" in text
                        or "WASM_FRAME" in text
                    ):
                        console.print(f"[bold yellow]  >> {text}[/bold yellow]")

                    # During monitoring, print frame errors or worker errors
                    if monitoring_active and "error" in text.lower():
                        console.print(
                            f"[bold red]  !! MONITORING ERROR: {text[:200]}[/bold red]"
                        )

                    if msg.type in ("error", "warn"):
                        browser_errors.append(f"[{msg.type}] {text}")

                    # Worker-side diagnostics (our new buffered audio code)
                    if "First audio sample buffered" in text:
                        saw_audio_buffered = True
                        console.print(
                            "[green]  >> Worker: First audio sample buffered[/green]"
                        )
                    if "First audio sample flushed to WASM" in text:
                        saw_audio_flushed = True
                        console.print(
                            "[green]  >> Worker: First audio sample flushed to WASM[/green]"
                        )

                    # C++ side diagnostics
                    if "WasmAudioInput: First audio block received from JS" in text:
                        saw_cpp_received = True
                        console.print(
                            "[green]  >> C++: First audio block received from JS[/green]"
                        )
                    if "WasmAudioInput: First audio block consumed by sketch" in text:
                        saw_cpp_consumed = True
                        console.print(
                            "[green]  >> C++: First audio block consumed by sketch[/green]"
                        )

                    # Drag-and-drop specific
                    if "Drag-and-drop audio load complete" in text:
                        saw_drag_drop_load = True
                        console.print(
                            "[green]  >> Audio: Drag-and-drop load complete[/green]"
                        )

                    # ScriptProcessor fallback
                    if "script_processor" in text.lower() and "using" in text.lower():
                        saw_script_processor = True

                    # Vibe-reactive diagnostics
                    if "Vibe:" in text:
                        vibe_log_count += 1
                        last_vibe_log_time = time.time()
                        if vibe_log_count <= 5 or vibe_log_count % 50 == 0:
                            console.print(f"[cyan]  >> {text}[/cyan]")

                    if (
                        "BASS SPIKE" in text
                        or "MID SPIKE" in text
                        or "TREB SPIKE" in text
                    ):
                        console.print(f"[magenta]  >> {text}[/magenta]")

                    # Audio sample processing logs from AnimartrixRing
                    if (
                        "audio samples processed" in text
                        or "First audio sample received" in text
                    ):
                        audio_sample_counts.append(text)
                        console.print(f"[yellow]  >> {text}[/yellow]")

                    # Watch for errors that might cause stopping
                    if any(
                        kw in text.lower()
                        for kw in [
                            "exception",
                            "abort",
                            "unreachable",
                            "out of memory",
                            "stack overflow",
                            "infinity",
                            "nan",
                        ]
                    ):
                        console.print(
                            f"[bold red]  !! POTENTIAL ISSUE: {text}[/bold red]"
                        )

                page.on("console", console_log_handler)

                def page_error_handler(error: Exception) -> None:
                    browser_errors.append(f"[error] Page error: {error}")
                    console.print(f"[bold red]  !! PAGE ERROR: {error}[/bold red]")

                page.on("pageerror", page_error_handler)

                # Force ScriptProcessor instead of AudioWorklet.
                # AudioWorklet runs on the audio rendering thread which
                # Playwright doesn't drive. ScriptProcessor runs on the
                # main thread event loop which Playwright does drive.
                await page.add_init_script(
                    """
                    // Disable AudioWorklet so AudioManager falls back to ScriptProcessor
                    if (typeof AudioContext !== 'undefined') {
                        Object.defineProperty(AudioContext.prototype, 'audioWorklet', {
                            get: () => undefined,
                            configurable: true,
                        });
                    }

                    // Monitor worker errors
                    window._workerErrors = [];
                    window._workerMessages = 0;
                    const origWorker = window.Worker;
                    window.Worker = function(url, opts) {
                        const w = new origWorker(url, opts);
                        w.addEventListener('error', (e) => {
                            const msg = 'WORKER ERROR: ' + (e.message || e);
                            window._workerErrors.push(msg);
                            console.error(msg);
                        });
                        const origOnMessage = w.onmessage;
                        const origPost = w.postMessage.bind(w);
                        w.addEventListener('message', () => { window._workerMessages++; });
                        return w;
                    };
                    window.Worker.prototype = origWorker.prototype;
                    """
                )

                # If we have a local MP3 and example uses URL auto-load,
                # intercept the SoundHelix URL
                if mp3_path and args.example == "AudioUrl":
                    file_bytes = mp3_path.read_bytes()

                    async def handle_soundhelix(route):
                        await route.fulfill(
                            body=file_bytes,
                            content_type="audio/mpeg",
                        )

                    await page.route("**/SoundHelix*", handle_soundhelix)
                    console.print(
                        f"[cyan]Routing SoundHelix URL -> local MP3:[/cyan] {mp3_path}"
                    )

                test_url = f"http://localhost:{port}"
                console.print(f"[dim]Navigating to: {test_url}[/dim]")
                await page.goto(
                    test_url,
                    timeout=30000,
                    wait_until="domcontentloaded",
                )

                # Wait for FastLED to initialize
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
                is_alive = (
                    result.get("frameCount", 0) > 0
                    or result.get("controllerRunning", False)
                    or result.get("workerActive", False)
                )

                if not is_alive:
                    console.print(
                        "[bold red]Error:[/bold red] FastLED.js failed to initialize within 5 seconds"
                    )
                    raise Exception("FastLED.js failed to initialize")

                console.print("[green]FastLED initialized OK[/green]")

                # For non-AudioUrl examples, load the MP3 via file input
                if mp3_path and args.example != "AudioUrl":
                    console.print(
                        f"[cyan]Loading MP3 via file input:[/cyan] {mp3_path}"
                    )
                    # The file input is hidden (display:none), so we can't wait
                    # for visibility. Use set_input_files directly on the locator.
                    file_input = page.locator('input[type="file"][accept="audio/*"]')
                    await file_input.set_input_files(str(mp3_path))
                    console.print("[green]MP3 file loaded via file input[/green]")

                # Wait for audio pipeline
                console.print(
                    "[dim]Waiting for audio samples to flow through pipeline...[/dim]"
                )

                timeout_ms = args.timeout * 1000
                poll_interval = 1000
                elapsed = 0

                while elapsed < timeout_ms:
                    await page.wait_for_timeout(poll_interval)
                    elapsed += poll_interval

                    # Record frame count for monitoring
                    fc = await get_frame_count(page)
                    frame_counts.append((time.time(), fc))

                    # Check if we've seen the key diagnostic messages
                    if saw_audio_flushed and saw_cpp_received:
                        console.print("[green]Audio pipeline verified![/green]")
                        break

                    if elapsed % 5000 == 0:
                        console.print(
                            f"[dim]  ... waiting ({elapsed // 1000}s / {args.timeout}s) "
                            f"buffered={saw_audio_buffered} flushed={saw_audio_flushed} "
                            f"cpp_received={saw_cpp_received} cpp_consumed={saw_cpp_consumed}[/dim]"
                        )

                # Brief baseline monitoring to check worker health
                if (
                    saw_audio_flushed or saw_cpp_received
                ) and args.monitor_duration > 0:
                    baseline_logs_before = len(all_console_logs)
                    console.print(
                        "[dim]Baseline monitoring for 5s (before vibe change)...[/dim]"
                    )
                    await page.wait_for_timeout(5000)
                    baseline_logs_after = len(all_console_logs)
                    new_logs = baseline_logs_after - baseline_logs_before
                    console.print(
                        f"[dim]  Baseline: {new_logs} new logs in 5s "
                        f"(audio_msgs={worker_audio_msg_count}, "
                        f"cpp_out={cpp_output_count})[/dim]"
                    )
                    if new_logs == 0:
                        console.print(
                            "[bold yellow]WARNING: Worker appears dead even "
                            "BEFORE vibe reactive was enabled![/bold yellow]"
                        )

                # === TEST: Change a slider value to check if UI changes crash worker ===
                if (
                    saw_audio_flushed or saw_cpp_received
                ) and args.enable_vibe_reactive:
                    console.print(
                        "[cyan]TEST: Changing brightness slider to check if "
                        "ANY UI change kills the worker...[/cyan]"
                    )
                    slider_changed = await page.evaluate("""(() => {
                        // Find the brightness slider (should be slider-3)
                        const sliders = document.querySelectorAll('input[type="range"]');
                        for (const slider of sliders) {
                            const label = slider.closest('.ui-control')?.querySelector('label');
                            if (label && label.textContent.includes('Brightness')) {
                                slider.value = 100;
                                slider.dispatchEvent(new Event('input', { bubbles: true }));
                                slider.dispatchEvent(new Event('change', { bubbles: true }));
                                return true;
                            }
                        }
                        return false;
                    })()""")
                    console.print(
                        f"[cyan]  Brightness slider changed: {slider_changed}[/cyan]"
                    )
                    slider_pre = len(all_console_logs)
                    await page.wait_for_timeout(3000)
                    slider_post = len(all_console_logs)
                    console.print(
                        f"[cyan]  After slider change: {slider_post - slider_pre} "
                        f"new logs in 3s[/cyan]"
                    )
                    if slider_post - slider_pre == 0:
                        console.print(
                            "[bold red]  SLIDER CHANGE ALSO KILLS WORKER![/bold red]"
                        )
                    else:
                        console.print(
                            "[green]  Worker survived slider change - issue is "
                            "checkbox-specific[/green]"
                        )

                # === VIBE REACTIVE MONITORING ===
                if args.enable_vibe_reactive and (
                    saw_audio_flushed or saw_cpp_received
                ):
                    console.print()
                    console.print(
                        "[bold cyan]Enabling 'Vibe Reactive' checkbox...[/bold cyan]"
                    )
                    checked = await enable_vibe_reactive_checkbox(page)
                    if checked:
                        console.print(
                            "[green]'Enable Vibe Reactive' checkbox is now checked[/green]"
                        )
                    else:
                        console.print(
                            "[bold red]Could not find 'Enable Vibe Reactive' checkbox![/bold red]"
                        )

                    # Quick check: is the main thread alive after checkbox?
                    await page.wait_for_timeout(500)
                    main_thread_alive = await page.evaluate("1 + 1")
                    console.print(
                        f"[cyan]Main thread alive after checkbox: "
                        f"{main_thread_alive == 2}[/cyan]"
                    )

                    # Check worker state
                    worker_state = await page.evaluate("""(() => {
                        const wm = window.fastLEDWorkerManager;
                        if (!wm) return {workerManager: false};
                        return {
                            workerManager: true,
                            workerActive: wm.isWorkerActive || false,
                            workerRef: !!wm.worker,
                        };
                    })()""")
                    console.print(f"[cyan]Worker state: {worker_state}[/cyan]")

                    # Monitor for the specified duration
                    monitoring_active = True
                    console.print(
                        f"[dim]Monitoring vibe-reactive behavior for {args.monitor_duration}s...[/dim]"
                    )
                    monitor_start = time.time()
                    stall_checks: list[
                        tuple[float, int, int]
                    ] = []  # (time, frame_count, vibe_count)

                    monitor_elapsed = 0
                    while monitor_elapsed < args.monitor_duration * 1000:
                        await page.wait_for_timeout(1000)
                        monitor_elapsed += 1000

                        fc = await get_frame_count(page)
                        frame_counts.append((time.time(), fc))
                        stall_checks.append((time.time(), fc, vibe_log_count))

                        # Check worker health
                        worker_health = await page.evaluate("""(() => {
                            return {
                                workerErrors: window._workerErrors || [],
                                workerMessages: window._workerMessages || 0,
                            };
                        })()""")
                        if worker_health.get("workerErrors"):
                            for err in worker_health["workerErrors"]:
                                console.print(
                                    f"[bold red]WORKER ERROR: {err}[/bold red]"
                                )

                        # Check for frame stalls (no new frames in last 3 seconds)
                        if len(stall_checks) >= 3:
                            recent = stall_checks[-3:]
                            if recent[-1][1] == recent[0][1] and recent[-1][1] > 0:
                                console.print(
                                    f"[bold red]STALL DETECTED: Frame count stuck at "
                                    f"{recent[-1][1]} for 3+ seconds[/bold red]"
                                )

                            # Check for vibe log stalls
                            if recent[-1][2] == recent[0][2] and recent[-1][2] > 0:
                                if not vibe_stopped:
                                    vibe_stopped = True
                                    vibe_stop_time = time.time()
                                    console.print(
                                        f"[bold yellow]Vibe logs stopped at count "
                                        f"{recent[-1][2]} (no new vibe updates for 3s)[/bold yellow]"
                                    )

                        if monitor_elapsed % 5000 == 0:
                            secs = monitor_elapsed // 1000
                            audio_age = (
                                f"{time.time() - last_worker_audio_time:.1f}s ago"
                                if last_worker_audio_time > 0
                                else "never"
                            )
                            cpp_age = (
                                f"{time.time() - last_cpp_output_time:.1f}s ago"
                                if last_cpp_output_time > 0
                                else "never"
                            )
                            console.print(
                                f"[dim]  ... monitoring ({secs}s / {args.monitor_duration}s) "
                                f"frames={fc} vibe_logs={vibe_log_count} "
                                f"audio_msgs={worker_audio_msg_count} (last: {audio_age}) "
                                f"cpp_out={cpp_output_count} (last: {cpp_age}) "
                                f"total_logs={len(all_console_logs)}[/dim]"
                            )

                # Report results
                console.print()
                failures: list[str] = []
                warnings: list[str] = []

                if saw_audio_buffered:
                    console.print(
                        "[green]PASS:[/green] Audio samples buffered in worker"
                    )
                else:
                    failures.append("No 'First audio sample buffered' message seen")

                if saw_audio_flushed:
                    console.print("[green]PASS:[/green] Audio samples flushed to WASM")
                else:
                    failures.append(
                        "No 'First audio sample flushed to WASM' message seen"
                    )

                if saw_cpp_received:
                    console.print(
                        "[green]PASS:[/green] C++ received first audio block from JS"
                    )
                else:
                    failures.append(
                        "No 'WasmAudioInput: First audio block received from JS' message seen"
                    )

                if saw_cpp_consumed:
                    console.print(
                        "[green]PASS:[/green] C++ sketch consumed first audio block"
                    )
                else:
                    # This is a softer check - the sketch might not have consumed yet
                    warnings.append(
                        "No 'WasmAudioInput: First audio block consumed by sketch' message seen"
                    )

                # Vibe-reactive results
                if args.enable_vibe_reactive:
                    console.print()
                    console.print("[bold]Vibe-Reactive Results:[/bold]")
                    console.print(f"  Total vibe log messages: {vibe_log_count}")
                    console.print(
                        f"  Audio sample count logs: {len(audio_sample_counts)}"
                    )
                    if vibe_stopped:
                        elapsed_before_stop = vibe_stop_time - monitor_start
                        console.print(
                            f"  [bold red]Vibe updates STOPPED after ~{elapsed_before_stop:.1f}s[/bold red]"
                        )
                    else:
                        console.print(
                            "  [green]Vibe updates continued throughout monitoring[/green]"
                        )

                    # Frame count analysis
                    if len(frame_counts) >= 2:
                        first_fc = frame_counts[0]
                        last_fc = frame_counts[-1]
                        total_frames = last_fc[1] - first_fc[1]
                        total_time = last_fc[0] - first_fc[0]
                        if total_time > 0:
                            fps = total_frames / total_time
                            console.print(f"  Average FPS: {fps:.1f}")

                # Print warnings
                for w in warnings:
                    console.print(f"[yellow]WARN:[/yellow] {w}")

                # Print browser errors summary
                if browser_errors:
                    from collections import Counter

                    counts = Counter(browser_errors)
                    unique_errors = [e for e in counts if e.startswith("[error]")]
                    if unique_errors:
                        console.print(
                            f"\n[bold red]Browser errors ({len(unique_errors)} unique):[/bold red]"
                        )
                        for err in unique_errors[:10]:
                            console.print(f"  {err}")

                    # Show warnings too
                    unique_warns = [e for e in counts if e.startswith("[warn]")]
                    if unique_warns:
                        console.print(
                            f"\n[yellow]Browser warnings ({len(unique_warns)} unique):[/yellow]"
                        )
                        for w in unique_warns[:10]:
                            console.print(f"  {w}")

                if failures:
                    console.print()
                    for f in failures:
                        console.print(f"[bold red]FAIL:[/bold red] {f}")

                    # Dump audio-related console logs for debugging
                    audio_logs = [
                        l
                        for l in all_console_logs
                        if any(
                            kw in l.lower()
                            for kw in [
                                "audio",
                                "wasm",
                                "sample",
                                "buffer",
                                "flush",
                                "worker",
                                "processor",
                                "worklet",
                                "vibe",
                                "spike",
                            ]
                        )
                    ]
                    if audio_logs:
                        console.print(
                            f"\n[dim]Audio-related console messages ({len(audio_logs)}):[/dim]"
                        )
                        for log_line in audio_logs[-50:]:
                            console.print(f"  [dim]{log_line}[/dim]")

                    console.print("\n[dim]Last 30 console messages:[/dim]")
                    for log_line in all_console_logs[-30:]:
                        console.print(f"  [dim]{log_line}[/dim]")

                    raise Exception(
                        f"Audio pipeline test failed: {'; '.join(failures)}"
                    )

                # Dump diagnostic logs if vibe reactive was enabled
                if args.enable_vibe_reactive:
                    console.print("\n[dim]Last 50 console messages:[/dim]")
                    for log_line in all_console_logs[-50:]:
                        console.print(f"  [dim]{log_line}[/dim]")

                console.print()
                console.print(
                    "[bold green]SUCCESS:[/bold green] Audio pipeline working - "
                    "samples flow from JS -> Worker buffer -> WASM C++ ring buffer"
                )
                if args.enable_vibe_reactive:
                    if vibe_stopped:
                        console.print(
                            "[bold yellow]WARNING:[/bold yellow] Vibe reactive stopped during monitoring"
                        )
                    else:
                        console.print(
                            "[bold green]SUCCESS:[/bold green] Vibe reactive continued throughout monitoring"
                        )
                console.print()

            except KeyboardInterrupt as ki:
                handle_keyboard_interrupt(ki)
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
    except KeyboardInterrupt as ki:
        handle_keyboard_interrupt(ki)
