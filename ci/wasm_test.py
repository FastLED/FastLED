import asyncio
import os
import sys
import time
from pathlib import Path

from playwright.async_api import ConsoleMessage, async_playwright  # type: ignore


HERE = Path(__file__).parent
PROJECT_ROOT = HERE.parent


# Ensure Playwright browsers are installed
def install_playwright_browsers():
    print("Installing Playwright browsers...")
    try:
        # Simulate the `playwright install` command
        os.system(f"{sys.executable} -m playwright install chromium")
        print("Playwright browsers installed successfully.")
    except Exception as e:
        print(f"Failed to install Playwright browsers: {e}", file=sys.stderr)
        sys.exit(1)


# Start an HTTP server on the dynamic port
def start_http_server(port: int, directory: Path):
    from fastled import Test  # type: ignore

    server_process = Test.spawn_http_server(
        directory=directory, port=port, open_browser=False
    )
    return server_process


async def main() -> None:
    install_playwright_browsers()
    # Find an available port
    port = (
        8080  # Todo, figure out why the http server ignores any port other than 8080.
    )
    print(f"Using port: {port}")

    # Start the HTTP server
    os.chdir(str(PROJECT_ROOT))
    directory = Path("examples/wasm/fastled_js")
    server_process = start_http_server(port=port, directory=directory)

    try:
        # Give the server some time to start
        time.sleep(2)

        # Use Playwright to test the server
        async with async_playwright() as p:
            browser = await p.chromium.launch()
            page = await browser.new_page()

            try:
                await page.goto(f"http://localhost:{port}", timeout=30000)

                # Listen for console messages
                def console_log_handler(msg: ConsoleMessage) -> None:
                    if "INVALID_OPERATION" in msg.text:
                        print(
                            "INVALID_OPERATION detected in console log", file=sys.stderr
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
                    print(
                        f"Success: FastLED.js was initialized and FastLED_onFrame was called {call_count} times"
                    )
                else:
                    print(
                        "Error: FastLED.js had something go wrong and FastLED_onFrame was not called within 5 seconds",
                        file=sys.stderr,
                    )
                    raise Exception("FastLED.js failed to initialize")

            except Exception as e:
                print(f"An error occurred: {e}", file=sys.stderr)
                raise Exception(f"An error occurred: {e}") from e

            finally:
                await browser.close()

    finally:
        # Terminate the server process
        server_process.terminate()


# Run the main function
if __name__ == "__main__":
    sys.exit(asyncio.run(main()))
