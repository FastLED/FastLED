/// @file channel_engine_rmt.h
/// @brief RMT5 implementation of ChannelEngine for ESP32

#pragma once

#include "fl/compiler_control.h"

// Allow compilation on both ESP32 hardware and stub platforms (for testing)
#include "platforms/is_platform.h"
#if defined(FL_IS_ESP32) || defined(FASTLED_STUB_IMPL)

#ifdef FL_IS_ESP32
#include "platforms/esp/32/feature_flags/enabled.h"
#endif

// On ESP32: Check FASTLED_RMT5 flag
// On stub platforms: Always allow compilation (no feature flag check)
#if !defined(FL_IS_ESP32) || FASTLED_RMT5  // Must check BEFORE including any RMT5 headers to prevent symbol conflicts

#include "fl/channels/engine.h"
#include "fl/channels/data.h"
#include "fl/engine_events.h"
#include "fl/stl/shared_ptr.h"

namespace fl {

/// @brief RMT5-based IChannelEngine implementation
///
/// Consolidates all RMT functionality:
/// - Direct channel management (no worker abstraction)
/// - Encoder caching by timing configuration (never deleted)
/// - Channel persistence between frames (avoid recreation overhead)
/// - On-demand HW channel allocation with sequential reuse
/// - Runtime DMA detection with graceful fallback
///
/// Managed by ChannelBusManager which handles frame lifecycle events.
///
/// Usage:
/// @code
/// auto engine = ChannelEngineRMT::create();
/// engine->enqueue(channelData);
/// engine->show();
/// while (engine->poll() == EngineState::BUSY) {
///     // wait
/// }
/// @endcode
class ChannelEngineRMT : public IChannelEngine {
public:
    /**
     * @brief Factory method to create RMT channel engine
     * @return Shared pointer to ChannelEngineRMT instance
     */
    static fl::shared_ptr<ChannelEngineRMT> create();

    /**
     * @brief Virtual destructor
     */
    virtual ~ChannelEngineRMT() = default;

    /**
     * @brief Check if engine can handle channel data
     * @param data Channel data to check
     * @return true if clockless channel (rejects SPI), false otherwise
     */
    virtual bool canHandle(const ChannelDataPtr& data) const override = 0;

    /**
     * @brief Enqueue channel data for transmission
     * @param channelData Channel data to transmit
     */
    virtual void enqueue(ChannelDataPtr channelData) override = 0;

    /**
     * @brief Trigger transmission of enqueued data
     */
    virtual void show() override = 0;

    /**
     * @brief Query engine state and perform maintenance
     * @return Current engine state (READY, BUSY, DRAINING, or ERROR)
     */
    virtual EngineState poll() override = 0;

    /**
     * @brief Get the engine name for affinity binding
     * @return "RMT"
     */
    fl::string getName() const override { return fl::string::from_literal("RMT"); }

    /**
     * @brief Get engine capabilities (CLOCKLESS protocols only)
     * @return Capabilities with supportsClockless=true, supportsSpi=false
     */
    Capabilities getCapabilities() const override {
        return Capabilities(true, false);  // Clockless only
    }

protected:
    ChannelEngineRMT() = default;
    ChannelEngineRMT(const ChannelEngineRMT&) = delete;
    ChannelEngineRMT& operator=(const ChannelEngineRMT&) = delete;
};

} // namespace fl

#endif // FASTLED_RMT5 or FASTLED_STUB_IMPL

#endif // FL_IS_ESP32 or FASTLED_STUB_IMPL
