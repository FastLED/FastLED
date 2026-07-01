// IWYU pragma: private

/// @file channel_driver_rmt4.cpp
/// @brief RMT4 implementation of ChannelEngine for ESP32 (IDF 4.x)

#include "platforms/is_platform.h"
#ifdef FL_IS_ESP32

#include "fl/stl/compiler_control.h"
#include "platforms/esp/32/feature_flags/enabled.h"

// Skip entirely if platform has no RMT hardware (ESP32-C2)
#if !FASTLED_ESP32_HAS_RMT
// No RMT hardware available
#elif defined(FL_IS_ESP_32C6) || defined(FL_IS_ESP_32C5) ||                    \
    defined(CONFIG_IDF_TARGET_ESP32P4) || defined(CONFIG_IDF_TARGET_ESP32H2)
// Skip RMT4 implementation for RMT5-only chips
#elif !FASTLED_RMT5 // Only compile for RMT4 (IDF 4.x)

#include "fl/log/log.h"
#include "fl/stl/move.h" // fl::move
#include "fl/stl/noexcept.h"
#include "platforms/esp/32/drivers/rmt/rmt_4/channel_driver_rmt4.h"
#include "platforms/esp/32/drivers/rmt/rmt_4/network_state_tracker_4.h"
#include "platforms/esp/32/drivers/rmt/rmt_4/rmt4_peripheral_esp.h"
#include "platforms/esp/32/drivers/rmt/rmt_4/rmt_memory_manager_4.h"

namespace fl {

// ═══════════════════════════════════════════════════════════════════════════
// Factory Methods
// ═══════════════════════════════════════════════════════════════════════════

ChannelEngineRMT4Ptr ChannelEngineRMT4::create() FL_NO_EXCEPT {
    return fl::make_shared<ChannelEngineRMT4Impl>();
}

ChannelEngineRMT4Ptr ChannelEngineRMT4::create(
    fl::shared_ptr<detail::IRMT4Peripheral> peripheral) FL_NO_EXCEPT {
    return fl::make_shared<ChannelEngineRMT4Impl>(fl::move(peripheral));
}

// ═══════════════════════════════════════════════════════════════════════════
// Constructors
// ═══════════════════════════════════════════════════════════════════════════

ChannelEngineRMT4Impl::ChannelEngineRMT4Impl() FL_NO_EXCEPT
    : ChannelEngineRMT4Impl(fl::make_shared<Rmt4PeripheralESP>()) {}

ChannelEngineRMT4Impl::ChannelEngineRMT4Impl(
    fl::shared_ptr<detail::IRMT4Peripheral> peripheral) FL_NO_EXCEPT
    : mPeripheral(fl::move(peripheral)),
      mRMT_intr_handle(nullptr),
      mRmtSpinlock(portMUX_INITIALIZER_UNLOCKED),
      mInitialized(false) {
    FL_WARN_F("ChannelEngineRMT4: Initializing RMT4 driver for IDF 4.x");

    // Reserve space for channels (inlined vector, no heap allocation)
    mChannels.reserve(FASTLED_RMT_MAX_CHANNELS);
    mEnqueuedChannels.reserve(16);
    mPendingChannels.reserve(16);

    if (!mPeripheral) {
        FL_WARN_F("ChannelEngineRMT4: null peripheral injected — driver inert");
        return;
    }

    // Register the single global ISR for all RMT channels via the
    // peripheral abstraction. ISR signature matches `intr_handler_t`.
    if (!mPeripheral->installIsr(&ChannelEngineRMT4Impl::handleInterrupt, this,
                                 &mRMT_intr_handle)) {
        FL_WARN_F("ChannelEngineRMT4: peripheral->installIsr failed");
        return;
    }

    mInitialized = true;
    FL_WARN_F("ChannelEngineRMT4: Initialized successfully");
}

// ═══════════════════════════════════════════════════════════════════════════
// Destructor
// ═══════════════════════════════════════════════════════════════════════════

ChannelEngineRMT4Impl::~ChannelEngineRMT4Impl() {
    FL_WARN_F("ChannelEngineRMT4: Shutting down");

    // Free the global ISR via the peripheral abstraction.
    if (mPeripheral && mRMT_intr_handle != nullptr) {
        mPeripheral->freeIsr(mRMT_intr_handle);
    }
    mRMT_intr_handle = nullptr;

    // Release all channels and uninstall RMT drivers via the peripheral.
    for (auto &state : mChannels) {
        if (state.inUse) {
            if (mPeripheral) {
                mPeripheral->setTxIntrEnable(static_cast<int>(state.channel),
                                             false);
                mPeripheral->uninstallDriver(static_cast<int>(state.channel));
            }
            // Give the pool word count back to the manager (#3469). A
            // miss here is silent — the manager already logs on the
            // free() path.
            RmtMemoryManager4::instance().free(static_cast<int>(state.channel),
                                               /*is_tx=*/true);
            state.inUse = false;
        }
    }

    mChannels.clear();
    mEnqueuedChannels.clear();
    mPendingChannels.clear();
    mInitialized = false;

    FL_WARN_F("ChannelEngineRMT4: Shutdown complete");
}

// ═══════════════════════════════════════════════════════════════════════════
// IChannelDriver Interface
// ═══════════════════════════════════════════════════════════════════════════

bool ChannelEngineRMT4Impl::canHandle(const ChannelDataPtr &data) const
    FL_NO_EXCEPT {
    if (!data) {
        return false;
    }
    // Clockless drivers only handle non-SPI chipsets
    return !data->isSpi();
}

// ═══════════════════════════════════════════════════════════════════════════
// Timing Symbol Helpers
// ═══════════════════════════════════════════════════════════════════════════

rmt_item32_t ChannelEngineRMT4Impl::makeZeroSymbol(
    const ChipsetTimingConfig &timing) FL_NO_EXCEPT {
    // Zero bit timing: T0H (high) + T0L (low)
    // For WS2812: T0H=400ns, T0L=850ns

    u32 T1 = timing.t1_ns; // T0H in nanoseconds
    u32 T2 = timing.t2_ns; // T0L (partial)
    u32 T3 = timing.t3_ns; // T0L (remaining)

    // Convert to CPU cycles, then to RMT cycles
    u32 T1_cycles = NS_TO_ESP_CYCLES(T1);
    u32 T2_cycles = NS_TO_ESP_CYCLES(T2);
    u32 T3_cycles = NS_TO_ESP_CYCLES(T3);

    rmt_item32_t zero;
    zero.level0 = 1; // High during T0H
    zero.duration0 = ESP_TO_RMT_CYCLES(T1_cycles);
    zero.level1 = 0; // Low during T0L
    zero.duration1 = ESP_TO_RMT_CYCLES(T2_cycles + T3_cycles);

    return zero;
}

rmt_item32_t ChannelEngineRMT4Impl::makeOneSymbol(
    const ChipsetTimingConfig &timing) FL_NO_EXCEPT {
    // One bit timing: T1H (high) + T1L (low)
    // For WS2812: T1H=850ns, T1L=400ns

    u32 T1 = timing.t1_ns; // T1H (partial)
    u32 T2 = timing.t2_ns; // T1H (remaining)
    u32 T3 = timing.t3_ns; // T1L

    // Convert to CPU cycles, then to RMT cycles
    u32 T1_cycles = NS_TO_ESP_CYCLES(T1);
    u32 T2_cycles = NS_TO_ESP_CYCLES(T2);
    u32 T3_cycles = NS_TO_ESP_CYCLES(T3);

    rmt_item32_t one;
    one.level0 = 1; // High during T1H
    one.duration0 = ESP_TO_RMT_CYCLES(T1_cycles + T2_cycles);
    one.level1 = 0; // Low during T1L
    one.duration1 = ESP_TO_RMT_CYCLES(T3_cycles);

    return one;
}

// ═══════════════════════════════════════════════════════════════════════════
// Note: IRAM functions (ISR handlers) are now inlined in the header
// ═══════════════════════════════════════════════════════════════════════════

// ═══════════════════════════════════════════════════════════════════════════
// Channel Management
//
// All ESP-IDF lifecycle calls go through `mPeripheral` (`IRMT4Peripheral`).
// ISR hot-path code (`fillNextBuffer`, `tx_start`, register writes) stays
// inline in the header — see #3458.
// ═══════════════════════════════════════════════════════════════════════════

ChannelEngineRMT4Impl::ChannelState *ChannelEngineRMT4Impl::acquireChannel(
    gpio_num_t pin, const ChipsetTimingConfig &timing) FL_NO_EXCEPT {
    // Three-tier channel allocation strategy (same as RMT5):
    // Strategy 1: Reuse channel with matching pin (zero-cost reuse)
    // Strategy 2: Reconfigure any idle channel (requires hardware
    // reconfiguration) Strategy 3: Create new channel if hardware available

    // Strategy 1: Find idle channel with matching pin (perfect match, no
    // reconfiguration)
    for (auto &state : mChannels) {
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

            FL_WARN_F("acquireChannel: Reusing channel %s for pin %s",
                      state.channel, static_cast<int>(pin));
            return &state;
        }
    }

    // Strategy 2: Find any idle channel and reconfigure it
    for (auto &state : mChannels) {
        if (!state.inUse) {
            state.inUse = true;

            // Reconfigure hardware for new pin/timing
            if (!configureChannel(&state, pin, timing)) {
                FL_WARN_F("acquireChannel: Failed to reconfigure channel %s",
                          state.channel);
                state.inUse = false;
                return nullptr;
            }

            FL_WARN_F("acquireChannel: Reconfigured channel %s for pin %s",
                      state.channel, static_cast<int>(pin));
            return &state;
        }
    }

    // Strategy 3: Create new channel if hardware available
    if (mChannels.size() >= FASTLED_RMT_MAX_CHANNELS) {
        FL_WARN_F("acquireChannel: All %s RMT channels in use, "
                  "time-multiplexing required",
                  FASTLED_RMT_MAX_CHANNELS);
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
        FL_WARN_F("acquireChannel: Failed to configure new channel %s",
                  newState.channel);
        return nullptr;
    }

    // Add to channels list
    mChannels.push_back(newState);
    ChannelState *stablePtr = &mChannels.back();

    FL_WARN_F(
        "acquireChannel: Created new channel %s for pin %s (total: %s/%s)",
        stablePtr->channel, static_cast<int>(pin), mChannels.size(),
        FASTLED_RMT_MAX_CHANNELS);

    return stablePtr;
}

void ChannelEngineRMT4Impl::releaseChannel(ChannelState *state) FL_NO_EXCEPT {
    // Release channel back to idle state (mark as available for reuse)
    // NOTE: We do NOT destroy the RMT channel - keep it configured for fast
    // reuse

    if (!state) {
        FL_WARN_F("releaseChannel: null state pointer");
        return;
    }

    if (!state->inUse) {
        FL_WARN_F("releaseChannel: Channel %s already released",
                  state->channel);
        return;
    }

    // Ensure hardware has stopped transmitting before releasing
    // This prevents race conditions if channel is reacquired immediately
    rmt_channel_t channel = state->channel;

    // Disable TX interrupts for this channel (platform-specific direct
    // register writes — fastest path; the peripheral surface only
    // covers the per-channel `rmt_set_tx_intr_en()` lifecycle call).
#if defined(FL_IS_ESP_32C3) || defined(FL_IS_ESP_32H2) ||                      \
    defined(FL_IS_ESP_32S3) || defined(FL_IS_ESP_32C6)
    RMT.int_ena.val &= ~(1 << channel);
#elif defined(FL_IS_ESP_32S2) || defined(FL_IS_ESP_32DEV)
    RMT.int_ena.val &= ~(1 << (channel * 3));
#else
    // Fallback for unknown ESP32 variants — go through the peripheral.
    if (mPeripheral) {
        mPeripheral->setTxIntrEnable(static_cast<int>(channel), false);
    }
#endif

    // Disconnect the GPIO from the RMT controller. This call is also
    // used from the IRAM ISR (`onTxDoneInterrupt`), so it stays as a
    // direct ROM call rather than going through `IRMT4Peripheral`.
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

    FL_WARN_F("releaseChannel: Released channel %s on pin %s", state->channel,
              static_cast<int>(state->pin));
}

bool ChannelEngineRMT4Impl::configureChannel(
    ChannelState *state, gpio_num_t pin,
    const ChipsetTimingConfig &timing) FL_NO_EXCEPT {
    // Port of ESP32RMTController::init() and startOnChannel() logic.
    // All ESP-IDF lifecycle calls go through `mPeripheral` (#3458).

    if (!state) {
        FL_WARN_F("configureChannel: null state pointer");
        return false;
    }
    if (!mPeripheral) {
        FL_WARN_F("configureChannel: null peripheral");
        return false;
    }

    // Update state configuration
    state->pin = pin;
    state->zero = makeZeroSymbol(timing);
    state->one = makeOneSymbol(timing);
    state->transmissionComplete = false;
    state->whichHalf = 0;
    state->pixelDataPos = 0;

    // Calculate expected timing for WiFi interference detection
    u32 T1_cycles = NS_TO_ESP_CYCLES(timing.t1_ns);
    u32 T2_cycles = NS_TO_ESP_CYCLES(timing.t2_ns);
    u32 T3_cycles = NS_TO_ESP_CYCLES(timing.t3_ns);
    state->cyclesPerFill =
        (T1_cycles + T2_cycles + T3_cycles) * PULSES_PER_FILL_RMT4;
    state->maxCyclesPerFill = state->cyclesPerFill + state->cyclesPerFill / 2;
    state->lastFill = 0;

    // Register the TX allocation with the RMT4 memory manager (#3469).
    // The manager tracks the global pool (512 words on classic ESP32,
    // 256 on S2), refuses overallocation, and — once
    // NetworkStateTracker4 is wired to a real detector — will return
    // 3 mem blocks instead of 2 during WiFi bursts. Today's stub
    // always returns 2, so behaviour is bit-for-bit identical to the
    // pre-#3469 driver until the follow-up flips the network detector
    // on.
    //
    // Free any prior allocation for this channel first, so Strategy 2
    // (reconfigure an idle channel for a new pin/timing) doesn't hit
    // the duplicate-alloc check. Silent-miss on the first-time-configure
    // path is expected.
    (void)RmtMemoryManager4::instance().free(static_cast<int>(state->channel),
                                             /*is_tx=*/true);

    const bool network_active = NetworkStateTracker4::instance().isActive();
    size_t alloc_words = 0;
    if (!RmtMemoryManager4::instance().tryAllocateTx(
            static_cast<int>(state->channel), network_active, alloc_words)) {
        // Pool exhausted — refuse. The caller (acquireChannel) will
        // treat this the same as a hardware-channel-count miss and
        // defer this strip to the pending queue.
        FL_WARN_F("configureChannel: memory manager refused TX allocation "
                  "on channel %s",
                  state->channel);
        return false;
    }

    // Apply the channel configuration via the peripheral.
    const u8 mem_blocks =
        RmtMemoryManager4::instance().calculateMemoryBlocks(network_active);
    detail::Rmt4ChannelConfig cfg(static_cast<int>(state->channel),
                                  static_cast<int>(pin), DIVIDER_RMT4,
                                  mem_blocks, PULSES_PER_FILL_RMT4);
    if (!mPeripheral->configureChannel(cfg)) {
        RmtMemoryManager4::instance().free(static_cast<int>(state->channel),
                                           /*is_tx=*/true);
        return false;
    }

    // Install the ESP-IDF RMT driver with no internal ring buffer —
    // FastLED's custom ISR owns the double-buffer refill.
    if (!mPeripheral->installDriver(static_cast<int>(state->channel))) {
        return false;
    }

    // Arm the half-buffer threshold interrupt.
    if (!mPeripheral->setTxThresholdIntrEnable(static_cast<int>(state->channel),
                                               true, PULSES_PER_FILL_RMT4)) {
        mPeripheral->uninstallDriver(static_cast<int>(state->channel));
        return false;
    }

    // Direct access to the RMT hardware buffer is part of the ISR hot
    // path and must remain inline here (cannot go through the
    // peripheral abstraction without breaking IRAM placement).
    state->memStart = &(RMTMEM.chan[state->channel].data32[0]);
    state->memPtr = state->memStart;

    // Assign the GPIO via the peripheral.
    if (!mPeripheral->setGpio(static_cast<int>(state->channel),
                              static_cast<int>(pin), false)) {
        mPeripheral->uninstallDriver(static_cast<int>(state->channel));
        return false;
    }

    FL_WARN_F("configureChannel: Configured channel %s on pin %s",
              state->channel, static_cast<int>(pin));

    return true;
}

void ChannelEngineRMT4Impl::processPendingChannels() FL_NO_EXCEPT {
    // Process the pending channel queue and start as many transmissions
    // as the hardware allows (up to FASTLED_RMT_MAX_CHANNELS concurrent)
    //
    // Port of: ESP32RMTController::startNext() flow from idf4_rmt_impl.cpp
    //
    // Time-multiplexing: If >8 strips are enqueued, channels are reused
    // as they complete (managed by pollDerived())

    // Iterate through pending queue (use index because we may remove items)
    for (size_t i = 0; i < mPendingChannels.size();) {
        const ChannelDataPtr &data = mPendingChannels[i];

        // Try to acquire a hardware channel for this strip
        ChannelState *state = acquireChannel(
            static_cast<gpio_num_t>(data->getPin()), data->getTiming());

        if (state == nullptr) {
            // All channels busy - time-multiplexing will resume later in poll()
            FL_WARN_F("processPendingChannels: All %s channels busy, deferring "
                      "%s pending strips",
                      FASTLED_RMT_MAX_CHANNELS, (mPendingChannels.size() - i));
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

void ChannelEngineRMT4Impl::startTransmission(
    ChannelState *state, const ChannelDataPtr &data) FL_NO_EXCEPT {
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
        FL_WARN_F("startTransmission: null state pointer");
        return;
    }

    // Store reference to source ChannelData
    state->sourceData = data;

    // Mark data as in-use (prevents strip driver from modifying during TX)
    data->setInUse(true);

    // Get pointer to encoded pixel data
    const auto &dataBuffer = data->getData();
    state->pixelData = dataBuffer.data();
    state->pixelDataSize = dataBuffer.size();

    // DEBUG: Log pixel data details
    FL_DBG_F(
        "RMT4: startTransmission() called with %s bytes, first 3 bytes: %s %s "
        "%s",
        state->pixelDataSize,
        (state->pixelDataSize > 0 ? static_cast<int>(state->pixelData[0]) : -1),
        (state->pixelDataSize > 1 ? static_cast<int>(state->pixelData[1]) : -1),
        (state->pixelDataSize > 2 ? static_cast<int>(state->pixelData[2])
                                  : -1));

    // Reset transmission state
    state->pixelDataPos = 0;
    state->whichHalf = 0;
    state->memPtr = state->memStart;
    state->transmissionComplete = false;
    state->lastFill = 0;
    state->transmissionStartTime = fl::millis(); // Start timeout timer

    // Fill both halves of the double-buffer
    // (false = skip time check on initial fill)
    fillNextBuffer(state, false);
    fillNextBuffer(state, false);

    // Enable TX interrupts for this channel via the peripheral.
    if (!mPeripheral ||
        !mPeripheral->setTxIntrEnable(static_cast<int>(state->channel), true)) {
        FL_WARN_F("startTransmission: setTxIntrEnable failed on channel %s",
                  state->channel);
        // Mark complete to trigger cleanup in poll()
        state->transmissionComplete = true;
        data->setInUse(false);
        return;
    }

    // Kick off transmission (critical section for register writes)
    portENTER_CRITICAL(&mRmtSpinlock);
    tx_start(state);
    portEXIT_CRITICAL(&mRmtSpinlock);

    FL_DBG_F("RMT4: Transmission started on channel %s, pin %s, %s bytes",
             state->channel, static_cast<int>(state->pin),
             state->pixelDataSize);
}

// Note: findChannelByNumber() and tx_start() are now inlined in the header

// ═══════════════════════════════════════════════════════════════════════════
// IChannelDriver Interface Implementation
// ═══════════════════════════════════════════════════════════════════════════

void ChannelEngineRMT4Impl::enqueue(ChannelDataPtr channelData) FL_NO_EXCEPT {
    // Store channel data for later transmission
    // Called by ChannelManager for each LED strip
    if (channelData) {
        mEnqueuedChannels.push_back(channelData);
    } else {
        FL_WARN_F("enqueue: Received null ChannelData");
    }
}

void ChannelEngineRMT4Impl::show() FL_NO_EXCEPT {
    // Trigger transmission of all enqueued data
    // Called by ChannelManager when user calls FastLED.show()

    FL_WARN_F("show: Transmitting %s enqueued channels",
              mEnqueuedChannels.size());

    if (!mEnqueuedChannels.empty()) {
        // Pass batched data to internal transmission handler
        beginTransmission(fl::span<const ChannelDataPtr>(mEnqueuedChannels));

        // Clear the enqueued batch (data is now in mPendingChannels)
        mEnqueuedChannels.clear();
    }
}

IChannelDriver::DriverState ChannelEngineRMT4Impl::poll() FL_NO_EXCEPT {
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
    for (auto &state : mChannels) {
        if (!state.inUse) {
            continue;
        }

        if (state.transmissionComplete) {
            // Transmission complete - release channel and mark data available
            FL_WARN_F("poll: Channel %s completed", state.channel);

            // Clear in-use flag on source data
            if (state.sourceData) {
                state.sourceData->setInUse(false);
            }

            // Release channel back to idle pool
            releaseChannel(&state);
        } else {
            // Channel still transmitting - check for timeout
#if FASTLED_RMT4_TRANSMISSION_TIMEOUT_MS > 0
            u32 elapsed = fl::millis() - state.transmissionStartTime;
            if (elapsed > FASTLED_RMT4_TRANSMISSION_TIMEOUT_MS) {
                // Timeout detected - force channel reset
                FL_WARN_F("poll: Channel %s timed out after %sms (limit: %sms)",
                          state.channel, elapsed,
                          FASTLED_RMT4_TRANSMISSION_TIMEOUT_MS);

                // Disable interrupts for this channel via the peripheral.
                if (mPeripheral) {
                    mPeripheral->setTxIntrEnable(
                        static_cast<int>(state.channel), false);
                }

                // Disconnect GPIO (kept as direct ROM call — also used
                // from the IRAM ISR, can't go through virtual dispatch).
                gpio_matrix_out(state.pin, SIG_GPIO_OUT_IDX, 0, 0);

                // Clear in-use flag on source data
                if (state.sourceData) {
                    state.sourceData->setInUse(false);
                }

                // Release channel back to idle pool
                releaseChannel(&state);

                anyTimeout = true;
                continue; // Skip anyBusy flag for this channel
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
        return DriverState::ERROR;
    }
    return anyBusy ? DriverState::BUSY : DriverState::READY;
}

void ChannelEngineRMT4Impl::beginTransmission(
    fl::span<const ChannelDataPtr> channelData) FL_NO_EXCEPT {
    // Main entry point for LED data transmission
    // Called by ChannelManager when user calls FastLED.show()
    //
    // Port of: The showPixels() → startNext() flow from legacy RMT4
    //
    // Flow:
    // 1. Block flash operations if enabled (prevents WiFi interference)
    // 2. Clear any previous pending queue
    // 3. Add all channels to pending queue
    // 4. Start as many transmissions as hardware allows
    // 5. Release flash lock after transmission starts

    FL_WARN_F("beginTransmission: Queueing %s channels", channelData.size());

#if FASTLED_ESP32_FLASH_LOCK == 1
// Block flash operations during LED transmission to prevent timing glitches
// This is especially important when WiFi is active
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(4, 0, 0)
    // IDF 4.x+ uses esp_flash_app_disable_protect()
    // Note: This API may not be available on all IDF versions
    // For now, we'll skip the flash lock and document the limitation
    FL_DBG_F("RMT4: Flash lock not yet implemented for IDF 4.x+");
#else
    // IDF 3.x uses spi_flash_op_lock()
    spi_flash_op_lock();
    FL_DBG_F("RMT4: Flash operations locked");
#endif
#endif

    // Clear pending queue (should be empty, but ensure clean state)
    mPendingChannels.clear();

    // Add all channels to pending queue
    for (const auto &data : channelData) {
        if (data) {
            mPendingChannels.push_back(data);
        } else {
            FL_WARN_F("beginTransmission: Null ChannelData in span");
        }
    }

    // Start as many transmissions as hardware allows
    processPendingChannels();

#if FASTLED_ESP32_FLASH_LOCK == 1
#if ESP_IDF_VERSION < ESP_IDF_VERSION_VAL(4, 0, 0)
    // Release flash lock after transmission starts
    spi_flash_op_unlock();
    FL_DBG_F("RMT4: Flash operations unlocked");
#endif
#endif
}

} // namespace fl

#endif // !FASTLED_RMT5 and not RMT5-only chip
#endif // FL_IS_ESP32
