# How to Profile and Benchmark FastLED Code

This guide documents the profiling workflow used for optimizing Chasing Spirals Q31 (7.9% speedup achieved). Follow these steps to profile your own code changes.

---

## Table of Contents

1. [Quick Start](#quick-start)
2. [Setting Up a Profiler Binary](#setting-up-a-profiler-binary)
3. [Compilation Modes](#compilation-modes)
4. [Running Benchmarks](#running-benchmarks)
5. [Statistical Analysis](#statistical-analysis)
6. [Accuracy Validation](#accuracy-validation)
7. [Docker-Based Profiling](#docker-based-profiling)
8. [Best Practices](#best-practices)

---

## Quick Start

```bash
# 1. Create a profiler in tests/fx/profile_myfeature.cpp
# 2. Compile with profile mode
uv run test.py profile_myfeature --cpp --build-mode profile --build

# 3. Run 20 iterations for statistics
for i in {1..20}; do .build/meson-profile/tests/profile_myfeature.exe; done

# 4. Run accuracy tests
uv run test.py myfeature --cpp

# 5. Analyze results and iterate
```

---

## Setting Up a Profiler Binary

### 1. Create a Standalone Profiler

Create `tests/fx/profile_myfeature.cpp`:

```cpp
#include "FastLED.h"
#include <chrono>
#include <cstdio>

using namespace fl;

static const uint16_t W = 32;
static const uint16_t H = 32;
static const uint16_t N = W * H;

static const int WARMUP_FRAMES = 20;
static const int PROFILE_FRAMES = 200;

// Noinline ensures profiler can target this function
__attribute__((noinline))
void renderVariant1(MyFx &fx, CRGB *leds, int frames, int start_frame) {
    for (int i = 0; i < frames; i++) {
        uint32_t t = static_cast<uint32_t>((start_frame + i) * 16);
        Fx::DrawContext ctx(t, leds);
        fx.draw(ctx);
    }
}

__attribute__((noinline))
void renderVariant2(MyFx &fx, CRGB *leds, int frames, int start_frame) {
    // Your optimized variant here
}

int main(int argc, char *argv[]) {
    bool do_variant1 = true;
    bool do_variant2 = true;

    // CLI argument parsing
    if (argc > 1) {
        do_variant1 = false;
        do_variant2 = false;
        if (::strcmp(argv[1], "variant1") == 0) {
            do_variant1 = true;
        } else if (::strcmp(argv[1], "variant2") == 0) {
            do_variant2 = true;
        } else {
            do_variant1 = true;
            do_variant2 = true;
        }
    }

    CRGB leds[N] = {};

    // ========================
    // Variant 1 (Baseline)
    // ========================
    if (do_variant1) {
        XYMap xy = XYMap::constructRectangularGrid(W, H);
        MyFx fx(xy, /* config */);

        // Warmup (not profiled)
        renderVariant1(fx, leds, WARMUP_FRAMES, 0);

        // Profile
        auto t0 = std::chrono::high_resolution_clock::now();
        renderVariant1(fx, leds, PROFILE_FRAMES, WARMUP_FRAMES);
        auto t1 = std::chrono::high_resolution_clock::now();

        auto elapsed_us = std::chrono::duration_cast<std::chrono::microseconds>(t1 - t0).count();
        printf("Variant1: %d frames in %lld us (%.1f us/frame)\n",
               PROFILE_FRAMES, (long long)elapsed_us,
               (double)elapsed_us / PROFILE_FRAMES);
    }

    // ========================
    // Variant 2 (Optimized)
    // ========================
    if (do_variant2) {
        // Similar structure to Variant 1
        // ...
    }

    return 0;
}
```

### 2. Key Profiler Design Principles

- **Warmup frames**: Prime caches and LUTs (10-20 frames)
- **Multiple iterations**: Run 100-200 frames for stable timing
- **Noinline functions**: Ensures profilers (callgrind) can target specific code
- **CLI arguments**: Test variants independently (e.g., `./profiler variant2`)
- **Separate contexts**: Each variant gets fresh setup to avoid cross-contamination

---

## Compilation Modes

FastLED supports 4 build modes optimized for different use cases:

### Profile Mode (Default for Benchmarking)

```bash
uv run test.py profile_myfeature --cpp --build-mode profile --build
```

**Flags**: `-Os -g`
- **Purpose**: Realistic production performance with debug symbols
- **Output**: `.build/meson-profile/tests/profile_myfeature.exe`
- **Use case**: Performance benchmarking (what users experience)

### Quick Mode (Default for Tests)

```bash
uv run test.py profile_myfeature --cpp --build-mode quick --build
```

**Flags**: `-O1 -g1`
- **Purpose**: Fast iteration during development
- **Use case**: Quick smoke tests

### Debug Mode (Deep Debugging)

```bash
uv run test.py profile_myfeature --cpp --build-mode debug --build
```

**Flags**: `-O0 -g3 -fsanitize=address,undefined`
- **Purpose**: Find memory bugs, undefined behavior
- **Sanitizers**: ASan (address), UBSan (undefined behavior)
- **Binary size**: 3.3x larger than quick mode
- **Use case**: Crash debugging, memory corruption

### Release Mode (Maximum Performance)

```bash
uv run test.py profile_myfeature --cpp --build-mode release --build
```

**Flags**: `-O2 -DNDEBUG`
- **Purpose**: Maximum optimization testing
- **Binary size**: Smallest (20% smaller than quick)
- **Use case**: Performance ceiling analysis

---

## Running Benchmarks

### Single Run

```bash
.build/meson-profile/tests/profile_myfeature.exe
```

### Multiple Runs for Statistics (Recommended)

```bash
# Run 20 iterations and extract timing
for i in {1..20}; do
    .build/meson-profile/tests/profile_myfeature.exe | grep "Variant2"
done
```

**Output Example:**
```
Variant2: 200 frames in 14300 us (71.5 us/frame)
Variant2: 200 frames in 14480 us (72.4 us/frame)
Variant2: 200 frames in 14100 us (70.5 us/frame)
...
```

### Test Specific Variants

```bash
# Profile only variant2 (faster)
.build/meson-profile/tests/profile_myfeature.exe variant2
```

### Automated Statistical Analysis

Create `.loop/analyze_perf.py`:

```python
import sys
import statistics

values = []
for line in sys.stdin:
    if "us/frame" in line:
        # Extract float from "(71.5 us/frame)"
        val = float(line.split("(")[1].split(" us/frame")[0])
        values.append(val)

if values:
    print(f"Best:   {min(values):.1f} μs")
    print(f"Median: {statistics.median(values):.1f} μs")
    print(f"Worst:  {max(values):.1f} μs")
    print(f"StdDev: {statistics.stdev(values):.1f} μs")
```

**Usage:**
```bash
for i in {1..20}; do .build/meson-profile/tests/profile_myfeature.exe variant2; done | python .loop/analyze_perf.py
```

---

## Statistical Analysis

### Interpreting Results

**Example output from Chasing Spirals optimization:**

| Implementation | Best | Median | Improvement |
|----------------|------|--------|-------------|
| Q31 Original | 74.0 μs | 77.6 μs | Baseline |
| Q16 Optimized | 69.9 μs | 71.5 μs | +7.9% |

**Key Metrics:**
- **Best case**: Lowest observed time (ideal conditions)
- **Median**: 50th percentile (typical performance)
- **Worst case**: Highest time (thermal throttling, background processes)
- **Standard deviation**: Consistency metric (lower is better)

### When is a Speedup Real?

1. **Median improvement > 2%**: Likely real (outside measurement noise)
2. **Best case improvement > 1%**: Worth investigating
3. **Improvement < 1%**: Measurement noise (not reliable)

**Run 20 iterations minimum** to account for:
- CPU thermal variation
- Background processes
- Cache state differences
- OS scheduler jitter

### Reducing Measurement Noise

```bash
# Close unnecessary programs
# Let CPU cool down between runs
sleep 10

# Run with process priority (Windows)
start /high .build/meson-profile/tests/profile_myfeature.exe

# Linux: use nice/taskset for CPU pinning
taskset -c 0 nice -n -20 ./profile_myfeature
```

---

## Accuracy Validation

**CRITICAL**: Every optimization MUST pass accuracy tests before being accepted.

### 1. Create Accuracy Tests

Add to `tests/fx/myfeature.cpp`:

```cpp
#include "test.h"

namespace {

const uint16_t W = 32;
const uint16_t H = 32;
const uint16_t N = W * H;

void renderBaseline(CRGB *leds, uint32_t timeMs) {
    XYMap xy = XYMap::constructRectangularGrid(W, H);
    MyFxBaseline fx(xy);
    Fx::DrawContext ctx(timeMs, leds);
    fx.draw(ctx);
}

void renderOptimized(CRGB *leds, uint32_t timeMs) {
    XYMap xy = XYMap::constructRectangularGrid(W, H);
    MyFxOptimized fx(xy);
    Fx::DrawContext ctx(timeMs, leds);
    fx.draw(ctx);
}

// Count mismatched pixels
int countMismatches(const CRGB *a, const CRGB *b, uint16_t count) {
    int mismatches = 0;
    for (uint16_t i = 0; i < count; i++) {
        if (a[i] != b[i]) {
            mismatches++;
        }
    }
    return mismatches;
}

// Compute per-channel average absolute error
float computeAvgError(const CRGB *a, const CRGB *b, uint16_t count) {
    int64_t total_error = 0;
    for (uint16_t i = 0; i < count; i++) {
        total_error += abs(int(a[i].r) - int(b[i].r));
        total_error += abs(int(a[i].g) - int(b[i].g));
        total_error += abs(int(a[i].b) - int(b[i].b));
    }
    return static_cast<float>(total_error) / (count * 3);
}

// Compute maximum per-channel error
int computeMaxError(const CRGB *a, const CRGB *b, uint16_t count) {
    int max_err = 0;
    for (uint16_t i = 0; i < count; i++) {
        int er = abs(int(a[i].r) - int(b[i].r));
        int eg = abs(int(a[i].g) - int(b[i].g));
        int eb = abs(int(a[i].b) - int(b[i].b));
        int m = er > eg ? er : eg;
        m = m > eb ? m : eb;
        if (m > max_err) max_err = m;
    }
    return max_err;
}

} // namespace

FL_TEST_CASE("MyFeature - low error at t=1000") {
    CRGB leds_baseline[N] = {};
    CRGB leds_optimized[N] = {};

    renderBaseline(leds_baseline, 1000);
    renderOptimized(leds_optimized, 1000);

    int mismatches = countMismatches(leds_baseline, leds_optimized, N);
    float avg_err = computeAvgError(leds_baseline, leds_optimized, N);
    int max_err = computeMaxError(leds_baseline, leds_optimized, N);

    FL_MESSAGE("t=1000: mismatches=", mismatches, "/", N,
            " avg_err=", avg_err, " max_err=", max_err);

    float error_pct = avg_err / 255.0f * 100.0f;
    FL_MESSAGE("Average error at t=1000: ", error_pct, "%");

    // Thresholds (adjust based on your tolerance)
    FL_CHECK_MESSAGE(error_pct < 1.0f,
                  "Average error should be < 1% at t=1000");
    FL_CHECK_MESSAGE(max_err <= 6,
                  "Max per-channel error should be <= 6 at t=1000");
}

FL_TEST_CASE("MyFeature - approximate at high time") {
    const uint32_t times[] = {1000000, 100000000, 2000000000};

    for (uint32_t high_time : times) {
        CRGB leds_baseline[N] = {};
        CRGB leds_optimized[N] = {};

        renderBaseline(leds_baseline, high_time);
        renderOptimized(leds_optimized, high_time);

        float avg_err = computeAvgError(leds_baseline, leds_optimized, N);
        int max_err = computeMaxError(leds_baseline, leds_optimized, N);

        float error_pct = avg_err / 255.0f * 100.0f;
        FL_MESSAGE("t=", high_time, ": avg_err=", avg_err,
                " max_err=", max_err, " error_pct=", error_pct, "%");

        FL_CHECK_MESSAGE(error_pct < 3.0f,
                      "Average error should be < 3% at high time values");
    }
}
```

### 2. Run Accuracy Tests

```bash
uv run test.py myfeature --cpp
```

### 3. Accuracy Thresholds

**For integer approximations of float code:**

| Metric | Low Time (t=1000) | High Time (t>1M) |
|--------|-------------------|------------------|
| **Avg error** | < 1% | < 3% |
| **Max error** | ≤ 6 per channel | ≤ 10 per channel |
| **Mismatches** | Informational only | Informational only |

**For exact implementations:**
- **Mismatches**: 0 (bit-perfect match)
- **Avg error**: 0.0%
- **Max error**: 0

---

## Docker-Based Profiling

Docker provides a consistent Linux environment with sanitizers (ASAN/LSAN) for catching memory bugs.

### Quick Docker Profile

```bash
# Run profiler in Docker (implies --debug with sanitizers)
bash test --docker tests/fx/profile_myfeature

# Fast mode (no sanitizers)
bash test --docker tests/fx/profile_myfeature --quick

# Release mode (optimized)
bash test --docker tests/fx/profile_myfeature --build-mode release
```

### Docker Compilation

Compile your profiler for Docker environment:

```bash
# Compile in Docker
bash compile --docker tests/fx/profile_myfeature.cpp

# Then run manually
docker run -it --rm -v $(pwd):/workspace fastled-test \
    /workspace/.build/meson-docker-debug/tests/profile_myfeature
```

### When to Use Docker

**Use Docker when:**
- ✅ Reproducing CI failures that only occur in Linux
- ✅ Testing with AddressSanitizer (ASAN) / LeakSanitizer (LSAN)
- ✅ Verifying cross-platform compatibility

**Avoid Docker when:**
- ❌ Iterating quickly on performance (3-5 minutes per test)
- ❌ Running 20+ benchmark iterations (use local build)
- ❌ Debugging Windows-specific code

### Docker Performance Notes

- **First run**: Downloads Docker image + Python packages (~5 minutes)
- **Subsequent runs**: Uses cached volumes (~30-60 seconds per test)
- **Sanitizers**: Add 2-3x slowdown but catch memory bugs
- **Use `--quick` flag**: Skip sanitizers for faster iteration

---

## Best Practices

### 1. Isolate Variables

**One change at a time:**
```bash
# ❌ Bad: Combined changes
- Reduced precision from Q24 to Q16
- Added batching
- Changed color order
# Which change caused the speedup?

# ✅ Good: Incremental changes
- Baseline: Q24
- Test 1: Q24 with batching
- Test 2: Q16 without batching
- Test 3: Q16 with batching
```

### 2. Profile Before Optimizing

**Measure first, optimize second:**
```bash
# 1. Profile to find bottleneck
valgrind --tool=callgrind ./profile_myfeature baseline

# 2. Analyze callgrind output
kcachegrind callgrind.out.12345

# 3. Optimize hot path (top 20% of time)

# 4. Verify speedup
```

### 3. Document Negative Results

**Example: PERLIN_OPTIMIZATION_RESULTS.md**
```markdown
## Phase 1: Permutation Table Prefetching

**Goal**: Reduce pipeline stalls with __builtin_prefetch()

**Result**: 0% improvement (74.3 μs → 74.1 μs, measurement noise)

**Conclusion**: Hardware prefetching already optimal. Manual hints provide no benefit.

**Code reverted**: Yes
```

**Why document failures?**
- Prevents future wasted effort
- Shows what was already tried
- Educates other developers

### 4. Compare Apples to Apples

**Use same conditions for all tests:**
- Same grid size (e.g., 32x32)
- Same number of frames (e.g., 200)
- Same warmup frames (e.g., 20)
- Same build mode (profile)
- Same machine state (idle CPU)

### 5. Trust the Compiler

**Modern compilers (Clang 21.1.5) are excellent:**

```cpp
// ❌ Manual optimization that compiler beats
#define MANUAL_INLINE __attribute__((always_inline))
MANUAL_INLINE int lerp(...) { ... }

// ✅ Let compiler decide
FASTLED_FORCE_INLINE int lerp(...) { ... }
```

**From Chasing Spirals optimization:**
- Manual prefetch: 0% improvement
- Manual gradient packing: -6.1% (slower!)
- Manual lerp inlining: -4.6% (slower!)

**Compiler already optimized:**
- Auto-inlining
- Hardware prefetching
- Register allocation
- Instruction scheduling

### 6. Version Control Your Variants

```bash
# Create feature branch for optimization work
git checkout -b optimize-chasing-spirals

# Commit each variant separately
git commit -m "Add Q16 Perlin variant (16 fractional bits)"
git commit -m "Add Batch4 color-grouped variant"

# Easy to revert if needed
git revert HEAD~1
```

---

## Example: Chasing Spirals Optimization

Complete workflow from the real optimization that achieved 7.9% speedup:

### Step 1: Create Profiler

`tests/fx/profile_chasing_spirals.cpp`:
- Float baseline (original Animartrix)
- Q31 original
- Q31 Batch4 (4-pixel loop unrolling)
- Q31 Batch4 Color-Grouped
- Q31 Batch8 (8-pixel loop unrolling)
- Q16 Batch4 Color-Grouped

### Step 2: Compile and Run

```bash
uv run test.py profile_chasing_spirals --cpp --build-mode profile --build

# Run 20 iterations for each variant
for i in {1..20}; do
    .build/meson-profile/tests/profile_chasing_spirals.exe q31 | grep "Q31 (original)"
done

for i in {1..20}; do
    .build/meson-profile/tests/profile_chasing_spirals.exe q16 | grep "Q16"
done
```

### Step 3: Analyze Results

| Implementation | Best | Median | vs Original |
|----------------|------|--------|-------------|
| Q31 Original | 74.0 μs | 77.6 μs | Baseline |
| Q31 Batch4 | 70.5 μs | 74.5 μs | +4.0% |
| Q31 Batch4 Color | 70.8 μs | 74.0 μs | +4.6% |
| **Q16 Batch4 Color** | **69.9 μs** | **71.5 μs** | **+7.9%** ✅ |

### Step 4: Validate Accuracy

```bash
uv run test.py animartrix2 --cpp
```

**Q16 Results:**
- t=1000: avg_err = **0.0048%** (threshold < 1.5%) ✅
- t=1000: max_err = **1** (threshold ≤ 8) ✅
- High time: avg_err < 0.1% ✅

### Step 5: Document

Created comprehensive documentation:
- `PERLIN_OPTIMIZATION_RESULTS.md` - Failed attempts (Phases 1-3)
- `CHASING_SPIRALS_PERFORMANCE_AUDIT.md` - Performance breakdown
- `OPTIMIZATION_SUMMARY.md` - Executive summary

### Step 6: Make Default

Updated `src/fl/fx/2d/animartrix2.hpp` line 117:
```cpp
{ANIM2_CHASING_SPIRALS, "CHASING_SPIRALS",
 &q16::Chasing_Spirals_Q16_Batch4_ColorGrouped},
```

---

## Troubleshooting

### "Test failed with return code 3221225477"

**Windows access violation (0xC0000005)**. Common causes:

1. **Null pointer dereference**
   ```cpp
   // Check your pointers!
   if (e->mCtx->leds == nullptr) { /* handle */ }
   ```

2. **Buffer overflow**
   ```cpp
   // Check array bounds
   FL_CHECK(i < total_pixels, "Index out of bounds");
   ```

3. **Type overflow**
   ```cpp
   // ❌ i16 HP_ONE = 1 << 16;  // Overflow!
   // ✅ i32 HP_ONE = 1 << 16;  // Correct
   ```

**Debug with:**
```bash
# Run in debug mode with sanitizers
uv run test.py profile_myfeature --cpp --debug
```

### "Performance worse than baseline"

1. **Check compiler optimization flags**
   ```bash
   # Use profile mode, not debug
   --build-mode profile
   ```

2. **Verify warmup frames are sufficient**
   ```cpp
   static const int WARMUP_FRAMES = 20;  // Prime caches
   ```

3. **Run more iterations** (reduce measurement noise)
   ```bash
   for i in {1..50}; do ./profiler; done
   ```

4. **Check CPU throttling**
   ```bash
   # Let CPU cool down
   sleep 60
   ```

### "Sanitizer reports memory leak"

```bash
# Run with leak detection
uv run test.py myfeature --cpp --debug

# Output shows:
# LEAK SUMMARY: 64 bytes in 1 allocation
```

**Fix:**
```cpp
// Use RAII (fl::vector, fl::unique_ptr) instead of raw pointers
fl::vector<int> data(100);  // Auto-cleanup
```

---

## Advanced: Valgrind Profiling

For deep performance analysis:

```bash
# Compile with profile mode
uv run test.py profile_myfeature --cpp --build-mode profile --build

# Run under callgrind
valgrind --tool=callgrind \
         --callgrind-out-file=callgrind.out \
         .build/meson-profile/tests/profile_myfeature baseline

# Analyze with kcachegrind (GUI)
kcachegrind callgrind.out

# Or text analysis
callgrind_annotate callgrind.out
```

**Look for:**
- Functions consuming > 10% of time
- Unexpected function calls in hot path
- Cache misses (--cache-sim=yes)

---

## Summary Checklist

Before declaring an optimization complete:

- [ ] Created standalone profiler binary
- [ ] Compiled with `--build-mode profile`
- [ ] Ran 20+ iterations for statistics
- [ ] Calculated best/median/worst times
- [ ] Verified speedup > 2% (outside noise)
- [ ] Created accuracy tests
- [ ] All accuracy tests pass
- [ ] Documented what was tried (including failures)
- [ ] Updated default if optimization is best
- [ ] Committed changes to version control

**Remember**: Measure twice, optimize once. Profile-guided optimization beats guesswork every time.
