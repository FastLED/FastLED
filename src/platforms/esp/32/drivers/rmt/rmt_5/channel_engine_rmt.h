/// @file channel_engine_rmt.h
/// @brief RMT5 implementation of ChannelEngine for ESP32

#pragma once

#include "fl/compiler_control.h"
#ifdef ESP32

#include "platforms/esp/32/feature_flags/enabled.h"

#if FASTLED_RMT5  // Must check BEFORE including any RMT5 headers to prevent symbol conflicts

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
    const char* getName() const override { return "RMT"; }

protected:
    ChannelEngineRMT() = default;
    ChannelEngineRMT(const ChannelEngineRMT&) = delete;
    ChannelEngineRMT& operator=(const ChannelEngineRMT&) = delete;
};

} // namespace fl

#endif // FASTLED_RMT5

#endif // ESP32
