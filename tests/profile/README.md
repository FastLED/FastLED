# Performance Profiling Tests

This directory contains standalone performance benchmarking binaries for FastLED functions and modules.

## Purpose

Profile tests enable AI-driven profile-guided optimization by:
- Generating consistent, reproducible performance measurements
- Providing structured output for automated analysis
- Supporting iterative optimization workflows
- Validating performance improvements

## Architecture

**NOT Unit Tests**: Profile tests are standalone binaries that:
- Don't use doctest or test runners
- Produce structured JSON output for parsing
- Run independently via `bash profile` command
- Measure real-world performance characteristics

## Quick Start

```bash
# Generate and profile a function
bash profile sincos16

# Profile in Docker (consistent environment)
bash profile sincos16 --docker

# More iterations for better statistics
bash profile sincos16 --docker --iterations 50

# With callgrind analysis
bash profile sincos16 --docker --callgrind
```

## Generated Files

### Profile Test (`profile_<function>.cpp`)
Auto-generated template that MUST be customized:
- Add realistic test inputs
- Call target function correctly
- Ensure volatile prevents optimization

Example structure:
```cpp
#include "FastLED.h"
#include <chrono>
#include <cstdio>

__attribute__((noinline))
void benchmarkBaseline() {
    volatile int16_t result;
    for (int i = 0; i < PROFILE_ITERATIONS; i++) {
        result = target_function(test_input);
    }
    (void)result;
}

int main() {
    // Warmup + benchmark + structured output
}
```

### Result Files
- `profile_<function>_results.json` - Raw data (all iterations)
- `profile_<function>_results.ai.json` - Statistical summary

## Workflow

1. **Generate**: `bash profile <function>` creates template
2. **Customize**: Edit `tests/profile/profile_<function>.cpp`
3. **Build**: Compiles with release flags (-O2)
4. **Run**: Executes multiple iterations
5. **Analyze**: Computes statistics (best/median/worst/stdev)
6. **Optimize**: Create improved variant
7. **Compare**: Re-run and measure speedup
8. **Validate**: Ensure accuracy preserved

## Example: Optimizing sincos16

```bash
# 1. Profile baseline
bash profile sincos16 --docker --iterations 30
# Result: 47.8 ns/call (median)

# 2. Create optimized variant
# (Edit src/fl/math.cpp - reduce LUT size)

# 3. Profile optimized
bash profile sincos16_optimized --docker --iterations 30
# Result: 38.1 ns/call (median)

# 4. Calculate speedup
# 20% faster (47.8 → 38.1 ns/call)

# 5. Validate accuracy
# Generate and run accuracy tests
```

## JSON Output Format

Profile tests must output results in a **specific JSON format** for automated analysis by `ci/profile_runner.py`.

### Required Format

```
PROFILE_RESULT:{<json_object>}
```

The JSON object must contain these fields:

```json
{
  "variant": "baseline",      // Variant name (e.g., "baseline", "optimized", "simd")
  "target": "sincos32",       // Function/feature being profiled
  "total_calls": 100000,      // Number of function calls executed
  "total_time_ns": 674000,    // Total elapsed time in nanoseconds
  "ns_per_call": 6.74,        // Average nanoseconds per call (total_time_ns / total_calls)
  "calls_per_sec": 148367834  // Calls per second (1e9 / ns_per_call)
}
```

### Using ProfileResultBuilder Helper

To simplify creating correctly-formatted output, use the `ProfileResultBuilder` helper:

```cpp
#include "tests/profile/profile_result.h"

int main(int argc, char *argv[]) {
    bool json_output = (argc > 1 && fl::strcmp(argv[1], "baseline") == 0);

    // Run benchmark
    u32 t0 = ::micros();
    benchmark_function(CALLS);
    u32 t1 = ::micros();

    if (json_output) {
        // Automatically calculates ns_per_call and calls_per_sec
        ProfileResultBuilder::print_result(
            "baseline",           // variant
            "function_name",      // target
            CALLS,                // total_calls
            t1 - t0              // elapsed_us
        );
    } else {
        printf("Results: %lu us\n", t1 - t0);
    }

    return 0;
}
```

**Benefits:**
- Ensures correct JSON format (runner expects exact field names)
- Automatic calculation of `ns_per_call` and `calls_per_sec`
- Handles the `PROFILE_RESULT:` prefix automatically
- Prevents common formatting errors

### Example Output

```
PROFILE_RESULT:{"variant":"baseline","target":"sincos32","total_calls":100000,"total_time_ns":674000,"ns_per_call":6.74,"calls_per_sec":148367834.0}
```

## Build Integration

Profile tests are:
- Excluded from regular test discovery (see `tests/test_config.py`)
- Built via `tests/profile/meson.build`
- Compiled as standalone executables
- Use release mode flags for accurate measurement

**Manual build:**
```bash
uv run test.py profile_<function> --cpp --build-mode release --build
.build/meson-release/tests/profile_<function>.exe baseline
```

## Best Practices

1. **Sufficient iterations** - 100k+ calls for stable timing
2. **Warmup phase** - Prime caches before measurement
3. **Docker for consistency** - Use `--docker` flag
4. **Multiple runs** - bash profile runs 20+ iterations
5. **Prevent optimization** - Use volatile and __attribute__((noinline))
6. **Realistic inputs** - Test with actual usage patterns

## See Also

- `CLAUDE.md` - Performance Profiling & Optimization section
- `ci/AGENTS.md` - Profiling workflow for AI agents
- `tests/AGENTS.md` - Profile test conventions
- `ci/profile/` - Profiling infrastructure scripts
