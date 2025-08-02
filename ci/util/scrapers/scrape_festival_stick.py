#!/usr/bin/env python3

import asyncio
import os
import sys
from datetime import datetime
from pathlib import Path

from playwright.async_api import async_playwright  # type: ignore


HERE = Path(__file__).parent
PROJECT_ROOT = HERE.parent.parent.parent  # scrapers is 3 levels down from project root
SCREENSHOTS_DIR = HERE / "screenshots"

# Ensure screenshots directory exists
SCREENSHOTS_DIR.mkdir(exist_ok=True)


# Ensure Playwright browsers are installed
def install_playwright_browsers():
    print("Installing Playwright browsers...")
    try:
        os.system(f"{sys.executable} -m playwright install chromium")
        print("Playwright browsers installed successfully.")
    except Exception as e:
        print(f"Failed to install Playwright browsers: {e}", file=sys.stderr)
        sys.exit(1)


async def scrape_festival_stick_example():
    """
    Navigate to the online FastLED tool and capture a screenshot of the FestivalStick example
    """
    install_playwright_browsers()

    # Online FastLED tool URL
    fastled_url = "https://fastled.onrender.com/docs"

    async with async_playwright() as p:
        # Launch browser with a visible window for debugging
        browser = await p.chromium.launch(headless=False, slow_mo=1000)
        page = await browser.new_page()

        # Set viewport size for consistent screenshots
        await page.set_viewport_size({"width": 1920, "height": 1080})

        try:
            print(f"Navigating to {fastled_url}...")
            await page.goto(fastled_url, timeout=30000)

            # Wait for the page to load
            await page.wait_for_load_state("networkidle")

            # Look for FastLED examples or upload functionality
            print("Looking for FastLED example functionality...")

            # Wait a bit for dynamic content to load
            await page.wait_for_timeout(3000)

            # Try to find any example selection or file upload elements
            examples_selector = None
            possible_selectors = [
                "select[name*='example']",
                "select[id*='example']",
                ".example-selector",
                "input[type='file']",
                "button:has-text('Example')",
                "button:has-text('FestivalStick')",
                "a:has-text('Example')",
                "a:has-text('FestivalStick')",
            ]

            for selector in possible_selectors:
                try:
                    element = await page.wait_for_selector(selector, timeout=2000)
                    if element:
                        print(f"Found element with selector: {selector}")
                        examples_selector = selector
                        break
                except Exception:
                    continue

            if not examples_selector:
                # If no specific example selector found, look for text content
                print("Looking for FestivalStick text on page...")
                try:
                    await page.wait_for_selector("text=FestivalStick", timeout=5000)
                    print("Found FestivalStick text on page!")
                except Exception:
                    print(
                        "FestivalStick text not found, taking screenshot of current page..."
                    )

            # Try to interact with the FastLED tool interface
            print("Attempting to interact with FastLED interface...")

            # Look for common web interface elements
            interface_elements = [
                "canvas",
                ".led-display",
                ".visualization",
                "#fastled-canvas",
                ".fastled-viewer",
            ]

            canvas_found = False
            canvas = None
            for element_selector in interface_elements:
                try:
                    canvas_element = await page.wait_for_selector(
                        element_selector, timeout=2000
                    )
                    if canvas_element:
                        print(f"Found display element: {element_selector}")
                        canvas_found = True
                        canvas = canvas_element
                        break
                except Exception:
                    continue

            if not canvas_found:
                print("No specific display canvas found, capturing full page...")

            # If there's a file upload option, try to upload the FestivalStick example
            try:
                file_input = await page.query_selector("input[type='file']")
                if file_input:
                    print("Found file input, attempting to upload FestivalStick.ino...")
                    festival_stick_path = (
                        PROJECT_ROOT
                        / "examples"
                        / "FestivalStick"
                        / "FestivalStick.ino"
                    )
                    if festival_stick_path.exists():
                        await file_input.set_input_files(str(festival_stick_path))
                        await page.wait_for_timeout(3000)  # Wait for upload to process
                        print("FestivalStick.ino uploaded successfully!")
                    else:
                        print(f"FestivalStick.ino not found at {festival_stick_path}")
            except Exception as e:
                print(f"Could not upload file: {e}")

            # Wait for any animations or dynamic content to settle
            await page.wait_for_timeout(5000)

            # Take screenshot
            timestamp = datetime.now().strftime("%Y%m%d_%H%M%S")
            screenshot_path = SCREENSHOTS_DIR / f"festival_stick_{timestamp}.png"

            print(f"Taking screenshot and saving to {screenshot_path}...")
            await page.screenshot(path=str(screenshot_path), full_page=True)

            print(f"Screenshot saved successfully to {screenshot_path}")

            # Also take a focused screenshot if we found a canvas
            if canvas_found and canvas is not None:
                try:
                    canvas_screenshot_path = (
                        SCREENSHOTS_DIR / f"festival_stick_canvas_{timestamp}.png"
                    )
                    await canvas.screenshot(path=str(canvas_screenshot_path))
                    print(f"Canvas screenshot saved to {canvas_screenshot_path}")
                except Exception as e:
                    print(f"Could not take canvas screenshot: {e}")

            # Keep browser open for a few seconds to see the result
            print("Keeping browser open for 10 seconds for inspection...")
            await page.wait_for_timeout(10000)

        except Exception as e:
            print(f"An error occurred during scraping: {e}", file=sys.stderr)

            # Take an error screenshot for debugging
            error_screenshot_path = (
                SCREENSHOTS_DIR
                / f"error_{datetime.now().strftime('%Y%m%d_%H%M%S')}.png"
            )
            try:
                await page.screenshot(path=str(error_screenshot_path), full_page=True)
                print(f"Error screenshot saved to {error_screenshot_path}")
            except Exception:
                pass

            raise e

        finally:
            await browser.close()


async def main():
    """Main function to run the scraping operation"""
    try:
        await scrape_festival_stick_example()
        print("FastLED FestivalStick scraping completed successfully!")
    except Exception as e:
        print(f"Scraping failed: {e}", file=sys.stderr)
        return 1

    return 0


if __name__ == "__main__":
    exit_code = asyncio.run(main())
    sys.exit(exit_code)
