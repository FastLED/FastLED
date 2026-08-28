#!/usr/bin/env python3
"""End-to-end check that a `.lnk` asset reaches the WASM filesystem.

Builds nothing -- run `bash compile wasm --examples AudioUrl` first. This
serves `examples/AudioUrl/fastled_js/`, loads it in headless Chromium, and
watches the console for the asset actually being fetched and injected.

Why a browser: the whole feature is "the bytes end up in the filesystem", and
nothing short of running the page proves that. A green compile proves only that
the loader code exists. `examples/AudioUrl` is the acceptance case because its
only asset is a remote `.lnk` -- `files.json` for it is literally `[]`, so if
the asset appears at all, it came through the manifest path.

Usage:
    uv run ci/tests/verify_wasm_assets.py [--example AudioUrl] [--headed]
"""

from __future__ import annotations

import argparse
import asyncio
import functools
import http.server
import socketserver
import sys
import threading
from pathlib import Path

from ci.util.global_interrupt_handler import handle_keyboard_interrupt


PROJECT_ROOT = Path(__file__).resolve().parents[2]

#: The asset AudioUrl declares, and the size we expect to see streamed. Both
#: come from examples/AudioUrl/data/track.mp3.lnk resolving to FastLED/assets.
ASSET_PATH = "data/track.mp3"


def _serve(directory: Path) -> tuple[socketserver.TCPServer, int]:
    """Start a background HTTP server on a free port, returning (server, port).

    COOP/COEP headers are required: the FastLED WASM build uses threads, so the
    page needs cross-origin isolation or the module refuses to instantiate.
    """

    class Handler(http.server.SimpleHTTPRequestHandler):
        # Windows resolves .js from the registry, which frequently yields
        # text/plain -- and a module script served as text/plain is rejected
        # outright by strict MIME checking, so nothing loads. Pin the types
        # that matter rather than depending on the host's file associations.
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

        def log_message(self, fmt: str, *args: object) -> None:
            pass  # keep the harness output readable

    handler = functools.partial(Handler, directory=str(directory))
    httpd = socketserver.TCPServer(("127.0.0.1", 0), handler)
    port = httpd.server_address[1]
    threading.Thread(target=httpd.serve_forever, daemon=True).start()
    return httpd, port


async def _run(example: str, headless: bool, timeout_s: int) -> int:
    from playwright.async_api import async_playwright

    served = PROJECT_ROOT / "examples" / example / "fastled_js"
    if not (served / "index.html").exists():
        print(
            f"FAIL: {served}/index.html missing -- run: bash compile wasm --examples {example}"
        )
        return 1

    httpd, port = _serve(served)
    url = f"http://127.0.0.1:{port}/index.html"
    print(f"serving {served} at {url}")

    logs: list[str] = []
    try:
        async with async_playwright() as p:
            browser = await p.chromium.launch(headless=headless)
            page = await browser.new_page()
            page.on("console", lambda m: logs.append(f"{m.type}: {m.text}"))
            page.on("pageerror", lambda e: logs.append(f"pageerror: {e}"))

            await page.goto(url, timeout=timeout_s * 1000)
            # The asset is fetched during load; give the module time to boot,
            # stream it, and run a few frames.
            await page.wait_for_timeout(timeout_s * 1000)
            await browser.close()
    finally:
        httpd.shutdown()

    joined = "\n".join(logs)
    print("\n--- browser console ---")
    for line in logs:
        print(f"  {line}")
    print("--- end console ---\n")

    checks = {
        "manifest loaded": "fastledAssetManifest" in joined,
        "asset resolved to its url": f"Asset '{ASSET_PATH}'" in joined,
        "asset injected into the filesystem": "manifest asset(s) into the filesystem"
        in joined,
        "bytes streamed": f"File fetched: {ASSET_PATH}" in joined,
        # Parity with a device: on ESP the file is simply present when setup()
        # runs. "Arrives eventually" means the preview is lying to the sketch,
        # so completeness *before* setup is the property under test.
        "loaded completely before setup()": "embedded asset(s) loaded completely before setup()"
        in joined,
        "no incomplete asset reported": "is incomplete: wrote" not in joined,
        # The manifest declares a digest; trusting it unverified was #4025.
        "sha256 verified": f"Asset '{ASSET_PATH}' sha256 verified" in joined,
        "no integrity failure": "failed integrity check" not in joined,
    }

    # Ordering is the whole point, so assert it rather than trusting the text.
    complete_at = next(
        (i for i, ln in enumerate(logs) if "loaded completely before setup()" in ln),
        None,
    )
    setup_at = next((i for i, ln in enumerate(logs) if "Starting fastled" in ln), None)
    checks["completion preceded setup() starting"] = (
        complete_at is not None and setup_at is not None and complete_at < setup_at
    )
    failures = [name for name, ok in checks.items() if not ok]
    for name, ok in checks.items():
        print(f"  [{'PASS' if ok else 'FAIL'}] {name}")

    if failures:
        print(f"\nFAIL: {len(failures)} check(s) failed: {', '.join(failures)}")
        return 1
    print("\nPASS: the .lnk asset reached the WASM filesystem")
    return 0


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--example", default="AudioUrl")
    ap.add_argument("--headed", action="store_true", help="show the browser")
    ap.add_argument("--timeout", type=int, default=15)
    args = ap.parse_args()
    try:
        return asyncio.run(_run(args.example, not args.headed, args.timeout))
    except KeyboardInterrupt as ki:
        handle_keyboard_interrupt(ki)
        raise


if __name__ == "__main__":
    sys.exit(main())
