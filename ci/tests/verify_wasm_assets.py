#!/usr/bin/env python3
"""End-to-end check that a `.lnk` asset reaches the WASM filesystem.

Builds nothing: compile ``AudioUrl`` first. The check serves the generated
page, opens it in Chromium, and verifies that the remote manifest asset is
complete before the sketch's setup starts.
"""

import argparse
import asyncio
import functools
import http.server
import socketserver
import sys
import threading
from dataclasses import dataclass
from pathlib import Path

from ci.util.global_interrupt_handler import handle_keyboard_interrupt


PROJECT_ROOT = Path(__file__).resolve().parents[2]
ASSET_PATH = "data/track.mp3"


@dataclass
class ServeResult:
    server: socketserver.TCPServer
    port: int


def _serve(directory: Path) -> ServeResult:
    """Serve a WASM build with the headers and MIME types Chromium requires."""

    class Handler(http.server.SimpleHTTPRequestHandler):
        extensions_map = {
            **http.server.SimpleHTTPRequestHandler.extensions_map,
            ".js": "text/javascript",
            ".mjs": "text/javascript",
            ".wasm": "application/wasm",
            ".json": "application/json",
            ".css": "text/css",
            ".map": "application/json",
        }

        def end_headers(self) -> None:
            self.send_header("Cross-Origin-Opener-Policy", "same-origin")
            self.send_header("Cross-Origin-Embedder-Policy", "require-corp")
            self.send_header("Cache-Control", "no-store")
            super().end_headers()

        def log_message(self, format: str, *args: object) -> None:  # noqa: A002
            pass

    handler = functools.partial(Handler, directory=str(directory))
    server = socketserver.TCPServer(("127.0.0.1", 0), handler)
    port = int(server.server_address[1])
    threading.Thread(target=server.serve_forever, daemon=True).start()
    return ServeResult(server=server, port=port)


async def _run(example: str, headless: bool, timeout_s: int) -> int:
    from playwright.async_api import async_playwright

    output_dir = PROJECT_ROOT / "examples" / example / "fastled_js"
    required_outputs = ("index.html", "fastled.js", "fastled.wasm")
    missing: list[str] = []
    for name in required_outputs:
        if not (output_dir / name).is_file():
            missing.append(name)
    if missing:
        print(
            f"FAIL: missing WASM build output in {output_dir}: {', '.join(missing)}\n"
            f"Build it first with: bash compile wasm --examples {example}",
            file=sys.stderr,
        )
        return 1

    serve_result = _serve(output_dir)
    logs: list[str] = []
    page_finished = asyncio.Event()

    def record_console(message: object) -> None:
        text = str(getattr(message, "text", message))
        logs.append(text)
        if "Starting fastled with Asyncify support" in text:
            page_finished.set()

    def record_page_error(error: object) -> None:
        logs.append(f"pageerror: {error}")
        page_finished.set()

    try:
        async with async_playwright() as playwright:
            browser = await playwright.chromium.launch(headless=headless)
            page = await browser.new_page()
            page.on("console", record_console)
            page.on("pageerror", record_page_error)
            await page.goto(
                f"http://127.0.0.1:{serve_result.port}/index.html",
                timeout=timeout_s * 1000,
                wait_until="domcontentloaded",
            )
            try:
                await asyncio.wait_for(page_finished.wait(), timeout=timeout_s)
            except TimeoutError:
                logs.append(f"timeout: setup did not start within {timeout_s} seconds")
            await browser.close()
    finally:
        serve_result.server.shutdown()
        serve_result.server.server_close()

    joined = "\n".join(logs)
    checks = {
        "manifest loaded": "fastledAssetManifest" in joined,
        "asset resolved": f"Asset '{ASSET_PATH}'" in joined,
        "asset bytes streamed": f"File fetched: {ASSET_PATH}" in joined,
        "asset complete before setup": (
            "embedded asset(s) loaded completely before setup()" in joined
        ),
        "no incomplete asset": "is incomplete: wrote" not in joined,
        "asset integrity verified": f"Asset '{ASSET_PATH}' sha256 verified" in joined,
        "no asset integrity failure": "failed integrity check" not in joined,
        "no uncaught page error": "pageerror:" not in joined,
    }
    complete_at = next(
        (
            i
            for i, line in enumerate(logs)
            if "loaded completely before setup()" in line
        ),
        None,
    )
    setup_at = next(
        (
            i
            for i, line in enumerate(logs)
            if "Starting fastled with Asyncify support" in line
        ),
        None,
    )
    checks["completion log precedes setup log"] = (
        complete_at is not None and setup_at is not None and complete_at < setup_at
    )

    print("--- browser console ---")
    for line in logs:
        print(line)
    print("--- asset checks ---")
    for name, passed in checks.items():
        print(f"[{'PASS' if passed else 'FAIL'}] {name}")

    failures: list[str] = []
    for name, passed in checks.items():
        if not passed:
            failures.append(name)
    if failures:
        print(f"FAIL: {', '.join(failures)}", file=sys.stderr)
        return 1
    print("PASS: embedded asset was complete before setup")
    return 0


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--example", default="AudioUrl")
    parser.add_argument("--headed", action="store_true")
    parser.add_argument("--timeout", type=int, default=45)
    args = parser.parse_args()
    try:
        return asyncio.run(_run(args.example, not args.headed, args.timeout))
    except KeyboardInterrupt as error:
        handle_keyboard_interrupt(error)
        raise


if __name__ == "__main__":
    sys.exit(main())
