#!/usr/bin/env python3
"""
FastLED Web Scraper Utility

This script provides an easy way to run the FastLED web scraper with different
configurations and examples.
"""

import argparse
import subprocess
import sys
from pathlib import Path


def run_scraper(
    example_name: str = "FestivalStick", headless: bool = False, timeout: int = 30
) -> int:
    """Run the FastLED web scraper with specified parameters"""

    script_path = Path(__file__).parent / "scrape_festival_stick.py"

    if not script_path.exists():
        print(f"Error: Scraper script not found at {script_path}", file=sys.stderr)
        return 1

    print(f"Running FastLED web scraper for {example_name}...")
    print(f"Headless mode: {headless}")
    print(f"Timeout: {timeout} seconds")

    try:
        # For now, just run the existing script
        # In the future, this could be enhanced to pass parameters
        result = subprocess.run(
            [sys.executable, str(script_path)],
            check=False,
            capture_output=True,
            text=True,
        )

        if result.returncode == 0:
            print("✅ Scraping completed successfully!")
            print(result.stdout)
        else:
            print("❌ Scraping failed!")
            print("STDOUT:", result.stdout)
            print("STDERR:", result.stderr)

        return result.returncode

    except Exception as e:
        print(f"Error running scraper: {e}", file=sys.stderr)
        return 1


def main() -> int:
    parser = argparse.ArgumentParser(description="Run FastLED web scraper")
    parser.add_argument(
        "--example",
        "-e",
        default="FestivalStick",
        help="Example name to scrape (default: FestivalStick)",
    )
    parser.add_argument(
        "--headless", action="store_true", help="Run browser in headless mode"
    )
    parser.add_argument(
        "--timeout", "-t", type=int, default=30, help="Timeout in seconds (default: 30)"
    )

    args = parser.parse_args()

    return run_scraper(args.example, args.headless, args.timeout)


if __name__ == "__main__":
    sys.exit(main())
