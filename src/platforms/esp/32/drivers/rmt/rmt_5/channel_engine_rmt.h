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
#include "ftl/span.h"
#include "ftl/vector.h"
#include "ftl/hash_map.h"
#include "ftl/time.h"
#include "fl/timeout.h"
#include "rmt5_worker_base.h"
#include "rmt5_worker.h"

namespace fl {

/// @brief RMT5-based ChannelEngine implementation
///
/// Manages RMT workers directly, allocating them dynamically until
/// the hardware runs out of channels.
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

    /// @brief Pending channel data to be transmitted when workers become available
    struct PendingChannel {
        ChannelDataPtr data;
        int pin;
        ChipsetTiming timing;
        uint32_t reset_us;
    };

    /// @brief Initialize workers on first use (allocates until hardware failure)
    void initializeWorkersIfNeeded();

    /// @brief Find an available worker
    RmtWorker* findAvailableWorker();

    /// @brief Release a worker back to available pool
    void releaseWorker(IRmtWorkerBase* worker);

    /// @brief Try to start transmission for a pending channel
    /// @return true if transmission started, false if no worker available
    bool tryStartPendingChannel(const PendingChannel& pending);

    /// @brief Process pending channels that couldn't be started earlier
    void processPendingChannels();

    /// @brief All workers (owned by this engine)
    fl::vector<RmtWorker*> mWorkers;

    /// @brief Track workers currently transmitting (not yet available)
    fl::vector_inlined<WorkerInfo, 16> mActiveWorkers;

    /// @brief Track workers that can be released back to the pool
    fl::vector_inlined<WorkerInfo, 16> mAvailableWorkers;

    /// @brief Pending channels waiting for available workers
    fl::vector_inlined<PendingChannel, 16> mPendingChannels;

    /// @brief Initialization flag
    bool mInitialized;
};

} // namespace fl

#endif // FASTLED_RMT5

#endif // ESP32
