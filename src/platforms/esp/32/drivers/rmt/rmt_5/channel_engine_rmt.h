/// @file channel_engine_rmt.h
/// @brief RMT5 implementation of ChannelEngine for ESP32

#pragma once

#include "fl/compiler_control.h"
#ifdef ESP32

#include "third_party/espressif/led_strip/src/enabled.h"

#if FASTLED_RMT5

#include "fl/channels/channel_engine.h"
#include "fl/channels/channel_data.h"
#include "fl/engine_events.h"
#include "fl/span.h"
#include "fl/vector.h"
#include "fl/hash_map.h"
#include "fl/time.h"
#include "fl/timeout.h"
#include "rmt5_worker_base.h"

namespace fl {

/// @brief RMT5-based ChannelEngine implementation
///
/// Simple implementation that wraps the RmtWorkerPool to provide
/// ChannelEngine interface for RMT5 hardware on ESP32.
///
/// Listens to onEndFrame events and polls workers until transmission completes.
class ChannelEngineRMT : public ChannelEngine, public EngineEvents::Listener {
public:
    ChannelEngineRMT();
    ~ChannelEngineRMT() override;

    // EngineEvents::Listener interface
    void onEndFrame() override;

protected:
    /// @brief Query engine state (hardware polling implementation)
    /// @return Current engine state (READY, BUSY, or ERROR)
    EngineState pollDerived() override;

    /// @brief Begin LED data transmission for all channels
    /// @param channelData Span of channel data to transmit
    void beginTransmission(fl::span<const ChannelDataPtr> channelData) override;

private:
    /// @brief Helper struct to track worker with its pin and reset time
    struct WorkerInfo {
        IRmtWorkerBase* worker;
        int pin;
        uint32_t reset_us;
    };

    /// @brief Track workers currently transmitting (not yet available)
    fl::vector_inlined<WorkerInfo, 16> mActiveWorkers;

    /// @brief Track workers that can be released back to the pool
    fl::vector_inlined<WorkerInfo, 16> mAvailableWorkers;

    /// @brief Track reset timers for each pin (microsecond-based)
    fl::unordered_map<int, Timeout> mPinResetTimers;
};

} // namespace fl

#endif // FASTLED_RMT5

#endif // ESP32
