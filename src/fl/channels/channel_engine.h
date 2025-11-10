/// @file channel_engine.h
/// @brief ESP32-P4 PARLIO DMA engine - the powerhouse that drives it all
///
/// This is the beating heart of the parallel I/O system - the DMA engine that
/// powers multi-channel LED output with hardware-accelerated timing.

#pragma once

#include "fl/span.h"
#include "fl/stdint.h"
#include "fl/shared_ptr.h"
#include "fl/string.h"

namespace fl {

// Forward declarations
class Channel;
class ChannelData;

FASTLED_SHARED_PTR(Channel);
FASTLED_SHARED_PTR(ChannelData);

/// @brief Abstract interface for drawing parallel I/O hardware
///
/// The engine manages exclusive access to the peripheral hardware,
/// ensuring only one group can use the DMA controller at a time
/// using semaphore-based locking for thread-safe access.
///
/// State Machine Behavior:
/// Typical flow: READY → BUSY → DRAINING → READY
///
/// Some implementations may skip BUSY state if they use internal mechanisms
/// (like ISRs) to asynchronously queue pending channels to the hardware.
///
/// Implementation is hidden in .cpp file for complete platform isolation.
class IChannelEngine {
public:
    enum class EngineState {
        READY,      ///< Hardware idle; ready to accept beginTransmission() non-blocking
        BUSY,       ///< Active: channels transmitting or queued (scheduler still enqueuing)
        DRAINING,   ///< All channels submitted; still transmitting; beginTransmission() will block
        ERROR,      ///< Engine encountered an error; check getLastError() for details
    };

    /// @brief Begin LED data transmission for all channels
    /// @param channelData Span of channel data to transmit
    /// @warning This will block if poll() returns BUSY or DRAINING.
    virtual void beginTransmission(fl::span<ChannelDataPtr> channelData) = 0;

    /// @brief Query engine state (may advance state machine)
    /// The caller needs to call poll() until the engine returns READY.
    /// @note Non-blocking. Implementations should override for optimized state queries.
    virtual EngineState poll() = 0;

    /// @brief Get the last error message
    /// @return Error description string, or empty string if no error occurred
    /// @note Only valid when poll() returns EngineState::ERROR
    virtual fl::string getLastError() = 0;

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
