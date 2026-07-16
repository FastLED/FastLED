"""Playwright regression test for the Hydropack WASM microphone control.

Regression coverage for issue #3688.

The first permission request is cancelled in-page. Chromium's fake capture
device then makes the second request deterministic, allowing this test to
verify that cancellation leaves the control reusable without physical audio
hardware.

Usage:
    uv run python ci/wasm_audio_microphone_test.py [example]
"""

import asyncio
import socket
import time
from pathlib import Path

from playwright.async_api import async_playwright

from ci.wasm_test import _find_free_port, start_http_server


PROJECT_ROOT = Path(__file__).parent.parent


async def main() -> None:
    example = "HydroPack"
    directory = PROJECT_ROOT / "examples" / example / "fastled_js"
    if not directory.exists():
        raise FileNotFoundError(
            f"{directory} does not exist; run bash compile wasm --examples {example}"
        )

    port = _find_free_port(9133)
    server = start_http_server(port, directory)
    try:
        for _ in range(20):
            if not server.is_alive():
                raise RuntimeError(f"WASM server failed to start on port {port}")
            try:
                with socket.create_connection(("localhost", port), timeout=1):
                    break
            except OSError:
                time.sleep(0.5)
        else:
            raise RuntimeError(f"WASM server did not start on port {port}")

        async with async_playwright() as playwright:
            browser = await playwright.chromium.launch(
                args=[
                    "--autoplay-policy=no-user-gesture-required",
                    "--use-fake-device-for-media-stream",
                    "--use-fake-ui-for-media-stream",
                ]
            )
            context = await browser.new_context()
            await context.grant_permissions(
                ["microphone"], origin=f"http://localhost:{port}"
            )
            page = await context.new_page()
            await page.add_init_script(
                """
                (() => {
                  const originalGetUserMedia = navigator.mediaDevices.getUserMedia.bind(
                    navigator.mediaDevices
                  );
                  let calls = 0;
                  navigator.mediaDevices.getUserMedia = async (constraints) => {
                    calls += 1;
                    if (calls === 1) {
                      throw new DOMException('Permission request dismissed', 'AbortError');
                    }
                    return originalGetUserMedia(constraints);
                  };
                })();
                """
            )
            await page.goto(f"http://localhost:{port}/", wait_until="domcontentloaded")

            microphone = page.locator("button.audio-mic-button")
            await microphone.first.wait_for(state="visible", timeout=30000)
            await microphone.first.click()

            error = page.locator(".audio-error")
            await error.wait_for(state="visible")
            assert await error.text_content() == "Microphone Access Cancelled"
            assert await microphone.first.is_enabled()
            assert await microphone.first.text_content() == "🎤 Microphone"

            await microphone.first.click()
            await microphone.first.wait_for(state="visible")
            await page.wait_for_function(
                """() => document.querySelector('button.audio-mic-button')?.textContent ===
                '🛑 Stop Recording'"""
            )

            await microphone.first.click()
            await page.wait_for_function(
                """() => document.querySelector('button.audio-mic-button')?.textContent ===
                '🎤 Microphone'"""
            )
            await browser.close()
    finally:
        server.terminate()
        server.join(timeout=5)


if __name__ == "__main__":
    asyncio.run(main())
