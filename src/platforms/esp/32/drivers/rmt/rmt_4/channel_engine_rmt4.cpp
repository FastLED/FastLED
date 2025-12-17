/// @file channel_engine_rmt4.cpp
/// @brief RMT4 implementation of ChannelEngine for ESP32 (IDF 4.x)

#ifdef ESP32

#include "fl/compiler_control.h"
#include "platforms/esp/32/feature_flags/enabled.h"

// Skip entirely if platform has no RMT hardware (ESP32-C2)
#if !FASTLED_ESP32_HAS_RMT
// No RMT hardware available
#elif defined(CONFIG_IDF_TARGET_ESP32C6) || defined(CONFIG_IDF_TARGET_ESP32C5) || \
    defined(CONFIG_IDF_TARGET_ESP32P4) || defined(CONFIG_IDF_TARGET_ESP32H2)
// Skip RMT4 implementation for RMT5-only chips
#elif !FASTLED_RMT5  // Only compile for RMT4 (IDF 4.x)

#include "channel_engine_rmt4.h"
#include "fl/dbg.h"
#include "fl/log.h"

namespace fl {

// ═══════════════════════════════════════════════════════════════════════════
// Factory Method
// ═══════════════════════════════════════════════════════════════════════════

ChannelEngineRMT4Ptr ChannelEngineRMT4::create() {
    return fl::make_shared<ChannelEngineRMT4Impl>();
}

// ═══════════════════════════════════════════════════════════════════════════
// Constructor
// ═══════════════════════════════════════════════════════════════════════════

ChannelEngineRMT4Impl::ChannelEngineRMT4Impl()
    : mRMT_intr_handle(nullptr)
    , mRmtSpinlock(portMUX_INITIALIZER_UNLOCKED)
    , mInitialized(false)
{
    FL_WARN("ChannelEngineRMT4: Initializing RMT4 engine for IDF 4.x");

    // Reserve space for channels (inlined vector, no heap allocation)
    mChannels.reserve(FASTLED_RMT_MAX_CHANNELS);
    mEnqueuedChannels.reserve(16);
    mPendingChannels.reserve(16);

    // Register global ISR handler for RMT interrupts
    // NOTE: The ISR must be registered once for all RMT channels
    esp_err_t err = esp_intr_alloc(
        ETS_RMT_INTR_SOURCE,
        ESP_INTR_FLAG_IRAM | ESP_INTR_FLAG_LEVEL3,
        &ChannelEngineRMT4Impl::handleInterrupt,
        this,  // Pass 'this' pointer to ISR for dispatch
        &mRMT_intr_handle
    );

    if (err != ESP_OK) {
        FL_WARN("ChannelEngineRMT4: Failed to allocate RMT interrupt, error=" << err);
        return;
    }

    mInitialized = true;
    FL_WARN("ChannelEngineRMT4: Initialized successfully");
}

// ═══════════════════════════════════════════════════════════════════════════
// Destructor
// ═══════════════════════════════════════════════════════════════════════════

ChannelEngineRMT4Impl::~ChannelEngineRMT4Impl() {
    FL_WARN("ChannelEngineRMT4: Shutting down");

    // Free the interrupt handler
    if (mRMT_intr_handle != nullptr) {
        esp_intr_free(mRMT_intr_handle);
        mRMT_intr_handle = nullptr;
    }

    // Release all channels and uninstall RMT drivers
    for (auto& state : mChannels) {
        if (state.inUse) {
            // Disable interrupts for this channel
            rmt_set_tx_intr_en(state.channel, false);

            // Uninstall the RMT driver for this channel
            rmt_driver_uninstall(state.channel);

            state.inUse = false;
        }
    }

    mChannels.clear();
    mEnqueuedChannels.clear();
    mPendingChannels.clear();
    mInitialized = false;

    FL_WARN("ChannelEngineRMT4: Shutdown complete");
}

// ═══════════════════════════════════════════════════════════════════════════
// Timing Symbol Helpers
// ═══════════════════════════════════════════════════════════════════════════

rmt_item32_t ChannelEngineRMT4Impl::makeZeroSymbol(const ChipsetTimingConfig& timing) {
    // Zero bit timing: T0H (high) + T0L (low)
    // For WS2812: T0H=400ns, T0L=850ns

    uint32_t T1 = timing.t1_ns;  // T0H in nanoseconds
    uint32_t T2 = timing.t2_ns;  // T0L (partial)
    uint32_t T3 = timing.t3_ns;  // T0L (remaining)

    // Convert to CPU cycles, then to RMT cycles
    uint32_t T1_cycles = NS_TO_ESP_CYCLES(T1);
    uint32_t T2_cycles = NS_TO_ESP_CYCLES(T2);
    uint32_t T3_cycles = NS_TO_ESP_CYCLES(T3);

    rmt_item32_t zero;
    zero.level0 = 1;  // High during T0H
    zero.duration0 = ESP_TO_RMT_CYCLES(T1_cycles);
    zero.level1 = 0;  // Low during T0L
    zero.duration1 = ESP_TO_RMT_CYCLES(T2_cycles + T3_cycles);

    return zero;
}

rmt_item32_t ChannelEngineRMT4Impl::makeOneSymbol(const ChipsetTimingConfig& timing) {
    // One bit timing: T1H (high) + T1L (low)
    // For WS2812: T1H=850ns, T1L=400ns

    uint32_t T1 = timing.t1_ns;  // T1H (partial)
    uint32_t T2 = timing.t2_ns;  // T1H (remaining)
    uint32_t T3 = timing.t3_ns;  // T1L

    // Convert to CPU cycles, then to RMT cycles
    uint32_t T1_cycles = NS_TO_ESP_CYCLES(T1);
    uint32_t T2_cycles = NS_TO_ESP_CYCLES(T2);
    uint32_t T3_cycles = NS_TO_ESP_CYCLES(T3);

    rmt_item32_t one;
    one.level0 = 1;  // High during T1H
    one.duration0 = ESP_TO_RMT_CYCLES(T1_cycles + T2_cycles);
    one.level1 = 0;  // Low during T1L
    one.duration1 = ESP_TO_RMT_CYCLES(T3_cycles);

    return one;
}

// ═══════════════════════════════════════════════════════════════════════════
// Note: IRAM functions (ISR handlers) are now inlined in the header
// ═══════════════════════════════════════════════════════════════════════════

// ═══════════════════════════════════════════════════════════════════════════
// Channel Management - Stubs (to be implemented in Phase 3B)
// ═══════════════════════════════════════════════════════════════════════════

ChannelEngineRMT4Impl::ChannelState* ChannelEngineRMT4Impl::acquireChannel(
    gpio_num_t pin,
    const ChipsetTimingConfig& timing
) {
    // Three-tier channel allocation strategy (same as RMT5):
    // Strategy 1: Reuse channel with matching pin (zero-cost reuse)
    // Strategy 2: Reconfigure any idle channel (requires hardware reconfiguration)
    // Strategy 3: Create new channel if hardware available

    // Strategy 1: Find idle channel with matching pin (perfect match, no reconfiguration)
    for (auto& state : mChannels) {
        if (!state.inUse && state.pin == pin) {
            state.inUse = true;
            // Recalculate timing symbols in case timing changed
            state.zero = makeZeroSymbol(timing);
            state.one = makeOneSymbol(timing);

            // Reset transmission state
            state.transmissionComplete = false;
            state.whichHalf = 0;
            state.pixelDataPos = 0;
            state.pixelData = nullptr;
            state.pixelDataSize = 0;
            state.lastFill = 0;
            state.transmissionStartTime = 0;

            FL_WARN("acquireChannel: Reusing channel " << state.channel
                   << " for pin " << static_cast<int>(pin));
            return &state;
        }
    }

    // Strategy 2: Find any idle channel and reconfigure it
    for (auto& state : mChannels) {
        if (!state.inUse) {
            state.inUse = true;

            // Reconfigure hardware for new pin/timing
            if (!configureChannel(&state, pin, timing)) {
                FL_WARN("acquireChannel: Failed to reconfigure channel " << state.channel);
                state.inUse = false;
                return nullptr;
            }

            FL_WARN("acquireChannel: Reconfigured channel " << state.channel
                   << " for pin " << static_cast<int>(pin));
            return &state;
        }
    }

    // Strategy 3: Create new channel if hardware available
    if (mChannels.size() >= FASTLED_RMT_MAX_CHANNELS) {
        FL_WARN("acquireChannel: All " << FASTLED_RMT_MAX_CHANNELS
               << " RMT channels in use, time-multiplexing required");
        return nullptr;
    }

    // Allocate new channel state
    ChannelState newState;
    newState.channel = static_cast<rmt_channel_t>(mChannels.size());
    newState.inUse = true;
    newState.transmissionComplete = false;
    newState.whichHalf = 0;
    newState.memPtr = nullptr;
    newState.memStart = nullptr;
    newState.pixelData = nullptr;
    newState.pixelDataSize = 0;
    newState.pixelDataPos = 0;
    newState.lastFill = 0;
    newState.cyclesPerFill = 0;
    newState.maxCyclesPerFill = 0;
    newState.transmissionStartTime = 0;

    // Configure the hardware
    if (!configureChannel(&newState, pin, timing)) {
        FL_WARN("acquireChannel: Failed to configure new channel " << newState.channel);
        return nullptr;
    }

    // Add to channels list
    mChannels.push_back(newState);
    ChannelState* stablePtr = &mChannels.back();

    FL_WARN("acquireChannel: Created new channel " << stablePtr->channel
           << " for pin " << static_cast<int>(pin)
           << " (total: " << mChannels.size() << "/" << FASTLED_RMT_MAX_CHANNELS << ")");

    return stablePtr;
}

void ChannelEngineRMT4Impl::releaseChannel(ChannelState* state) {
    // Release channel back to idle state (mark as available for reuse)
    // NOTE: We do NOT destroy the RMT channel - keep it configured for fast reuse

    if (!state) {
        FL_WARN("releaseChannel: null state pointer");
        return;
    }

    if (!state->inUse) {
        FL_WARN("releaseChannel: Channel " << state->channel << " already released");
        return;
    }

    // Ensure hardware has stopped transmitting before releasing
    // This prevents race conditions if channel is reacquired immediately
    rmt_channel_t channel = state->channel;

    // Disable TX interrupts for this channel (platform-specific)
#if CONFIG_IDF_TARGET_ESP32C3 || CONFIG_IDF_TARGET_ESP32H2 || CONFIG_IDF_TARGET_ESP32S3 || CONFIG_IDF_TARGET_ESP32C6
    RMT.int_ena.val &= ~(1 << channel);
#elif CONFIG_IDF_TARGET_ESP32S2 || CONFIG_IDF_TARGET_ESP32
    RMT.int_ena.val &= ~(1 << (channel * 3));
#else
    // Fallback to API call for unknown platforms
    rmt_set_tx_intr_en(channel, false);
#endif

    // Disconnect the GPIO from the RMT controller
    gpio_matrix_out(state->pin, SIG_GPIO_OUT_IDX, 0, 0);

    // Mark channel as idle (available for reuse)
    state->inUse = false;
    state->transmissionComplete = false;

    // Clear pixel data reference (buffer is owned by ChannelData)
    state->pixelData = nullptr;
    state->pixelDataSize = 0;
    state->pixelDataPos = 0;

    // Clear source data reference
    state->sourceData.reset();

    // NOTE: Keep the following intact for reuse:
    // - state->channel (RMT hardware channel number)
    // - state->pin (for matching in acquireChannel strategy 1)
    // - state->zero, state->one (timing symbols, may be reused)
    // - state->memStart, state->memPtr (hardware memory pointers)
    // - RMT driver remains installed (fast reacquisition)

    FL_WARN("releaseChannel: Released channel " << state->channel
           << " on pin " << static_cast<int>(state->pin));
}

bool ChannelEngineRMT4Impl::configureChannel(
    ChannelState* state,
    gpio_num_t pin,
    const ChipsetTimingConfig& timing
) {
    // Port of ESP32RMTController::init() and startOnChannel() logic
    // Configures RMT hardware for the given pin and timing

    if (!state) {
        FL_WARN("configureChannel: null state pointer");
        return false;
    }

    esp_err_t err = ESP_OK;

    // Update state configuration
    state->pin = pin;
    state->zero = makeZeroSymbol(timing);
    state->one = makeOneSymbol(timing);
    state->transmissionComplete = false;
    state->whichHalf = 0;
    state->pixelDataPos = 0;

    // Calculate expected timing for WiFi interference detection
    uint32_t T1_cycles = NS_TO_ESP_CYCLES(timing.t1_ns);
    uint32_t T2_cycles = NS_TO_ESP_CYCLES(timing.t2_ns);
    uint32_t T3_cycles = NS_TO_ESP_CYCLES(timing.t3_ns);
    state->cyclesPerFill = (T1_cycles + T2_cycles + T3_cycles) * PULSES_PER_FILL_RMT4;
    state->maxCyclesPerFill = state->cyclesPerFill + state->cyclesPerFill / 2;
    state->lastFill = 0;

    // RMT configuration for transmission
    rmt_config_t rmt_tx;
    fl::memset(&rmt_tx, 0, sizeof(rmt_config_t));
    rmt_tx.channel = state->channel;
    rmt_tx.rmt_mode = RMT_MODE_TX;
    rmt_tx.gpio_num = pin;
    rmt_tx.mem_block_num = FASTLED_RMT_MEM_BLOCKS;
    rmt_tx.clk_div = DIVIDER_RMT4;
    rmt_tx.tx_config.loop_en = false;
    rmt_tx.tx_config.carrier_level = RMT_CARRIER_LEVEL_LOW;
    rmt_tx.tx_config.carrier_en = false;
    rmt_tx.tx_config.idle_level = RMT_IDLE_LEVEL_LOW;
    rmt_tx.tx_config.idle_output_en = true;

    // Apply the configuration
    err = rmt_config(&rmt_tx);
    if (err != ESP_OK) {
        FL_WARN("configureChannel: rmt_config failed, error=" << err);
        return false;
    }

    // Install RMT driver (no internal buffer - we use custom ISR)
    err = rmt_driver_install(state->channel, 0, 0);
    if (err != ESP_OK) {
        FL_WARN("configureChannel: rmt_driver_install failed, error=" << err);
        return false;
    }

    // Set up threshold interrupt for double-buffer refill
    err = rmt_set_tx_thr_intr_en(state->channel, true, PULSES_PER_FILL_RMT4);
    if (err != ESP_OK) {
        FL_WARN("configureChannel: rmt_set_tx_thr_intr_en failed, error=" << err);
        rmt_driver_uninstall(state->channel);
        return false;
    }

    // Get pointer to RMT hardware memory (direct access for ISR)
    state->memStart = &(RMTMEM.chan[state->channel].data32[0]);
    state->memPtr = state->memStart;

    // Assign the pin to this channel (must be done before transmission)
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(4, 4, 0)
    err = rmt_set_gpio(state->channel, RMT_MODE_TX, pin, false);
#else
    err = rmt_set_pin(state->channel, RMT_MODE_TX, pin);
#endif
    if (err != ESP_OK) {
        FL_WARN("configureChannel: rmt_set_gpio/rmt_set_pin failed, error=" << err);
        rmt_driver_uninstall(state->channel);
        return false;
    }

    FL_WARN("configureChannel: Configured channel " << state->channel
           << " on pin " << static_cast<int>(pin));

    return true;
}

void ChannelEngineRMT4Impl::processPendingChannels() {
    // Process the pending channel queue and start as many transmissions
    // as the hardware allows (up to FASTLED_RMT_MAX_CHANNELS concurrent)
    //
    // Port of: ESP32RMTController::startNext() flow from idf4_rmt_impl.cpp
    //
    // Time-multiplexing: If >8 strips are enqueued, channels are reused
    // as they complete (managed by pollDerived())

    // Iterate through pending queue (use index because we may remove items)
    for (size_t i = 0; i < mPendingChannels.size(); ) {
        const ChannelDataPtr& data = mPendingChannels[i];

        // Try to acquire a hardware channel for this strip
        ChannelState* state = acquireChannel(static_cast<gpio_num_t>(data->getPin()), data->getTiming());

        if (state == nullptr) {
            // All channels busy - time-multiplexing will resume later in poll()
            FL_WARN("processPendingChannels: All " << FASTLED_RMT_MAX_CHANNELS
                   << " channels busy, deferring " << (mPendingChannels.size() - i)
                   << " pending strips");
            break;
        }

        // Channel acquired - start transmission
        startTransmission(state, data);

        // Remove from pending queue (swap-remove for O(1) removal)
        mPendingChannels[i] = mPendingChannels.back();
        mPendingChannels.pop_back();
        // Don't increment i - we just moved a new item to this position
    }
}

void ChannelEngineRMT4Impl::startTransmission(ChannelState* state, const ChannelDataPtr& data) {
    // Initialize channel state and start RMT transmission
    // Port of: ESP32RMTController::startOnChannel() from idf4_rmt_impl.cpp
    //
    // Flow:
    // 1. Store reference to source data (for lifecycle management)
    // 2. Initialize transmission counters
    // 3. Fill both buffer halves
    // 4. Enable interrupts
    // 5. Kick off transmission via tx_start()

    if (!state) {
        FL_WARN("startTransmission: null state pointer");
        return;
    }

    // Store reference to source ChannelData
    state->sourceData = data;

    // Mark data as in-use (prevents strip driver from modifying during TX)
    data->setInUse(true);

    // Get pointer to encoded pixel data
    const auto& dataBuffer = data->getData();
    state->pixelData = dataBuffer.data();
    state->pixelDataSize = dataBuffer.size();

    // DEBUG: Log pixel data details
    FL_DBG("RMT4: startTransmission() called with " << state->pixelDataSize
           << " bytes, first 3 bytes: "
           << (state->pixelDataSize > 0 ? static_cast<int>(state->pixelData[0]) : -1) << " "
           << (state->pixelDataSize > 1 ? static_cast<int>(state->pixelData[1]) : -1) << " "
           << (state->pixelDataSize > 2 ? static_cast<int>(state->pixelData[2]) : -1));

    // Reset transmission state
    state->pixelDataPos = 0;
    state->whichHalf = 0;
    state->memPtr = state->memStart;
    state->transmissionComplete = false;
    state->lastFill = 0;
    state->transmissionStartTime = millis();  // Start timeout timer

    // Fill both halves of the double-buffer
    // (false = skip time check on initial fill)
    fillNextBuffer(state, false);
    fillNextBuffer(state, false);

    // Enable TX interrupts for this channel
    esp_err_t err = rmt_set_tx_intr_en(state->channel, true);
    if (err != ESP_OK) {
        FL_WARN("startTransmission: rmt_set_tx_intr_en failed, error=" << err);
        // Mark complete to trigger cleanup in poll()
        state->transmissionComplete = true;
        data->setInUse(false);
        return;
    }

    // Kick off transmission (critical section for register writes)
    portENTER_CRITICAL(&mRmtSpinlock);
    tx_start(state);
    portEXIT_CRITICAL(&mRmtSpinlock);

    FL_DBG("RMT4: Transmission started on channel " << state->channel
           << ", pin " << static_cast<int>(state->pin)
           << ", " << state->pixelDataSize << " bytes");
}

// Note: findChannelByNumber() and tx_start() are now inlined in the header

// ═══════════════════════════════════════════════════════════════════════════
// IChannelEngine Interface Implementation
// ═══════════════════════════════════════════════════════════════════════════

void ChannelEngineRMT4Impl::enqueue(ChannelDataPtr channelData) {
    // Store channel data for later transmission
    // Called by ChannelBusManager for each LED strip
    if (channelData) {
        mEnqueuedChannels.push_back(channelData);
    } else {
        FL_WARN("enqueue: Received null ChannelData");
    }
}

void ChannelEngineRMT4Impl::show() {
    // Trigger transmission of all enqueued data
    // Called by ChannelBusManager when user calls FastLED.show()

    FL_WARN("show: Transmitting " << mEnqueuedChannels.size() << " enqueued channels");

    if (!mEnqueuedChannels.empty()) {
        // Pass batched data to internal transmission handler
        beginTransmission(fl::span<const ChannelDataPtr>(mEnqueuedChannels));

        // Clear the enqueued batch (data is now in mPendingChannels)
        mEnqueuedChannels.clear();
    }
}

IChannelEngine::EngineState ChannelEngineRMT4Impl::poll() {
    // Query hardware state and manage channel lifecycle
    // Port of: Polling logic that replaced gTX_sem blocking in legacy RMT4
    //
    // Flow:
    // 1. Check each channel for completion (via transmissionComplete flag)
    // 2. Detect and handle transmission timeouts
    // 3. Release completed channels back to idle pool
    // 4. Try to start any pending transmissions from queue
    // 5. Return BUSY if any channels active or pending, else READY

    bool anyBusy = false;
    bool anyTimeout = false;

    // Check all active channels for completion or timeout
    for (auto& state : mChannels) {
        if (!state.inUse) {
            continue;
        }

        if (state.transmissionComplete) {
            // Transmission complete - release channel and mark data available
            FL_WARN("poll: Channel " << state.channel << " completed");

            // Clear in-use flag on source data
            if (state.sourceData) {
                state.sourceData->setInUse(false);
            }

            // Release channel back to idle pool
            releaseChannel(&state);
        } else {
            // Channel still transmitting - check for timeout
#if FASTLED_RMT4_TRANSMISSION_TIMEOUT_MS > 0
            uint32_t elapsed = millis() - state.transmissionStartTime;
            if (elapsed > FASTLED_RMT4_TRANSMISSION_TIMEOUT_MS) {
                // Timeout detected - force channel reset
                FL_WARN("poll: Channel " << state.channel << " timed out after "
                       << elapsed << "ms (limit: " << FASTLED_RMT4_TRANSMISSION_TIMEOUT_MS << "ms)");

                // Disable interrupts for this channel
                rmt_set_tx_intr_en(state.channel, false);

                // Disconnect GPIO
                gpio_matrix_out(state.pin, SIG_GPIO_OUT_IDX, 0, 0);

                // Clear in-use flag on source data
                if (state.sourceData) {
                    state.sourceData->setInUse(false);
                }

                // Release channel back to idle pool
                releaseChannel(&state);

                anyTimeout = true;
                continue;  // Skip anyBusy flag for this channel
            }
#endif
            anyBusy = true;
        }
    }

    // Try to start pending channels (if any idle channels became available)
    if (!mPendingChannels.empty()) {
        processPendingChannels();

        // If still pending after processing, we're busy
        if (!mPendingChannels.empty()) {
            anyBusy = true;
        }
    }

    // Return ERROR if any timeouts occurred, otherwise BUSY/READY
    if (anyTimeout) {
        return EngineState::ERROR;
    }
    return anyBusy ? EngineState::BUSY : EngineState::READY;
}

void ChannelEngineRMT4Impl::beginTransmission(fl::span<const ChannelDataPtr> channelData) {
    // Main entry point for LED data transmission
    // Called by ChannelBusManager when user calls FastLED.show()
    //
    // Port of: The showPixels() → startNext() flow from legacy RMT4
    //
    // Flow:
    // 1. Block flash operations if enabled (prevents WiFi interference)
    // 2. Clear any previous pending queue
    // 3. Add all channels to pending queue
    // 4. Start as many transmissions as hardware allows
    // 5. Release flash lock after transmission starts

    FL_WARN("beginTransmission: Queueing " << channelData.size() << " channels");

#if FASTLED_ESP32_FLASH_LOCK == 1
    // Block flash operations during LED transmission to prevent timing glitches
    // This is especially important when WiFi is active
    #if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(4, 0, 0)
    // IDF 4.x+ uses esp_flash_app_disable_protect()
    // Note: This API may not be available on all IDF versions
    // For now, we'll skip the flash lock and document the limitation
    FL_DBG("RMT4: Flash lock not yet implemented for IDF 4.x+");
    #else
    // IDF 3.x uses spi_flash_op_lock()
    spi_flash_op_lock();
    FL_DBG("RMT4: Flash operations locked");
    #endif
#endif

    // Clear pending queue (should be empty, but ensure clean state)
    mPendingChannels.clear();

    // Add all channels to pending queue
    for (const auto& data : channelData) {
        if (data) {
            mPendingChannels.push_back(data);
        } else {
            FL_WARN("beginTransmission: Null ChannelData in span");
        }
    }

    // Start as many transmissions as hardware allows
    processPendingChannels();

#if FASTLED_ESP32_FLASH_LOCK == 1
    #if ESP_IDF_VERSION < ESP_IDF_VERSION_VAL(4, 0, 0)
    // Release flash lock after transmission starts
    spi_flash_op_unlock();
    FL_DBG("RMT4: Flash operations unlocked");
    #endif
#endif
}

} // namespace fl

#endif // !FASTLED_RMT5 and not RMT5-only chip
#endif // ESP32
