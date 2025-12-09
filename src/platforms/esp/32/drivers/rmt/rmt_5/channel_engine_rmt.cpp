/// @file channel_engine_rmt.cpp
/// @brief RMT5 ChannelEngine implementation
///
/// To enable RMT operational logging (channel creation, queueing, transmission):
///   #define FASTLED_LOG_RMT_ENABLED
///
/// RMT logging uses FL_LOG_RMT which is compile-time controlled via fl/log.h.
/// When disabled (default), FL_LOG_RMT produces no code overhead (zero-cost abstraction).

#ifdef ESP32

#include "fl/compiler_control.h"
#include "platforms/esp/32/feature_flags/enabled.h"

#if FASTLED_RMT5

#include "channel_engine_rmt.h"
#include "rmt5_encoder.h"
#include "rmt_memory_manager.h"
#include "common.h"
#include "wifi_detector.h"
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

namespace fl {

//=============================================================================
// Static member initialization
//=============================================================================
// (None required)

//=============================================================================
// Constructor / Destructor
//=============================================================================

ChannelEngineRMT::ChannelEngineRMT()
    : mDMAChannelsInUse(0)
    , mAllocationFailed(false)
    , mLastKnownWiFiState(false)
{
    // Suppress ESP-IDF RMT "no free channels" errors (expected during time-multiplexing)
    // Only show critical RMT errors (ESP_LOG_ERROR and above)
    esp_log_level_set("rmt", ESP_LOG_NONE);

    FL_LOG_RMT("RMT Channel Engine initialized");
}

ChannelEngineRMT::~ChannelEngineRMT() {
    // Wait for all active transmissions to complete
    while (poll() == EngineState::BUSY) {
        fl::delayMicroseconds(100);
    }

    // Get memory manager reference
    auto& memMgr = RmtMemoryManager::instance();

    // Destroy all channels
    for (auto& ch : mChannels) {
        if (ch.channel) {
            rmt_tx_wait_all_done(ch.channel, pdMS_TO_TICKS(1000));
            rmt_disable(ch.channel);
            rmt_del_channel(ch.channel);

            // Free DMA if this channel was using it
            if (ch.useDMA) {
                memMgr.freeDMA(ch.memoryChannelId, true);  // true = TX channel
                mDMAChannelsInUse--;
            }

            // Free memory from memory manager
            memMgr.free(ch.memoryChannelId, true);  // true = TX channel
        }

        // Delete per-channel encoder
        if (ch.encoder) {
            rmt_del_encoder(ch.encoder);
            ch.encoder = nullptr;
        }
    }
    mChannels.clear();

    FL_LOG_RMT("RMT Channel Engine destroyed");
}

//=============================================================================
// IChannelEngine Interface Implementation
//=============================================================================

void ChannelEngineRMT::enqueue(ChannelDataPtr channelData) {
    mEnqueuedChannels.push_back(channelData);
}

void ChannelEngineRMT::show() {
    if (mEnqueuedChannels.empty()) {
        return;
    }
    FL_ASSERT(mTransmittingChannels.empty(), "ChannelEngineRMT: Cannot show() while channels are still transmitting");
    FL_ASSERT(poll() == EngineState::READY, "ChannelEngineRMT: Cannot show() while hardware is busy");

    // Mark all channels as in use before transmission
    for (auto& channel : mEnqueuedChannels) {
        channel->setInUse(true);
    }

    // Create a const span from the enqueued channels and pass to beginTransmission
    fl::span<const ChannelDataPtr> channelSpan(mEnqueuedChannels.data(), mEnqueuedChannels.size());
    beginTransmission(channelSpan);

    // Move enqueued channels to transmitting list (async operation started)
    // Don't clear mInUse flags yet - poll() will do that when transmission completes
    mTransmittingChannels = fl::move(mEnqueuedChannels);
}

ChannelEngineRMT::EngineState ChannelEngineRMT::poll() {
    // Check hardware status
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
            FL_LOG_RMT("Channel on pin " << ch.pin << " completed transmission");

            // Disable channel to release HW resources
            if (ch.channel) {
                esp_err_t err = rmt_disable(ch.channel);
                if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
                    FL_LOG_RMT("Failed to disable channel: " << err);
                }
            }

            // Release channel back to pool
            FL_LOG_RMT("Releasing channel " << ch.pin);
            releaseChannel(&ch);

            // Try to start pending channels
            processPendingChannels();
        } else {
            FL_LOG_RMT("Channel on pin " << ch.pin << " still transmitting (inUse=true, complete=false)");
        }
    }

    // Check if any pending channels remain
    if (!mPendingChannels.empty()) {
        anyActive = true;
        FL_LOG_RMT("Pending channels: " << mPendingChannels.size());
    } else if (activeCount > 0) {
        FL_LOG_RMT("No pending channels, but " << activeCount << " active channels (" << completedCount << " completed)");
    }

    // When transmission completes (READY or ERROR), clear the "in use" flags
    // and release the transmitted channels
    EngineState state = anyActive ? EngineState::BUSY : EngineState::READY;
    if (state == EngineState::READY) {
        for (auto& channel : mTransmittingChannels) {
            channel->setInUse(false);
        }
        mTransmittingChannels.clear();
    }

    return state;
}

//=============================================================================
// Internal Transmission Logic
//=============================================================================

void ChannelEngineRMT::beginTransmission(fl::span<const ChannelDataPtr> channelData) {
    if (channelData.size() == 0) {
        FL_LOG_RMT("beginTransmission: No channels to transmit");
        return;
    }
    FL_LOG_RMT("ChannelEngineRMT::beginTransmission() is running");

    // WiFi-aware channel reconfiguration (once per frame)
    #if FASTLED_RMT_WIFI_REDUCE_CHANNELS
        bool wifiActive = WiFiDetector::isWiFiActive();
        reconfigureForWiFi(wifiActive);
    #endif

    // Reset allocation failure flag at start of each frame to allow retry
    if (mAllocationFailed) {
        FL_LOG_RMT("Resetting allocation failure flag (retry at start of frame)");
        mAllocationFailed = false;
    }

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

//=============================================================================
// Channel Management
//=============================================================================

ChannelEngineRMT::ChannelState* ChannelEngineRMT::acquireChannel(gpio_num_t pin, const ChipsetTiming& timing, fl::size dataSize) {
    // Strategy 1: Find channel with matching pin (zero-cost reuse)
    // This applies to both DMA and non-DMA channels
    FL_LOG_RMT("acquireChannel: Finding channel with matching pin " << static_cast<int>(pin));
    for (auto& ch : mChannels) {
        if (!ch.inUse && ch.channel && ch.pin == pin) {
            ch.inUse = true;
            configureChannel(&ch, pin, timing, dataSize);
            FL_LOG_RMT("Reusing " << (ch.useDMA ? "DMA" : "non-DMA") << " channel for pin " << static_cast<int>(pin));
            return &ch;
        }
    }

    // Strategy 2: Find any idle non-DMA channel (requires reconfiguration)
    for (auto& ch : mChannels) {
        if (!ch.inUse && ch.channel && !ch.useDMA) {
            ch.inUse = true;
            configureChannel(&ch, pin, timing, dataSize);
            FL_LOG_RMT("Reconfiguring idle non-DMA channel for pin " << static_cast<int>(pin));
            return &ch;
        }
    }

    // Strategy 3: Create new channel if HW available
    // BUT: Skip if allocation previously failed (reset at start of next frame in beginTransmission)
    if (mAllocationFailed) {
        FL_LOG_RMT("Skipping channel creation (allocation failed, will retry next frame)");
        return nullptr;
    }

    ChannelState newCh = {};
    if (createChannel(&newCh, pin, timing, dataSize)) {
        newCh.inUse = true;
        mChannels.push_back(newCh);

        // CRITICAL: Register callback AFTER pushing to vector to ensure stable pointer
        ChannelState* stablePtr = &mChannels.back();
        if (!registerChannelCallback(stablePtr)) {
            FL_WARN("Failed to register callback for new channel");
            if (stablePtr->useDMA) {
                mDMAChannelsInUse--;
            }
            rmt_del_channel(stablePtr->channel);
            mChannels.pop_back();
            mAllocationFailed = true;  // Mark failure
            return nullptr;
        }

        FL_LOG_RMT("Created new channel for pin " << static_cast<int>(pin)
               << " (total: " << mChannels.size() << ")");
        return stablePtr;
    }

    // No HW channels available - mark allocation failed
    FL_LOG_RMT("Channel allocation failed - max channels reached");
    mAllocationFailed = true;
    return nullptr;
}

void ChannelEngineRMT::releaseChannel(ChannelState* channel) {
    FL_ASSERT(channel != nullptr, "releaseChannel called with nullptr");

    // CRITICAL: Wait for RMT hardware to fully complete transmission
    // The ISR callback (transmitDoneCallback) may fire slightly before the last bits
    // have fully propagated out of the RMT peripheral. We must ensure hardware is
    // truly done before allowing buffer reuse to prevent race conditions.
    esp_err_t wait_result = rmt_tx_wait_all_done(channel->channel, pdMS_TO_TICKS(100));
    FL_ASSERT(wait_result == ESP_OK, "RMT transmission wait timeout - hardware may be stalled");

    // Release pooled buffer if one was acquired
    if (!channel->pooledBuffer.empty()) {
        if (channel->useDMA) {
            mBufferPool.releaseDMA();
        } else {
            mBufferPool.releaseInternal(channel->pooledBuffer);
        }
        channel->pooledBuffer = fl::span<uint8_t>();
    }

    channel->inUse = false;
    channel->transmissionComplete = false;
    // NOTE: Keep channel and encoder alive for reuse
}

bool ChannelEngineRMT::createChannel(ChannelState* state, gpio_num_t pin, const ChipsetTiming& timing, fl::size dataSize) {
    // ============================================================================
    // RMT5 MEMORY MANAGEMENT - Now using centralized RmtMemoryManager
    // ============================================================================
    // Memory allocation policy:
    // - TX channels: Always double-buffer (2× SOC_RMT_MEM_WORDS_PER_CHANNEL)
    // - DMA channels: Bypass on-chip memory (allocated from DRAM instead)
    // - RX channels: User-specified size (managed separately in RmtRxChannel)
    //
    // The RmtMemoryManager tracks all allocations to prevent over-allocation and
    // coordinates memory usage between TX and RX channels.
    // ============================================================================

    // Set memory channel ID (will be the index in mChannels vector after push_back)
    state->memoryChannelId = static_cast<uint8_t>(mChannels.size());

    // Get memory manager reference
    auto& memMgr = RmtMemoryManager::instance();

    // Get current WiFi state for memory allocation
    bool wifiActive = WiFiDetector::isWiFiActive();

    // ============================================================================
    // DMA ALLOCATION POLICY - ESP32-S3 First Channel Only (TX or RX)
    // ============================================================================
    // ESP32-S3 has ONLY ONE RMT DMA channel (hardware limitation).
    // This DMA channel is SHARED between TX and RX channels.
    //
    // Allocation priority:
    //   1. FIRST channel created (TX or RX): Uses DMA (if data size > 0)
    //   2. ALL subsequent channels: Use non-DMA (on-chip double-buffer)
    //
    // DMA allocation is managed centrally by RmtMemoryManager to coordinate
    // between TX (ChannelEngineRMT) and RX (RmtRxChannel) drivers.
    // ============================================================================
    bool tryDMA = false;
#if FASTLED_RMT5_DMA_SUPPORTED
    // Platform supports DMA (ESP32-S3 only)
    // Check with memory manager if DMA is available (shared TX/RX resource)
    if (memMgr.isDMAAvailable()) {
        // DMA slot available - first channel across TX/RX
        tryDMA = true;
        FL_LOG_RMT("TX Channel #" << (mChannels.size() + 1) << ": DMA slot available for pin "
                   << static_cast<int>(pin) << " (data size: " << dataSize << " bytes)");
    } else {
        // DMA exhausted (could be taken by TX or RX channel)
        FL_LOG_RMT("TX Channel #" << (mChannels.size() + 1) << ": DMA slot taken (limit: "
                   << FASTLED_RMT5_MAX_DMA_CHANNELS << "), using non-DMA for pin " << static_cast<int>(pin));
    }
#else
    // Platform does not support RMT DMA (ESP32, S2, C3, C6, H2)
    FL_LOG_RMT("TX Channel #" << (mChannels.size() + 1) << ": Platform does not support RMT DMA, using non-DMA for pin "
               << static_cast<int>(pin));
#endif

    // STEP 1: Try DMA channel creation (first channel only on ESP32-S3)
    if (tryDMA && dataSize > 0) {
        // Allocate memory from memory manager (DMA bypasses on-chip memory)
        auto alloc_result = memMgr.allocateTx(state->memoryChannelId, true, wifiActive);  // true = use DMA
        if (!alloc_result.ok()) {
            FL_WARN("Memory manager TX allocation failed for DMA channel " << static_cast<int>(state->memoryChannelId));
            return false;
        }

        // DMA mode: allocate enough symbols for the entire LED strip
        // Each byte needs 8 RMT symbols (1 symbol per bit)
        // Add extra space for reset pulse (typically 1 symbol, but use 16 for safety)
        fl::size dma_mem_block_symbols = (dataSize * 8) + 16;
        FL_LOG_RMT("DMA allocation: " << dma_mem_block_symbols << " symbols for "
                   << dataSize << " bytes (" << (dataSize/3) << " LEDs)");

        // Select interrupt priority based on WiFi state
        // WiFi interrupts run at level 4 - boost RMT to level 5 on supported platforms
        int intr_priority = FL_RMT5_INTERRUPT_LEVEL;
#if FASTLED_RMT_WIFI_PRIORITY_BOOST
        if (wifiActive) {
            intr_priority = FL_RMT5_INTERRUPT_LEVEL_WIFI_MODE;
            FL_LOG_RMT("WiFi active: Boosting RMT interrupt priority to level " << intr_priority);
        }
#endif

        rmt_tx_channel_config_t dma_config = {};
        dma_config.gpio_num = pin;
        dma_config.clk_src = RMT_CLK_SRC_DEFAULT;
        dma_config.resolution_hz = FASTLED_RMT5_CLOCK_HZ;
        dma_config.mem_block_symbols = dma_mem_block_symbols;
        dma_config.trans_queue_depth = 1;
        dma_config.flags.invert_out = 0;
        dma_config.flags.with_dma = 1;  // Enable DMA
        dma_config.intr_priority = intr_priority;

        esp_err_t dma_err = rmt_new_tx_channel(&dma_config, &state->channel);
        if (dma_err == ESP_OK) {
            // DMA SUCCESS - claim DMA slot in memory manager
            if (!memMgr.allocateDMA(state->memoryChannelId, true)) {  // true = TX channel
                FL_WARN("DMA hardware creation succeeded but memory manager allocation failed");
                rmt_del_channel(state->channel);
                state->channel = nullptr;
                memMgr.free(state->memoryChannelId, true);
                return false;
            }

            mDMAChannelsInUse++;
            state->pin = pin;
            state->timing = timing;
            state->useDMA = true;
            state->transmissionComplete = false;

            // Create encoder for this DMA channel
            esp_err_t enc_err = fastled_rmt_new_encoder(timing, FASTLED_RMT5_CLOCK_HZ, &state->encoder);
            if (enc_err != ESP_OK) {
                FL_WARN("Failed to create encoder for DMA channel: " << esp_err_to_name(enc_err));
                rmt_del_channel(state->channel);
                state->channel = nullptr;
                mDMAChannelsInUse--;
                // Free DMA and memory allocation
                memMgr.freeDMA(state->memoryChannelId, true);
                memMgr.free(state->memoryChannelId, true);
                return false;
            }

            FL_LOG_RMT("✓ TX Channel #" << (mChannels.size() + 1) << ": DMA enabled on GPIO " << static_cast<int>(pin)
                       << " (" << dma_mem_block_symbols << " symbols)");
            return true;
        } else {
            // DMA FAILED - free memory and fall through to non-DMA
            // Free memory allocation
            memMgr.free(state->memoryChannelId, true);
            FL_WARN("DMA channel creation failed: " << esp_err_to_name(dma_err)
                    << " - unexpected failure on DMA-capable platform, falling back to non-DMA");
        }
    }

    // STEP 2: Create non-DMA channel (either DMA not attempted, failed, or disabled)
    // Allocate memory from memory manager (double-buffer policy)
    auto alloc_result = memMgr.allocateTx(state->memoryChannelId, false, wifiActive);  // false = non-DMA
    if (!alloc_result.ok()) {
        FL_WARN("Memory manager TX allocation failed for channel " << static_cast<int>(state->memoryChannelId)
                << " - insufficient on-chip memory");
        return false;
    }

    fl::size mem_block_symbols = alloc_result.value();

    // Log channel number and allocation type
    size_t channel_num = mChannels.size() + 1;
    if (memMgr.getDMAChannelsInUse() > 0) {
        FL_LOG_RMT("✓ TX Channel #" << channel_num << ": Non-DMA (double-buffer: " << mem_block_symbols
                   << " words) - DMA slot taken by another channel");
    } else {
        FL_LOG_RMT("✓ TX Channel #" << channel_num << ": Non-DMA (double-buffer: " << mem_block_symbols
                   << " words) - No DMA support on platform");
    }

    // Select interrupt priority based on WiFi state
    // WiFi interrupts run at level 4 - boost RMT to level 5 on supported platforms
    int intr_priority = FL_RMT5_INTERRUPT_LEVEL;
#if FASTLED_RMT_WIFI_PRIORITY_BOOST
    if (wifiActive) {
        intr_priority = FL_RMT5_INTERRUPT_LEVEL_WIFI_MODE;
        FL_LOG_RMT("WiFi active: Boosting RMT interrupt priority to level " << intr_priority);
    }
#endif

    rmt_tx_channel_config_t tx_config = {};
    tx_config.gpio_num = pin;
    tx_config.clk_src = RMT_CLK_SRC_DEFAULT;
    tx_config.resolution_hz = FASTLED_RMT5_CLOCK_HZ;
    tx_config.mem_block_symbols = mem_block_symbols;
    tx_config.trans_queue_depth = 1;
    tx_config.flags.invert_out = 0;
    tx_config.flags.with_dma = 0;  // Non-DMA
    tx_config.intr_priority = intr_priority;

    esp_err_t err = rmt_new_tx_channel(&tx_config, &state->channel);
    if (err != ESP_OK) {
        // This is expected when all HW channels are in use (time-multiplexing scenario)
        FL_LOG_RMT("Failed to create non-DMA RMT channel on pin " << static_cast<int>(pin)
                << ": " << esp_err_to_name(err));
        state->channel = nullptr;
        // Free memory allocation
        memMgr.free(state->memoryChannelId, true);
        return false;
    }

    // NOTE: Callback registration moved to registerChannelCallback()
    // to avoid dangling pointer issues when ChannelState is copied

    state->pin = pin;
    state->timing = timing;
    state->useDMA = false;  // Non-DMA channel
    state->transmissionComplete = false;

    // Create encoder for this channel
    esp_err_t enc_err = fastled_rmt_new_encoder(timing, FASTLED_RMT5_CLOCK_HZ, &state->encoder);
    if (enc_err != ESP_OK) {
        FL_WARN("Failed to create encoder for channel: " << esp_err_to_name(enc_err));
        rmt_del_channel(state->channel);
        state->channel = nullptr;
        // Free memory allocation
        memMgr.free(state->memoryChannelId, true);
        return false;
    }

    FL_LOG_RMT("Non-DMA RMT channel created on GPIO " << static_cast<int>(pin)
               << " (" << mem_block_symbols << " symbols) with dedicated encoder");
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

    FL_LOG_RMT("Registered callback for channel on GPIO " << static_cast<int>(state->pin));
    return true;
}

void ChannelEngineRMT::configureChannel(ChannelState* state, gpio_num_t pin, const ChipsetTiming& timing, fl::size dataSize) {
    // If pin changed, destroy and recreate channel
    if (state->channel && state->pin != pin) {
        FL_LOG_RMT("Pin changed from " << static_cast<int>(state->pin)
               << " to " << static_cast<int>(pin) << ", recreating channel");

        // Wait for any pending transmission to complete
        rmt_tx_wait_all_done(state->channel, pdMS_TO_TICKS(100));

        // Delete encoder before deleting channel
        if (state->encoder) {
            rmt_del_encoder(state->encoder);
            state->encoder = nullptr;
        }

        // Delete channel directly - no need to disable since we're deleting
        // (rmt_del_channel handles cleanup internally)
        rmt_del_channel(state->channel);
        state->channel = nullptr;

        // Free DMA and memory allocation
        auto& memMgr = RmtMemoryManager::instance();
        if (state->useDMA) {
            memMgr.freeDMA(state->memoryChannelId, true);  // true = TX channel
            mDMAChannelsInUse--;
        }
        memMgr.free(state->memoryChannelId, true);

        state->useDMA = false;
    }

    // Create channel if needed
    if (!state->channel) {
        if (!createChannel(state, pin, timing, dataSize)) {
            FL_LOG_RMT("Failed to recreate channel for pin " << static_cast<int>(pin));
            return;
        }

        // Register callback after creation (state pointer is already stable here)
        if (!registerChannelCallback(state)) {
            FL_WARN("Failed to register callback after reconfiguration");
            if (state->encoder) {
                rmt_del_encoder(state->encoder);
                state->encoder = nullptr;
            }
            if (state->useDMA) {
                mDMAChannelsInUse--;
            }
            rmt_del_channel(state->channel);
            state->channel = nullptr;

            // Free DMA and memory allocation
            auto& memMgr = RmtMemoryManager::instance();
            if (state->useDMA) {
                memMgr.freeDMA(state->memoryChannelId, true);  // true = TX channel
                mDMAChannelsInUse--;
            }
            memMgr.free(state->memoryChannelId, true);

            state->useDMA = false;
            return;
        }
    }

    // Update timing - encoder is already created in createChannel()
    state->timing = timing;
    state->transmissionComplete = false;
}

//=============================================================================
// WiFi-Aware Channel Destruction Helpers
//=============================================================================

void ChannelEngineRMT::destroyChannel(ChannelState* state) {
    FL_ASSERT(state != nullptr, "destroyChannel called with nullptr");

    if (!state->channel) {
        return;  // Already destroyed
    }

    // Wait for transmission to complete (should already be done if !inUse)
    esp_err_t wait_result = rmt_tx_wait_all_done(state->channel, pdMS_TO_TICKS(100));
    if (wait_result != ESP_OK) {
        FL_WARN("destroyChannel: rmt_tx_wait_all_done timeout for pin " << static_cast<int>(state->pin));
    }

    // Delete encoder
    if (state->encoder) {
        esp_err_t enc_err = rmt_del_encoder(state->encoder);
        if (enc_err != ESP_OK) {
            FL_WARN("destroyChannel: Failed to delete encoder: " << esp_err_to_name(enc_err));
        }
        state->encoder = nullptr;
    }

    // Delete channel
    esp_err_t del_err = rmt_del_channel(state->channel);
    if (del_err != ESP_OK) {
        FL_WARN("destroyChannel: Failed to delete channel: " << esp_err_to_name(del_err));
    }
    state->channel = nullptr;

    // Free memory resources
    auto& memMgr = RmtMemoryManager::instance();
    if (state->useDMA) {
        memMgr.freeDMA(state->memoryChannelId, true);  // true = TX channel
        mDMAChannelsInUse--;
    }
    memMgr.free(state->memoryChannelId, true);  // true = TX channel

    state->useDMA = false;

    FL_LOG_RMT("Destroyed channel on pin " << static_cast<int>(state->pin)
               << " (memoryChannelId: " << static_cast<int>(state->memoryChannelId) << ")");
}

void ChannelEngineRMT::destroyLeastUsedChannels(size_t count) {
    if (count == 0) {
        return;
    }

    FL_LOG_RMT("Destroying " << count << " least-used channels");

    // Destroy channels from end of vector (FIFO - oldest channels at end)
    // NOTE: Future enhancement could track lastUsedTimestamp for true LRU behavior
    size_t destroyed = 0;
    while (destroyed < count && !mChannels.empty()) {
        auto& state = mChannels.back();

        // Only destroy if not in use
        if (!state.inUse) {
            destroyChannel(&state);
            mChannels.pop_back();
            destroyed++;
        } else {
            // Cannot destroy in-use channel - skip for now
            // This could happen if WiFi activates during transmission
            FL_WARN("destroyLeastUsedChannels: Cannot destroy in-use channel on pin "
                    << static_cast<int>(state.pin) << ", skipping");
            break;
        }
    }

    FL_LOG_RMT("Destroyed " << destroyed << " channels (requested: " << count << ")");
}

size_t ChannelEngineRMT::calculateTargetChannelCount(bool wifiActive) {
    if (!wifiActive) {
        // No WiFi: Use maximum channels for platform (2× memory blocks)
        #if defined(CONFIG_IDF_TARGET_ESP32)
            return 4;  // 512 words ÷ 128 = 4 channels
        #elif defined(CONFIG_IDF_TARGET_ESP32S2)
            return 2;  // 256 words ÷ 128 = 2 channels
        #elif defined(CONFIG_IDF_TARGET_ESP32S3)
            return 3;  // 1 DMA + 2 on-chip (192 ÷ 96 = 2)
        #elif defined(CONFIG_IDF_TARGET_ESP32C3) || defined(CONFIG_IDF_TARGET_ESP32C6) || \
              defined(CONFIG_IDF_TARGET_ESP32H2) || defined(CONFIG_IDF_TARGET_ESP32C5)
            return 1;  // C3/C6/H2/C5: Only 96 words
        #else
            return 1;  // Unknown platform: conservative default
        #endif
    } else {
        // WiFi active: Reduce channels to allow 3× buffering (except C3/C6/H2/C5)
        #if defined(CONFIG_IDF_TARGET_ESP32)
            return 2;  // 512 words ÷ 192 = 2 channels (3× buffering)
        #elif defined(CONFIG_IDF_TARGET_ESP32S2)
            return 1;  // 256 words ÷ 192 = 1 channel (3× buffering)
        #elif defined(CONFIG_IDF_TARGET_ESP32S3)
            return 2;  // 1 DMA + 1 on-chip (192 words ÷ 144 = 1 on-chip with 3×)
        #elif defined(CONFIG_IDF_TARGET_ESP32C3) || defined(CONFIG_IDF_TARGET_ESP32C6) || \
              defined(CONFIG_IDF_TARGET_ESP32H2) || defined(CONFIG_IDF_TARGET_ESP32C5)
            return 1;  // C3/C6/H2/C5: Cannot use 3× (insufficient memory), keep 1 channel
        #else
            return 1;  // Unknown platform: conservative default
        #endif
    }
}

void ChannelEngineRMT::reconfigureForWiFi(bool wifiActive) {
    // Check if WiFi state changed
    if (wifiActive == mLastKnownWiFiState) {
        return;  // No change - nothing to do
    }

    FL_DBG("WiFi state changed: " << (wifiActive ? "ACTIVE" : "INACTIVE")
           << " (was: " << (mLastKnownWiFiState ? "ACTIVE" : "INACTIVE") << ")");

    // Calculate target channel count for new WiFi state
    size_t targetChannels = calculateTargetChannelCount(wifiActive);
    FL_DBG("Target channel count: " << targetChannels << " (current: " << mChannels.size() << ")");

    // PHASE 1: Destroy excess channels if WiFi activated and we exceed target
    if (wifiActive && mChannels.size() > targetChannels) {
        size_t channelsToDestroy = mChannels.size() - targetChannels;
        FL_DBG("WiFi activated - destroying " << channelsToDestroy << " excess channels");
        destroyLeastUsedChannels(channelsToDestroy);
    }

    // PHASE 2: Reconfigure remaining idle channels with new memory allocation
    // This ensures existing channels use WiFi-appropriate memory (2× vs 3× blocks)
    auto& memMgr = RmtMemoryManager::instance();
    size_t reconfigured = 0;

    for (size_t i = 0; i < mChannels.size(); ++i) {
        ChannelState& state = mChannels[i];

        // Skip channels that are in use or already destroyed
        if (state.inUse || !state.channel) {
            continue;
        }

        // Destroy the channel and its encoder
        FL_DBG("Reconfiguring idle channel " << i << " (pin: " << static_cast<int>(state.pin) << ")");

        // Delete encoder first
        if (state.encoder) {
            rmt_del_encoder(state.encoder);
            state.encoder = nullptr;
        }

        // Delete channel
        if (state.channel) {
            rmt_tx_wait_all_done(state.channel, pdMS_TO_TICKS(100));
            rmt_del_channel(state.channel);
            state.channel = nullptr;
        }

        // Free DMA if this channel was using it
        if (state.useDMA) {
            memMgr.freeDMA(state.memoryChannelId, true);  // true = TX channel
            mDMAChannelsInUse--;
            state.useDMA = false;
        }

        // Free memory allocation
        memMgr.free(state.memoryChannelId, true);  // true = TX channel

        // Recreate channel with WiFi-appropriate memory allocation
        // Note: createChannel() will call memMgr.allocateTx() with current WiFi state
        if (createChannel(&state, state.pin, state.timing, 0)) {
            // Re-register callback for the recreated channel
            if (!registerChannelCallback(&state)) {
                FL_WARN("Failed to re-register callback for reconfigured channel " << i);
                // Channel creation succeeded but callback failed - destroy it
                if (state.channel) {
                    rmt_del_channel(state.channel);
                    state.channel = nullptr;
                }
                if (state.encoder) {
                    rmt_del_encoder(state.encoder);
                    state.encoder = nullptr;
                }
            } else {
                reconfigured++;
                FL_DBG("Successfully reconfigured channel " << i);
            }
        } else {
            FL_WARN("Failed to recreate channel " << i << " during WiFi reconfiguration");
        }
    }

    // Update last known state
    mLastKnownWiFiState = wifiActive;
    FL_DBG("WiFi reconfiguration complete - " << reconfigured << " channels reconfigured");
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

        // Get data size for DMA buffer calculation
        fl::size dataSize = pending.data->getSize();

        // Acquire channel for this transmission
        ChannelState* channel = acquireChannel(static_cast<gpio_num_t>(pending.pin), pending.timing, dataSize);
        if (!channel) {
            ++i;  // No HW available, leave in queue
            continue;
        }

        // Verify channel has encoder (should always be true if createChannel succeeded)
        if (!channel->encoder) {
            FL_WARN("Channel missing encoder for pin " << pending.pin);
            releaseChannel(channel);
            ++i;
            continue;
        }

        // Start transmission
        channel->reset_us = pending.reset_us;
        channel->transmissionComplete = false;

        // Acquire buffer from pool (PSRAM -> DRAM/DMA transfer)
        // Note: dataSize already retrieved earlier for channel acquisition
        fl::span<uint8_t> pooledBuffer = channel->useDMA ?
            mBufferPool.acquireDMA(dataSize) :
            mBufferPool.acquireInternal(dataSize);

        if (pooledBuffer.empty()) {
            FL_WARN("Failed to acquire pooled buffer for pin " << pending.pin
                    << " (" << dataSize << " bytes, DMA=" << channel->useDMA << ")");
            releaseChannel(channel);
            ++i;
            continue;
        }

        // Copy data from PSRAM to pooled buffer using writeWithPadding
        // Note: writeWithPadding uses zero padding since RMT can handle any byte array size
        pending.data->writeWithPadding(pooledBuffer);

        // Store pooled buffer in channel state for release on completion
        channel->pooledBuffer = pooledBuffer;

        esp_err_t err = rmt_enable(channel->channel);
        if (err != ESP_OK) {
            FL_LOG_RMT("Failed to enable channel: " << esp_err_to_name(err));
            // Release buffer back to pool before releasing channel
            if (channel->useDMA) {
                mBufferPool.releaseDMA();
            } else {
                mBufferPool.releaseInternal(channel->pooledBuffer);
            }
            channel->pooledBuffer = fl::span<uint8_t>();
            releaseChannel(channel);
            ++i;
            continue;
        }

        // Explicitly reset encoder before each transmission to ensure clean state
        err = channel->encoder->reset(channel->encoder);
        if (err != ESP_OK) {
            FL_LOG_RMT("Failed to reset encoder: " << esp_err_to_name(err));
            rmt_disable(channel->channel);
            // Release buffer back to pool before releasing channel
            if (channel->useDMA) {
                mBufferPool.releaseDMA();
            } else {
                mBufferPool.releaseInternal(channel->pooledBuffer);
            }
            channel->pooledBuffer = fl::span<uint8_t>();
            releaseChannel(channel);
            ++i;
            continue;
        }

        rmt_transmit_config_t tx_config = {};
        tx_config.loop_count = 0;
        // Pass pooled buffer (DRAM/DMA) instead of PSRAM pointer
        err = rmt_transmit(channel->channel, channel->encoder,
                          pooledBuffer.data(),
                          pooledBuffer.size(),
                          &tx_config);
        if (err != ESP_OK) {
            FL_LOG_RMT("Failed to transmit: " << esp_err_to_name(err));
            rmt_disable(channel->channel);
            // Release buffer back to pool before releasing channel
            if (channel->useDMA) {
                mBufferPool.releaseDMA();
            } else {
                mBufferPool.releaseInternal(channel->pooledBuffer);
            }
            channel->pooledBuffer = fl::span<uint8_t>();
            releaseChannel(channel);
            ++i;
            continue;
        }

        FL_LOG_RMT("Started transmission for pin " << pending.pin
                   << " (" << pending.data->getSize() << " bytes)");

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

/// @brief ISR callback fired when RMT transmission completes
///
/// CRITICAL TIMING CONTRACT:
/// This callback is invoked by ESP-IDF when the RMT peripheral signals that
/// the transmission queue is empty. However, there is a small timing window
/// where the callback may fire BEFORE the last bits have fully propagated
/// out of the RMT shift register and onto the GPIO pin.
///
/// RACE CONDITION PREVENTION:
/// To prevent buffer corruption, releaseChannel() MUST call rmt_tx_wait_all_done()
/// with a timeout BEFORE marking the channel as available for reuse. This ensures
/// the RMT hardware has fully completed transmission before:
///   1. The channel's pooled buffer is released back to the buffer pool
///   2. The ChannelData's mInUse flag is cleared (allowing new pixel data writes)
///   3. The channel is marked as available for acquisition by other transmissions
///
/// Without this hardware wait, the following race condition can occur:
///   - Frame N transmission starts (RMT hardware reading buffer A)
///   - ISR fires (callback sets transmissionComplete = true)
///   - Main thread calls releaseChannel() → clears mInUse flag
///   - User calls FastLED.show() for Frame N+1
///   - ClocklessRMT::showPixels() sees mInUse == false → proceeds to encode
///   - NEW pixel data overwrites buffer A while RMT is still shifting it out
///   - LEDs display corrupted mix of Frame N and Frame N+1 data
///
/// SYNCHRONIZATION STRATEGY:
/// - ISR: Sets transmissionComplete flag (lightweight, non-blocking)
/// - Main thread poll(): Calls releaseChannel() when transmissionComplete is true
/// - releaseChannel(): Calls rmt_tx_wait_all_done() to ensure hardware is done
/// - ClocklessRMT::showPixels(): Asserts !mInUse before writing new pixel data
///
/// This multi-layered approach provides both correctness (hardware wait) and
/// fail-fast debugging (assertions catch any timing bugs).
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
    // NOTE: This flag triggers releaseChannel(), which performs hardware wait
    state->transmissionComplete = true;

    // Non-blocking design - no semaphore signal needed
    return false;  // No task switch needed
}

} // namespace fl

#endif // FASTLED_RMT5

#endif // ESP32
