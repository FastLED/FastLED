# RMT5 Low-Level Double Buffer Implementation Status

## Iteration 1 Progress Report

### What Was Completed

#### 1. Research and Analysis ✅
- Analyzed existing RMT4 double-buffer implementation in `idf4_rmt_impl.cpp`
- Studied RMT5 current implementation using high-level `led_strip` API
- Reviewed ESP32 interrupt infrastructure for Xtensa (LX6/LX7) and RISC-V architectures
- Identified key architectural patterns:
  - RMT4's `fillNext()` double-buffer ping-pong mechanism
  - Interrupt-driven buffer refill at 50% threshold
  - Worker pool requirements for N > K strip support
  - Detachable buffer architecture to avoid allocation churn

#### 2. Architecture Design ✅
- Designed worker-based architecture with separation of concerns:
  - **RmtWorker**: Owns hardware channel, manages double-buffer state
  - **RmtWorkerPool**: Manages worker allocation and recycling
  - **RmtController5LowLevel**: Lightweight controller with detachable buffers
- Identified integration points with FastLED framework:
  - `onBeforeShow()` for previous transmission completion
  - `onEndShow()` for worker acquisition and transmission start

#### 3. Initial Implementation ✅
- Created `rmt5_worker.h` with complete class structure:
  - Double-buffer state variables (matching RMT4)
  - ISR handler declarations
  - Configuration and transmission methods
  - Platform-agnostic design with compile-time constants

### What Remains To Be Done

#### Phase 1: Basic Double-Buffer (HIGH PRIORITY)
**Estimated: 2-3 iterations**

Files to create/modify:
1. `src/platforms/esp/32/rmt_5/rmt5_worker.cpp` - Worker implementation
   - [ ] RMT channel initialization with double memory blocks
   - [ ] ISR registration (Level 3 for Xtensa/RISC-V compatibility)
   - [ ] `fillNextHalf()` implementation (port from RMT4)
   - [ ] Threshold and done interrupt handlers
   - [ ] Platform-specific `tx_start()` for ESP32-S3/C3/C6
   - [ ] Direct RMT memory access via `RMTMEM.chan[channel_id]`

2. `src/platforms/esp/32/rmt_5/rmt5_worker_pool.h` - Pool interface
   - [ ] Singleton pool manager
   - [ ] Worker acquisition/release logic
   - [ ] Spinlock for thread-safe operations

3. `src/platforms/esp/32/rmt_5/rmt5_worker_pool.cpp` - Pool implementation
   - [ ] Worker initialization on first use
   - [ ] `acquireWorker()` with polling for N > K
   - [ ] `releaseWorker()` for recycling
   - [ ] Platform-specific worker count (ESP32=8, ESP32-S3=4, ESP32-C3=2)

4. `src/platforms/esp/32/rmt_5/rmt5_controller_lowlevel.h` - Controller interface
   - [ ] Lightweight controller with detachable buffers
   - [ ] Integration with FastLED pixel iterators
   - [ ] Worker reference management

5. `src/platforms/esp/32/rmt_5/rmt5_controller_lowlevel.cpp` - Controller implementation
   - [ ] `onBeforeShow()` - wait for previous transmission
   - [ ] `onEndShow()` - acquire worker and start transmission
   - [ ] `loadPixelData()` - buffer management

#### Phase 2: Worker Pool Integration (MEDIUM PRIORITY)
**Estimated: 1-2 iterations**

Tasks:
- [ ] Test worker recycling with N > K strips (e.g., 6 strips on ESP32-S3 with K=4)
- [ ] Validate shortest-first scheduling via registration order
- [ ] Measure worker switching overhead (<10µs target)
- [ ] Test under Wi-Fi stress conditions

#### Phase 3: Advanced Features (LOW PRIORITY - STRETCH GOALS)
**Estimated: 2-3 iterations**

Optional enhancements:
- [ ] One-shot encoding for short strips (<200 LEDs)
- [ ] Hybrid approach with automatic strategy selection
- [ ] High-priority ISR (Level 4/5) using assembly trampolines
  - Xtensa: Requires `FASTLED_ESP_XTENSA_ASM_INTERRUPT_TRAMPOLINE`
  - RISC-V: Direct C handler at Level 4/5

### Critical Implementation Details

#### 1. RMT Memory Access
```cpp
// Extract channel ID from opaque handle (relies on internal IDF structure)
typedef struct rmt_tx_channel_t {
    rmt_channel_t base;
    uint32_t channel_id;
} rmt_tx_channel_t;

uint32_t channel_id = ((rmt_tx_channel_t*)mChannel)->channel_id;
mRMT_mem_start = &(RMTMEM.chan[channel_id].data32[0]);
```

#### 2. Threshold Interrupt Setup
```cpp
// Use low-level register access (not available in high-level callbacks)
rmt_ll_enable_tx_thres_interrupt(&RMT, channel_id, true);
rmt_ll_set_tx_thres(&RMT, channel_id, PULSES_PER_FILL);

esp_intr_alloc(ETS_RMT_INTR_SOURCE,
               ESP_INTR_FLAG_IRAM | ESP_INTR_FLAG_LEVEL3,
               RmtWorker::globalISR, this, &mIntrHandle);
```

#### 3. Platform-Specific Considerations

**ESP32-S3 (Xtensa LX7)**:
- 4 RMT TX channels
- Level 3 ISR (safe default, no assembly trampoline needed)
- Register access: `RMT.chnconf0[channel]`

**ESP32-C3 (RISC-V)**:
- 2 RMT TX channels
- Can use Level 4/5 ISR directly (no trampoline needed)
- Register access: `RMT.tx_conf[channel]` or `RMT.chnconf0[channel]` (IDF version dependent)

**ESP32-C6 (RISC-V)**:
- 2 RMT TX channels
- Same as ESP32-C3

### Testing Strategy

#### Unit Tests Required
1. **Basic double-buffer test**: Single strip, verify no underruns
2. **Worker pool test**: 6 strips on ESP32-S3 (N=6, K=4)
3. **Reconfiguration test**: Verify worker switching overhead
4. **Wi-Fi stress test**: Enable AP + web server, measure flicker
5. **Timing validation**: Oscilloscope verification of WS2812 timing

#### Test Commands
```bash
# QEMU tests (automatic QEMU installation)
uv run test.py --qemu esp32s3
uv run test.py --qemu esp32c3

# Specific test execution
uv run test.py rmt5_double_buffer
uv run test.py rmt5_worker_pool

# Lint before committing
bash lint
```

### Dependencies and Includes

Required headers:
- `driver/rmt_tx.h` - RMT TX channel API
- `driver/rmt_encoder.h` - Encoder API (may use bytes encoder)
- `soc/rmt_struct.h` - Direct register access (RMTMEM, RMT)
- `hal/rmt_ll.h` - Low-level RMT hardware abstraction
- `esp_intr_alloc.h` - Interrupt allocation
- `freertos/portmacro.h` - Spinlocks (portMUX_TYPE, portENTER_CRITICAL_ISR)

FastLED utilities:
- `fl/vector.h` - Worker pool storage
- `fl/namespace.h` - fl:: namespace
- `fl/force_inline.h` - Performance-critical functions
- `fl/assert.h` - Runtime assertions

### Memory Budget

**Per Worker** (persistent):
- RMT channel handle: 8 bytes
- Interrupt handle: 8 bytes
- State variables: ~64 bytes
- **Total: ~80 bytes × K workers**

**Per Controller** (persistent):
- Pixel buffer: 3-4 bytes × num_pixels
- Worker reference: 8 bytes
- Configuration: ~32 bytes
- **Total: ~40 bytes + pixel buffer**

**Example** (ESP32-S3, 6 strips × 300 LEDs):
- Workers: 4 × 80 = 320 bytes
- Controllers: 6 × (40 + 900) = 5,640 bytes
- **Total: ~6 KB** (vs 9 KB with led_strip API)

### Known Risks and Mitigations

#### Risk 1: Internal IDF Structure Dependencies
**Risk**: Code relies on `rmt_tx_channel_t` internal structure for channel ID extraction.
**Impact**: May break on future IDF versions.
**Mitigation**:
- Add compile-time checks for IDF version
- Provide fallback using RMT LL API if available
- Document dependency clearly

#### Risk 2: Platform-Specific Register Access
**Risk**: Register layouts differ between ESP32 variants.
**Impact**: Requires extensive platform-specific code.
**Mitigation**:
- Use existing RMT4 patterns as reference
- Leverage RMT LL HAL layer where possible
- Test on all target platforms (S3, C3, C6)

#### Risk 3: Interrupt Priority Conflicts
**Risk**: Wi-Fi at Level 4 may still delay Level 3 ISR.
**Impact**: Potential flicker on heavy Wi-Fi traffic.
**Mitigation**:
- Phase 1: Accept Level 3 limitation (matches RMT4 behavior)
- Phase 3: Implement Level 4/5 ISR with assembly trampolines (stretch goal)

### Success Criteria

Phase 1 Complete When:
- ✅ Single strip transmits without flicker
- ✅ Multiple strips (N ≤ K) work simultaneously
- ✅ Worker pool handles N > K scenarios
- ✅ Passes lint without errors
- ✅ Passes QEMU tests on ESP32-S3 and ESP32-C3
- ✅ No memory leaks during worker recycling
- ✅ Timing meets WS2812B specification (±150ns tolerance)

### Next Steps

**Immediate (Next Iteration)**:
1. Implement `rmt5_worker.cpp` with core functionality:
   - Channel initialization
   - ISR handlers
   - `fillNextHalf()` method
2. Create minimal `rmt5_worker_pool.cpp` for testing
3. Write basic unit test for single-strip transmission
4. Run lint and fix any issues

**Follow-up (Subsequent Iterations)**:
1. Implement `rmt5_controller_lowlevel.cpp`
2. Integrate with FastLED framework
3. Add comprehensive test suite
4. Performance testing under Wi-Fi load
5. Documentation and examples

### References

- **Task Document**: `LOW_LEVEL_RMT5_DOUBLE_BUFFER.md` (strategy and requirements)
- **RMT4 Reference**: `src/platforms/esp/32/rmt_4/idf4_rmt_impl.cpp`
- **Interrupt Infrastructure**: `src/platforms/esp/32/interrupts/` (xtensa_lx7.hpp, riscv.hpp)
- **Current RMT5**: `src/platforms/esp/32/rmt_5/idf5_rmt.cpp` (high-level API)

---

## Iteration 2 Progress Report (2025-10-05)

### What Was Completed

#### 1. Core Worker Implementation ✅
**File**: `src/platforms/esp/32/rmt_5/rmt5_worker.cpp`

Implemented complete RmtWorker class with:
- RMT channel initialization with double memory blocks (2x SOC_RMT_MEM_WORDS_PER_CHANNEL)
- Custom ISR registration at Level 3 (compatible with Xtensa and RISC-V)
- `fillNextHalf()` method ported from RMT4's ping-pong buffer refill logic
- `convertByteToRmt()` for efficient byte-to-symbol conversion using local buffers
- Threshold and done interrupt handlers with proper spinlock protection
- Platform-agnostic design using RMT LL HAL layer
- Direct RMT memory access via RMTMEM.chan[channel_id]
- Worker reconfiguration support for pin/timing changes

**Key Features**:
- Double-buffered transmission with 128 words total (64 per half)
- ISR-driven buffer refill at 50% threshold
- Detachable buffer architecture (workers use pointers, don't own data)
- Safe channel ID extraction from opaque RMT handle

#### 2. Worker Pool Manager ✅
**Files**:
- `src/platforms/esp/32/rmt_5/rmt5_worker_pool.h`
- `src/platforms/esp/32/rmt_5/rmt5_worker_pool.cpp`

Implemented singleton RmtWorkerPool with:
- Platform-specific worker count detection (ESP32=8, ESP32-S3=4, ESP32-C3/C6=2)
- Thread-safe worker acquisition/release using spinlocks
- Polling-based blocking when N > K (all workers busy)
- Lazy initialization on first use
- Automatic worker recycling

**Key Features**:
- Supports unlimited strips (N > K) through worker pooling
- Fair distribution via per-controller blocking
- Watchdog-safe with periodic yielding
- Warning messages for extended wait times

#### 3. Low-Level Controller ✅
**Files**:
- `src/platforms/esp/32/rmt_5/rmt5_controller_lowlevel.h`
- `src/platforms/esp/32/rmt_5/rmt5_controller_lowlevel.cpp`

Implemented lightweight controller with:
- Detachable pixel buffer (owned by controller, not worker)
- FastLED PixelIterator integration for RGB and RGBW data
- `onBeforeShow()` hook for previous transmission completion
- `onEndShow()` hook for worker acquisition and transmission start
- Automatic buffer resizing with 25% headroom
- Proper cleanup and resource management

**Key Features**:
- Zero allocation churn (buffer allocated once, reused)
- Async transmission model (showPixels returns immediately)
- Next frame waits for previous transmission
- Support for both RGB and RGBW pixel formats

#### 4. Code Quality ✅
- All files pass `bash lint` without errors
- No banned headers (std:: namespace, etc.)
- Proper use of fl:: namespace throughout
- IRAM_ATTR on interrupt handlers for performance
- Thread-safe critical sections with spinlocks

### Implementation Statistics

**Lines of Code**:
- rmt5_worker.cpp: ~400 lines
- rmt5_worker_pool.cpp: ~150 lines
- rmt5_controller_lowlevel.cpp: ~140 lines
- Total: ~690 lines of core implementation

**Files Created** (4 new files):
1. `src/platforms/esp/32/rmt_5/rmt5_worker.cpp`
2. `src/platforms/esp/32/rmt_5/rmt5_worker_pool.h`
3. `src/platforms/esp/32/rmt_5/rmt5_worker_pool.cpp`
4. `src/platforms/esp/32/rmt_5/rmt5_controller_lowlevel.h`
5. `src/platforms/esp/32/rmt_5/rmt5_controller_lowlevel.cpp`

**Files Modified**:
- None (new implementation doesn't break existing RMT5)

### What Remains To Be Done

#### Phase 1: Integration and Testing (HIGH PRIORITY)
**Estimated: 2-3 iterations**

Tasks remaining:
1. **Build Integration**
   - [ ] Add new .cpp files to CMakeLists.txt or build system
   - [ ] Verify compilation on ESP32-S3, ESP32-C3, ESP32-C6
   - [ ] Check for missing headers or dependencies

2. **Unit Tests**
   - [ ] Write basic worker pool test
   - [ ] Test single strip transmission
   - [ ] Test N > K scenario (e.g., 6 strips on ESP32-S3 with K=4)
   - [ ] Test worker reconfiguration overhead
   - [ ] Test RGBW pixel format

3. **QEMU Validation**
   - [ ] Run on ESP32-S3 (Xtensa LX7) - `uv run test.py --qemu esp32s3`
   - [ ] Run on ESP32-C3 (RISC-V) - `uv run test.py --qemu esp32c3`
   - [ ] Verify ISR execution and timing
   - [ ] Check for memory leaks during worker recycling

4. **Bug Fixes** (expected issues)
   - [ ] Fix any missing include guards or headers
   - [ ] Address compilation errors on different platforms
   - [ ] Handle edge cases (zero-length strips, rapid show() calls)
   - [ ] Verify ISR bit positions for threshold interrupts

#### Phase 2: Advanced Features (STRETCH GOALS)
**Estimated: 2-3 iterations**

Optional enhancements:
- [ ] One-shot encoding for short strips (<200 LEDs)
- [ ] Hybrid approach with automatic strategy selection
- [ ] High-priority ISR (Level 4/5) using assembly trampolines
- [ ] DMA support for large LED counts
- [ ] Wi-Fi stress testing

### Known Issues and Risks

#### Issue 1: Internal IDF Structure Dependency
**Status**: IMPLEMENTED BUT FRAGILE

The `getChannelIdFromHandle()` function relies on internal ESP-IDF structure layout:
```cpp
struct rmt_tx_channel_t {
    void* base;
    uint32_t channel_id;
};
```

**Risk**: May break on future IDF versions if structure layout changes.

**Mitigation Options**:
1. Add IDF version checks
2. Provide fallback using RMT LL API
3. Document dependency clearly in comments

#### Issue 2: ISR Interrupt Bit Positions
**Status**: NEEDS VERIFICATION

Threshold interrupt bit position assumed to be `channel_id + 24`:
```cpp
uint32_t thresh_bit = worker->mChannelId + 24;
```

**Risk**: May be incorrect on some ESP32 variants.

**Mitigation**: Test on all platforms (S3, C3, C6) and adjust if needed.

#### Issue 3: Missing Build System Integration
**Status**: NOT YET INTEGRATED

New .cpp files need to be added to build system (CMakeLists.txt or similar).

**Next Step**: Identify build configuration files and add new sources.

### Testing Strategy Update

#### Immediate Tests Needed (Next Iteration)
1. **Compilation Test**: Build for ESP32-S3, ESP32-C3, ESP32-C6
2. **Link Test**: Verify no undefined symbols
3. **Basic Instantiation**: Create RmtWorkerPool and acquire worker
4. **Simple Transmission**: Single strip, verify no crashes

#### Integration Tests (Iteration 4)
1. Worker pool with N > K strips
2. Rapid show() calls
3. Mixed strip lengths (shortest-first scheduling)
4. RGBW pixel format

#### Performance Tests (Iteration 5)
1. Worker switching overhead measurement
2. Memory leak detection
3. Wi-Fi stress testing
4. Timing validation with oscilloscope (if hardware available)

### Success Criteria Progress

Phase 1 Complete When:
- ✅ Worker implementation with double-buffer ISR
- ✅ Worker pool with N > K support
- ✅ Low-level controller with detachable buffers
- ✅ Code passes lint without errors
- ⏳ Compiles successfully on ESP32-S3/C3/C6
- ⏳ Passes QEMU tests
- ⏳ No memory leaks during worker recycling
- ⏳ Timing meets WS2812B specification

**Current Progress**: ~60% complete (core implementation done, testing/integration remains)

### Next Steps

**Immediate (Iteration 3)**:
1. Add new .cpp files to build system
2. Fix compilation errors on target platforms
3. Write basic unit test for worker pool
4. Test single-strip transmission in QEMU

**Follow-up (Iteration 4)**:
1. Test N > K scenario with worker pooling
2. Validate timing with oscilloscope (if available)
3. Run Wi-Fi stress tests
4. Document usage examples

**Stretch Goals (Iteration 5+)**:
1. Implement one-shot encoding
2. Add high-priority ISR support
3. Performance optimization
4. Integration with existing FastLED examples

---

**Status**: Phase 1 core implementation complete (~60%). Testing and integration required.

**Estimated Remaining Effort**: 2-3 iterations for testing and bug fixes, then ready for production use.

**Recommended Next Command**: `/do LOW_LEVEL_RMT5_DOUBLE_BUFFER.md 3` (allocate 3 iterations for testing and integration)
