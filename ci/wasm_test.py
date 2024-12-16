import asyncio
import os
import subprocess
import sys
import time
from http.server import HTTPServer, SimpleHTTPRequestHandler

from playwright.async_api import async_playwright


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
def start_http_server(port, directory):
    os.chdir(directory)
    server = HTTPServer(("localhost", port), SimpleHTTPRequestHandler)
    print(f"Serving on port {port}")
    server_process = subprocess.Popen(
        [sys.executable, "-m", "http.server", str(port)],
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
    )
    return server_process, server


async def main():
    install_playwright_browsers()
    # Find an available port
    port = 8888
    print(f"Using port: {port}")

    # Start the HTTP server
    directory = "examples/wasm/fastled_js"
    server_process, _ = start_http_server(port, directory)

    try:
        # Give the server some time to start
        time.sleep(5)

        # Use Playwright to test the server
        async with async_playwright() as p:
            browser = await p.chromium.launch()
            page = await browser.new_page()

            try:
                await page.goto(f"http://localhost:{port}", timeout=30000)

                # Listen for console messages
                def console_log_handler(msg):
                    if "INVALID_OPERATION" in msg.text:
                        print(
                            "INVALID_OPERATION detected in console log", file=sys.stderr
                        )
                        sys.exit(1)

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
                    sys.exit(1)

            except Exception as e:
                print(f"An error occurred: {e}", file=sys.stderr)
                sys.exit(1)

            finally:
                await browser.close()

    finally:
        # Terminate the server process
        server_process.terminate()


# Run the main function
if __name__ == "__main__":
    asyncio.run(main())
