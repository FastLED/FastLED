/// @file channel_engine.h
/// @brief ESP32-P4 PARLIO DMA engine - the powerhouse that drives it all
///
/// This is the beating heart of the parallel I/O system - the DMA engine that
/// powers multi-channel LED output with hardware-accelerated timing.

#pragma once

#include "fl/span.h"
#include "fl/stdint.h"
#include "fl/shared_ptr.h"

namespace fl {

// Forward declarations
class IChannelGroup;

FASTLED_SHARED_PTR(IChannelGroup);

/// @brief Abstract interface for the PARLIO DMA engine
///
/// The engine manages exclusive access to the PARLIO peripheral hardware,
/// ensuring only one group can use the DMA controller at a time
/// using semaphore-based locking for thread-safe access.
///
/// Implementation is hidden in .cpp file for complete platform isolation.
class IChannelEngine {
public:

    /// @brief Virtual destructor
    virtual ~IChannelEngine() = default;

    /// @brief Execute show operation for all channel groups
    /// @param groups Span of channel groups to process
    virtual void onShow(fl::span<IChannelGroupPtr> groups) = 0;

protected:
    /// @brief Protected constructor (interface pattern)
    IChannelEngine() = default;

    // Non-copyable, non-movable
    IChannelEngine(const IChannelEngine&) = delete;
    IChannelEngine& operator=(const IChannelEngine&) = delete;
    IChannelEngine(IChannelEngine&&) = delete;
    IChannelEngine& operator=(IChannelEngine&&) = delete;

    // operator delete is forbidden
    void operator delete(void*) = delete;
};

}  // namespace fl
