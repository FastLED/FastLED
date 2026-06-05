/// @file channel_driver.h
/// @brief Minimal interface for LED channel transmission drivers
///
/// This pure interface defines the contract for LED channel drivers without
/// imposing any specific state management or batching strategy. Concrete
/// implementations handle their own internal state as needed.
///
/// Design Philosophy:
/// - Pure interface: No state management, no helper methods
/// - Three operations: enqueue(), show(), poll()
/// - Flexible: Implementations decide when/how to batch and transmit
/// - Composable: Easy to delegate and wrap (e.g., ChannelManager)
///
/// Migration from ChannelEngine:
/// The old ChannelEngine base class managed shared state (mPendingChannels,
/// mTransmittingChannels, mLastError) which complicated delegation patterns.
/// This interface leaves all state management to concrete implementations.

#pragma once

#include "fl/stl/shared_ptr.h"
#include "fl/stl/string.h"  // IWYU pragma: keep
#include "fl/stl/noexcept.h"

namespace fl {

// Forward declarations
class ChannelData;
FASTLED_SHARED_PTR(ChannelData);

/// @brief Minimal interface for LED channel transmission drivers
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
class IChannelDriver {
public:
    /// @brief Driver capabilities
    struct Capabilities {
        bool supportsClockless;  ///< Supports clockless protocols (WS2812, SK6812, etc.)
        bool supportsSpi;        ///< Supports SPI protocols (APA102, SK9822, etc.)

        /// @brief Default constructor (no capabilities)
        constexpr Capabilities() FL_NOEXCEPT : supportsClockless(false), supportsSpi(false) {}

        /// @brief Constructor with explicit capabilities
        constexpr Capabilities(bool clockless, bool spi) FL_NOEXCEPT
            : supportsClockless(clockless), supportsSpi(spi) {}
    };

    /// @brief Driver state with optional error message
    /// @note Backward compatible: DriverState::READY, BUSY, DRAINING, ERROR still work
    struct DriverState {
        enum Value {
            READY,      ///< Hardware idle; ready to accept new transmissions
            BUSY,       ///< Active: channels transmitting or queued
            DRAINING,   ///< All channels submitted; still transmitting
            ERROR,      ///< Driver encountered an error
        };

        Value state;        ///< Current driver state
        fl::string error;   ///< Error message (only populated when state == ERROR)

        /// @brief Construct from state only (no error)
        DriverState(Value v) FL_NOEXCEPT : state(v), error() {}

        /// @brief Construct from state and error message
        DriverState(Value v, const fl::string& e) FL_NOEXCEPT : state(v), error(e) {}

        /// @brief Implicit conversion to Value for backward compatibility
        operator Value() const FL_NOEXCEPT { return state; }

        /// @brief Comparison operators for backward compatibility
        bool operator==(Value v) const FL_NOEXCEPT { return state == v; }
        bool operator!=(Value v) const FL_NOEXCEPT { return state != v; }
    };

    /// @brief Enqueue channel data for transmission
    /// @param channelData Channel data to transmit
    /// @note Behavior depends on implementation - may batch or transmit immediately
    /// @note Non-blocking. Data is stored until show() is called (typical pattern).
    /// @note Clever implementations may begin transmission early to save memory.
    virtual void enqueue(ChannelDataPtr channelData) FL_NOEXCEPT = 0;

    /// @brief Trigger transmission of enqueued data
    /// @note May block depending on current driver state (poll() returns BUSY/DRAINING)
    /// @note Typical behavior: Wait for hardware to be READY, then transmit all enqueued data
    virtual void show() FL_NOEXCEPT = 0;

    /// @brief Query driver state and perform maintenance
    /// @return DriverState containing state and optional error message
    /// @note Non-blocking. Returns immediately with current hardware status.
    /// @note Implementations should use this to:
    ///   - Check hardware transmission status
    ///   - Clear channel "in use" flags when transmission completes
    ///   - Return error message via DriverState when state == ERROR
    virtual DriverState poll() FL_NOEXCEPT = 0;

    /// @brief Get the driver name for affinity binding
    /// @return Driver name (e.g., "RMT", "SPI", "PARLIO"), or empty string if unnamed
    /// @note Used by Channel affinity system to bind channels to specific drivers
    virtual fl::string getName() const FL_NOEXCEPT { return fl::string::from_literal(""); }

    /// @brief Get driver capabilities (clockless, SPI, or both)
    /// @return Capabilities struct with bool flags for supported protocols
    /// @note Used by diagnostic logging to show which protocols each driver supports
    virtual Capabilities getCapabilities() const FL_NOEXCEPT = 0;

    /// @brief Check if this driver can handle the given channel data
    /// @param data Channel data to check (chipset configuration, pin, timing)
    /// @return true if this driver can render the channel data, false otherwise
    /// @note Drivers must implement this to filter by chipset type (e.g., SPI-only, clockless-only)
    /// @note Used by ChannelManager to route channels to compatible drivers
    virtual bool canHandle(const ChannelDataPtr& data) const FL_NOEXCEPT = 0;

    /// @brief Wait for driver to become READY
    /// @param timeoutMs Optional timeout in milliseconds (0 = no timeout)
    /// @return true if driver became READY, false if timeout occurred
    bool waitForReady(u32 timeoutMs = 1000) FL_NOEXCEPT;
    bool waitForReadyOrDraining(u32 timeoutMs = 1000) FL_NOEXCEPT;

    /// @brief Block until this driver finishes any in-flight transmit.
    ///
    /// Phase 2 of #2815: drivers that have an ISR-driven completion
    /// primitive (semaphore, IDF wait-all-done call, event-group) should
    /// override this to block directly on it. The default delegates to
    /// `waitForReady(timeoutMs)` so unmigrated drivers (and all bare-metal
    /// / non-FreeRTOS targets) continue to work via the tiered-spin path
    /// from Phase 1 (#2818).
    ///
    /// `ChannelManager::waitForReady()` short-circuits to
    /// `driver->waitDone(timeoutMs)` when exactly one driver is BUSY,
    /// bypassing the spin tier entirely on the single-driver fast path.
    /// The wait then wakes precisely when the DMA-done ISR fires
    /// (~microseconds) instead of paying the 250 us spin budget.
    ///
    /// @param timeoutMs Optional timeout in milliseconds (0 = no timeout)
    /// @return true if driver became READY, false if timeout occurred
    virtual bool waitDone(u32 timeoutMs = 1000) FL_NOEXCEPT {
        return waitForReady(timeoutMs);
    }

    /// @brief Virtual destructor
    virtual ~IChannelDriver() FL_NOEXCEPT = default;

protected:
    IChannelDriver() FL_NOEXCEPT = default;

    // Non-copyable, non-movable
    IChannelDriver(const IChannelDriver&) FL_NOEXCEPT = delete;
    IChannelDriver& operator=(const IChannelDriver&) FL_NOEXCEPT = delete;
    IChannelDriver(IChannelDriver&&) FL_NOEXCEPT = delete;
    IChannelDriver& operator=(IChannelDriver&&) FL_NOEXCEPT = delete;

    template<typename Condition>
    bool waitForCondition(Condition condition, u32 timeoutMs = 1000) FL_NOEXCEPT;
};

}  // namespace fl
