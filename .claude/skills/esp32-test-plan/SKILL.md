---
name: esp32-test-plan
description: Generate a structured multi-layer test plan for FastLED changes targeting ESP32. Covers host unit tests, WASM compile checks, platform compile checks, and hardware validation. Use after defining an implementation contract, before writing any code.
argument-hint: <feature or change description>
context: fork
agent: esp32-test-plan-agent
---

Generate a comprehensive test plan for a FastLED ESP32 change, covering all applicable testing layers from host logic to hardware validation.

$ARGUMENTS

## Testing Philosophy in FastLED

FastLED uses a layered testing strategy where each layer catches a different class of bugs:

| Layer | What It Catches | Speed | Hardware Needed |
|-------|----------------|-------|----------------|
| Host unit tests | Logic bugs, math errors, data structure issues | Seconds | No |
| WASM compile | Cross-platform compile errors, API misuse | 1-2 min | No |
| Platform compile | Platform-specific guards, missing headers | 5-15 min | No |
| Hardware validation | Actual LED output correctness, timing | Manual | Yes |

**Rule**: Always maximize no-hardware test coverage. Hardware tests are the last resort, not the first.

## Step 1: Classify the Change

Determine which layers apply based on the type of change:

### Pure Logic Change (math, data structures, color algorithms)
- ✅ Host unit tests (primary)
- ✅ WASM compile check
- ❌ Platform compile (usually not needed)
- ❌ Hardware validation (not needed for pure logic)

### Platform Driver Change (RMT, I2S, SPI, PARLIO)
- ✅ Host unit tests (for any extractable logic)
- ✅ WASM compile check
- ✅ Platform compile (ESP32 + affected variants)
- ✅ Hardware validation (required — timing must be verified on real hardware)

### Public API Change (new parameter, new function, changed behavior)
- ✅ Host unit tests (test new API surface)
- ✅ WASM compile check (ensure examples still compile)
- ✅ Platform compile (at minimum ESP32 + AVR to catch regressions)
- ⚠️ Hardware validation (if behavior change affects output)

### Build System / CI Change
- ✅ CI run to verify
- ❌ Hardware validation (not needed)

### Documentation / Skill Change
- ❌ All layers (no code change)

## Step 2: Define Test Cases

For each applicable layer, list specific test cases:

**Positive cases**: Valid inputs produce expected output
**Negative cases**: Invalid inputs fail gracefully (NULL pointers, out-of-bounds, zero length)
**Boundary cases**: Limits of buffers, ranges, LED counts
**Regression cases**: Existing behavior must be preserved

### FastLED-Specific Test Considerations

- **LED count boundaries**: Test at 1 LED, typical (60, 144, 300), and large (1000+) counts
- **Color correctness**: Test primary colors (R, G, B), black, white, and mixed values
- **Brightness scaling**: Test setBrightness(0), setBrightness(128), setBrightness(255)
- **Platform guards**: Ensure code compiles on non-ESP32 platforms without changes
- **Memory constraints**: Check that code works within AVR 2KB SRAM limit if applicable

## Step 3: FastLED Test Commands

```bash
# Run all host unit tests
bash test --cpp

# Run a specific test by name
bash test TestName

# Run test in debug mode (ASAN/LSAN/UBSAN — always use when a test fails)
bash test TestName --debug

# Compile for WASM (catches most cross-platform issues without hardware)
bash compile wasm --examples Blink
bash compile wasm --examples Chromancer

# Compile for a specific ESP32 variant
bash compile esp32dev --examples Blink
bash compile esp32s3 --examples Blink
bash compile esp32c3 --examples Blink

# Run linting
bash lint

# Hardware validation (requires connected ESP32)
bash validate --rmt          # For RMT-based LED drivers
bash validate --spi          # For SPI-based LED drivers
bash validate --parlio       # For PARLIO (ESP32-S3 parallel)
```

## Step 4: Writing Host Unit Tests for FastLED

Host tests live in `tests/`. Conventions:
- Mirror source path: `src/fl/foo.h` → `tests/fl/foo.cpp`
- Use `FL_CHECK_EQ`, `FL_REQUIRE_TRUE` assertion macros
- Include `test.h` and `FastLED.h`
- No mocks — test real implementations
- Keep tests simple and fast (< 10 seconds each)

```cpp
// Example test structure for tests/fl/my_feature.cpp
#include "test.h"
#include "FastLED.h"
#include "fl/my_feature.h"

using namespace fl;

namespace {

TEST_CASE("MyFeature - basic operation") {
    MyFeature f;
    FL_CHECK_EQ(f.compute(0), 0);
    FL_CHECK_EQ(f.compute(255), 255);
}

TEST_CASE("MyFeature - boundary values") {
    MyFeature f;
    FL_REQUIRE_TRUE(f.compute(256) <= 255); // no overflow
}

} // namespace
```

## Step 5: Generate Test Plan Document

```markdown
# Test Plan: [Feature Name]

## Change Reference
- Implementation contract: [link or description]
- Related issue: [GitHub issue number]

## Test Layers

### Layer 1: Host Unit Tests
**Command**: `bash test TestName`

| Test Case | Input | Expected Output | Status |
|-----------|-------|-----------------|--------|
| [description] | [input] | [expected] | ⬜ Pending |

### Layer 2: WASM Compile Check
**Command**: `bash compile wasm --examples Blink`

| Check | Expected | Status |
|-------|----------|--------|
| Compiles without errors | No compiler errors | ⬜ Pending |
| Compiles without warnings | 0 new warnings | ⬜ Pending |

### Layer 3: Platform Compile Checks
**Command**: `bash compile <platform> --examples Blink`

| Platform | Command | Status |
|---------|---------|--------|
| ESP32 | `bash compile esp32dev --examples Blink` | ⬜ Pending |
| ESP32-S3 | `bash compile esp32s3 --examples Blink` | ⬜ Pending |
| ESP32-C3 | `bash compile esp32c3 --examples Blink` | ⬜ Pending |
| AVR (Uno) | `bash compile uno --examples Blink` | ⬜ Pending |

### Layer 4: Hardware Validation
**Command**: `bash validate --<driver>`

| Test Case | Setup | Expected Behavior | Status |
|-----------|-------|-------------------|--------|
| [description] | [board + wiring] | [expected LED output] | ⬜ Pending (hardware) |

## Infrastructure Requirements
- [ ] New test file: `tests/fl/[name].cpp`
- [ ] Example sketch modification (if testing new API): `examples/[name]/`
- [ ] Hardware board: [ESP32 variant] + [LED strip type]

## No-Hardware Tests
[Tests that run on host — must cover the majority of logic]

## Hardware-Required Tests
[Tests that need real ESP32 + LED strip]
- Must be verified by a human with the hardware
- Document serial output and visual confirmation

## Coverage Goals
- All new public API functions have at least one test case
- All error paths have a negative test case
- Critical timing constants have a verification test or hardware measurement
- No regressions: `bash test --cpp` passes with zero new failures

## Pass/Fail Criteria

| Criterion | Measurement |
|-----------|------------|
| Host tests pass | `bash test --cpp` exits 0 |
| WASM compiles | `bash compile wasm` exits 0 |
| Platform compiles | All listed platforms exit 0 |
| Hardware validated | Human-verified LED output correct |
| No regressions | 0 new test failures vs. main branch |
```

## Rules

1. **Every implementation contract must have a test plan before coding starts.**
2. **Maximize no-hardware test coverage** — extract pure logic into host-testable form.
3. **Hardware validation is non-optional for driver or timing changes** — WASM is not sufficient proof.
4. **Debug mode is mandatory when a test fails**: `bash test TestName --debug` enables ASAN/UBSAN that catch memory errors not visible in quick mode.
5. **Never skip the full regression check** (`bash test --cpp`) before declaring done.
