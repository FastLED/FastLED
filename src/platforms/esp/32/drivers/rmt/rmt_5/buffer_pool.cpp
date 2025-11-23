#include "buffer_pool.h"

#if FASTLED_RMT5

#include "fl/compiler_control.h"
#include "fl/dbg.h"
#include "fl/warn.h"

namespace fl {

RMTBufferPool::RMTBufferPool() {
    // Start with empty pool - buffers allocated on-demand
}

RMTBufferPool::~RMTBufferPool() {
    // Free all internal buffers
    for (auto& slot : mInternalBuffers) {
        if (slot.data) {
            fl::InternalFree(slot.data);
            slot.data = nullptr;
            slot.capacity = 0;
        }
    }

    // Free DMA buffer
    if (mDMABuffer.data) {
        fl::DMAFree(mDMABuffer.data);
        mDMABuffer.data = nullptr;
        mDMABuffer.capacity = 0;
    }
}

int RMTBufferPool::findSuitableSlot(fl::size size) {
    int bestFit = -1;
    fl::size bestCapacity = ~fl::size(0); // MAX_SIZE

    // Find smallest buffer that fits (best-fit strategy)
    for (fl::size i = 0; i < mInternalBuffers.size(); ++i) {
        const auto& slot = mInternalBuffers[i];
        if (!slot.inUse && slot.capacity >= size && slot.capacity < bestCapacity) {
            bestFit = static_cast<int>(i);
            bestCapacity = slot.capacity;

            // Perfect fit - stop searching
            if (slot.capacity == size) {
                break;
            }
        }
    }

    return bestFit;
}

int RMTBufferPool::allocateOrResizeSlot(fl::size size) {
    // First, try to find an unused slot we can resize
    for (fl::size i = 0; i < mInternalBuffers.size(); ++i) {
        auto& slot = mInternalBuffers[i];
        if (!slot.inUse && slot.capacity < size) {
            // Resize this buffer
            uint8_t* newData = static_cast<uint8_t*>(fl::InternalRealloc(slot.data, size));
            if (!newData) {
                FL_WARN("RMTBufferPool: Failed to realloc internal buffer from "
                        << slot.capacity << " to " << size << " bytes");
                return -1;
            }
            slot.data = newData;
            slot.capacity = size;
            FL_LOG_RMT("RMTBufferPool: Resized buffer " << i << " to " << size << " bytes");
            return static_cast<int>(i);
        }
    }

    // No suitable buffer to resize - create new slot
    BufferSlot newSlot;
    newSlot.data = static_cast<uint8_t*>(fl::InternalAlloc(size));
    if (!newSlot.data) {
        FL_WARN("RMTBufferPool: Failed to allocate new internal buffer of " << size << " bytes");
        return -1;
    }
    newSlot.capacity = size;
    newSlot.inUse = false;

    mInternalBuffers.push_back(newSlot);
    FL_LOG_RMT("RMTBufferPool: Allocated new buffer " << (mInternalBuffers.size() - 1)
           << " with " << size << " bytes");
    return static_cast<int>(mInternalBuffers.size() - 1);
}

fl::span<uint8_t> RMTBufferPool::acquireInternal(fl::size size) {
    if (size == 0) {
        return fl::span<uint8_t>();
    }

    // Try to find existing suitable buffer
    int slotIndex = findSuitableSlot(size);

    // If no suitable buffer, allocate or resize
    if (slotIndex < 0) {
        slotIndex = allocateOrResizeSlot(size);
        if (slotIndex < 0) {
            return fl::span<uint8_t>(); // Allocation failed
        }
    }

    // Mark buffer as in-use and return sub-span
    auto& slot = mInternalBuffers[slotIndex];
    slot.inUse = true;
    return fl::span<uint8_t>(slot.data, size);
}

fl::span<uint8_t> RMTBufferPool::acquireDMA(fl::size size) {
    if (size == 0) {
        return fl::span<uint8_t>();
    }

    if (mDMABuffer.inUse) {
        FL_WARN("RMTBufferPool: DMA buffer already in use (hardware limit: 1 DMA channel)");
        return fl::span<uint8_t>();
    }

    // Allocate or resize DMA buffer if needed
    if (mDMABuffer.capacity < size) {
        // DMA memory doesn't support realloc - must allocate new and free old
        if (mDMABuffer.data) {
            fl::DMAFree(mDMABuffer.data);
            mDMABuffer.data = nullptr;
            mDMABuffer.capacity = 0;
        }

        mDMABuffer.data = static_cast<uint8_t*>(fl::DMAAlloc(size));
        if (!mDMABuffer.data) {
            FL_WARN("RMTBufferPool: Failed to allocate DMA buffer of " << size << " bytes");
            return fl::span<uint8_t>();
        }
        mDMABuffer.capacity = size;
        FL_LOG_RMT("RMTBufferPool: Allocated DMA buffer with " << size << " bytes");
    }

    mDMABuffer.inUse = true;
    return fl::span<uint8_t>(mDMABuffer.data, size);
}

void RMTBufferPool::releaseInternal(fl::span<uint8_t> buffer) {
    if (buffer.empty()) {
        return;
    }

    // Find the slot that owns this buffer
    uint8_t* bufferPtr = buffer.data();
    for (auto& slot : mInternalBuffers) {
        if (slot.data == bufferPtr) {
            if (!slot.inUse) {
                FL_WARN("RMTBufferPool: Releasing buffer that was not marked as in-use");
            }
            slot.inUse = false;
            return;
        }
    }

    FL_WARN("RMTBufferPool: Attempted to release unknown buffer " << static_cast<void*>(bufferPtr));
}

void RMTBufferPool::releaseDMA() {
    if (!mDMABuffer.inUse) {
        FL_WARN("RMTBufferPool: Releasing DMA buffer that was not in use");
    }
    mDMABuffer.inUse = false;
}

RMTBufferPool::Stats RMTBufferPool::getStats() const {
    Stats stats;
    stats.numInternalBuffers = mInternalBuffers.size();
    stats.totalInternalCapacity = 0;
    for (const auto& slot : mInternalBuffers) {
        stats.totalInternalCapacity += slot.capacity;
    }
    stats.dmaBufferCapacity = mDMABuffer.capacity;
    stats.dmaBufferInUse = mDMABuffer.inUse;
    return stats;
}

} // namespace fl

#endif // FASTLED_RMT5
