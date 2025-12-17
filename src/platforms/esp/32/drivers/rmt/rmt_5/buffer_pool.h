#pragma once

#include "platforms/esp/32/feature_flags/enabled.h"

#if FASTLED_RMT5

#include "fl/stl/allocator.h"
#include "fl/stl/span.h"
#include "fl/stl/vector.h"
#include "fl/stl/stdint.h"

namespace fl {

/// RMT Buffer Pool for efficient memory management
///
/// **Problem**: ChannelData uses PSRAM (via vector_psram), which is problematic for RMT with DMA:
/// - PSRAM access is slower than DRAM
/// - DMA requires internal SRAM (MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL)
///
/// **Solution**: Pool pre-allocated buffers in fast internal DRAM
/// - Main pool: Raw uint8_t* buffers using MALLOC_CAP_INTERNAL (fast DRAM)
/// - DMA buffer: Separate buffer using MALLOC_CAP_DMA (allocated on-demand)
/// - Uses writeWithPadding() to copy from PSRAM to pooled buffer before transmission
///
/// **Memory Strategy**:
/// - Buffers are reused across transmissions to avoid constant reallocation
/// - Smaller data can use larger buffers (returns sub-span)
/// - Buffers grow via realloc when needed (efficient for raw data)
/// - DMA buffer is separate and only allocated when DMA is enabled
///
/// **Thread Safety**: Not thread-safe. ChannelEngineRMT manages synchronization.
class RMTBufferPool {
public:
    RMTBufferPool();
    ~RMTBufferPool();

    // Disable copy/move (pool manages raw pointers)
    RMTBufferPool(const RMTBufferPool&) = delete;
    RMTBufferPool& operator=(const RMTBufferPool&) = delete;

    /// Acquire a buffer from the pool (internal DRAM, non-DMA)
    /// - Finds exact-size or larger buffer and returns sub-span
    /// - If no suitable buffer available, allocates new buffer or resizes existing
    /// - Returns empty span on allocation failure
    ///
    /// @param size Minimum size needed in bytes
    /// @return Span of acquired buffer (capacity >= size), or empty span on failure
    fl::span<uint8_t> acquireInternal(fl::size size);

    /// Acquire the DMA buffer (DMA-capable memory)
    /// - Only ONE DMA buffer exists (ESP32 hardware limitation: max 1 RMT channel with DMA)
    /// - Buffer is allocated on first use and grows as needed
    /// - Returns empty span if DMA buffer is in use or allocation fails
    ///
    /// @param size Minimum size needed in bytes
    /// @return Span of acquired DMA buffer (capacity >= size), or empty span on failure
    fl::span<uint8_t> acquireDMA(fl::size size);

    /// Release an internal buffer back to the pool
    /// - Marks buffer as available for reuse
    /// - Buffer memory is NOT freed (kept for future reuse)
    ///
    /// @param buffer The buffer to release (must be from acquireInternal)
    void releaseInternal(fl::span<uint8_t> buffer);

    /// Release the DMA buffer back to the pool
    /// - Marks DMA buffer as available for reuse
    /// - Buffer memory is NOT freed (kept for future reuse)
    void releaseDMA();

    /// Get pool statistics for debugging
    struct Stats {
        fl::size numInternalBuffers;    // Total internal buffers allocated
        fl::size totalInternalCapacity; // Total bytes allocated in internal pool
        fl::size dmaBufferCapacity;     // DMA buffer size (0 if not allocated)
        bool dmaBufferInUse;            // Whether DMA buffer is currently in use
    };
    Stats getStats() const;

private:
    /// Internal buffer slot in the pool
    struct BufferSlot {
        uint8_t* data;     // Raw buffer pointer (MALLOC_CAP_INTERNAL)
        fl::size capacity; // Allocated size in bytes
        bool inUse;        // Whether buffer is currently acquired

        BufferSlot() : data(nullptr), capacity(0), inUse(false) {}
    };

    /// DMA buffer (separate from internal pool due to hardware limitations)
    struct DMABuffer {
        uint8_t* data;     // Raw buffer pointer (MALLOC_CAP_DMA)
        fl::size capacity; // Allocated size in bytes
        bool inUse;        // Whether buffer is currently acquired

        DMABuffer() : data(nullptr), capacity(0), inUse(false) {}
    };

    fl::vector<BufferSlot> mInternalBuffers; // Pool of internal DRAM buffers
    DMABuffer mDMABuffer;                    // Single DMA buffer (hardware limit)

    /// Find a suitable buffer slot (exact size or larger, not in use)
    /// Returns -1 if no suitable buffer found
    int findSuitableSlot(fl::size size);

    /// Allocate a new buffer slot or resize an existing one
    /// Returns -1 on allocation failure
    int allocateOrResizeSlot(fl::size size);
};

} // namespace fl

#endif // FASTLED_RMT5
