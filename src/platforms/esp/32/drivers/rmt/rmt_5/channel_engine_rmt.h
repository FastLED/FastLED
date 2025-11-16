/// @file channel_engine_rmt.h
/// @brief RMT5 implementation of ChannelEngine for ESP32

#pragma once

#include "fl/compiler_control.h"
#ifdef ESP32

#include "third_party/espressif/led_strip/src/enabled.h"

#if FASTLED_RMT5

#include "fl/channels/channel_engine.h"
#include "fl/channels/channel_data.h"
#include "fl/chipsets/led_timing.h"
#include "fl/engine_events.h"
#include "ftl/span.h"
#include "ftl/vector.h"
#include "ftl/hash_map.h"
#include "ftl/time.h"
#include "fl/timeout.h"

FL_EXTERN_C_BEGIN
#include "driver/rmt_tx.h"
#include "driver/gpio.h"
FL_EXTERN_C_END

namespace fl {

/// @brief RMT5-based ChannelEngine implementation
///
/// Consolidates all RMT functionality:
/// - Direct channel management (no worker abstraction)
/// - Encoder caching by timing configuration (never deleted)
/// - Channel persistence between frames (avoid recreation overhead)
/// - On-demand HW channel allocation with sequential reuse
///
/// Listens to onEndFrame events and polls channels until transmission completes.
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
    /// @brief RMT channel state (replaces RmtWorkerSimple)
    struct ChannelState {
        rmt_channel_handle_t channel;
        gpio_num_t pin;
        ChipsetTiming timing;
        volatile bool transmissionComplete;
        bool inUse;
        uint32_t reset_us;
    };

    /// @brief Pending channel data to be transmitted when HW channels become available
    struct PendingChannel {
        ChannelDataPtr data;
        int pin;
        ChipsetTiming timing;
        uint32_t reset_us;
    };

    /// @brief Hash function for ChipsetTiming (encoder cache key)
    struct TimingHash {
        size_t operator()(const ChipsetTiming& t) const {
            // Simple hash combining all timing values
            // Use FNV-1a algorithm for better distribution
            size_t hash = 2166136261u;
            hash = (hash ^ t.T1) * 16777619u;
            hash = (hash ^ t.T2) * 16777619u;
            hash = (hash ^ t.T3) * 16777619u;
            hash = (hash ^ t.RESET) * 16777619u;
            return hash;
        }
    };

    /// @brief Equality function for ChipsetTiming (encoder cache key)
    struct TimingEqual {
        bool operator()(const ChipsetTiming& a, const ChipsetTiming& b) const {
            return a.T1 == b.T1 && a.T2 == b.T2 &&
                   a.T3 == b.T3 && a.RESET == b.RESET;
        }
    };

    /// @brief Acquire a channel for given pin and timing
    /// @return Pointer to channel state, or nullptr if no HW available
    ChannelState* acquireChannel(gpio_num_t pin, const ChipsetTiming& timing);

    /// @brief Release a channel (marks as available for reuse)
    void releaseChannel(ChannelState* channel);

    /// @brief Create new RMT channel with given configuration
    /// @return true if channel created successfully
    bool createChannel(ChannelState* state, gpio_num_t pin, const ChipsetTiming& timing);

    /// @brief Register ISR callback for channel (must be called AFTER ChannelState is in final location)
    /// @return true if callback registered successfully
    bool registerChannelCallback(ChannelState* state);

    /// @brief Configure existing channel (handle pin/timing changes)
    void configureChannel(ChannelState* state, gpio_num_t pin, const ChipsetTiming& timing);

    /// @brief Get or create cached encoder for timing configuration
    /// @return Encoder handle, or nullptr on failure
    rmt_encoder_handle_t getOrCreateEncoder(const ChipsetTiming& timing);

    /// @brief Process pending channels that couldn't be started earlier
    void processPendingChannels();

    /// @brief ISR callback for transmission completion
    static bool IRAM_ATTR transmitDoneCallback(rmt_channel_handle_t channel,
                                               const rmt_tx_done_event_data_t *edata,
                                               void *user_data);

    /// @brief All RMT channels (active and idle)
    fl::vector_inlined<ChannelState, 16> mChannels;

    /// @brief Pending channels waiting for available HW
    fl::vector_inlined<PendingChannel, 16> mPendingChannels;

    /// @brief Encoder cache: timing -> encoder handle (never deleted until shutdown)
    fl::hash_map<ChipsetTiming, rmt_encoder_handle_t, TimingHash, TimingEqual> mEncoderCache;
};

} // namespace fl

#endif // FASTLED_RMT5

#endif // ESP32
