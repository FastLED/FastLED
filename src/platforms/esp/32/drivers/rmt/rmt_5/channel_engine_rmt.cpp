/// @file channel_engine_rmt.cpp
/// @brief RMT5 ChannelEngine implementation

#ifdef ESP32

#include "fl/compiler_control.h"
#include "third_party/espressif/led_strip/src/enabled.h"

#if FASTLED_RMT5

#include "channel_engine_rmt.h"
#include "rmt5_encoder.h"
#include "common.h"
#include "fl/warn.h"
#include "fl/dbg.h"
#include "fl/chipsets/led_timing.h"
#include "ftl/algorithm.h"
#include "ftl/time.h"
#include "ftl/assert.h"
#include "fl/delay.h"
#include "fl/log.h"

FL_EXTERN_C_BEGIN
#include "driver/rmt_tx.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"  // For pdMS_TO_TICKS
FL_EXTERN_C_END

#define RMT_ENGINE_TAG "channel_engine_rmt"

namespace fl {

//=============================================================================
// Constructor / Destructor
//=============================================================================

ChannelEngineRMT::ChannelEngineRMT()
{
    // Register as listener for end frame events
    EngineEvents::addListener(this);
    FL_DBG("RMT Channel Engine initialized");
}

ChannelEngineRMT::~ChannelEngineRMT() {
    // Unregister from events
    EngineEvents::removeListener(this);

    // Wait for all active transmissions to complete
    while (pollDerived() == EngineState::BUSY) {
        fl::delayMicroseconds(100);
    }

    // Destroy all channels
    for (auto& ch : mChannels) {
        if (ch.channel) {
            rmt_tx_wait_all_done(ch.channel, pdMS_TO_TICKS(1000));
            rmt_disable(ch.channel);
            rmt_del_channel(ch.channel);
        }
    }
    mChannels.clear();

    // Destroy all cached encoders
    for (const auto& pair : mEncoderCache) {
        rmt_del_encoder(pair.second);
    }
    mEncoderCache.clear();

    FL_DBG("RMT Channel Engine destroyed");
}

//=============================================================================
// Public Interface
//=============================================================================

void ChannelEngineRMT::onEndFrame() {
    FL_WARN("ChannelEngineRMT::onEndFrame() - starting");
    show();
    int pollCount = 0;
    while (poll() == EngineState::BUSY) {
        pollCount++;
        if (pollCount % 100 == 0) {  // Log every 10ms (100 * 100us)
            FL_WARN("ChannelEngineRMT::onEndFrame() stuck in busy loop, poll count: " << pollCount);
        }
        fl::delayMicroseconds(100);
    }
    FL_WARN("ChannelEngineRMT::onEndFrame() - completed after " << pollCount << " polls");
}

void ChannelEngineRMT::beginTransmission(fl::span<const ChannelDataPtr> channelData) {
    if (channelData.size() == 0) {
        return;
    }
    FL_DBG("ChannelEngineRMT::beginTransmission() is running");

    // Set log level based on build type
#ifdef NDEBUG
    esp_log_level_set(RMT_ENGINE_TAG, ESP_LOG_INFO);
#else
    esp_log_level_set(RMT_ENGINE_TAG, ESP_LOG_VERBOSE);
#endif

    // Sort: smallest strips first (helps async parallelism)
    fl::vector_inlined<ChannelDataPtr, 16> sorted;
    for (const auto& data : channelData) {
        sorted.push_back(data);
    }
    fl::sort(sorted.begin(), sorted.end(), [](const ChannelDataPtr& a, const ChannelDataPtr& b) {
        return a->getSize() > b->getSize();  // Reverse order for back-to-front processing
    });

    // Queue all channels as pending first
    for (const auto& data : sorted) {
        int pin = data->getPin();
        const auto& timingCfg = data->getTiming();
        ChipsetTiming timing = {
            timingCfg.t1_ns,
            timingCfg.t2_ns,
            timingCfg.t3_ns,
            timingCfg.reset_us,
            timingCfg.name
        };
        mPendingChannels.push_back({data, pin, timing, timingCfg.reset_us});
    }

    // Start as many transmissions as HW channels allow
    processPendingChannels();
}

ChannelEngine::EngineState ChannelEngineRMT::pollDerived() {
    bool anyActive = false;
    int activeCount = 0;
    int completedCount = 0;

    // Check each channel for completion
    for (auto& ch : mChannels) {
        if (!ch.inUse) {
            continue;
        }

        activeCount++;
        anyActive = true;

        if (ch.transmissionComplete) {
            completedCount++;
            FL_WARN("Channel on pin " << ch.pin << " completed transmission");

            // Disable channel to release HW resources
            if (ch.channel) {
                esp_err_t err = rmt_disable(ch.channel);
                if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
                    FL_WARN("Failed to disable channel: " << err);
                }
            }

            // Release channel back to pool
            FL_WARN("Releasing channel " << ch.pin);
            releaseChannel(&ch);

            // Try to start pending channels
            processPendingChannels();
        } else {
            FL_WARN("Channel on pin " << ch.pin << " still transmitting (inUse=true, complete=false)");
        }
    }

    // Check if any pending channels remain
    if (!mPendingChannels.empty()) {
        anyActive = true;
        FL_WARN("Pending channels: " << mPendingChannels.size());
    } else if (activeCount > 0) {
        FL_WARN("No pending channels, but " << activeCount << " active channels (" << completedCount << " completed)");
    }

    return anyActive ? EngineState::BUSY : EngineState::READY;
}

//=============================================================================
// Encoder Cache
//=============================================================================

rmt_encoder_handle_t ChannelEngineRMT::getOrCreateEncoder(const ChipsetTiming& timing) {
    // Look up in cache
    auto it = mEncoderCache.find(timing);
    if (it != mEncoderCache.end()) {
        FL_DBG("Reusing cached encoder for timing: " << timing.name);
        return it->second;
    }

    // Create new encoder
    rmt_encoder_handle_t encoder;
    esp_err_t err = fastled_rmt_new_encoder(timing, FASTLED_RMT5_CLOCK_HZ, &encoder);
    if (err != ESP_OK) {
        FL_WARN("Failed to create encoder: " << esp_err_to_name(err));
        return nullptr;
    }

    // Cache it forever (never deleted until shutdown)
    mEncoderCache[timing] = encoder;
    FL_DBG("Created and cached encoder for timing: " << timing.name);
    return encoder;
}

//=============================================================================
// Channel Management
//=============================================================================

ChannelEngineRMT::ChannelState* ChannelEngineRMT::acquireChannel(gpio_num_t pin, const ChipsetTiming& timing) {
    // Strategy 1: Find channel with matching pin (zero-cost reuse)
    for (auto& ch : mChannels) {
        if (!ch.inUse && ch.channel && ch.pin == pin) {
            ch.inUse = true;
            configureChannel(&ch, pin, timing);
            FL_DBG("Reusing channel for pin " << static_cast<int>(pin));
            return &ch;
        }
    }

    // Strategy 2: Find any idle channel (requires reconfiguration)
    for (auto& ch : mChannels) {
        if (!ch.inUse && ch.channel) {
            ch.inUse = true;
            configureChannel(&ch, pin, timing);
            FL_DBG("Reconfiguring idle channel for pin " << static_cast<int>(pin));
            return &ch;
        }
    }

    // Strategy 3: Create new channel if HW available
    ChannelState newCh = {};
    if (createChannel(&newCh, pin, timing)) {
        newCh.inUse = true;
        mChannels.push_back(newCh);

        // CRITICAL: Register callback AFTER pushing to vector to ensure stable pointer
        ChannelState* stablePtr = &mChannels.back();
        if (!registerChannelCallback(stablePtr)) {
            FL_WARN("Failed to register callback for new channel");
            rmt_del_channel(stablePtr->channel);
            mChannels.pop_back();
            return nullptr;
        }

        FL_DBG("Created new channel for pin " << static_cast<int>(pin)
               << " (total: " << mChannels.size() << ")");
        return stablePtr;
    }

    // No HW channels available
    return nullptr;
}

void ChannelEngineRMT::releaseChannel(ChannelState* channel) {
    FL_ASSERT(channel != nullptr, "releaseChannel called with nullptr");
    channel->inUse = false;
    channel->transmissionComplete = false;
    // NOTE: Keep channel and encoder alive for reuse
}

bool ChannelEngineRMT::createChannel(ChannelState* state, gpio_num_t pin, const ChipsetTiming& timing) {
    rmt_tx_channel_config_t tx_config = {};
    tx_config.gpio_num = pin;
    tx_config.clk_src = RMT_CLK_SRC_DEFAULT;
    tx_config.resolution_hz = FASTLED_RMT5_CLOCK_HZ;
    tx_config.mem_block_symbols = FASTLED_RMT5_MAX_PULSES;
    tx_config.trans_queue_depth = 1;
    tx_config.flags.invert_out = 0;
    tx_config.flags.with_dma = 0;

    esp_err_t err = rmt_new_tx_channel(&tx_config, &state->channel);
    if (err != ESP_OK) {
        FL_WARN("Failed to create RMT channel on pin " << static_cast<int>(pin)
                << ": " << esp_err_to_name(err));
        state->channel = nullptr;
        return false;
    }

    // NOTE: Callback registration moved to registerChannelCallback()
    // to avoid dangling pointer issues when ChannelState is copied

    state->pin = pin;
    state->timing = timing;
    state->transmissionComplete = false;
    FL_DBG("RMT channel created on GPIO " << static_cast<int>(pin));
    return true;
}

bool ChannelEngineRMT::registerChannelCallback(ChannelState* state) {
    FL_ASSERT(state != nullptr, "registerChannelCallback called with nullptr");
    FL_ASSERT(state->channel != nullptr, "registerChannelCallback called with null channel");

    // Register transmission completion callback
    // CRITICAL: state pointer must be stable (not on stack, not subject to vector reallocation)
    rmt_tx_event_callbacks_t cbs = {};
    cbs.on_trans_done = transmitDoneCallback;
    esp_err_t err = rmt_tx_register_event_callbacks(state->channel, &cbs, state);
    if (err != ESP_OK) {
        FL_WARN("Failed to register callbacks: " << esp_err_to_name(err));
        return false;
    }

    FL_DBG("Registered callback for channel on GPIO " << static_cast<int>(state->pin));
    return true;
}

void ChannelEngineRMT::configureChannel(ChannelState* state, gpio_num_t pin, const ChipsetTiming& timing) {
    // If pin changed, destroy and recreate channel
    if (state->channel && state->pin != pin) {
        FL_DBG("Pin changed from " << static_cast<int>(state->pin)
               << " to " << static_cast<int>(pin) << ", recreating channel");
        rmt_tx_wait_all_done(state->channel, pdMS_TO_TICKS(100));
        rmt_disable(state->channel);
        rmt_del_channel(state->channel);
        state->channel = nullptr;
    }

    // Create channel if needed
    if (!state->channel) {
        if (!createChannel(state, pin, timing)) {
            FL_WARN("Failed to recreate channel for pin " << static_cast<int>(pin));
            return;
        }

        // Register callback after creation (state pointer is already stable here)
        if (!registerChannelCallback(state)) {
            FL_WARN("Failed to register callback after reconfiguration");
            rmt_del_channel(state->channel);
            state->channel = nullptr;
            return;
        }
    }

    // Update timing (encoder will be fetched from cache)
    state->timing = timing;
    state->transmissionComplete = false;
}

//=============================================================================
// Pending Channel Processing
//=============================================================================

void ChannelEngineRMT::processPendingChannels() {
    if (mPendingChannels.empty()) {
        return;
    }

    // Try to start pending channels
    for (size_t i = 0; i < mPendingChannels.size(); ) {
        const auto& pending = mPendingChannels[i];

        // Acquire channel for this transmission
        ChannelState* channel = acquireChannel(static_cast<gpio_num_t>(pending.pin), pending.timing);
        if (!channel) {
            ++i;  // No HW available, leave in queue
            continue;
        }

        // Get cached encoder (or create new one)
        rmt_encoder_handle_t encoder = getOrCreateEncoder(pending.timing);
        if (!encoder) {
            FL_WARN("Failed to get encoder for pin " << pending.pin);
            releaseChannel(channel);
            ++i;
            continue;
        }

        // Start transmission
        channel->reset_us = pending.reset_us;
        channel->transmissionComplete = false;

        esp_err_t err = rmt_enable(channel->channel);
        if (err != ESP_OK) {
            FL_WARN("Failed to enable channel: " << esp_err_to_name(err));
            releaseChannel(channel);
            ++i;
            continue;
        }

        // Explicitly reset encoder before each transmission to ensure clean state
        err = encoder->reset(encoder);
        if (err != ESP_OK) {
            FL_WARN("Failed to reset encoder: " << esp_err_to_name(err));
            rmt_disable(channel->channel);
            releaseChannel(channel);
            ++i;
            continue;
        }

        rmt_transmit_config_t tx_config = {};
        tx_config.loop_count = 0;
        err = rmt_transmit(channel->channel, encoder,
                          pending.data->getData().data(),
                          pending.data->getSize(),
                          &tx_config);
        if (err != ESP_OK) {
            FL_WARN("Failed to transmit: " << esp_err_to_name(err));
            rmt_disable(channel->channel);
            releaseChannel(channel);
            ++i;
            continue;
        }

        ESP_LOGD(RMT_ENGINE_TAG, "Started transmission for pin %d (%zu bytes)",
                 pending.pin, static_cast<size_t>(pending.data->getSize()));

        // Remove from pending queue (swap with last and pop)
        if (i < mPendingChannels.size() - 1) {
            mPendingChannels[i] = mPendingChannels[mPendingChannels.size() - 1];
        }
        mPendingChannels.pop_back();
        // Don't increment i - we just moved a new element here
    }
}

//=============================================================================
// ISR Callback
//=============================================================================

bool IRAM_ATTR ChannelEngineRMT::transmitDoneCallback(
    rmt_channel_handle_t channel,
    const rmt_tx_done_event_data_t *edata,
    void *user_data)
{
    ChannelState* state = static_cast<ChannelState*>(user_data);
    if (!state) {
        // ISR callback received null user_data - this is a bug
        return false;
    }

    // Mark transmission as complete (polled by main thread)
    state->transmissionComplete = true;

    // Non-blocking design - no semaphore signal needed
    return false;  // No task switch needed
}

} // namespace fl

#endif // FASTLED_RMT5

#endif // ESP32
