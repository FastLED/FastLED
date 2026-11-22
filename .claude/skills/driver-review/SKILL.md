---
name: driver-review
description: Review and implement hardware driver code — DMA safety, interrupt correctness, timing constraints, peripheral register usage, channel drivers, and peripheral mock implementations. Use when writing, modifying, or reviewing LED drivers, SPI/I2S/RMT/UART/PARLIO/LCD_CAM peripherals, GPIO configuration, or peripheral mock code.
disable-model-invocation: true
---

# Hardware Driver Review & Implementation Guide

Review and implement hardware driver code changes for embedded-specific safety and correctness issues.

## Your Task

1. Run `git diff --cached` and `git diff` to see all changes
2. Identify files that are hardware driver code (see "What Counts as Driver Code" below)
3. For **reviews**: Check ALL driver code changes against the Review Rules below
4. For **implementations**: Follow the Implementation Guide below
5. Fix straightforward violations directly
6. Report summary of all findings

## What Counts as Driver Code

Files matching these patterns:
- `src/platforms/**` — Platform-specific implementations
- `src/fl/channels/**` — LED channel engine and DMA pipeline
- `**/drivers/**` — Hardware driver implementations
- Files containing: DMA buffers, SPI/I2S/RMT/UART/PARLIO peripheral access, GPIO configuration, interrupt handlers, timer configuration

---

# Part 1: Review Rules

### 1. DMA Safety
- [ ] DMA buffers allocated with `MALLOC_CAP_INTERNAL | MALLOC_CAP_DMA`
- [ ] DMA buffers are 4-byte aligned (use `__attribute__((aligned(4)))` or aligned allocator)
- [ ] No stack-allocated DMA buffers (must be heap or static)
- [ ] DMA descriptors in internal SRAM (not PSRAM/SPIRAM)
- [ ] Cache coherence handled: `esp_cache_msync()` or non-cacheable memory for DMA
- [ ] DMA transfer size within hardware limits
- [ ] Buffer lifetime extends beyond DMA completion (no use-after-free)

### 2. Interrupt Safety
- [ ] ISR functions marked with `IRAM_ATTR` (ESP32) or proper section attributes
- [ ] No heap allocation (`malloc`, `new`, `fl::vector`) inside ISRs
- [ ] No mutex/semaphore take with blocking timeout in ISRs (use `portMAX_DELAY` = 0 only)
- [ ] No `printf`, `FL_DBG`, `FL_WARN`, or logging in ISRs
- [ ] No flash access from ISRs (all ISR code and data in IRAM/DRAM)
- [ ] ISR-safe queue operations only (`xQueueSendFromISR`, not `xQueueSend`)
- [ ] Critical sections use proper primitives (`portENTER_CRITICAL_ISR` not `portENTER_CRITICAL`)
- [ ] ISR handlers return correct value (`true` if higher-priority task woken)

### 3. Peripheral Register Access
- [ ] Registers accessed through volatile pointers or HAL functions
- [ ] No read-modify-write races on shared registers (use atomic or critical section)
- [ ] Peripheral clock enabled before register access
- [ ] Peripheral properly initialized before use and cleaned up on teardown
- [ ] GPIO matrix/IOMUX configured correctly for peripheral signals

### 4. Timing Constraints
- [ ] SPI/I2S/RMT clock calculations match LED protocol requirements
- [ ] Reset timing meets protocol minimums (WS2812: >280us, SK6812: >80us)
- [ ] No blocking waits in time-critical paths
- [ ] Watchdog fed in long-running operations
- [ ] `vTaskDelay(1)` or `yield()` in busy loops to prevent watchdog reset

### 5. Memory Safety
- [ ] Buffer sizes checked before DMA transfer setup
- [ ] No buffer overflows in encoding functions (bounds checking on output buffer)
- [ ] Encoding output size calculated correctly (e.g., wave8: 8 SPI bits per LED bit)
- [ ] Chunk sizes aligned to hardware requirements (SPI: 4-byte aligned)

### 6. Channel Engine Patterns (FastLED-specific)
- [ ] `show()` waits for `poll() == READY` before starting new frame
- [ ] No branching on intermediate states (DRAINING, STREAMING) in wait loops
- [ ] Channel released after transmission complete (frees peripheral for next channel)
- [ ] State machine handles all transitions (no stuck states)
- [ ] Error recovery path exists (timeout, reset to IDLE)

### 7. Peripheral Mock Rules (CRITICAL)
- [ ] **NO background threads** — mock must be fully synchronous
- [ ] **NO wall-clock timing** (`fl::micros()`, `sleep_for`) — use `mSimulatedTimeUs`
- [ ] **NO mutex/condition_variable** — single-threaded, no synchronization needed
- [ ] Synchronous callback pump via `pumpDeferredCallbacks()` with re-entrancy guard
- [ ] `waitDone()` returns instantly — never polls or sleeps
- [ ] `reset()` clears ALL state — called between test cases for isolation
- [ ] Transmitted data captured in history vector for test inspection
- [ ] Singleton via `fl::Singleton<Impl>`

### 8. Power and Reset
- [ ] Brown-out detection configured if needed
- [ ] Peripheral reset on initialization (clean state)
- [ ] GPIO pins set to safe state on driver teardown
- [ ] Power domains managed correctly (light sleep compatibility)

### 9. Multi-Platform Considerations
- [ ] Platform guards (`#ifdef ESP32`, `#ifdef FL_IS_ARM`) correct and complete
- [ ] No platform-specific types leaking into shared headers
- [ ] Fallback/no-op implementations for unsupported platforms
- [ ] Integer types match platform expectations (see `src/platforms/*/int.h`)

---

# Part 2: Implementation Guide

## Architecture

FastLED's driver stack has three layers:

```
IChannelDriver              (driver.h — show/poll state machine)
  └─ ChannelEngine*         (groups channels by timing, iterates chipset groups)
      └─ IPeripheral        (virtual interface — real HW or mock)
          ├─ PeripheralEsp  (real ESP-IDF calls)
          └─ PeripheralMock (synchronous test simulation)
```

## File Structure for New Peripheral `foo`

```
src/platforms/esp/32/drivers/foo/
  ├─ ifoo_peripheral.h              # Virtual interface (no ESP-IDF types)
  ├─ foo_peripheral_esp.h           # Real hardware implementation
  ├─ foo_peripheral_mock.h          # Mock class declaration
  ├─ foo_peripheral_mock.cpp.hpp    # Mock implementation (synchronous)
  └─ channel_driver_foo.cpp.hpp     # Channel driver using IFooPeripheral

tests/unit/platforms/esp/32/drivers/foo/
  ├─ foo_peripheral_mock.cpp        # Mock peripheral unit tests
  └─ channel_driver_foo.cpp         # Driver integration tests
```

**Reference implementations:**
- I2S: `src/platforms/esp/32/drivers/i2s/`
- LCD_CAM: `src/platforms/esp/32/drivers/lcd_cam/`
- PARLIO: `src/platforms/esp/32/drivers/parlio/`

## Peripheral Interface

Define the virtual interface in `ifoo_peripheral.h`:

- **No ESP-IDF types** — use `void*`, `u16*`, basic types only
- All methods `FL_NOEXCEPT override`
- Buffer management with 64-byte alignment (DMA requirement)
- Time simulation: `getMicroseconds()`, `delay(ms)`
- Callback registration: `registerCallback(void* fn, void* ctx)`

## Peripheral Mock Implementation

### Required Members

```cpp
// Lifecycle
bool mInitialized, mEnabled, mBusy;
size_t mTransmitCount;
FooConfig mConfig;

// ISR callback
void* mCallback;
void* mUserCtx;

// Simulation settings
u32 mTransmitDelayUs;
bool mTransmitDelayForced;
bool mShouldFailTransmit;

// Test inspection
fl::vector<TransmitRecord> mHistory;

// Pending state
size_t mPendingTransmits;

// Simulated time (deterministic — advances only via delay() calls)
u64 mSimulatedTimeUs;

// Synchronous callback pump
bool mFiringCallbacks;            // Re-entrancy guard
size_t mDeferredCallbackCount;    // Pending callbacks to fire
```

### Core Pattern: transmit() → pump → fireCallback()

**transmit()** — Queue + pump:
```cpp
bool transmit(const u16* buffer, size_t size_bytes) {
    if (!mInitialized || mShouldFailTransmit) return false;

    // Capture data for test inspection
    TransmitRecord record;
    record.buffer_copy.resize(size_bytes / 2);
    fl::memcpy(record.buffer_copy.data(), buffer, size_bytes);
    record.size_bytes = size_bytes;
    record.timestamp_us = mSimulatedTimeUs;
    mHistory.push_back(fl::move(record));

    // Queue + fire synchronously
    mTransmitCount++;
    mBusy = true;
    mPendingTransmits++;
    mDeferredCallbackCount++;
    pumpDeferredCallbacks();
    return true;
}
```

**waitDone()** — Instant check, never polls:
```cpp
bool waitDone(u32 timeout_ms) {
    if (!mInitialized) return false;
    (void)timeout_ms;  // Not used — synchronous mock
    if (mPendingTransmits == 0) { mBusy = false; return true; }
    return false;
}
```

**pumpDeferredCallbacks()** — Re-entrant safe:
```cpp
void pumpDeferredCallbacks() {
    if (mFiringCallbacks) return;  // Re-entrancy guard
    mFiringCallbacks = true;
    while (mDeferredCallbackCount > 0) {
        mDeferredCallbackCount--;
        fireCallback();
    }
    mFiringCallbacks = false;
}
```

**fireCallback()** — One callback at a time:
```cpp
void fireCallback() {
    if (mPendingTransmits > 0) mPendingTransmits--;
    if (mPendingTransmits == 0) mBusy = false;
    if (mCallback != nullptr) {
        using CallbackType = bool (*)(void*, const void*, void*);
        auto fn = reinterpret_cast<CallbackType>(mCallback);
        fn(nullptr, nullptr, mUserCtx);
    }
}
```

**Time simulation:**
```cpp
u64 getMicroseconds() { return mSimulatedTimeUs; }
void delay(u32 ms) { mSimulatedTimeUs += static_cast<u64>(ms) * 1000; }
```

**reset()** — Full state reset:
```cpp
void reset() {
    mInitialized = mEnabled = mBusy = false;
    mTransmitCount = 0;
    mConfig = FooConfig();
    mCallback = nullptr; mUserCtx = nullptr;
    mTransmitDelayUs = 0; mTransmitDelayForced = false; mShouldFailTransmit = false;
    mHistory.clear(); mPendingTransmits = 0;
    mSimulatedTimeUs = 0; mFiringCallbacks = false; mDeferredCallbackCount = 0;
}
```

### Mock-Specific Test API (required)

```cpp
void simulateTransmitComplete();           // Manually complete one pending transmit
void setTransmitFailure(bool should_fail); // Force transmit() to return false
void setTransmitDelay(u32 microseconds);   // Set forced delay
const fl::vector<TransmitRecord>& getTransmitHistory() const;
fl::span<const u16> getLastTransmitData() const;
size_t getTransmitCount() const;
bool isEnabled() const;
void clearTransmitHistory();
void reset();
```

## Channel Driver

Implement `IChannelDriver`. State machine:
```
READY → (enqueue) → READY → (show) → BUSY → (poll) → DRAINING → (poll) → READY
```

**Constructor pattern:**
```cpp
ChannelDriverFoo();                                          // Production
ChannelDriverFoo(fl::shared_ptr<IFooPeripheral> peripheral); // Testing
```

**Chipset grouping:** Channels with different timing (T0H, T1H, T0L, T1L) must be transmitted in separate groups. Sort by transmission time.

## Tests

```cpp
namespace {
void resetFooMockState() {
    auto& mock = FooPeripheralMock::instance();
    mock.reset();  // CRITICAL: reset between every test
}
}

FL_TEST_CASE("FooPeripheralMock - basic transmit") {
    resetFooMockState();
    auto& mock = FooPeripheralMock::instance();
    FooConfig config;
    config.num_lanes = 4;
    config.pclk_hz = 3200000;
    FL_REQUIRE(mock.initialize(config));
    u16* buffer = mock.allocateBuffer(1024);
    FL_REQUIRE(buffer != nullptr);
    FL_CHECK(mock.transmit(buffer, 1024));
    FL_CHECK(mock.waitDone(100));
    FL_CHECK(mock.getTransmitCount() == 1);
    mock.freeBuffer(buffer);
}
```

## Driver Registration Priority

In `channel_manager_esp32.cpp.hpp`:
- PARLIO: 4 (highest)
- LCD_RGB: 3
- RMT: 2
- I2S: 1
- SPI: 0
- UART: -1

## Buffer Alignment (64-byte for DMA)

```cpp
u16* allocateBuffer(size_t size_bytes) {
    size_t aligned = ((size_bytes + 63) / 64) * 64;
#ifdef FL_IS_WIN
    return static_cast<u16*>(_aligned_malloc(aligned, 64));
#else
    return static_cast<u16*>(aligned_alloc(64, aligned));
#endif
}
void freeBuffer(u16* buffer) {
    if (!buffer) return;
#ifdef FL_IS_WIN
    _aligned_free(buffer);
#else
    fl::free(buffer);
#endif
}
```

---

## Output Format (for reviews)

```
## Hardware Driver Review Results

### File-by-file Analysis
- **src/platforms/esp/32/drivers/spi/channel_engine_spi.cpp.hpp**: [findings]

### Findings by Category
- **DMA Safety**: N issues
- **Interrupt Safety**: N issues
- **Mock Rules**: N issues
- **Timing Constraints**: N issues

### Summary
- Files reviewed: N
- Violations found: N
- Violations fixed: N
```

## Instructions
- Focus on driver/platform code — skip application-level changes
- Be thorough on DMA and interrupt safety (these cause hard-to-debug crashes)
- **Mock violations are P0** — async mocks with threads/wall-clock = flaky tests
- Reference `agents/docs/cpp-standards.md` for general C++ rules
- Make corrections directly when safe
- Ask for user confirmation on significant changes
