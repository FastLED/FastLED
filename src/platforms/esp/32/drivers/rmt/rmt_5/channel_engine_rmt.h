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
#include "buffer_pool.h"

FL_EXTERN_C_BEGIN
#include "driver/rmt_tx.h"
#include "driver/gpio.h"
FL_EXTERN_C_END

namespace fl {

/// @brief DMA availability state (learned at runtime)
enum class DMAState {
    UNKNOWN,      ///< Haven't attempted DMA channel creation yet
    AVAILABLE,    ///< DMA successfully created (ESP32-S3)
    UNAVAILABLE   ///< DMA creation failed - hardware limitation (ESP32-C3/C6/H2)
};

/// @brief RMT5-based ChannelEngine implementation
///
/// Consolidates all RMT functionality:
/// - Direct channel management (no worker abstraction)
/// - Encoder caching by timing configuration (never deleted)
/// - Channel persistence between frames (avoid recreation overhead)
/// - On-demand HW channel allocation with sequential reuse
/// - Runtime DMA detection with graceful fallback
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
        bool useDMA;  // Whether this channel uses DMA
        uint32_t reset_us;
        fl::span<uint8_t> pooledBuffer;  // Buffer acquired from pool (must be released on complete)
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
    /// @param dataSize Size of LED data in bytes (0 = use default buffer size)
    /// @return Pointer to channel state, or nullptr if no HW available
    ChannelState* acquireChannel(gpio_num_t pin, const ChipsetTiming& timing, fl::size dataSize = 0);

    /// @brief Release a channel (marks as available for reuse)
    void releaseChannel(ChannelState* channel);

    /// @brief Create new RMT channel with given configuration
    /// @param dataSize Size of LED data in bytes (0 = use default buffer size)
    /// @return true if channel created successfully
    bool createChannel(ChannelState* state, gpio_num_t pin, const ChipsetTiming& timing, fl::size dataSize = 0);

    /// @brief Register ISR callback for channel (must be called AFTER ChannelState is in final location)
    /// @return true if callback registered successfully
    bool registerChannelCallback(ChannelState* state);

    /// @brief Configure existing channel (handle pin/timing changes)
    /// @param dataSize Size of LED data in bytes (0 = use default buffer size)
    void configureChannel(ChannelState* state, gpio_num_t pin, const ChipsetTiming& timing, fl::size dataSize = 0);

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

    /// @brief Buffer pool for PSRAM -> DRAM/DMA memory transfer
    RMTBufferPool mBufferPool;

    /// @brief Track DMA channel usage (only 1 allowed on ESP32-S3)
    int mDMAChannelsInUse;

    /// @brief Track allocation failures to avoid hammering the driver
    bool mAllocationFailed;

    /// @brief Track last frame number to allow retry once per frame
    uint32_t mLastRetryFrame;

    /// @brief Runtime DMA availability state (shared across all instances, learned on first attempt - lazy)
    static DMAState sDMAAvailability;
};

} // namespace fl

#endif // FASTLED_RMT5

#endif // ESP32
