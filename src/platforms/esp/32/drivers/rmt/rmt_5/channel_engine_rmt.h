/// @file channel_engine_rmt.h
/// @brief RMT5 implementation of ChannelEngine for ESP32

#pragma once

#include "fl/compiler_control.h"
#ifdef ESP32

#include "platforms/esp/32/feature_flags/enabled.h"

#if FASTLED_RMT5  // Must check BEFORE including any RMT5 headers to prevent symbol conflicts

#include "fl/channels/channel_engine.h"
#include "fl/channels/channel_data.h"
#include "fl/chipsets/led_timing.h"
#include "fl/engine_events.h"
#include "ftl/span.h"
#include "ftl/vector.h"
#include "ftl/time.h"
#include "fl/timeout.h"
#include "buffer_pool.h"

FL_EXTERN_C_BEGIN
#include "driver/rmt_tx.h"
#include "driver/gpio.h"
FL_EXTERN_C_END

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
class ChannelEngineRMT : public IChannelEngine {
public:
    ChannelEngineRMT();
    ~ChannelEngineRMT() override;

    /// @brief Enqueue channel data for transmission
    /// @param channelData Channel data to transmit
    void enqueue(ChannelDataPtr channelData) override;

    /// @brief Trigger transmission of enqueued data
    void show() override;

    /// @brief Query engine state and perform maintenance
    /// @return Current engine state (READY, BUSY, DRAINING, or ERROR)
    EngineState poll() override;

private:
    /// @brief Begin LED data transmission for all channels (internal)
    /// @param channelData Span of channel data to transmit
    void beginTransmission(fl::span<const ChannelDataPtr> channelData);

    /// @brief RMT channel state (replaces RmtWorkerSimple)
    struct ChannelState {
        rmt_channel_handle_t channel;
        rmt_encoder_handle_t encoder;  // Per-channel encoder (prevents race conditions)
        gpio_num_t pin;
        ChipsetTiming timing;
        volatile bool transmissionComplete;
        bool inUse;
        bool useDMA;  // Whether this channel uses DMA
        uint32_t reset_us;
        fl::span<uint8_t> pooledBuffer;  // Buffer acquired from pool (must be released on complete)
        uint8_t memoryChannelId;  // Virtual channel ID for memory manager accounting (vector index)
    };

    /// @brief Pending channel data to be transmitted when HW channels become available
    struct PendingChannel {
        ChannelDataPtr data;
        int pin;
        ChipsetTiming timing;
        uint32_t reset_us;
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

    /// @brief Process pending channels that couldn't be started earlier
    void processPendingChannels();

    /// @brief Destroy a single channel and free resources
    /// @param state Channel state to destroy (must not be in use)
    void destroyChannel(ChannelState* state);

    /// @brief Destroy least-used channels to free resources
    /// @param count Number of channels to destroy (from end of mChannels vector)
    void destroyLeastUsedChannels(size_t count);

    /// @brief Calculate target channel count based on network state and platform
    /// @param networkActive Whether any network (WiFi, Ethernet, or Bluetooth) is currently active
    /// @return Target number of channels for current state
    size_t calculateTargetChannelCount(bool networkActive);

    /// @brief Reconfigure channels for network state change (destroy/recreate as needed)
    /// @param networkActive Whether any network (WiFi, Ethernet, or Bluetooth) is currently active
    void reconfigureForNetwork(bool networkActive);

    /// @brief ISR callback for transmission completion
    static bool IRAM_ATTR transmitDoneCallback(rmt_channel_handle_t channel,
                                               const rmt_tx_done_event_data_t *edata,
                                               void *user_data);

    /// @brief All RMT channels (active and idle)
    fl::vector_inlined<ChannelState, 16> mChannels;

    /// @brief Pending channel data waiting for show() to be called
    fl::vector_inlined<ChannelDataPtr, 16> mEnqueuedChannels;

    /// @brief Pending channels waiting for available HW (after show() called)
    fl::vector_inlined<PendingChannel, 16> mPendingChannels;

    /// @brief Channels currently being transmitted (for cleanup on poll())
    fl::vector_inlined<ChannelDataPtr, 16> mTransmittingChannels;

    /// @brief Buffer pool for PSRAM -> DRAM/DMA memory transfer
    RMTBufferPool mBufferPool;

    /// @brief Track DMA channel usage
    ///
    /// ESP32-S3 Hardware Limitation: Only 1 RMT DMA channel available
    /// - mDMAChannelsInUse == 0: DMA available (first channel)
    /// - mDMAChannelsInUse >= 1: DMA exhausted (all subsequent channels use non-DMA)
    ///
    /// This counter is incremented when a DMA channel is successfully created
    /// and decremented when a DMA channel is destroyed.
    int mDMAChannelsInUse;

    /// @brief Track allocation failures to avoid hammering the driver
    bool mAllocationFailed;

    /// @brief Track last known network state for change detection
    bool mLastKnownNetworkState;
};

} // namespace fl

#endif // FASTLED_RMT5

#endif // ESP32
