# FastLED Web Scrapers

This directory contains web scraping tools for testing and capturing screenshots from the online FastLED tool.

## Contents

- **`scrape_festival_stick.py`** - Main web scraping script that navigates to the online FastLED tool and captures screenshots
- **`run_fastled_scraper.py`** - Utility script for easy execution with different configurations
- **`screenshots/`** - Directory containing captured screenshots with timestamps
- **`fastled_python_investigation.md`** - Comprehensive documentation of the investigation and implementation

## Quick Start

### Run from project root:
```bash
# Basic usage
uv run python ci/ci/scrapers/scrape_festival_stick.py

# With utility script options
uv run python ci/ci/scrapers/run_fastled_scraper.py --headless --timeout 60
```

### Run from scrapers directory:
```bash
cd ci/ci/scrapers
uv run python scrape_festival_stick.py
uv run python run_fastled_scraper.py --help
```

## Features

- **Automated browser navigation** to https://fastled.onrender.com/docs
- **Smart element detection** for FastLED interface components
- **Screenshot capture** with timestamp organization
- **File upload attempts** for FastLED examples
- **Error handling** with debug screenshots
- **Headless and visible modes** for different use cases

## Requirements

The scrapers use the existing FastLED Python environment with:
- `playwright` for browser automation
- Python dependencies from `pyproject.toml`
- Automatic Playwright browser installation

## Integration

These scrapers are integrated with the FastLED CI infrastructure and complement the existing web testing in `ci/wasm_test.py`.

For detailed information, see `fastled_python_investigation.md`.
