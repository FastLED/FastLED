#!/usr/bin/env python3
"""
Enhanced Playwright test to detect and capture all console errors
for iterative fixing of WebGL/Three.js issues in FastLED WASM.
"""

import asyncio
import os
import sys
import time
from pathlib import Path
from typing import Any, Dict, List

from playwright.async_api import ConsoleMessage, async_playwright  # type: ignore


HERE = Path(__file__).parent
PROJECT_ROOT = HERE.parent


class ConsoleErrorCollector:
    """Collects and categorizes console errors for analysis"""

    def __init__(self):
        self.errors: List[Dict[str, Any]] = []
        self.warnings: List[Dict[str, Any]] = []
        self.all_messages: List[Dict[str, Any]] = []

    def add_message(self, msg: ConsoleMessage):
        """Add a console message to the appropriate collection"""
        message_data = {
            "type": msg.type,
            "text": msg.text,
            "location": f"{msg.location.get('url', 'unknown')}:{msg.location.get('lineNumber', 0)}"
            if msg.location
            else "unknown",
            "timestamp": time.time(),
        }

        self.all_messages.append(message_data)

        if msg.type == "error":
            # Filter out debug messages that are incorrectly classified as errors
            text_lower = msg.text.lower()
            is_debug_message = any(
                pattern in text_lower
                for pattern in ["canvas update:", "canvas resize", "🔍"]
            )

            if not is_debug_message:
                self.errors.append(message_data)
                print(f"❌ ERROR: {msg.text}")
                if msg.location:
                    print(f"   Location: {message_data['location']}")
            else:
                print(f"🔍 DEBUG: {msg.text} (filtered from errors)")

        elif msg.type == "warning":
            self.warnings.append(message_data)
            print(f"⚠️  WARNING: {msg.text}")

        elif msg.type in ["log", "info", "debug"]:
            # Look for specific error patterns in log messages
            text_lower = msg.text.lower()

            # Check for actual error indicators, but exclude successful operations
            is_error = any(
                pattern in text_lower
                for pattern in [
                    "failed to create",
                    "error",
                    "exception",
                    "crash",
                    "cannot read properties",
                    "cannot set properties",
                    "typeerror",
                    "referenceerror",
                    "undefined is not",
                    "context lost",
                    "webgl error",
                    "precision test failed",
                ]
            )

            # Exclude successful operations and normal logging
            is_success = any(
                pattern in text_lower
                for pattern in [
                    "initialized successfully",
                    "created successfully",
                    "loaded successfully",
                    "completed successfully",
                    "setting up",
                    "setting renderer size",
                    "validation successful",
                    "skipping webgl validation",
                    "merged settings",
                    "fps set to",
                    "ready with stats",
                    "system ready",
                    "event system",
                    "canvas update:",
                    "canvas resize",
                ]
            )

            if is_error and not is_success:
                print(f"🔍 SUSPICIOUS LOG: {msg.text}")
                self.errors.append(message_data)

    def has_errors(self) -> bool:
        """Check if any errors were collected"""
        return len(self.errors) > 0

    def get_error_summary(self) -> str:
        """Get a summary of all errors found"""
        if not self.has_errors():
            return "✅ No errors detected!"

        summary = f"\n📊 CONSOLE ERROR SUMMARY:\n"
        summary += f"   Errors: {len(self.errors)}\n"
        summary += f"   Warnings: {len(self.warnings)}\n"
        summary += f"   Total Messages: {len(self.all_messages)}\n\n"

        if self.errors:
            summary += "🔥 ERRORS FOUND:\n"
            for i, error in enumerate(self.errors, 1):
                summary += f"   {i}. [{error['type'].upper()}] {error['text']}\n"
                summary += f"      Location: {error['location']}\n\n"

        return summary


def install_playwright_browsers():
    """Ensure Playwright browsers are installed"""
    print("🔧 Installing Playwright browsers...")
    try:
        os.system(f"{sys.executable} -m playwright install chromium")
        print("✅ Playwright browsers installed successfully.")
    except Exception as e:
        print(f"❌ Failed to install Playwright browsers: {e}", file=sys.stderr)
        sys.exit(1)


def start_http_server(port: int, directory: Path):
    """Start an HTTP server on the specified port"""
    import http.server
    import os
    import socketserver
    import subprocess
    import threading
    from contextlib import contextmanager

    class QuietHTTPServer:
        def __init__(self, directory: str, port: int):
            self.directory = directory
            self.port = port
            self.httpd = None
            self.thread = None

        def start(self):
            original_dir = os.getcwd()
            try:
                os.chdir(str(self.directory))

                # Custom handler with proper MIME types
                class CustomHTTPRequestHandler(http.server.SimpleHTTPRequestHandler):
                    def log_message(self, format: str, *args: Any) -> None:
                        pass  # Suppress logs

                    def guess_type(self, path: Any) -> str:
                        import mimetypes

                        mimetype, encoding = mimetypes.guess_type(path)
                        # Fix MIME types for JavaScript modules and WASM
                        if str(path).endswith(".js"):
                            return "application/javascript"
                        elif str(path).endswith(".mjs"):
                            return "application/javascript"
                        elif str(path).endswith(".wasm"):
                            return "application/wasm"
                        return mimetype or "text/plain"

                self.httpd = socketserver.TCPServer(
                    ("", self.port), CustomHTTPRequestHandler
                )
                self.thread = threading.Thread(target=self.httpd.serve_forever)
                self.thread.daemon = True
                self.thread.start()
                print(f"✅ HTTP server started on port {self.port}")
                time.sleep(0.5)  # Give server time to start
            except Exception as e:
                print(f"❌ Failed to start HTTP server: {e}")
                os.chdir(original_dir)
                raise

        def terminate(self):
            if self.httpd:
                self.httpd.shutdown()
                self.httpd.server_close()
            if self.thread:
                self.thread.join(timeout=1)

    return QuietHTTPServer(str(directory), port)


async def run_console_test(test_iteration: int = 1) -> ConsoleErrorCollector:
    """Run a single iteration of the console error test"""

    print(f"\n🚀 Starting test iteration {test_iteration}")

    # Find an available port
    import socket

    with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as s:
        s.bind(("", 0))
        port = s.getsockname()[1]
    print(f"🌐 Using port: {port}")

    # Start the HTTP server
    os.chdir(str(PROJECT_ROOT))
    directory = Path("examples/Blink/fastled_js")
    server_process = start_http_server(port=port, directory=directory)
    server_process.start()

    collector = ConsoleErrorCollector()

    try:
        # Give the server some time to start
        time.sleep(3)

        # Use Playwright to test the server
        async with async_playwright() as p:
            browser = await p.chromium.launch(headless=True)
            page = await browser.new_page()

            try:
                # Set up console message handler
                page.on("console", collector.add_message)

                # Navigate to the page with gfx=0 parameter
                test_url = f"http://localhost:{port}?gfx=0"
                print(f"🔗 Navigating to {test_url}")
                await page.goto(test_url, timeout=30000)

                # Wait for the page to load and initialize
                print("⏳ Waiting for FastLED initialization...")

                # Set up frame counting
                await page.evaluate(
                    """
                    window.frameCallCount = 0;
                    window.errorCount = 0;

                    // Override console.error to track errors
                    const originalError = console.error;
                    console.error = function(...args) {
                        window.errorCount++;
                        originalError.apply(console, args);
                    };

                    // Track FastLED frame calls
                    const originalFastLEDOnFrame = globalThis.FastLED_onFrame || function() {};
                    globalThis.FastLED_onFrame = (jsonStr) => {
                        console.log('FastLED_onFrame called successfully');
                        window.frameCallCount++;
                        return originalFastLEDOnFrame(jsonStr);
                    };
                """
                )

                # Wait for initialization and monitor for errors
                await page.wait_for_timeout(8000)  # Increased timeout

                # Check results
                call_count = await page.evaluate("window.frameCallCount")
                error_count = await page.evaluate("window.errorCount")

                print(f"📈 Frame calls: {call_count}")
                print(f"🚨 JavaScript errors: {error_count}")

                if call_count > 0:
                    print(
                        f"✅ FastLED.js initialized successfully - {call_count} frame calls"
                    )
                else:
                    print("❌ FastLED.js failed to initialize properly")
                    collector.errors.append(
                        {
                            "type": "initialization_error",
                            "text": "FastLED.js failed to initialize - no frame calls detected",
                            "location": "test",
                            "timestamp": time.time(),
                        }
                    )

            except Exception as e:
                print(f"❌ Test error: {e}")
                collector.errors.append(
                    {
                        "type": "test_error",
                        "text": str(e),
                        "location": "playwright",
                        "timestamp": time.time(),
                    }
                )

            finally:
                await browser.close()

    finally:
        # Terminate the server process
        server_process.terminate()
        time.sleep(1)  # Give server time to shut down

    return collector


async def main() -> None:
    """Main test function with iterative error fixing"""

    install_playwright_browsers()

    print("🎯 Starting iterative console error testing")
    print("=" * 60)

    max_iterations = 20
    iteration = 1

    while iteration <= max_iterations:
        print(f"\n{'=' * 20} ITERATION {iteration}/{max_iterations} {'=' * 20}")

        # Run the test
        collector = await run_console_test(iteration)

        # Print results
        print(collector.get_error_summary())

        if not collector.has_errors():
            print(f"🎉 SUCCESS! No errors found after {iteration} iteration(s)")
            print("✅ All console errors have been resolved!")
            return

        # If this is the last iteration, report failure
        if iteration >= max_iterations:
            print(f"❌ FAILURE: Still have errors after {max_iterations} iterations")
            print("\n🔍 FINAL ERROR REPORT:")
            print(collector.get_error_summary())

            print("\n💡 RECOMMENDATIONS:")
            print("1. Review the console errors above")
            print("2. Check WebGL/Three.js compatibility issues")
            print("3. Verify OffscreenCanvas handling")
            print("4. Check canvas context creation")

            sys.exit(1)

        print(f"\n⏭️  Proceeding to iteration {iteration + 1}...")
        print("🔧 Please fix the errors shown above and run the test again")

        # For now, we'll break here to let the user fix errors
        # In a real iterative system, you would automatically apply fixes
        break

        iteration += 1


if __name__ == "__main__":
    try:
        sys.exit(asyncio.run(main()))
    except KeyboardInterrupt:
        print("\n🛑 Test interrupted by user")
        sys.exit(1)
    except Exception as e:
        print(f"❌ Test failed with exception: {e}")
        sys.exit(1)
