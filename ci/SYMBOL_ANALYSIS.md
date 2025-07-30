# FastLED Symbol Analysis

This document describes the new generic symbol analysis functionality that works across all supported platforms including UNO, ESP32, and others.

## Overview

The symbol analysis tool provides comprehensive analysis of compiled ELF files to help identify optimization opportunities and understand binary size allocation. Unlike the previous ESP32-specific implementation, this tool:

- ✅ **Works on ALL platforms** (UNO, ESP32, Teensy, STM32, etc.)
- ✅ **Pulls ALL symbols** without filtering or classification
- ✅ **No FastLED-specific filtering** - analyzes everything
- ✅ **Generic and extensible** for any platform with build_info.json

## Quick Start

### Analyze a specific platform

```bash
cd ci
uv run symbol_analysis.py --board uno
uv run symbol_analysis.py --board esp32dev
```

### Run analysis on all available platforms

```bash
uv run demo_symbol_analysis.py
```

### Use in GitHub Actions

```bash
uv run symbol_analysis_runner.py --board uno --example Blink
```

## Files

- **`symbol_analysis.py`** - Main generic symbol analysis tool
- **`symbol_analysis_runner.py`** - GitHub Actions wrapper
- **`demo_symbol_analysis.py`** - Demonstration script for multiple platforms
- **`SYMBOL_ANALYSIS.md`** - This documentation file

## Example Output

### UNO Platform Results

```
================================================================================
UNO SYMBOL ANALYSIS REPORT
================================================================================

SUMMARY:
  Total symbols: 51
  Total symbol size: 3767 bytes (3.7 KB)

LARGEST SYMBOLS (all symbols, sorted by size):
   1.   1204 bytes - ClocklessController<...>::showPixels(...)
   2.    572 bytes - CFastLED::show(unsigned char) [clone .constprop.18]
   3.    460 bytes - main
   4.    204 bytes - CPixelLEDController<...>::show(...)
   5.    168 bytes - CPixelLEDController<...>::showColor(...)

SYMBOL TYPE BREAKDOWN:
  t: 19 symbols, 2710 bytes (2.6 KB)
  T: 14 symbols, 904 bytes (0.9 KB)
  b: 15 symbols, 89 bytes (0.1 KB)
  d: 3 symbols, 64 bytes (0.1 KB)
```

### ESP32 Platform Results

```
================================================================================
ESP32DEV SYMBOL ANALYSIS REPORT
================================================================================

SUMMARY:
  Total symbols: 2503
  Total symbol size: 237092 bytes (231.5 KB)

LARGEST SYMBOLS (all symbols, sorted by size):
   1.  12009 bytes - _vfprintf_r
   2.  11813 bytes - _svfprintf_r
   3.   8010 bytes - _vfiprintf_r
   4.   7854 bytes - _svfiprintf_r
   5.   4192 bytes - port_IntStack

SYMBOL TYPE BREAKDOWN:
  T: 1240 symbols, 156230 bytes (152.6 KB)
  t: 371 symbols, 45841 bytes (44.8 KB)
  d: 475 symbols, 13032 bytes (12.7 KB)
  D: 60 symbols, 9421 bytes (9.2 KB)
  W: 126 symbols, 5209 bytes (5.1 KB)
```

### ESP32S3 Platform Results

```
================================================================================
ESP32S3 SYMBOL ANALYSIS REPORT
================================================================================

SUMMARY:
  Total symbols: 2831
  Total symbol size: 260710 bytes (254.6 KB)

LARGEST SYMBOLS (all symbols, sorted by size):
   1.  11309 bytes - _vfprintf_r
   2.  11118 bytes - _svfprintf_r
   3.   7337 bytes - _svfiprintf_r
   4.   7265 bytes - _vfiprintf_r
   5.   4192 bytes - port_IntStack
   6.   1164 bytes - printBeforeSetupInfo()
   7.    828 bytes - fl::RmtController5::loadPixelData(PixelIterator&)

SYMBOL TYPE BREAKDOWN:
  T: 1416 symbols, 172591 bytes (168.5 KB)
  t: 429 symbols, 48854 bytes (47.7 KB)
  d: 529 symbols, 14189 bytes (13.9 KB)
  D: 72 symbols, 9831 bytes (9.6 KB)
  W: 122 symbols, 5457 bytes (5.3 KB)
```

## Output Files

The tool generates JSON files with complete symbol data:

- `.build/{board}_symbol_analysis.json` - Complete analysis results

### JSON Structure

```json
{
  "summary": {
    "board": "uno",
    "total_symbols": 51,
    "total_size": 3767,
    "largest_symbols": [...],
    "type_breakdown": {...},
    "dependencies": {...}
  },
  "all_symbols_sorted_by_size": [
    {
      "address": "00001240",
      "size": 1204,
      "type": "t",
      "name": "_ZN19ClocklessController...",
      "demangled_name": "ClocklessController<...>::showPixels(...)"
    },
    ...
  ],
  "dependencies": {
    "module.cpp.o": ["symbol1", "symbol2", ...]
  }
}
```

## Supported Platforms

The tool automatically detects and works with any platform that has:
- A `build_info.json` file in `.build/{platform}/`
- Standard toolchain tools (nm, c++filt) in the build info aliases

Successfully tested platforms:
- ✅ **UNO** (AVR) - 51 symbols, 3.7 KB
- ✅ **ESP32DEV** (Xtensa) - 2503 symbols, 231.5 KB  
- ✅ **ESP32S3** (Xtensa) - 2831 symbols, 254.6 KB
- ✅ **TEENSY31** (ARM Cortex-M4) - 238 symbols, 10.9 KB
- ✅ **TEENSYLC** (ARM Cortex-M0+) - 241 symbols, 9.1 KB
- ✅ **DIGIX** (ARM Cortex-M3) - 298 symbols, 17.9 KB
- ✅ **STM32** (ARM Cortex-M3) - 311 symbols, 15.6 KB

## Key Improvements

### From ESP32-specific to Generic

**Before (ESP32 only):**
```python
# Hard-coded ESP32 board detection
esp32_boards = ["esp32dev", "esp32", "esp32s2", ...]

# FastLED-specific filtering
if any(keyword in search_text for keyword in [
    "fastled", "cfastled", "crgb", "hsv", ...]):
    fastled_symbols.append(symbol_info)
```

**After (Generic):**
```python
# Auto-detect any platform with build_info.json
for item in build_dir.iterdir():
    if item.is_dir() and (item / "build_info.json").exists():
        available_boards.append((build_info_file, item.name))

# No filtering - analyze ALL symbols
symbols.append(symbol_info)  # Everything included
```

### Symbol Analysis Improvements

1. **No Classification** - All symbols are included without filtering
2. **Complete Demangling** - All C++ symbols are properly demangled
3. **Platform Agnostic** - Works with any toolchain (AVR, ARM, Xtensa, etc.)
4. **Size-based Sorting** - All results sorted by symbol size for easy optimization targeting

## GitHub Actions Integration

The `symbol_analysis_runner.py` script provides GitHub Actions compatibility:

```yaml
- name: Run Symbol Analysis
  run: python ci/symbol_analysis_runner.py --board uno --example Blink
```

This runner:
- Validates build_info.json exists
- Handles errors gracefully with `--skip-on-failure`
- Provides proper exit codes for CI/CD

## Migration from ESP32 Analysis

If you were using the old ESP32-specific analysis:

**Old command:**
```bash
cd ci/ci && python esp32_symbol_analysis.py
```

**New command:**
```bash
cd ci && uv run symbol_analysis.py --board esp32dev
```

The new tool provides all the same information plus:
- Support for more platforms
- ALL symbols instead of just FastLED ones
- Better error handling
- Cleaner output format

## Advanced Usage

### Analyzing Custom Platforms

To add support for a new platform:

1. Ensure the platform builds correctly with PlatformIO
2. Verify `build_info.json` is generated in `.build/{platform}/`
3. Run: `uv run symbol_analysis.py --board {platform}`

### Batch Analysis

```bash
# Analyze all available platforms
uv run demo_symbol_analysis.py

# Analyze specific platforms in sequence
for board in uno esp32dev teensy31; do
    uv run symbol_analysis.py --board $board
done
```

### Integration with Build Systems

```python
# In your Python build script
from ci.ci.symbol_analysis import main as analyze_symbols
import sys

# Override arguments
sys.argv = ["symbol_analysis.py", "--board", "uno"]
analyze_symbols()
```

## Troubleshooting

### Common Issues

1. **"build_info.json not found"**
   - Solution: Compile the platform first: `uv run ci/ci-compile.py {board} --examples Blink`

2. **"KeyError: 'board_name'"**
   - Solution: The board name in the directory doesn't match the key in build_info.json
   - Check: `cat .build/{board}/build_info.json | jq 'keys'`

3. **"Tool not found"**
   - Solution: The toolchain isn't installed or path is incorrect in build_info.json

### Debug Mode

To debug issues, run with verbose Python output:
```bash
uv run python -v symbol_analysis.py --board uno
```

## Contributing

When adding new features:

1. Maintain platform independence
2. Don't add filtering logic
3. Test on multiple platforms
4. Update this documentation
5. Add tests to verify functionality

## Performance

Platform analysis times (approximate):
- **UNO**: < 1 second (51 symbols)
- **Teensy**: < 2 seconds (238-241 symbols)  
- **STM32**: < 3 seconds (311 symbols)
- **ESP32**: < 10 seconds (2503 symbols)

The tool scales well with symbol count and provides progress feedback during demangling.
