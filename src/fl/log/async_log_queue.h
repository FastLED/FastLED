#pragma once

/// @file fl/log/async_log_queue.h
/// @brief High-performance ISR-safe async logging queue (SPSC ring buffer) - declarations only

#include "fl/stl/int.h"
#include "fl/stl/noexcept.h"
#include "fl/stl/static_assert.h"

namespace fl {

class string;

/// @brief High-performance SPSC async log queue
/// @tparam DescriptorCount Number of message descriptors (must be power of 2)
/// @tparam ArenaSize Size of string arena in bytes (must be power of 2)
template <fl::size DescriptorCount = 128, fl::size ArenaSize = 4096>
class AsyncLogQueue {
    // Compile-time assertions for power-of-2 sizes (enables cheap modulo with &)
    FL_STATIC_ASSERT((DescriptorCount & (DescriptorCount - 1)) == 0,
                  "DescriptorCount must be power of 2");
    FL_STATIC_ASSERT((ArenaSize & (ArenaSize - 1)) == 0,
                  "ArenaSize must be power of 2");
    FL_STATIC_ASSERT(DescriptorCount >= 4, "DescriptorCount must be >= 4");
    FL_STATIC_ASSERT(ArenaSize >= 32, "ArenaSize must be >= 32");

public:
    /// Maximum message length (bounded for ISR safety)
    enum { MAX_MESSAGE_LENGTH = 512 };

    /// Descriptor for one log message
    struct Descriptor {
        fl::u32 mStartIdx;  ///< Offset into arena where message starts
        fl::u16 mLength;    ///< Length of message in bytes
        fl::u16 mPadding;   ///< Reserved for alignment (unused)

        Descriptor() FL_NO_EXCEPT;
    };

    AsyncLogQueue() FL_NO_EXCEPT;

    /// @brief Push a message from fl::string (ISR-safe)
    bool push(const fl::string& msg) FL_NO_EXCEPT;

    /// @brief Push a C-string message (ISR-safe)
    bool push(const char* str) FL_NO_EXCEPT;

    /// @brief Consumer: Try to pop one message (main thread only)
    bool tryPop(const char** outPtr, fl::u16* outLen) FL_NO_EXCEPT;

    /// @brief Consumer: Commit the popped message to free space (main thread only)
    void commit() FL_NO_EXCEPT;

    /// @brief Get number of messages dropped due to overflow
    fl::u32 droppedCount() const FL_NO_EXCEPT;

    /// @brief Get current number of messages in queue
    fl::size size() const FL_NO_EXCEPT;

    /// @brief Check if queue is empty
    bool empty() const FL_NO_EXCEPT;

    /// @brief Get maximum descriptor capacity
    constexpr fl::size capacity() const FL_NO_EXCEPT {
        return DescriptorCount - 1;  // One slot reserved for full/empty distinction
    }

private:
    /// Implementation details
    bool push(const char* str, fl::u16 len) FL_NO_EXCEPT;
    static fl::u16 boundedStrlen(const char* str, fl::u16 maxLen) FL_NO_EXCEPT;
    bool arenaHasSpace(fl::u32 aHead, fl::u32 aTail, fl::u16 len) const FL_NO_EXCEPT;
    fl::u32 loadHead() const FL_NO_EXCEPT;
    fl::u32 loadTail() const FL_NO_EXCEPT;
    fl::u32 loadArenaTail() const FL_NO_EXCEPT;
    void atomicIncDropped() FL_NO_EXCEPT;

    // Member variables
    Descriptor mDescriptors[DescriptorCount];  ///< Ring of message descriptors
    char mArena[ArenaSize];                    ///< String storage arena

    // Ring indices (modified under critical section for memory ordering)
    volatile fl::u32 mHead;       ///< Producer write position (descriptor ring)
    volatile fl::u32 mTail;       ///< Consumer read position (descriptor ring)
    volatile fl::u32 mArenaHead;  ///< Producer write position (arena)
    volatile fl::u32 mArenaTail;  ///< Consumer read position (arena)

    volatile fl::u32 mDropped;    ///< Count of dropped messages (overflow)
};

} // namespace fl
