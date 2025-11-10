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
class IChannel;

FASTLED_SHARED_PTR(IChannel);

/// @brief Abstract interfac for drawing parallel I/O hardware
///
/// The engine manages exclusive access to the peripheral hardware,
/// ensuring only one group can use the DMA controller at a time
/// using semaphore-based locking for thread-safe access.
///
/// Implementation is hidden in .cpp file for complete platform isolation.
class IChannelEngine {
public:
    enum class EngineState {
        IDLE,       ///< No channels active or queued; ready to start a new frame
        BUSY,       ///< Mixed: some channels transmitting, others queued (scheduler still enqueuing)
        DRAINING,   ///< All chennels submitted; CPU-side updates stopped; DMA still transmitting
        COMPLETE,   ///< All transmissions finished; hardware stable and up-to-date
    };

    /// @brief Execute show operation for all channel groups
    /// @param channels Span of channel groups to process
    /// @warning This will block if poll() returns BUSY or DRAINING.
    virtual void onBeginShow(fl::span<IChannelPtr> channels) = 0;

    /// The caller needs to call poll() until the engine returns DRAINING or COMPLETE.
    /// @note None blocking.
    virtual EngineState poll() = 0;

protected:
    /// @brief Protected constructor (interface pattern)
    IChannelEngine() = default;
    virtual ~IChannelEngine() = default;  // FastLED will never delete a channel engine.

    // Non-copyable, non-movable
    IChannelEngine(const IChannelEngine&) = delete;
    IChannelEngine& operator=(const IChannelEngine&) = delete;
    IChannelEngine(IChannelEngine&&) = delete;
    IChannelEngine& operator=(IChannelEngine&&) = delete;
};

}  // namespace fl
