# Agent Loop: SPI Test Failures Investigation

## Executive Summary

**Status**: Test suite failing (1/119 tests fail) - `fl_spi` test cannot initialize SPI devices
**Attempts**: 3/5 codeup runs attempted
**Critical Issue**: SPI device `begin()` fails due to bus manager promotion logic conflicts with stub platform

## Problem History

### Initial Issue (Attempt 1)
- **Command**: `codeup` (global command for code validation)
- **Error**: Ninja build error - `spi_dual_stub.cpp` missing
- **Cause**: Files were renamed but meson build cache still referenced old names:
  - `spi_single_stub.cpp` → `spi_1_stub.cpp`
  - `spi_dual_stub.cpp` → `spi_2_stub.cpp`
  - `spi_quad_stub.cpp` → `spi_4_stub.cpp`
  - `spi_octal_stub.cpp` → `spi_8_stub.cpp`

### Fix Applied (Attempt 1→2)
- Removed stale meson build directory: `rm -rf .build/meson`
- This forced meson to rediscover sources using `ci/collect_sources.py`

### Secondary Issue (Attempt 2)
- **Error**: Same test failure - alignment panic `misaligned address 0x1`
- **Stack Trace**:
  ```
  thread 48488 panic: member access within misaligned address 0x1 for type 'fl::spi::Transaction::Impl'
  C:\Users\niteris\dev\fastled8\src\fl\spi\device.cpp:415:0: in fl::spi::Transaction::wait
      if (pImpl->completed) {
  C:\Users\niteris\dev\fastled8\tests\fl\spi.cpp:827:0: in DOCTEST_ANON_FUNC_62
      CHECK(txn.wait());
  ```

### Root Cause Analysis
The alignment error was a **symptom**, not the root cause:

1. **Real Problem**: Test code was accessing `Result<Transaction>.value()` when `result.ok() == false`
2. **Why**: The test used `CHECK(result.ok())` which continues execution on failure
3. **Result**: Accessing uninitialized storage containing garbage, interpreted as pointer `0x1`

### Fix Applied (Attempt 2→3)
**File**: `tests/fl/spi.cpp:814-834`

Changed from:
```cpp
Device spi(cfg);
spi.begin();  // No error checking

uint8_t data[4] = {0x01, 0x02, 0x03, 0x04};
Result<Transaction> result = spi.writeAsync(data, 4);

CHECK(result.ok());  // Continues on failure!

Transaction txn = fl::move(result.value());  // Crash if CHECK failed
```

To:
```cpp
Device spi(cfg);
fl::optional<fl::Error> begin_result = spi.begin();
REQUIRE(!begin_result);  // Stops test on failure

uint8_t data[4] = {0x01, 0x02, 0x03, 0x04};
Result<Transaction> result = spi.writeAsync(data, 4);

REQUIRE(result.ok());  // Stops test on failure before accessing value()

Transaction txn = fl::move(result.value());  // Safe now
```

### Current Issue (Attempt 3)
**Error**: `REQUIRE(!begin_result)` is failing - `spi.begin()` returns an error

**Test Output**:
```
../../tests/fl/spi.cpp:819: FATAL ERROR: REQUIRE( !begin_result ) is NOT correct!
  values: REQUIRE( false )

stderr:
src/platforms/shared/spi_bus_manager.h(805): SPI: Initialized Dual-SPI controller 'MockDual0'
WARN: SPI Manager: Promoted clock pin 18 to Dual-SPI (2 devices)
WARN: SPI Manager: Cannot promote clock pin 18 (platform limitation)
WARN: SPI Manager: Disabled device 1 on clock pin 18 (conflict)
WARN: SPI Manager: Disabled device 2 on clock pin 18 (conflict)
WARN: SPI Device: Bus initialization failed
```

## Core Problem: Bus Manager Promotion Logic

### The Sequence
1. Test creates `Device spi(Config(18, 19))`  - requesting pin 18 as clock
2. Test calls `spi.begin()`
3. Bus manager detects opportunity to "promote" to Dual-SPI (2 lanes)
4. Bus manager tries to initialize Dual-SPI on pin 18
5. **Promotion succeeds initially** (line 805)
6. Bus manager then checks platform limitations
7. **Platform limitation detected** - stub doesn't support multi-lane on pin 18
8. Bus manager disables devices and fails initialization

### Why This Is Happening
The SPI bus manager has logic to automatically promote single-lane requests to multi-lane (dual/quad/octal) when multiple devices share the same clock pin. However:

- **Stub platform has limitations** - cannot actually support multi-lane SPI
- **Promotion succeeds** before limitation check
- **Then rolls back** but initialization still fails
- Tests expect simple single-lane SPI on stub platform

### Code Locations

#### Bus Manager (Promotion Logic)
- **File**: `src/platforms/shared/spi_bus_manager.h`
- **Line 805**: Dual-SPI initialization succeeds
- **Line 1011**: Fallback message "Using standard single-lane SPI (bus manager passthrough mode)"
- **Line 1114**: `releaseBusHardware()` called
- **Line 1120**: `releaseBusHardware() bus_type=...`

#### Device Initialization
- **File**: `src/fl/spi/device.cpp`
- Calls bus manager to initialize
- Returns error when bus manager fails

#### Test File
- **File**: `tests/fl/spi.cpp`
- **Line 814-834**: `TEST_CASE("Device writeAsync operations")`
- **Line 816**: Creates Config with pins 18, 19
- **Line 818-819**: Calls `begin()` and checks for error

#### Stub Implementation
- **Files**: `src/platforms/stub/spi_1_stub.{cpp,h}` (and _2, _4, _8, _16)
- Mock implementations for testing
- Should support basic single-lane SPI without promotion

## Investigation Needed

### Questions to Answer
1. **Why is bus manager trying to promote on pin 18?**
   - Is there a previous test that registered multiple devices on pin 18?
   - Is there global state not being cleaned up between tests?

2. **Should stub platform support multi-lane SPI at all?**
   - Review stub platform capabilities
   - Check if promotion should be disabled for stub builds

3. **Is the promotion logic correct?**
   - Should it check platform limitations BEFORE attempting promotion?
   - Should it gracefully fall back to single-lane instead of failing?

4. **Are there global state issues?**
   - Does bus manager maintain state across tests?
   - Do we need better cleanup in test teardown?

### Files to Review

**Bus Manager Core Logic**:
- `src/platforms/shared/spi_bus_manager.h` - Main implementation (header-only?)
- Search for: `"Promoted clock pin"`, `"Cannot promote"`, `"platform limitation"`

**Stub Platform Config**:
- `src/platforms/stub/spi_1_stub.cpp` - Single-lane stub
- `src/platforms/stub/spi_2_stub.cpp` - Dual-lane stub
- `src/platforms/stub/spi_4_stub.cpp` - Quad-lane stub
- `src/platforms/stub/spi_8_stub.cpp` - Octal-lane stub
- Check what capabilities each reports

**Device Initialization**:
- `src/fl/spi/device.cpp` - Device::begin() implementation
- `src/fl/spi/device.h` - Device interface

**Test Suite**:
- `tests/fl/spi.cpp` - All SPI tests
- Check test ordering, setup/teardown
- Look for global state initialization

## Potential Solutions

### Option 1: Disable Promotion for Stub Platform
Add platform detection to skip promotion on stub builds:
```cpp
#ifdef STUB_PLATFORM
// Skip multi-lane promotion on stub platform
return single_lane_init();
#endif
```

### Option 2: Fix Promotion Check Order
Check platform limitations BEFORE attempting promotion:
```cpp
if (can_promote_to_dual(pin) && platform_supports_dual()) {
    // Only try if platform supports it
    return dual_spi_init();
}
```

### Option 3: Better Fallback Handling
Make promotion failure non-fatal, fall back to single-lane:
```cpp
Result dual_result = try_dual_init();
if (!dual_result.ok()) {
    // Fall back to single-lane
    return single_lane_init();
}
```

### Option 4: Test Isolation
Ensure tests don't leak state:
```cpp
TEST_CASE("...") {
    // Setup
    SPIBusManager::reset();  // Clear any global state

    // ... test code ...

    // Teardown
    SPIBusManager::reset();
}
```

## Git Status

### Staged Files (32 files)
All SPI-related refactoring changes are staged:
- `src/FastLED.h`
- `src/fl/spi/device.{cpp,h}`
- `src/fl/spi/impl.h`
- `src/platforms/README_SPI_*.md` (documentation)
- `src/platforms/esp/32/drivers/i2s/*` (ESP32 I2S driver)
- `src/platforms/shared/spi_hw_{1,2,4,8,16}.{cpp,h}` (hardware layer)
- `src/platforms/stub/spi_{1,2,4,8,16}_stub.{cpp,h}` (test stubs)
- `tests/fl/spi.cpp` (test improvements)

### No Unstaged Changes
All work has been staged.

## Commands for Next Agent

### Run Tests
```bash
# Full test suite
uv run test.py --cpp

# Just SPI test with verbose output
cd .build/meson && meson test fl_spi --verbose

# Clean rebuild
rm -rf .build/meson && uv run test.py --cpp
```

### Search Bus Manager Code
```bash
# Find promotion logic
grep -rn "Promoted clock pin" src/platforms/shared/

# Find platform limitation checks
grep -rn "platform limitation" src/platforms/shared/

# Find bus manager initialization
grep -rn "class SPIBusManager" src/
```

### Debug Output
The test already has extensive debug output from:
- `FL_LOG_SPI()` macros
- Bus manager state transitions
- All logs appear in test stderr

### Code Review
```bash
# Review recent SPI changes
git diff --cached src/fl/spi/
git diff --cached src/platforms/shared/spi_bus_manager.h

# Check git history for context
git log --oneline -10
```

## Success Criteria

1. ✅ **Build succeeds** - No ninja errors (ACHIEVED)
2. ✅ **Test suite compiles** - All 119 tests build (ACHIEVED)
3. ❌ **All tests pass** - Currently 118/119 pass, `fl_spi` fails
4. ❌ **No crashes** - Still panicking but now at REQUIRE, not at crash
5. ❌ **`codeup` returns 0** - Will succeed once tests pass

## Priority

**HIGH** - This is blocking all git operations as per project policy (no commits until `codeup` passes)

## Notes

- **No panic anymore**: The `REQUIRE` fix prevents the alignment crash
- **Error is now clear**: `begin()` returns error, test stops gracefully
- **Logs are informative**: Can trace exact failure path in bus manager
- **Problem is localized**: Only affects SPI bus manager promotion logic
- **Existing code changed recently**: This likely worked before refactoring

## Related Documentation

- `CLAUDE.md` - Project guidelines (don't commit until codeup passes)
- `src/platforms/README_SPI_ADVANCED.md` - SPI multi-lane documentation (staged)
- Bus manager should have inline documentation in the header

## Agent Handoff

Next agent should:
1. Read this entire file to understand context
2. Review bus manager promotion logic in `src/platforms/shared/spi_bus_manager.h`
3. Decide on solution approach (Options 1-4 above)
4. Implement fix
5. Test with `uv run test.py --cpp`
6. Update this file with findings
7. If tests pass, run `codeup` (attempt 4/5)
8. If codeup passes, HALT (success!)
9. If issues remain, document and pass to next agent

**Good luck! 🚀**

---

## UPDATE: Fix Applied (Attempt 4/5)

### Investigation Results (3 Parallel Agents)

Three agents investigated in parallel and all converged on the same root cause:

**Agent 1 (Bus Manager)**: Found `releaseBusHardware()` doesn't reset `num_devices`
**Agent 2 (Stub Platform)**: Confirmed stubs ARE supposed to support multi-lane SPI
**Agent 3 (Test State)**: Traced exact sequence showing `num_devices` leakage between tests

### Root Cause Confirmed

**File**: `src/platforms/shared/spi_bus_manager.h:1113-1166`

The `releaseBusHardware()` function resets:
- ✅ `is_initialized = false` (line 1164)
- ✅ `bus_type = SOFT_SPI` (line 1165)
- ❌ **Missing**: `num_devices` reset

**Result**: Stale `num_devices` value persists across tests, causing incorrect promotion.

### Fix Applied

**File**: `src/platforms/shared/spi_bus_manager.h`
**Line**: 1166 (added)

```cpp
// Reset bus state
bus.is_initialized = false;
bus.bus_type = SPIBusType::SOFT_SPI;
bus.num_devices = 0;  // Reset device count to prevent stale state
```

**Rationale**:
- Minimal change (1 line)
- Fixes root cause directly
- No logic changes, just proper cleanup
- Works across all platforms
- No test changes needed

### Testing Status

**Result**: Initial fix caused a new issue! `num_devices = 0` buses were being initialized by the global `initialize()` loop.

### Second Fix Required

**Problem**: `initialize()` loops through ALL buses and tries to initialize any with `is_initialized = false`, including buses with `num_devices = 0`.

**Additional Fix Applied**:

**File**: `src/platforms/shared/spi_bus_manager.h`
**Line**: 725-728 (added)

```cpp
bool initializeBus(SPIBusInfo& bus) {
    // No devices? Skip initialization (bus was released)
    if (bus.num_devices == 0) {
        return true;  // Not an error, just nothing to initialize
    }

    // Single device? Use standard single-line SPI
    if (bus.num_devices == 1) {
```

**Rationale**:
- When `releaseBusHardware()` resets `num_devices = 0`, the bus remains in the array
- Global `initialize()` tries to initialize ALL buses with `is_initialized = false`
- Need to skip buses with no devices (they were released and are waiting for reuse)

### Final Status

✅ **All tests pass!** (exit code 0)
✅ **119/119 tests successful**

Running final `codeup` validation (attempt 4/5)...
