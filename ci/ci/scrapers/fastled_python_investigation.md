# FastLED Python Code Investigation

## Overview
This document summarizes the investigation of existing Python code in the FastLED project, particularly focusing on Playwright integration and web scraping capabilities for the FastLED online tool.

**Location**: All scripts are located in the `ci/ci/scrapers/` directory, organized within the CI infrastructure alongside the existing testing infrastructure, including the original Playwright implementation in `ci/wasm_test.py`.

## Existing Python Infrastructure

### Dependencies (pyproject.toml)
The project already includes comprehensive dependencies for development and testing:
- **Playwright**: `playwright` - for browser automation
- **Testing**: `pytest`, `pytest-xdist` for parallel testing
- **FastLED**: `fastled>=1.2.26` - the FastLED Python package
- **FastLED WASM**: `fastled-wasm` - WebAssembly support
- **HTTP Client**: `httpx` - for HTTP requests
- **Build Tools**: `uv`, `ziglang`, `ninja`, `cmake`
- **Code Quality**: `ruff`, `mypy`, `pyright`, `clang-format`, `isort`, `black`

### Existing Playwright Implementation (`ci/wasm_test.py`)

The project already has a sophisticated Playwright setup that:

1. **Automatic Browser Installation**: 
   ```python
   def install_playwright_browsers():
       os.system(f"{sys.executable} -m playwright install chromium")
   ```

2. **FastLED WASM Testing**:
   - Starts an HTTP server for WASM examples
   - Tests browser automation with the FastLED.js library
   - Monitors `FastLED_onFrame` callback execution
   - Validates WebGL/WASM functionality

3. **Error Handling**:
   - Console log monitoring for errors
   - Timeout handling for page loads
   - Proper server cleanup

### MCP Server Integration (`mcp_server.py`)

The project includes a comprehensive MCP (Model Context Protocol) server with tools for:
- Running tests with various options
- Compiling examples for different platforms
- Code fingerprinting and change detection
- Linting and formatting
- Project information and status

## FestivalStick Example Analysis

### Core Functionality
The FestivalStick example (`examples/FestivalStick/`) is a sophisticated LED pattern demo featuring:

1. **Corkscrew LED Mapping**: 
   - 19.25 turns, 288 LEDs
   - Maps 2D patterns to spiral LED positions
   - Uses `fl::Corkscrew` class for geometric calculations

2. **UI Controls**:
   - Speed, position, brightness controls
   - Noise pattern generation with customizable parameters  
   - Color palette selection (Party, Heat, Ocean, Forest, Rainbow)
   - Rendering mode options (Noise, Position, Mixed)
   - Color boost with saturation/luminance functions

3. **Advanced Features**:
   - Multi-sampling for accurate LED positioning
   - Real-time noise generation with cylindrical mapping
   - Auto-advance mode with manual position override
   - ScreenMap integration for web visualization

### Key Components
```cpp
// Corkscrew configuration
#define NUM_LEDS  288
#define CORKSCREW_TURNS 19.25

// Runtime corkscrew with flexible configuration
Corkscrew::Input corkscrewInput(CORKSCREW_TURNS, NUM_LEDS, 0);
Corkscrew corkscrew(corkscrewInput);

// Frame buffer for 2D pattern drawing
fl::Grid<CRGB> frameBuffer;

// ScreenMap for web interface visualization
fl::ScreenMap corkscrewScreenMap = corkscrew.toScreenMap(0.2f);
```

## Web Scraping Script Implementation

### Script Features (`scrape_festival_stick.py`)

1. **Robust Web Navigation**:
   - Navigates to https://fastled.onrender.com/docs
   - Handles dynamic content loading
   - Searches for multiple possible interface elements

2. **Smart Element Detection**:
   - Looks for example selection dropdowns
   - Detects file upload capabilities
   - Finds canvas/visualization elements
   - Searches for FestivalStick-specific content

3. **Screenshot Capabilities**:
   - Full page screenshots with timestamps
   - Focused canvas screenshots when available
   - Error screenshots for debugging
   - Multiple resolution support (1920x1080 default)

4. **File Upload Attempt**:
   - Automatically tries to upload `FestivalStick.ino`
   - Handles missing file scenarios gracefully
   - Waits for upload processing

### Script Workflow
1. Install Playwright browsers automatically
2. Launch visible browser with slow motion for debugging
3. Navigate to online FastLED tool
4. Search for example/upload functionality
5. Attempt to load FestivalStick example
6. Capture screenshots of the visualization
7. Save timestamped results to `screenshots/` directory

## Project Testing Infrastructure

### Unit Tests
- Location: `tests/` directory
- Command: `bash test` (per user rules)
- Comprehensive C++ unit tests for all components
- Platform compilation tests
- Code quality checks

### Example Compilation
- Multi-platform support: `uno`, `esp32`, `teensy`, etc.
- Command: `./compile <platform> --examples <example_name>`
- Batch compilation for multiple platforms
- Interactive and automated modes

## Key Findings

1. **Comprehensive Infrastructure**: The FastLED project already has extensive Python tooling with Playwright, testing, and web automation capabilities.

2. **Advanced LED Visualization**: The FestivalStick example represents sophisticated LED pattern generation with real-time parameter control and web visualization.

3. **Web Integration Ready**: The existing WASM testing infrastructure provides a solid foundation for web-based LED visualization and interaction.

4. **Documentation Gap**: While the code is well-implemented, there could be more comprehensive documentation of the web tooling capabilities.

## Recommendations

1. **Extend Web Scraping**: The created script could be enhanced to:
   - Test multiple examples automatically
   - Capture video recordings of animations
   - Perform parameter sweeps for different configurations

2. **Integration Testing**: Consider adding the web scraping script to the CI/CD pipeline for automated web interface testing.

3. **User Documentation**: Create user guides for the online FastLED tool and example usage.

## Results

✅ **Successfully captured screenshot**: `ci/ci/scrapers/screenshots/festival_stick_20250620_224055.png` (82KB)

The script successfully:
1. Navigated to https://fastled.onrender.com/docs
2. Detected and interacted with the FastLED web interface
3. Captured a full-page screenshot of the FestivalStick example visualization
4. Saved the result with timestamp for easy identification

## Files Created/Modified

- `ci/ci/scrapers/scrape_festival_stick.py` - Main web scraping script with Playwright automation
- `ci/ci/scrapers/run_fastled_scraper.py` - Utility script for easy execution with different configurations
- `ci/ci/scrapers/screenshots/` - Directory containing captured images
- `ci/ci/scrapers/screenshots/festival_stick_20250620_224055.png` - Successfully captured screenshot (82KB)
- `ci/ci/scrapers/fastled_python_investigation.md` - This documentation file

## Usage Examples

```bash
# Run the scraper directly from project root
uv run ci/ci/scrapers/scrape_festival_stick.py

# Use the utility script with options
uv run ci/ci/scrapers/run_fastled_scraper.py --example FestivalStick --headless --timeout 60

# Make scripts executable and run from project root
chmod +x ci/ci/scrapers/scrape_festival_stick.py ci/ci/scrapers/run_fastled_scraper.py
./ci/ci/scrapers/scrape_festival_stick.py
./ci/ci/scrapers/run_fastled_scraper.py --help

# Or run from within the scrapers directory
cd ci/ci/scrapers
uv run scrape_festival_stick.py
uv run run_fastled_scraper.py --help
```
