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
#include "fl/vector.h"

namespace fl {

// Forward declarations
class Channel;
class ChannelData;

FASTLED_SHARED_PTR(Channel);
FASTLED_SHARED_PTR(ChannelData);

/// @brief Base class for LED channel transmission engines
///
/// ============================================================================
/// IMPLEMENTERS: YOU MUST OVERRIDE THESE TWO METHODS IN YOUR DERIVED CLASS:
/// 1. poll() - Check hardware state and return current engine status
/// 2. beginTransmission() - Actually transmit the LED data to hardware
/// ============================================================================
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
/// Usage Pattern:
/// 1. Channels call enqueue() to submit data for transmission
/// 2. User calls show() to trigger actual transmission
/// 3. show() internally calls beginTransmission() with batched data
///
/// Implementation is hidden in .cpp file for complete platform isolation.
class ChannelEngine {
public:
    enum class EngineState {
        READY,      ///< Hardware idle; ready to accept beginTransmission() non-blocking
        BUSY,       ///< Active: channels transmitting or queued (scheduler still enqueuing)
        DRAINING,   ///< All channels submitted; still transmitting; beginTransmission() will block
        ERROR,      ///< Engine encountered an error; check getLastError() for details
    };

    /// @brief Enqueue channel data for later transmission
    /// @param channelData Channel data to transmit when show() is called
    /// @note Non-blocking. Data is batched until show() is called.
    /// @note Very clever engines may begin transmission immediately after
    ///       certain batch sizes to save memory.
    virtual void enqueue(ChannelDataPtr channelData);

    /// @brief Transmit all enqueued channel data
    /// @note Calls beginTransmission() with all batched channel data, then clears the queue
    void show();

    /// @brief Query engine state and manage channel buffer flags
    ///
    /// This method calls pollDerived() to check hardware status. When transmission
    /// completes (READY, ERROR states), it automatically clears the "in use" flags
    /// on all transmitted channels and clears the transmission queue.
    ///
    /// @return Current engine state (READY, BUSY, DRAINING, or ERROR)
    /// @note Non-blocking. Returns immediately with current hardware status.
    EngineState poll();

    /// @brief Get the last error message
    /// @return Error description string, or empty string if no error occurred
    /// @note Only valid when poll() returns EngineState::ERROR
    fl::string getLastError() { return mLastError; }

protected:
    //==========================================================================
    // IMPLEMENTERS: YOU MUST OVERRIDE THIS METHOD
    //==========================================================================
    /// @brief Query engine state (hardware polling implementation)
    ///
    /// **OVERRIDE THIS METHOD IN YOUR DERIVED CLASS**
    ///
    /// This method should check the hardware state and return the current status.
    /// The base poll() method will call this and handle channel cleanup automatically.
    ///
    /// @return Current engine state (READY, BUSY, DRAINING, or ERROR)
    /// @note Non-blocking. Should return immediately with current hardware status.
    virtual EngineState pollDerived() = 0;

    //==========================================================================
    // IMPLEMENTERS: YOU MUST OVERRIDE THIS METHOD
    //==========================================================================
    /// @brief Begin LED data transmission for all channels
    ///
    /// **OVERRIDE THIS METHOD IN YOUR DERIVED CLASS**
    ///
    /// This is where you implement the actual hardware transmission logic.
    /// Write the LED data to your hardware peripheral (e.g., DMA, SPI, bit-banging).
    ///
    /// @param channelData Span of channel data to transmit (const - do not modify the pointers)
    /// @warning This will block if poll() returns BUSY or DRAINING.
    /// @note Called automatically by show() - you don't call this directly from user code
    virtual void beginTransmission(fl::span<const ChannelDataPtr> channelData) = 0;

    /// @brief Protected constructor (base class pattern)
    ChannelEngine() = default;
    virtual ~ChannelEngine() = default;  // FastLED will never delete a channel engine.

    // Non-copyable, non-movable
    ChannelEngine(const ChannelEngine&) = delete;
    ChannelEngine& operator=(const ChannelEngine&) = delete;
    ChannelEngine(ChannelEngine&&) = delete;
    ChannelEngine& operator=(ChannelEngine&&) = delete;

private:
    /// @brief Pending channel data waiting for show() to be called
    /// @note Uses inlined vector with capacity 16 to avoid heap allocation for typical use cases
    fl::vector_inlined<ChannelDataPtr, 16> mPendingChannels;

    /// @brief Channels currently being transmitted (async operation in progress)
    /// @note These channels will have their "in use" flags cleared when poll() returns READY or ERROR
    fl::vector_inlined<ChannelDataPtr, 16> mTransmittingChannels;

    fl::string mLastError;
};


}  // namespace fl
