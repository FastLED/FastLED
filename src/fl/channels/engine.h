/// @file channel_engine.h
/// @brief Minimal interface for LED channel transmission engines
///
/// This pure interface defines the contract for LED channel engines without
/// imposing any specific state management or batching strategy. Concrete
/// implementations handle their own internal state as needed.
///
/// Design Philosophy:
/// - Pure interface: No state management, no helper methods
/// - Three operations: enqueue(), show(), poll()
/// - Flexible: Implementations decide when/how to batch and transmit
/// - Composable: Easy to delegate and wrap (e.g., ChannelBusManager)
///
/// Migration from ChannelEngine:
/// The old ChannelEngine base class managed shared state (mPendingChannels,
/// mTransmittingChannels, mLastError) which complicated delegation patterns.
/// This interface leaves all state management to concrete implementations.

#pragma once

#include "fl/stl/shared_ptr.h"

namespace fl {

// Forward declarations
class ChannelData;
FASTLED_SHARED_PTR(ChannelData);

/// @brief Minimal interface for LED channel transmission engines
///
/// Pure interface with no state management. Concrete implementations
/// handle their own batching, error tracking, and cleanup logic.
///
/// State Machine Behavior:
/// Typical flow: READY → BUSY → DRAINING → READY
///
/// Usage Pattern:
/// 1. Call enqueue() one or more times to submit LED data
/// 2. Call show() to trigger transmission
/// 3. Call poll() to check transmission status and perform cleanup
///
/// Implementation Guidelines:
/// - enqueue(): Store channel data for later transmission
/// - show(): Initiate transmission of enqueued data (may block if BUSY/DRAINING)
/// - poll(): Return current hardware state and perform cleanup when complete
class IChannelEngine {
public:
    enum class EngineState {
        READY,      ///< Hardware idle; ready to accept new transmissions
        BUSY,       ///< Active: channels transmitting or queued
        DRAINING,   ///< All channels submitted; still transmitting
        ERROR,      ///< Engine encountered an error
    };

    /// @brief Enqueue channel data for transmission
    /// @param channelData Channel data to transmit
    /// @note Behavior depends on implementation - may batch or transmit immediately
    /// @note Non-blocking. Data is stored until show() is called (typical pattern).
    /// @note Clever implementations may begin transmission early to save memory.
    virtual void enqueue(ChannelDataPtr channelData) = 0;

    /// @brief Trigger transmission of enqueued data
    /// @note May block depending on current engine state (poll() returns BUSY/DRAINING)
    /// @note Typical behavior: Wait for hardware to be READY, then transmit all enqueued data
    virtual void show() = 0;

    /// @brief Query engine state and perform maintenance
    /// @return Current engine state (READY, BUSY, DRAINING, or ERROR)
    /// @note Non-blocking. Returns immediately with current hardware status.
    /// @note Implementations should use this to:
    ///   - Check hardware transmission status
    ///   - Clear channel "in use" flags when transmission completes
    ///   - Update internal error state if needed
    virtual EngineState poll() = 0;

    /// @brief Get the engine name for affinity binding
    /// @return Engine name (e.g., "RMT", "SPI", "PARLIO"), or nullptr if unnamed
    /// @note Used by Channel affinity system to bind channels to specific engines
    virtual const char* getName() const { return nullptr; }

    /// @brief Virtual destructor
    virtual ~IChannelEngine() = default;

protected:
    IChannelEngine() = default;

    // Non-copyable, non-movable
    IChannelEngine(const IChannelEngine&) = delete;
    IChannelEngine& operator=(const IChannelEngine&) = delete;
    IChannelEngine(IChannelEngine&&) = delete;
    IChannelEngine& operator=(IChannelEngine&&) = delete;
};

}  // namespace fl
