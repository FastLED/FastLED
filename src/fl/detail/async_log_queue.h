#pragma once

/// @file detail/async_log_queue.h
/// @brief High-performance ISR-safe async logging queue (SPSC ring buffer) - declarations only

#include "fl/stl/string.h"
#include "fl/int.h"

namespace fl {

/// @brief High-performance SPSC async log queue
/// @tparam DescriptorCount Number of message descriptors (must be power of 2)
/// @tparam ArenaSize Size of string arena in bytes (must be power of 2)
template <fl::size DescriptorCount = 128, fl::size ArenaSize = 4096>
class AsyncLogQueue {
    // Compile-time assertions for power-of-2 sizes (enables cheap modulo with &)
    static_assert((DescriptorCount & (DescriptorCount - 1)) == 0,
                  "DescriptorCount must be power of 2");
    static_assert((ArenaSize & (ArenaSize - 1)) == 0,
                  "ArenaSize must be power of 2");
    static_assert(DescriptorCount >= 4, "DescriptorCount must be >= 4");
    static_assert(ArenaSize >= 32, "ArenaSize must be >= 32");

public:
    /// Maximum message length (bounded for ISR safety)
    enum { MAX_MESSAGE_LENGTH = 512 };

    /// Descriptor for one log message
    struct Descriptor {
        fl::u32 mStartIdx;  ///< Offset into arena where message starts
        fl::u16 mLength;    ///< Length of message in bytes
        fl::u16 mPadding;   ///< Reserved for alignment (unused)

        Descriptor();
    };

    AsyncLogQueue();

    /// @brief Push a message from fl::string (ISR-safe)
    bool push(const fl::string& msg);

    /// @brief Push a C-string message (ISR-safe)
    bool push(const char* str);

    /// @brief Consumer: Try to pop one message (main thread only)
    bool tryPop(const char** outPtr, fl::u16* outLen);

    /// @brief Consumer: Commit the popped message to free space (main thread only)
    void commit();

    /// @brief Get number of messages dropped due to overflow
    fl::u32 droppedCount() const;

    /// @brief Get current number of messages in queue
    fl::size size() const;

    /// @brief Check if queue is empty
    bool empty() const;

    /// @brief Get maximum descriptor capacity
    constexpr fl::size capacity() const {
        return DescriptorCount - 1;  // One slot reserved for full/empty distinction
    }

private:
    /// Implementation details
    bool push(const char* str, fl::u16 len);
    static fl::u16 boundedStrlen(const char* str, fl::u16 maxLen);
    bool arenaHasSpace(fl::u32 aHead, fl::u32 aTail, fl::u16 len) const;
    fl::u32 loadHead() const;
    fl::u32 loadTail() const;
    fl::u32 loadArenaTail() const;
    void atomicIncDropped();

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
