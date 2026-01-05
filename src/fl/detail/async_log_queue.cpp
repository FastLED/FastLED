/// @file detail/async_log_queue.cpp
/// @brief High-performance ISR-safe async logging queue implementation

#include "fl/detail/async_log_queue.h"
#include "fl/isr.h"  // For fl::isr::CriticalSection
#include "fl/math_macros.h"

namespace fl {

// NOTE: CriticalSection implementation moved to fl/isr.cpp

// ============================================================================
// AsyncLogQueue::Descriptor implementation
// ============================================================================

template <fl::size DescriptorCount, fl::size ArenaSize>
AsyncLogQueue<DescriptorCount, ArenaSize>::Descriptor::Descriptor()
    : mStartIdx(0), mLength(0), mPadding(0) {}

// ============================================================================
// AsyncLogQueue public methods
// ============================================================================

template <fl::size DescriptorCount, fl::size ArenaSize>
AsyncLogQueue<DescriptorCount, ArenaSize>::AsyncLogQueue()
    : mHead(0), mTail(0), mArenaHead(0), mArenaTail(0), mDropped(0) {
    // Initialize all descriptors to zero (optional, for debugging)
    for (fl::size i = 0; i < DescriptorCount; i++) {
        mDescriptors[i] = Descriptor();
    }
}

template <fl::size DescriptorCount, fl::size ArenaSize>
bool AsyncLogQueue<DescriptorCount, ArenaSize>::push(const fl::string& msg) {
    fl::size len = msg.length();
    if (len > MAX_MESSAGE_LENGTH) {
        len = MAX_MESSAGE_LENGTH;
    }
    return push(msg.c_str(), static_cast<fl::u16>(len));
}

template <fl::size DescriptorCount, fl::size ArenaSize>
bool AsyncLogQueue<DescriptorCount, ArenaSize>::push(const char* str) {
    fl::u16 len = boundedStrlen(str, MAX_MESSAGE_LENGTH);
    return push(str, len);
}

template <fl::size DescriptorCount, fl::size ArenaSize>
bool AsyncLogQueue<DescriptorCount, ArenaSize>::tryPop(const char** outPtr, fl::u16* outLen) {
    // Read head with memory barrier (acquire semantics)
    fl::u32 head = loadHead();
    fl::u32 tail = mTail;

    if (tail == head) {
        return false;  // Queue empty
    }

    // Read descriptor at tail position
    const Descriptor& desc = mDescriptors[tail];

    // Return pointer into arena (contiguous due to padding at wrap)
    *outPtr = &mArena[desc.mStartIdx];
    *outLen = desc.mLength;

    return true;
}

template <fl::size DescriptorCount, fl::size ArenaSize>
void AsyncLogQueue<DescriptorCount, ArenaSize>::commit() {
    fl::u32 tail = mTail;
    const Descriptor& desc = mDescriptors[tail];

    // Free arena space by advancing arena tail
    fl::u32 newArenaTail = (mArenaTail + desc.mLength) & (ArenaSize - 1);

    {
        fl::isr::CriticalSection cs;  // Protect arena tail update
        mArenaTail = newArenaTail;
    }

    // Clear descriptor (optional, for debugging/sentinel semantics)
    mDescriptors[tail] = Descriptor();

    // Advance tail (consumer publishes completion)
    fl::u32 newTail = (tail + 1) & (DescriptorCount - 1);

    {
        fl::isr::CriticalSection cs;  // Protect tail update
        mTail = newTail;
    }
}

template <fl::size DescriptorCount, fl::size ArenaSize>
fl::u32 AsyncLogQueue<DescriptorCount, ArenaSize>::droppedCount() const {
    return mDropped;
}

template <fl::size DescriptorCount, fl::size ArenaSize>
fl::size AsyncLogQueue<DescriptorCount, ArenaSize>::size() const {
    fl::u32 head = loadHead();
    fl::u32 tail = mTail;
    return (head - tail) & (DescriptorCount - 1);
}

template <fl::size DescriptorCount, fl::size ArenaSize>
bool AsyncLogQueue<DescriptorCount, ArenaSize>::empty() const {
    return loadHead() == mTail;
}

// ============================================================================
// AsyncLogQueue private methods
// ============================================================================

template <fl::size DescriptorCount, fl::size ArenaSize>
bool AsyncLogQueue<DescriptorCount, ArenaSize>::push(const char* str, fl::u16 len) {
    if (len == 0) {
        return true;  // Empty message, accept but don't store
    }

    // Load indices (head is producer-owned, tail needs barrier)
    fl::u32 head = mHead;
    fl::u32 tail = loadTail();
    fl::u32 next = (head + 1) & (DescriptorCount - 1);

    // Check if descriptor ring is full
    if (next == tail) {
        atomicIncDropped();
        return false;
    }

    // Check if arena has enough free space
    fl::u32 aHead = mArenaHead;
    fl::u32 aTail = loadArenaTail();

    if (!arenaHasSpace(aHead, aTail, len)) {
        atomicIncDropped();
        return false;
    }

    // Allocate space in arena
    fl::u32 start = aHead;

    // Check if message would wrap past arena end
    if (start + len > ArenaSize) {
        // Insert padding to wrap to start of arena (contiguous records)
        fl::u32 padding = ArenaSize - start;

        // Check if padding + message fits
        if (!arenaHasSpace(aHead, aTail, static_cast<fl::u16>(padding + len))) {
            atomicIncDropped();
            return false;
        }

        // Advance arena head to skip padding bytes
        aHead = (aHead + padding) & (ArenaSize - 1);  // Should wrap to 0
        // Store message at wrapped position
        start = aHead;
    }

    // Copy message into arena (the only potentially slow operation in ISR)
    for (fl::u16 i = 0; i < len; i++) {
        mArena[start + i] = str[i];
    }

    // Advance arena head by message length
    fl::u32 newArenaHead = (aHead + len) & (ArenaSize - 1);
    {
        fl::isr::CriticalSection cs;  // Protect arena head update
        mArenaHead = newArenaHead;
    }

    // Write descriptor (data is already copied, safe to publish)
    mDescriptors[head].mStartIdx = start;
    mDescriptors[head].mLength = len;

    // Publish by advancing head (critical section for memory ordering)
    {
        fl::isr::CriticalSection cs;
        mHead = next;
    }

    return true;
}

template <fl::size DescriptorCount, fl::size ArenaSize>
fl::u16 AsyncLogQueue<DescriptorCount, ArenaSize>::boundedStrlen(const char* str, fl::u16 maxLen) {
    fl::u16 len = 0;
    while (len < maxLen && str[len] != '\0') {
        len++;
    }
    return len;
}

template <fl::size DescriptorCount, fl::size ArenaSize>
bool AsyncLogQueue<DescriptorCount, ArenaSize>::arenaHasSpace(fl::u32 aHead, fl::u32 aTail, fl::u16 len) const {
    // Calculate free space in ring buffer
    fl::u32 used = (aHead - aTail) & (ArenaSize - 1);
    fl::u32 free = ArenaSize - used - 1;  // Reserve 1 byte to distinguish full/empty

    return len <= free;
}

template <fl::size DescriptorCount, fl::size ArenaSize>
fl::u32 AsyncLogQueue<DescriptorCount, ArenaSize>::loadHead() const {
    fl::isr::CriticalSection cs;
    return mHead;
}

template <fl::size DescriptorCount, fl::size ArenaSize>
fl::u32 AsyncLogQueue<DescriptorCount, ArenaSize>::loadTail() const {
    fl::isr::CriticalSection cs;
    return mTail;
}

template <fl::size DescriptorCount, fl::size ArenaSize>
fl::u32 AsyncLogQueue<DescriptorCount, ArenaSize>::loadArenaTail() const {
    fl::isr::CriticalSection cs;
    return mArenaTail;
}

template <fl::size DescriptorCount, fl::size ArenaSize>
void AsyncLogQueue<DescriptorCount, ArenaSize>::atomicIncDropped() {
    fl::isr::CriticalSection cs;
    mDropped = mDropped + 1;  // C++20-compliant volatile increment
}

// ============================================================================
// Explicit template instantiations for common sizes
// ============================================================================

// Default size (128 descriptors, 4KB arena)
template class AsyncLogQueue<128, 4096>;

// Small test size (8 descriptors, 64 bytes arena)
template class AsyncLogQueue<8, 64>;

// Medium test size (16 descriptors, 256 bytes arena)
template class AsyncLogQueue<16, 256>;

// Large test size (128 descriptors, 1KB arena)
template class AsyncLogQueue<128, 1024>;

} // namespace fl
