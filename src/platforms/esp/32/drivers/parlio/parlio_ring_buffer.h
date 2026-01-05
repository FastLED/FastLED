/// @file parlio_ring_buffer.h
/// @brief Ring buffer management and DMA buffer population for PARLIO
///
/// This header documents the ring buffer streaming architecture for PARLIO DMA.
/// Ring buffers enable continuous LED transmission without blocking by pre-populating
/// multiple DMA buffers that the ISR submits to hardware as each completes.
///
/// ## Ring Buffer Architecture (3-Buffer Design)
///
/// ### Buffer Lifecycle States
/// ```
/// State 1: TRANSMITTING  - Hardware is actively sending this buffer via DMA
/// State 2: READY         - Buffer populated and queued, waiting for hardware
/// State 3: POPULATING    - CPU is filling this buffer with waveform data
/// ```
///
/// ### Ring Buffer Coordination
/// The ring buffer uses three indices to track state:
/// - `mRingReadIdx`: Next buffer for hardware to transmit (ISR consumes)
/// - `mRingWriteIdx`: Next buffer for CPU to populate (worker populates)
/// - `mRingCount`: Number of buffers ready/transmitting (0-3)
///
/// **Invariant**: `mRingCount` distinguishes full (3) from empty (0) states
///
/// ### ISR Coordination Pattern
/// 1. **CPU (populateNextDMABuffer)**:
///    - Checks `mRingCount < RING_BUFFER_COUNT` (has space?)
///    - Populates buffer at `mRingWriteIdx`
///    - Increments `mRingWriteIdx` (modulo 3)
///    - Increments `mRingCount`
///
/// 2. **ISR (txDoneCallback)**:
///    - Hardware completes buffer at `mRingReadIdx - 1`
///    - Submits next buffer at `mRingReadIdx`
///    - Increments `mRingReadIdx` (modulo 3)
///    - Decrements `mRingCount`
///    - Arms worker timer if `mRingCount < 3` and more data pending
///
/// 3. **Worker Timer ISR (workerIsrCallback)**:
///    - Fires 10µs after txDoneCallback arms it
///    - Populates ONE buffer (same logic as CPU path)
///    - Stops timer (one-shot behavior)
///    - Timer re-armed by next txDoneCallback
///
/// ### Streaming Mode
/// When LED data exceeds ring buffer capacity, the system uses **streaming mode**:
/// - Ring buffers hold partial LED data (not full frame)
/// - CPU/worker ISR refills buffers as hardware consumes them
/// - Multiple iterations through ring required to complete frame
/// - Enables arbitrarily large LED strips without OOM
///
/// Example (3000 LEDs with 256KB cap):
/// - Ring buffer capacity: 85KB per buffer (capped)
/// - LEDs per buffer: ~1040 LEDs
/// - Iterations: 3 buffers × 1 pass = 3120 LEDs (full frame in one pass)
/// - If cap reduced: Multiple passes required (streaming)
///
/// ## Memory Management
/// - Total cap: `FASTLED_PARLIO_MAX_RING_BUFFER_TOTAL_BYTES` (256KB default)
/// - Per-buffer cap: Total / 3 buffers
/// - Cache alignment: 64-byte boundaries (ESP32-C6 cache line size)
/// - Allocation: `heap_caps_aligned_alloc` with `FL_PARLIO_DMA_MALLOC_FLAGS`
///
/// ## Performance Notes
/// - **HOT PATH**: `populateDmaBuffer()` and `populateNextDMABuffer()` are FL_IRAM
/// - **NO LOGGING** in hot paths (causes 98× slowdown)
/// - **LED Boundary Alignment**: All buffers aligned to 3-byte (RGB) boundaries
/// - **Reset Padding**: Appended only to final buffer in stream
///
/// ## Implementation Location
/// The ring buffer functions are implemented in `parlio_engine.cpp`:
/// - `hasRingSpace()` - Check if ring has capacity for more buffers
/// - `allocateRingBuffers()` - One-time allocation with cache alignment
/// - `populateDmaBuffer()` - **FL_IRAM** - Generate waveform data for byte range
/// - `populateNextDMABuffer()` - **FL_IRAM** - Populate next ring buffer slot
///
/// These functions remain in ParlioEngine class because they access private members
/// (mRingBuffers, mWaveformExpansionBuffer, mScratchBuffer, etc.)

#pragma once

#include "fl/compiler_control.h"
#include "fl/stl/function.h"

// Forward declarations
extern "C" void heap_caps_free(void *ptr);

namespace fl {
namespace detail {

//=============================================================================
// Custom Deleter for heap_caps_malloc'd Memory
//=============================================================================

struct HeapCapsDeleter {
    void operator()(uint8_t *ptr) const {
        if (ptr) {
            heap_caps_free(ptr);
        }
    }
};

//=============================================================================
// Ring Buffer Structure
//=============================================================================

/// @brief Ring buffer for PARLIO DMA streaming (fixed 3-buffer design)
///
/// Manages exactly 3 DMA buffers for continuous LED transmission without blocking.
/// Buffers cycle through states: POPULATING → READY → TRANSMITTING → (repeat)
/// Uses POD C arrays for optimal ISR performance - no heap allocations, no overhead.
///
/// Design: Buffers are owned externally and passed in via constructor.
/// Destructor callback handles cleanup when ring buffer is destroyed.
struct ParlioRingBuffer3 {
    static constexpr size_t RING_BUFFER_COUNT = 3;

    // Ring buffer storage (POD C arrays - no dynamic allocation)
    uint8_t* ptrs[RING_BUFFER_COUNT];       // Buffer pointers (not owned)
    size_t sizes[RING_BUFFER_COUNT];        // Actual DMA buffer size in each buffer (includes reset padding)
    size_t input_sizes[RING_BUFFER_COUNT];  // Input byte count (source data, excludes reset padding)
    size_t capacity;                        // Capacity of each buffer (bytes)

    // Destructor callback for buffer cleanup (set by owner)
    // Called once per buffer (3 times total)
    fl::function<void(uint8_t*)> on_destroy;

    /// @brief Initialize ring buffer with external buffers
    /// @param buffer0 Pointer to first buffer
    /// @param buffer1 Pointer to second buffer
    /// @param buffer2 Pointer to third buffer
    /// @param buffer_capacity Capacity of each buffer (all must be same size)
    /// @param destroy_callback Function to call on destruction to free each buffer
    ParlioRingBuffer3(uint8_t* buffer0, uint8_t* buffer1, uint8_t* buffer2,
                      size_t buffer_capacity,
                      fl::function<void(uint8_t*)> destroy_callback)
        : ptrs{buffer0, buffer1, buffer2},
          sizes{0, 0, 0},
          input_sizes{0, 0, 0},
          capacity(buffer_capacity),
          on_destroy(fl::move(destroy_callback)) {
    }

    /// @brief Destructor - calls cleanup callback for each buffer
    ~ParlioRingBuffer3() {
        if (on_destroy) {
            for (size_t i = 0; i < RING_BUFFER_COUNT; i++) {
                if (ptrs[i]) {
                    on_destroy(ptrs[i]);
                }
            }
        }
    }

    // Disable copy (pointers are not owned, copying would be unsafe)
    ParlioRingBuffer3(const ParlioRingBuffer3&) = delete;
    ParlioRingBuffer3& operator=(const ParlioRingBuffer3&) = delete;

    // Disable move (destructor callback makes moving unsafe)
    ParlioRingBuffer3(ParlioRingBuffer3&&) = delete;
    ParlioRingBuffer3& operator=(ParlioRingBuffer3&&) = delete;
};

} // namespace detail
} // namespace fl
