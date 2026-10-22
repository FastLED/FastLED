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
#include "fl/stl/cstring.h"
#include "fl/stl/noexcept.h"
#include "platforms/esp/32/drivers/rmt/rmt_4/channel_driver_rmt4.h"
#include "platforms/esp/32/drivers/rmt/rmt_4/network_state_tracker_4.h"
#include "platforms/esp/32/drivers/rmt/rmt_4/rmt4_peripheral_esp.h"
#include "platforms/esp/32/drivers/rmt/rmt_4/rmt_memory_manager_4.h"
#include "platforms/esp/32/drivers/rmt/rmt_4/rmt4_refill_deadline.h"

FL_EXTERN_C_BEGIN
#include "esp_heap_caps.h"
#include "esp_timer.h"
#include "soc/soc_memory_layout.h" // IWYU pragma: keep
FL_EXTERN_C_END

namespace fl {
namespace {

struct Rmt4Symbols {
    rmt_item32_t zero;
    rmt_item32_t one;
    Rmt4RefillDeadline deadline;
    atomic_bool lateRefill{false};
#if FL_RMT4_DIAGNOSTICS
    atomic<u32> translatedBytes{0};
    atomic<u32> cumulativeTranslatedBytes{0};
    atomic<u32> latchChecks{0};
    atomic<u32> latchViolations{0};
    atomic<u32> minLatchHeldUs{0xffffffffu};
    atomic<u32> maxRequiredResetUs{0};
    atomic<u32> lateAborts{0};
    atomic<u32> maxDeltaCycles{0};
    atomic<u32> abortDeltaCycles{0};
    atomic<u32> allowedDeltaCycles{0};
    atomic<u32> preloadCycles{0};
    atomic<u32> firstIsrCycles{0};
    atomic<u32> lastFillCycles{0};
    atomic<u32> postStartCycles{0};
    atomic<u32> postStartUs{0};
    atomic<u32> firstIsrUs{0};
    atomic<u32> maxDeltaUs{0};
    atomic<u32> abortDeltaUs{0};
    atomic<u32> allowedDeltaUs{0};
    atomic_int preloadCore{-1};
    atomic_int isrCore{-1};
#endif
};

// Translators never retain an engine pointer. The SDK may invoke them with
// caches disabled, so their timing symbols live in internal DRAM.
// FL_LINT_ALLOW_GLOBAL(ISR-only timing storage must stay in internal DRAM without singleton initialization)
DRAM_ATTR Rmt4Symbols sRmt4Symbols[FASTLED_RMT_MAX_CHANNELS];

FL_NO_INLINE IRAM_ATTR void translateRmt4Bytes(
    const void *source, rmt_item32_t *destination, size_t sourceSize,
    size_t wantedItems, size_t *translatedSize, size_t *itemCount,
    Rmt4Symbols &symbols) FL_NO_EXCEPT {
    // Shared hardware time remains comparable if task preload and the SDK's
    // shared ISR run on different cores. CCOUNT is per-core.
    const auto previous = symbols.deadline.previousFill();
    const u32 nowUs = static_cast<u32>(esp_timer_get_time());
    const bool inIsr = xPortInIsrContext();
#if FL_RMT4_DIAGNOSTICS
    const u32 deltaUs = nowUs - previous.timeUs;
    const u32 now = __clock_cycles();
    if (inIsr) {
        if (symbols.isrCore.load(memory_order_relaxed) < 0) {
            symbols.firstIsrCycles.store(now, memory_order_relaxed);
            symbols.firstIsrUs.store(nowUs, memory_order_relaxed);
            symbols.isrCore.store(xPortGetCoreID(), memory_order_relaxed);
        }
    } else {
        symbols.preloadCycles.store(now, memory_order_relaxed);
        symbols.preloadCore.store(xPortGetCoreID(), memory_order_relaxed);
    }
    const u32 previousCycles = symbols.lastFillCycles.load(memory_order_relaxed);
    const u32 delta = now - previousCycles;
    if (previousCycles != 0 &&
        delta > symbols.maxDeltaCycles.load(memory_order_relaxed)) {
        symbols.maxDeltaCycles.store(delta, memory_order_relaxed);
    }
    if (previous.armed && deltaUs > symbols.maxDeltaUs.load(memory_order_relaxed)) {
        symbols.maxDeltaUs.store(deltaUs, memory_order_relaxed);
    }
#endif
    if (!symbols.deadline.tryRefill(previous, nowUs, inIsr)) {
        // Preserve the legacy late-refill safeguard: end this frame rather
        // than knowingly continue after a missed hardware buffer deadline.
        *translatedSize = sourceSize;
        *itemCount = 0;
#if FL_RMT4_DIAGNOSTICS
        symbols.abortDeltaCycles.store(delta, memory_order_relaxed);
        symbols.abortDeltaUs.store(deltaUs, memory_order_relaxed);
        symbols.lateAborts.store(
            symbols.lateAborts.load(memory_order_relaxed) + 1,
            memory_order_relaxed);
#endif
        symbols.lateRefill.store(true, memory_order_release);
        return;
    }
#if FL_RMT4_DIAGNOSTICS
    symbols.lastFillCycles.store(now, memory_order_relaxed);
#endif
    const auto *bytes = static_cast<const u8 *>(source);
    size_t count = wantedItems / 8;
    if (count > sourceSize) {
        count = sourceSize;
    }
    for (size_t i = 0; i < count; ++i) {
        const u8 value = bytes[i];
        for (u8 bit = 0; bit < 8; ++bit) {
            destination[i * 8 + bit].val =
                (value & (0x80u >> bit)) ? symbols.one.val : symbols.zero.val;
        }
    }
    *translatedSize = count;
    *itemCount = count * 8;
#if FL_RMT4_DIAGNOSTICS
    symbols.translatedBytes.store(
        symbols.translatedBytes.load(memory_order_relaxed) + count,
        memory_order_relaxed);
    symbols.cumulativeTranslatedBytes.store(
        symbols.cumulativeTranslatedBytes.load(memory_order_relaxed) + count,
        memory_order_relaxed);
#endif
}

#if FASTLED_RMT_MAX_CHANNELS > 0
void IRAM_ATTR translateRmt4Channel0(const void *source, rmt_item32_t *destination,
                                    size_t sourceSize, size_t wantedItems,
                                    size_t *translatedSize, size_t *itemCount) FL_NO_EXCEPT {
    translateRmt4Bytes(source, destination, sourceSize, wantedItems,
                       translatedSize, itemCount, sRmt4Symbols[0]);
}
#endif
#if FASTLED_RMT_MAX_CHANNELS > 1
void IRAM_ATTR translateRmt4Channel1(const void *source, rmt_item32_t *destination,
                                    size_t sourceSize, size_t wantedItems,
                                    size_t *translatedSize, size_t *itemCount) FL_NO_EXCEPT {
    translateRmt4Bytes(source, destination, sourceSize, wantedItems,
                       translatedSize, itemCount, sRmt4Symbols[1]);
}
#endif
#if FASTLED_RMT_MAX_CHANNELS > 2
void IRAM_ATTR translateRmt4Channel2(const void *source, rmt_item32_t *destination,
                                    size_t sourceSize, size_t wantedItems,
                                    size_t *translatedSize, size_t *itemCount) FL_NO_EXCEPT {
    translateRmt4Bytes(source, destination, sourceSize, wantedItems,
                       translatedSize, itemCount, sRmt4Symbols[2]);
}
#endif
#if FASTLED_RMT_MAX_CHANNELS > 3
void IRAM_ATTR translateRmt4Channel3(const void *source, rmt_item32_t *destination,
                                    size_t sourceSize, size_t wantedItems,
                                    size_t *translatedSize, size_t *itemCount) FL_NO_EXCEPT {
    translateRmt4Bytes(source, destination, sourceSize, wantedItems,
                       translatedSize, itemCount, sRmt4Symbols[3]);
}
#endif
#if FASTLED_RMT_MAX_CHANNELS > 4
void IRAM_ATTR translateRmt4Channel4(const void *source, rmt_item32_t *destination,
                                    size_t sourceSize, size_t wantedItems,
                                    size_t *translatedSize, size_t *itemCount) FL_NO_EXCEPT {
    translateRmt4Bytes(source, destination, sourceSize, wantedItems,
                       translatedSize, itemCount, sRmt4Symbols[4]);
}
#endif
#if FASTLED_RMT_MAX_CHANNELS > 5
void IRAM_ATTR translateRmt4Channel5(const void *source, rmt_item32_t *destination,
                                    size_t sourceSize, size_t wantedItems,
                                    size_t *translatedSize, size_t *itemCount) FL_NO_EXCEPT {
    translateRmt4Bytes(source, destination, sourceSize, wantedItems,
                       translatedSize, itemCount, sRmt4Symbols[5]);
}
#endif
#if FASTLED_RMT_MAX_CHANNELS > 6
void IRAM_ATTR translateRmt4Channel6(const void *source, rmt_item32_t *destination,
                                    size_t sourceSize, size_t wantedItems,
                                    size_t *translatedSize, size_t *itemCount) FL_NO_EXCEPT {
    translateRmt4Bytes(source, destination, sourceSize, wantedItems,
                       translatedSize, itemCount, sRmt4Symbols[6]);
}
#endif
#if FASTLED_RMT_MAX_CHANNELS > 7
void IRAM_ATTR translateRmt4Channel7(const void *source, rmt_item32_t *destination,
                                    size_t sourceSize, size_t wantedItems,
                                    size_t *translatedSize, size_t *itemCount) FL_NO_EXCEPT {
    translateRmt4Bytes(source, destination, sourceSize, wantedItems,
                       translatedSize, itemCount, sRmt4Symbols[7]);
}
#endif

sample_to_rmt_t translatorForChannel(int channel) FL_NO_EXCEPT {
    switch (channel) {
#if FASTLED_RMT_MAX_CHANNELS > 0
    case 0: return &translateRmt4Channel0;
#endif
#if FASTLED_RMT_MAX_CHANNELS > 1
    case 1: return &translateRmt4Channel1;
#endif
#if FASTLED_RMT_MAX_CHANNELS > 2
    case 2: return &translateRmt4Channel2;
#endif
#if FASTLED_RMT_MAX_CHANNELS > 3
    case 3: return &translateRmt4Channel3;
#endif
#if FASTLED_RMT_MAX_CHANNELS > 4
    case 4: return &translateRmt4Channel4;
#endif
#if FASTLED_RMT_MAX_CHANNELS > 5
    case 5: return &translateRmt4Channel5;
#endif
#if FASTLED_RMT_MAX_CHANNELS > 6
    case 6: return &translateRmt4Channel6;
#endif
#if FASTLED_RMT_MAX_CHANNELS > 7
    case 7: return &translateRmt4Channel7;
#endif
    default: return nullptr;
    }
}

} // namespace

ChannelEngineRMT4Ptr ChannelEngineRMT4::create() FL_NO_EXCEPT {
    return fl::make_shared<ChannelEngineRMT4Impl>();
}

ChannelEngineRMT4Ptr ChannelEngineRMT4::create(
    fl::shared_ptr<detail::IRMT4Peripheral> peripheral) FL_NO_EXCEPT {
    return fl::make_shared<ChannelEngineRMT4Impl>(fl::move(peripheral));
}

ChannelEngineRMT4Impl::ChannelEngineRMT4Impl() FL_NO_EXCEPT
    : ChannelEngineRMT4Impl(fl::make_shared<Rmt4PeripheralESP>()) {}

ChannelEngineRMT4Impl::ChannelEngineRMT4Impl(
    fl::shared_ptr<detail::IRMT4Peripheral> peripheral) FL_NO_EXCEPT
    : mPeripheral(fl::move(peripheral)) {
    mChannels.reserve(FASTLED_RMT_MAX_CHANNELS);
}

ChannelEngineRMT4Impl::~ChannelEngineRMT4Impl() {
    for (auto &state : mChannels) {
        destroyChannel(&state);
    }
    clearPendingChannels();
    for (auto &data : mEnqueuedChannels) { data->setInUse(false); }
    unlockFlash();
}

void ChannelEngineRMT4Impl::clearPendingChannels() FL_NO_EXCEPT {
    for (auto &data : mPendingChannels) { data->setInUse(false); }
    mPendingChannels.clear();
}

void ChannelEngineRMT4Impl::InternalSourceDeleter::operator()(u8 *pointer) const FL_NO_EXCEPT {
    heap_caps_free(pointer);
}

void ChannelEngineRMT4Impl::lockFlash() FL_NO_EXCEPT {
#if FASTLED_ESP32_FLASH_LOCK == 1 && ESP_IDF_VERSION < ESP_IDF_VERSION_VAL(4, 0, 0)
    if (!mFlashLocked) {
        spi_flash_op_lock();
        mFlashLocked = true;
    }
#endif
}

void ChannelEngineRMT4Impl::unlockFlash() FL_NO_EXCEPT {
#if FASTLED_ESP32_FLASH_LOCK == 1 && ESP_IDF_VERSION < ESP_IDF_VERSION_VAL(4, 0, 0)
    if (mFlashLocked) {
        spi_flash_op_unlock();
        mFlashLocked = false;
    }
#endif
}

bool ChannelEngineRMT4Impl::canHandle(const ChannelDataPtr &data) const FL_NO_EXCEPT {
    return data && !data->isSpi();
}

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


void ChannelEngineRMT4Impl::destroyChannel(ChannelState *state) FL_NO_EXCEPT {
    if (state->configured) {
        // Stop the SDK ISR before releasing its source buffer or memory span.
        rmt_tx_stop(state->channel);
        gpio_set_level(state->pin, 0);
        gpio_matrix_out(state->pin, SIG_GPIO_OUT_IDX, false, false);
        mPeripheral->uninstallDriver(static_cast<int>(state->channel));
        auto &manager = RmtMemoryManager4::instance();
        manager.releaseBlocks(static_cast<int>(state->channel), state->memBlocks);
        manager.free(static_cast<int>(state->channel), true);
        state->configured = false;
    }
    releaseChannel(state);
}

void ChannelEngineRMT4Impl::releaseChannel(ChannelState *state) FL_NO_EXCEPT {
#if FL_RMT4_DIAGNOSTICS
    // Count only normal, nonempty SDK transmissions. Teardown has already
    // cleared configured; empty frames start a reset timer without SDK TX.
    if (state->configured && state->resetWaitStarted && state->sourceData &&
        state->sourceData->getSize() != 0) {
        auto &symbols = sRmt4Symbols[state->channel];
        const u32 heldUs = fl::micros() - state->resetStartTimeUs;
        symbols.latchChecks.store(symbols.latchChecks.load(memory_order_relaxed) + 1,
                                  memory_order_relaxed);
        if (heldUs < state->resetDurationUs) {
            symbols.latchViolations.store(
                symbols.latchViolations.load(memory_order_relaxed) + 1,
                memory_order_relaxed);
        }
        if (heldUs < symbols.minLatchHeldUs.load(memory_order_relaxed)) {
            symbols.minLatchHeldUs.store(heldUs, memory_order_relaxed);
        }
        if (state->resetDurationUs > symbols.maxRequiredResetUs.load(memory_order_relaxed)) {
            symbols.maxRequiredResetUs.store(state->resetDurationUs, memory_order_relaxed);
        }
    }
#endif
    state->internalSource.reset();
    if (state->sourceData) {
        state->sourceData->setInUse(false);
        state->sourceData.reset();
    }
    // Leave the configured idle-low signal attached through the latch interval.
    // Disconnect only once the channel is ready for another pin.
    if (state->configured) {
        gpio_set_level(state->pin, 0);
        gpio_matrix_out(state->pin, SIG_GPIO_OUT_IDX, false, false);
    }
    state->inUse = false;
    state->resetWaitStarted = false;
}

bool ChannelEngineRMT4Impl::configureChannel(
    ChannelState *state, gpio_num_t pin,
    const ChipsetTimingConfig &timing) FL_NO_EXCEPT {
    const int channel = static_cast<int>(state->channel);
    const detail::Rmt4ChannelConfig config(channel, static_cast<int>(pin),
        DIVIDER_RMT4, state->memBlocks,
        FASTLED_RMT_MEM_WORDS_PER_CHANNEL * state->memBlocks / 2);
    if (!mPeripheral->configureChannel(config) ||
        !mPeripheral->installDriver(channel)) {
        return false;
    }
    if (rmt_translator_init(state->channel, translatorForChannel(channel)) != ESP_OK) {
        mPeripheral->uninstallDriver(channel);
        return false;
    }
    state->configured = true;
    state->pin = pin;
    sRmt4Symbols[channel].zero = makeZeroSymbol(timing);
    sRmt4Symbols[channel].one = makeOneSymbol(timing);
    const u32 nanosecondsPerHalf = (timing.t1_ns + timing.t2_ns + timing.t3_ns) *
        (FASTLED_RMT_MEM_WORDS_PER_CHANNEL * state->memBlocks / 2);
    sRmt4Symbols[channel].deadline.maxRefillUs =
        (nanosecondsPerHalf + nanosecondsPerHalf / 2 + 999) / 1000;
    return true;
}

ChannelEngineRMT4Impl::ChannelState *ChannelEngineRMT4Impl::acquireChannel(
    gpio_num_t pin, const ChipsetTimingConfig &timing) FL_NO_EXCEPT {
    if (!mPeripheral) {
        mError = true;
        return nullptr;
    }
    // A removed controller can be replaced on the same GPIO before its
    // retained frame completes. Keep the replacement queued until that pad's
    // previous transmission and latch interval finish.
    for (const auto &state : mChannels) {
        if (state.inUse && state.pin == pin) {
            return nullptr;
        }
    }
    for (auto &state : mChannels) {
        if (state.configured && !state.inUse) {
            // A completed channel's GPIO was disconnected at release. Always
            // reconnect, including when its next strip uses the same pin.
            if (!mPeripheral->setGpio(static_cast<int>(state.channel),
                                      static_cast<int>(pin), false)) {
                mError = true;
                return nullptr;
            }
            state.pin = pin;
            sRmt4Symbols[state.channel].zero = makeZeroSymbol(timing);
            sRmt4Symbols[state.channel].one = makeOneSymbol(timing);
            const u32 nanosecondsPerHalf = (timing.t1_ns + timing.t2_ns + timing.t3_ns) *
                (FASTLED_RMT_MEM_WORDS_PER_CHANNEL * state.memBlocks / 2);
            sRmt4Symbols[state.channel].deadline.maxRefillUs =
                (nanosecondsPerHalf + nanosecondsPerHalf / 2 + 999) / 1000;
            state.inUse = true;
            return &state;
        }
    }

    auto &manager = RmtMemoryManager4::instance();
    for (int channel = 0; channel < FASTLED_RMT_MAX_CHANNELS; ++channel) {
        size_t allocatedWords = 0;
        const bool networkActive = NetworkStateTracker4::instance().isActive();
        // The ledger chooses two or three blocks, including demotion when the
        // pool is tight. Physical reservation then rejects overlapping spans.
        if (!manager.tryAllocateTx(channel, networkActive, allocatedWords)) {
            continue;
        }
        const u8 blocks = static_cast<u8>(allocatedWords / manager.wordsPerBlock());
        if (!manager.tryReserveBlocks(channel, blocks)) {
            manager.free(channel, true);
            continue;
        }
        ChannelState state;
        state.channel = static_cast<rmt_channel_t>(channel);
        state.memBlocks = blocks;
        if (!configureChannel(&state, pin, timing)) {
            manager.releaseBlocks(channel, blocks);
            manager.free(channel, true);
            mError = true;
            return nullptr;
        }
        state.inUse = true;
        // Dead timeout slots can be reused without growing the state vector.
        for (auto &old : mChannels) {
            if (!old.configured) {
                old = fl::move(state);
                return &old;
            }
        }
        const size_t previous = mChannels.size();
        mChannels.push_back(fl::move(state));
        if (mChannels.size() == previous) {
            destroyChannel(&state);
            mError = true;
            return nullptr;
        }
        return &mChannels.back();
    }
    return nullptr;
}

void ChannelEngineRMT4Impl::startTransmission(
    ChannelState *state, const ChannelDataPtr &data) FL_NO_EXCEPT {
    state->sourceData = data;
    data->setInUse(true);
    state->resetWaitStarted = false;
    state->resetDurationUs = data->getTiming().reset_us;
    state->transmissionStartTime = fl::millis();
    auto &diagnostics = sRmt4Symbols[state->channel];
    diagnostics.deadline.reset(diagnostics.deadline.maxRefillUs);
    diagnostics.lateRefill.store(false, memory_order_relaxed);
#if FL_RMT4_DIAGNOSTICS
    diagnostics.translatedBytes.store(0, memory_order_relaxed);
    diagnostics.lateAborts.store(0, memory_order_relaxed);
    diagnostics.maxDeltaCycles.store(0, memory_order_relaxed);
    diagnostics.abortDeltaCycles.store(0, memory_order_relaxed);
    diagnostics.allowedDeltaCycles.store(diagnostics.deadline.maxRefillUs * (F_CPU / 1000000UL), memory_order_relaxed);
    diagnostics.preloadCycles.store(0, memory_order_relaxed);
    diagnostics.firstIsrCycles.store(0, memory_order_relaxed);
    diagnostics.lastFillCycles.store(0, memory_order_relaxed);
    diagnostics.postStartCycles.store(0, memory_order_relaxed);
    diagnostics.postStartUs.store(0, memory_order_relaxed);
    diagnostics.firstIsrUs.store(0, memory_order_relaxed);
    diagnostics.maxDeltaUs.store(0, memory_order_relaxed);
    diagnostics.abortDeltaUs.store(0, memory_order_relaxed);
    diagnostics.allowedDeltaUs.store(diagnostics.deadline.maxRefillUs, memory_order_relaxed);
    diagnostics.preloadCore.store(-1, memory_order_relaxed);
    diagnostics.isrCore.store(-1, memory_order_relaxed);
#endif
    const auto &bytes = data->getData();
    if (bytes.empty()) {
        state->resetWaitStarted = true;
        state->resetStartTimeUs = fl::micros();
        return;
    }
    const u8 *source = bytes.data();
    if (!esp_ptr_internal(source)) {
        // ChannelData can use PSRAM. The IRAM translator must never read
        // cache-backed source memory while flash operations disable caches.
        auto *copy = static_cast<u8 *>(heap_caps_malloc(
            bytes.size(), MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT));
        if (!copy) {
            mError = true;
            destroyChannel(state);
            return;
        }
        state->internalSource = fl::make_shared<InternalSource>(copy);
        if (!state->internalSource) {
            heap_caps_free(copy);
            mError = true;
            destroyChannel(state);
            return;
        }
        fl::memcpy(copy, source, bytes.size());
        source = copy;
    }
    if (rmt_write_sample(state->channel, source, bytes.size(), false) != ESP_OK) {
        mError = true;
        destroyChannel(state);
        return;
    }
    // The SDK performs task-context preload before it starts hardware. Exclude
    // that setup time from the first refill deadline. Preserve an ISR's newer
    // timestamp if it already ran before rmt_write_sample returned.
    const u32 startedUs = static_cast<u32>(esp_timer_get_time());
    diagnostics.deadline.recordHardwareStart(startedUs);
#if FL_RMT4_DIAGNOSTICS
    diagnostics.postStartCycles.store(__clock_cycles(), memory_order_relaxed);
    diagnostics.postStartUs.store(startedUs, memory_order_relaxed);
#endif
}

void ChannelEngineRMT4Impl::processPendingChannels() FL_NO_EXCEPT {
    for (size_t i = 0; i < mPendingChannels.size();) {
        const ChannelDataPtr &data = mPendingChannels[i];
        ChannelState *state = acquireChannel(static_cast<gpio_num_t>(data->getPin()),
                                              data->getTiming());
        if (!state) {
            bool active = false;
            for (const auto &channel : mChannels) {
                active |= channel.inUse;
            }
            if (!active) {
                // RX or another owner can reserve the entire physical pool.
                // Report failure instead of waiting forever with no TX active.
                mError = true;
            }
            break;
        }
        startTransmission(state, data);
        mPendingChannels[i] = mPendingChannels.back();
        mPendingChannels.pop_back();
    }
    if (mError) {
        // A hard setup failure must reach the manager instead of leaving a
        // permanently BUSY pending queue that can never acquire a channel.
        clearPendingChannels();
    }
}

void ChannelEngineRMT4Impl::enqueue(ChannelDataPtr data) FL_NO_EXCEPT {
    if (data) {
        const size_t previous = mEnqueuedChannels.size();
        mEnqueuedChannels.push_back(fl::move(data));
        if (mEnqueuedChannels.size() == previous) {
            data->setInUse(false);
            mError = true;
        }
    } else {
        mError = true;
    }
}

void ChannelEngineRMT4Impl::show() FL_NO_EXCEPT {
    lockFlash();
    for (auto &data : mEnqueuedChannels) {
        const size_t previous = mPendingChannels.size();
        mPendingChannels.push_back(fl::move(data));
        if (mPendingChannels.size() == previous) {
            data->setInUse(false);
            mError = true;
        }
    }
    mEnqueuedChannels.clear();
    if (mError) {
        clearPendingChannels();
        return;
    }
    processPendingChannels();
}

IChannelDriver::DriverState ChannelEngineRMT4Impl::poll() FL_NO_EXCEPT {
    for (auto &state : mChannels) {
        if (!state.inUse) {
            continue;
        }
        if (sRmt4Symbols[state.channel].lateRefill.exchange(false, memory_order_acquire)) {
            destroyChannel(&state);
            mError = true;
            continue;
        }
        if (!state.resetWaitStarted && rmt_wait_tx_done(state.channel, 0) == ESP_OK) {
            state.resetWaitStarted = true;
            state.resetStartTimeUs = fl::micros();
        }
        // Completion can race the first flag load. Once TX is done the ISR
        // cannot publish another late refill for this frame.
        if (state.resetWaitStarted &&
            sRmt4Symbols[state.channel].lateRefill.exchange(false, memory_order_acquire)) {
            destroyChannel(&state);
            mError = true;
            continue;
        }
        if (state.resetWaitStarted) {
            if (fl::micros() - state.resetStartTimeUs >= state.resetDurationUs) {
                releaseChannel(&state);
            }
            continue;
        }
#if FASTLED_RMT4_TRANSMISSION_TIMEOUT_MS > 0
        if (fl::millis() - state.transmissionStartTime > FASTLED_RMT4_TRANSMISSION_TIMEOUT_MS) {
            // Stopping alone leaves the SDK's TX semaphore taken. Uninstall
            // before reuse so a timed-out channel cannot deadlock its next TX.
            destroyChannel(&state);
            mError = true;
        }
#endif
    }
    if (mError) {
        clearPendingChannels();
        mError = false;
        bool active = false;
        for (const auto &state : mChannels) {
            active |= state.inUse;
        }
        if (!active) {
            unlockFlash();
        }
        return DriverState::ERROR;
    }
    processPendingChannels();
    if (mError) {
        mError = false;
        bool active = false;
        for (const auto &state : mChannels) {
            active |= state.inUse;
        }
        if (!active) {
            unlockFlash();
        }
        return DriverState::ERROR;
    }
    for (const auto &state : mChannels) {
        if (state.inUse) {
            return DriverState::BUSY;
        }
    }
    if (mPendingChannels.empty()) {
        unlockFlash();
    }
    return mPendingChannels.empty() ? DriverState::READY : DriverState::BUSY;
}

Rmt4TxDiagnostics ChannelEngineRMT4Impl::txDiagnostics(int channel) const FL_NO_EXCEPT {
    Rmt4TxDiagnostics result;
#if FL_RMT4_DIAGNOSTICS
    for (int index = 0; index < FASTLED_RMT_MAX_CHANNELS; ++index) {
        if (channel >= 0 && channel != index) { continue; }
        const auto &symbols = sRmt4Symbols[index];
        const u32 bytes = symbols.translatedBytes.load(memory_order_acquire);
        result.translatedBytes += bytes;
        result.cumulativeTranslatedBytes +=
            symbols.cumulativeTranslatedBytes.load(memory_order_acquire);
        const u32 latchChecks = symbols.latchChecks.load(memory_order_acquire);
        result.latchChecks += latchChecks;
        result.latchViolations += symbols.latchViolations.load(memory_order_acquire);
        if (latchChecks) {
            const u32 heldUs = symbols.minLatchHeldUs.load(memory_order_acquire);
            if (result.minLatchHeldUs == 0 || heldUs < result.minLatchHeldUs) {
                result.minLatchHeldUs = heldUs;
            }
            const u32 requiredUs = symbols.maxRequiredResetUs.load(memory_order_acquire);
            if (requiredUs > result.maxRequiredResetUs) {
                result.maxRequiredResetUs = requiredUs;
            }
        }
        result.lateAborts += symbols.lateAborts.load(memory_order_acquire);
        const u32 maximum = symbols.maxDeltaCycles.load(memory_order_acquire);
        if (maximum > result.maxDeltaCycles) { result.maxDeltaCycles = maximum; }
        if (bytes) {
            result.abortDeltaCycles = symbols.abortDeltaCycles.load(memory_order_acquire);
            result.allowedDeltaCycles = symbols.allowedDeltaCycles.load(memory_order_acquire);
            result.preloadCycles = symbols.preloadCycles.load(memory_order_acquire);
            result.firstIsrCycles = symbols.firstIsrCycles.load(memory_order_acquire);
            result.postStartCycles = symbols.postStartCycles.load(memory_order_acquire);
            result.postStartUs = symbols.postStartUs.load(memory_order_acquire);
            result.firstIsrUs = symbols.firstIsrUs.load(memory_order_acquire);
            result.maxDeltaUs = symbols.maxDeltaUs.load(memory_order_acquire);
            result.abortDeltaUs = symbols.abortDeltaUs.load(memory_order_acquire);
            result.allowedDeltaUs = symbols.allowedDeltaUs.load(memory_order_acquire);
            result.preloadCore = symbols.preloadCore.load(memory_order_acquire);
            result.isrCore = symbols.isrCore.load(memory_order_acquire);
        }
    }
#else
    (void)channel;
#endif
    return result;
}

} // namespace fl

#endif // !FASTLED_RMT5 and not RMT5-only chip
#endif // FL_IS_ESP32
