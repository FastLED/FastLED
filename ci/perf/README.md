# FastLED Header Compilation Performance Tracking

This directory contains tools for tracking FastLED header compilation performance using Clang's `-ftime-trace` feature.

## Understanding Timing Measurements

This tool reports **two types** of timing measurements. Understanding the difference is critical:

### 📊 Cumulative Time (Total Include Time)
**This is what we check against thresholds** because it represents the real user experience.

- **What it measures**: The TOTAL time from when the compiler starts processing a header until it finishes, INCLUDING all nested headers it includes recursively
- **Example**: When FastLED.h shows 1680ms cumulative time:
  - FastLED.h itself: ~276ms
  - Plus controller.h: ~856ms
  - Plus fl/stdint.h: ~588ms
  - Plus all other nested includes
  - **Total = 1680ms** (what the user experiences)
- **Why it matters**: When a user writes `#include <FastLED.h>`, they wait for the full cumulative time. This is the real-world impact.
- **Used for**: Threshold checking, performance targets, user experience metrics

### 🔬 Direct Time (Header-Only Time)
For detailed analysis and debugging only. NOT used for threshold checking.

- **What it measures**: ONLY the time spent processing the header's own content, EXCLUDING nested includes
- **Example**: FastLED.h direct time ~276ms = just parsing FastLED.h code itself
- **Why it matters**: Helps identify which specific header has complex code vs. which just includes many other headers
- **Used for**: Debugging, identifying optimization targets

### Why We Check Cumulative Time

**The thresholds in `thresholds.json` check CUMULATIVE times** because:
1. ✅ Reflects real user experience when including headers
2. ✅ Matches what developers see during compilation
3. ✅ Includes cost of all dependencies (the complete picture)
4. ✅ Prevents "death by a thousand cuts" (many small slow includes)

**Example**: If FastLED.h cumulative time is 1680ms, that's what a user experiences. We set the threshold to 2000ms max to ensure reasonable compile times in user sketches.

## Quick Start

```bash
# Run performance analysis with text report
uv run python ci/perf/compile_perf.py

# Generate JSON output
uv run python ci/perf/compile_perf.py --output=json

# Save trace file for debugging
uv run python ci/perf/compile_perf.py --save-trace

# Check against thresholds
uv run python ci/perf/compile_perf.py --check-thresholds

# Use custom compiler
uv run python ci/perf/compile_perf.py --compiler=clang++-17

# Set custom warning threshold (default: 50ms)
uv run python ci/perf/compile_perf.py --threshold=100
```

## Files

- **`test_compile.cpp`** - Minimal test file that includes FastLED.h
- **`compile_perf.py`** - Main script that compiles with -ftime-trace and analyzes results
- **`thresholds.json`** - Performance thresholds configuration
- **`README.md`** - This file

## How It Works

1. Compiles `test_compile.cpp` with `clang++ -ftime-trace`
2. Parses the generated JSON trace file
3. Analyzes:
   - Total compilation time
   - Compilation phases (Frontend, Backend, Templates, etc.)
   - Individual header parse times
   - Slowest operations
4. Generates reports and checks against thresholds

## GitHub Action

The `.github/workflows/header-perf.yml` workflow:
- Runs on pushes to master/main that modify header files
- Runs on pull requests that modify header files
- Posts performance report as PR comment
- Uploads detailed reports as artifacts
- Fails if performance exceeds thresholds

## Thresholds

Current thresholds from `thresholds.json` (ALL thresholds check **CUMULATIVE** times):

- **Total compile time**: 2000ms max
- **Header warning threshold**: 50ms cumulative (warns if any header's total include time exceeds this)
- **Header error threshold**: 150ms cumulative (fails if any header's total include time exceeds this)
- **Template instantiation**: <15% of total time
- **Known slow headers**: Specific cumulative thresholds for headers we're actively tracking:
  - `FastLED.h`: 2000ms cumulative (main user-facing header)
  - `controller.h`: 900ms cumulative
  - `cpixel_ledcontroller.h`: 900ms cumulative
  - `pixeltypes.h`: 800ms cumulative
  - `fl/str.h`: 650ms cumulative
  - `crgb.hpp`: 650ms cumulative

**Note**: All thresholds are for cumulative times (including nested headers) to reflect real user compilation experience.

## Example Output

```
================================================================================
FASTLED COMPILATION PERFORMANCE REPORT
================================================================================
Generated: 2025-10-07 23:51:35
Compiler: clang++
File: ci/perf/test_compile.cpp

SUMMARY
--------------------------------------------------------------------------------
Total Compilation Time:    205.06 ms
Frontend Time:             1.81 ms (0.9%)
Backend Time:              7.83 ms (3.8%)

COMPILATION PHASES
--------------------------------------------------------------------------------
Templates                    14.70 ms (  7.2%)

FASTLED HEADERS (Level 1 - Direct Includes)
--------------------------------------------------------------------------------
     0.0 ms - FastLED.h
     0.0 ms - fl/stdint.h
     ...

TOP 20 SLOWEST OPERATIONS
--------------------------------------------------------------------------------
  1.    75.0 ms - Total ParseDeclarationOrFunctionDefinition
  2.    68.4 ms - Total ParseClass
  ...

PERFORMANCE FLAGS
--------------------------------------------------------------------------------
✓  No headers exceed 50.0ms threshold
✓  Template instantiation time is acceptable (<15% of total)
✓  Lexing time is acceptable (<10% of total)
```

## Requirements

- Clang compiler with `-ftime-trace` support (Clang 9+)
- Python 3.11+
- FastLED source code

## Troubleshooting

**"Compiler 'clang++' not found"**
- Install Clang or specify path with `--compiler=/path/to/clang++`

**"Trace file not generated"**
- Ensure your Clang version supports `-ftime-trace` (Clang 9+)
- Try with `--save-trace` to debug

**Compilation errors**
- The script uses `FASTLED_TESTING` mode to simplify platform dependencies
- Should work on Windows, Linux, and macOS

## Integration with CI

The GitHub Action automatically:
1. Runs on header changes
2. Posts performance reports to PRs
3. Uploads artifacts
4. Checks thresholds and fails if exceeded

## Future Enhancements

- Historical trend tracking
- Comparison between PR and base branch
- Multi-compiler testing (GCC, MSVC)
- Interactive HTML dashboard
- Baseline enforcement (fail if >10% regression)
