// IWYU pragma: private

/// @file channel_driver_lcd_clockless.cpp.hpp
/// @brief LCD_CAM I80 clockless channel driver implementation
///
/// ISR-driven chunked DMA for clockless LED chipsets (WS2812, SK6812).
/// Replaces the misnamed "I2S" driver on ESP32-S3.
/// See channel_driver_lcd_clockless.h and issue #2258.

#include "platforms/is_platform.h"

#if defined(FASTLED_STUB_IMPL) || \
    (!defined(ARDUINO) &&          \
     (defined(__linux__) || defined(__APPLE__) || defined(_WIN32)))
#define FL_LCD_CLOCKLESS_COMPILE 1
#elif defined(FL_IS_ESP32)
#include "sdkconfig.h"
#include "platforms/esp/32/feature_flags/enabled.h"
#if FASTLED_ESP32_HAS_LCD_SPI
#define FL_LCD_CLOCKLESS_COMPILE 1
#endif
#endif

#ifdef FL_LCD_CLOCKLESS_COMPILE

#include "platforms/esp/32/drivers/lcd_spi/channel_driver_lcd_clockless.h"
#include "fl/channels/detail/bit_spread_lut.hpp"
#include "fl/channels/engine_worker_defaults.h"
#include "fl/chipsets/led_timing.h"
#include "fl/log/log.h"
#include "fl/task/executor.h"
#include "fl/stl/noexcept.h"
#include "fl/stl/align.h"
#include "fl/stl/static_assert.h"
#include "platforms/memory_barrier.h"

#if defined(FL_IS_ESP32) && !defined(FASTLED_STUB_IMPL)
#include "freertos/FreeRTOS.h"  // IWYU pragma: keep
#include "freertos/task.h"      // IWYU pragma: keep
#endif

// Include ESP peripheral for factory function
#if defined(FL_IS_ESP_32S3)
#include "fl/stl/has_include.h"
#if FL_HAS_INCLUDE("esp_lcd_panel_io.h")
// IWYU pragma: begin_keep
#include "platforms/esp/32/drivers/lcd_spi/lcd_spi_peripheral_esp.h"
// IWYU pragma: end_keep
#define FL_LCD_CLOCKLESS_HAS_ESP_PERIPHERAL 1
#endif
#endif
#ifndef FL_LCD_CLOCKLESS_HAS_ESP_PERIPHERAL
#define FL_LCD_CLOCKLESS_HAS_ESP_PERIPHERAL 0
#endif

namespace fl {

namespace {
#if defined(FL_IS_ESP32) && !defined(FASTLED_STUB_IMPL)
#if defined(CONFIG_FREERTOS_UNICORE) && CONFIG_FREERTOS_UNICORE
constexpr int kLcdWorkerCoreCount = 1;
#elif defined(configNUMBER_OF_CORES)
constexpr int kLcdWorkerCoreCount = configNUMBER_OF_CORES;
#else
constexpr int kLcdWorkerCoreCount = portNUM_PROCESSORS;
#endif
constexpr int kLcdWorkerPriorityCount = configMAX_PRIORITIES;
#else
// Host tests exercise the ESP32-S3 configuration contract without an RTOS.
constexpr int kLcdWorkerCoreCount = 2;
constexpr int kLcdWorkerPriorityCount = 25;
#endif
constexpr int kLcdWorkerDefaultPriority =
    (kLcdWorkerPriorityCount > 2) ? (kLcdWorkerPriorityCount - 2) : 1;

// Keep scheduler-specific numbers at the platform boundary. Auto retains
// the existing high-priority LCD/I2S worker policy; no extra task/state is
// allocated for the named policies.
FL_STATIC_ASSERT(kLcdWorkerPriorityCount > 1 && kLcdWorkerPriorityCount <= 256,
                 "LCD worker priorities must fit in a byte");

FL_STATIC_ASSERT(static_cast<fl::i32>(detail::kEngineWorkerDefaultCore) < kLcdWorkerCoreCount,
                 "FL_ENGINE_WORKER_CORE selects an unavailable LCD worker core");

constexpr Core resolveLcdWorkerCore(Core core) FL_NO_EXCEPT {
    return core != Core::Auto ? core
        : detail::kEngineWorkerDefaultCore == Core::Auto ? Core::Any
        : detail::kEngineWorkerDefaultCore;
}

constexpr Priority lcdWorkerPriorityPolicy(Priority priority) FL_NO_EXCEPT {
    return priority != Priority::Auto ? priority : detail::kEngineWorkerDefaultPriority;
}

constexpr u8 resolveLcdWorkerPriority(Priority priority) FL_NO_EXCEPT {
    // Configuration validates the enum before storage. Group the two high
    // policies as a range so size-optimized GCC need not emit a lookup table.
    return lcdWorkerPriorityPolicy(priority) <= Priority::High ? kLcdWorkerDefaultPriority
           : lcdWorkerPriorityPolicy(priority) == Priority::Medium ? kLcdWorkerPriorityCount / 2 : 1;
}

void recordWorkerMaximum(fl::atomic_u32 &maximum, u64 elapsedUs) FL_NO_EXCEPT {
    const u32 elapsed = elapsedUs > 0xffffffffULL
                            ? 0xffffffffu : static_cast<u32>(elapsedUs);
    u32 current = maximum.load(fl::memory_order_relaxed);
    while (current < elapsed &&
           !maximum.compare_exchange_weak(current, elapsed,
                                          fl::memory_order_relaxed)) {}
}

FASTLED_FORCE_INLINE FL_IRAM
void encodeWave8Words(const u8 (&lanes)[16],
                      const u32 (&zeroPairs)[4],
                      const u32 (&xorPairs)[4],
                      u16 (&output)[64]) FL_NO_EXCEPT {
    u8 columns[16];
    detail::spread_transpose16_symbol(lanes, columns);
    // Ring allocations are at least 64-byte aligned, and every byte's DMA
    // window is 128 bytes. Constant-size memcpy keeps u16 storage alias-safe
    // while allowing an aligned native u32 store for each pair of LCD ticks.
    u16 *dma = fl::assume_aligned<4>(output);
    size_t wordIndex = 0;
    for (size_t bit = 0; bit < 8; ++bit) {
        u32 mask = static_cast<u32>(columns[bit * 2]) |
                   (static_cast<u32>(columns[bit * 2 + 1]) << 8);
        mask |= mask << 16;
        for (size_t pair = 0; pair < 4; ++pair) {
            const u32 word = zeroPairs[pair] ^ (mask & xorPairs[pair]);
            FL_BUILTIN_MEMCPY(dma + wordIndex, &word, sizeof(word));
            wordIndex += 2;
        }
    }
}
} // namespace

//=============================================================================
// Constructor / Destructor
//=============================================================================

ChannelDriverLcdClockless::ChannelDriverLcdClockless(
    fl::shared_ptr<detail::ILcdSpiPeripheral> peripheral) FL_NO_EXCEPT
    : mPeripheral(fl::move(peripheral)), mInitialized(false),
      mStartError(StartError::None),
      mEnqueuedChannels(), mTransmittingChannels(),
      mRingBuffers{nullptr, nullptr, nullptr}, mRingCapacity(0),
      mNumLanes(0), mBusy(false), mIsrCtx(), mChunkInputBytesOverride(0),
      mWorkerTaskHandle(nullptr), mWorkerStop(false), mWorkerRunning(false),
      mWorkerConfig(), mWorkerCore(-2), mWorkerObservedCore(-1),
      mWorkerMaxEncodeUs(0), mWorkerMaxSubmitUs(0), mWorkerStreamError(false),
      mPollNeededCallback(), mUseWave3(true), mWave3Lut(), mWave8Lut(), mClockHz(0),
      mOutputBytesPerInputByte(0) {
    mIsrCtx.reset();
}

ChannelDriverLcdClockless::~ChannelDriverLcdClockless() {
    if (mBusy && mPeripheral) {
        // Wait up to 2 s for DMA to drain. If the peripheral is still busy
        // after that, it is either a stuck controller on real hardware or a
        // mock with no auto-completion. Free either way: LeakSanitizer in
        // unit tests flags the alternative, and a 2 s stall already means
        // something is badly wrong so leaking is not a meaningful safety net.
        bool done = mPeripheral->waitTransmitDone(2000);
        mBusy = false;
        if (!done) {
            FL_WARN("ChannelDriverLcdClockless: DMA wait timed out â€” "
                    "freeing ring buffers anyway");
        }
    }
    stopWorkerTask();
    freeRingBuffers();
}

//=============================================================================
// Ring Buffer Management
//=============================================================================

void ChannelDriverLcdClockless::freeRingBuffers() FL_NO_EXCEPT {
    for (size_t i = 0; i < kRingBufferCount; i++) {
        if (mPeripheral && mRingBuffers[i] != nullptr) {
            mPeripheral->freeBuffer(mRingBuffers[i]);
            mRingBuffers[i] = nullptr;
        }
    }
    mRingCapacity = 0;
}

bool ChannelDriverLcdClockless::allocateRingBuffers(
    size_t slotCapacityBytes) FL_NO_EXCEPT {
    if (mRingCapacity >= slotCapacityBytes && mRingBuffers[0] != nullptr) {
        return true;
    }

    freeRingBuffers();

    for (size_t i = 0; i < kRingBufferCount; i++) {
        mRingBuffers[i] = mPeripheral->allocateBuffer(slotCapacityBytes);
        if (mRingBuffers[i] == nullptr) {
            FL_WARN("ChannelDriverLcdClockless: ring buffer alloc failed");
            freeRingBuffers();
            return false;
        }
    }
    mRingCapacity = slotCapacityBytes;
    return true;
}

//=============================================================================
// Worker Task
//=============================================================================

bool ChannelDriverLcdClockless::configureWorkerImpl(
    const WorkerConfig &config) FL_NO_EXCEPT {
    if (config.isDefault()) {
        return true;
    }

    const i8 requestedCore = static_cast<i8>(config.core);
    if (config.core != Core::Auto && config.core != Core::Any &&
        config.core != Core::CallingCore &&
        (requestedCore < 0 || requestedCore >= kLcdWorkerCoreCount)) {
        return false;
    }
    if (static_cast<u8>(config.priority) > static_cast<u8>(Priority::Low)) {
        return false;
    }

    const Core currentCore = resolveLcdWorkerCore(mWorkerConfig.core);
    const int currentPriority = resolveLcdWorkerPriority(mWorkerConfig.priority);
    const Core core = config.core == Core::Auto ? currentCore : config.core;
    const int priority = config.priority == Priority::Auto
                             ? currentPriority : resolveLcdWorkerPriority(config.priority);

    // One LCD peripheral owns one worker. A second strip cannot change its
    // first explicit policy, including before the task has been created.
    if (!mWorkerConfig.isDefault() || mWorkerCore != -2) {
        return core == currentCore && priority == currentPriority;
    }

    mWorkerConfig.core = core;
    if (config.priority != Priority::Auto) {
        mWorkerConfig.priority = config.priority;
    }
    return true;
}

int ChannelDriverLcdClockless::workerCore() const FL_NO_EXCEPT {
    return mWorkerCore;
}

int ChannelDriverLcdClockless::workerPriority() const FL_NO_EXCEPT {
    if (mWorkerCore == -2) {
        return -1;
    }
    return resolveLcdWorkerPriority(mWorkerConfig.priority);
}

int ChannelDriverLcdClockless::workerObservedCore() const FL_NO_EXCEPT {
    return mWorkerObservedCore.load(fl::memory_order_acquire);
}

void ChannelDriverLcdClockless::fillWorkerDiagnostics(
    WorkerDiagnostics &result) const FL_NO_EXCEPT {
    result.core = workerCore();
    result.priority = workerPriority();
    result.observedCore = workerObservedCore();
    if (mStartError == StartError::None) {
        result.submittedChunks = static_cast<u32>(
            mIsrCtx.mSubmittedChunks.load(fl::memory_order_acquire));
        result.completedChunks = static_cast<u32>(
            mIsrCtx.mCompletedChunks.load(fl::memory_order_acquire));
        result.totalChunks = static_cast<u32>(mIsrCtx.mTotalChunks);
    } else {
        result.submittedChunks = 0;
        result.completedChunks = 0;
        result.totalChunks = 0;
    }
    result.maxEncodeUs = mWorkerMaxEncodeUs.load(fl::memory_order_acquire);
    result.maxSubmitUs = mWorkerMaxSubmitUs.load(fl::memory_order_acquire);
    result.streamError = mWorkerStreamError.load(fl::memory_order_acquire);
}

bool ChannelDriverLcdClockless::ensureWorkerTask() FL_NO_EXCEPT {
#if defined(FL_IS_ESP32) && !defined(FASTLED_STUB_IMPL)
    if (mWorkerTaskHandle.load(fl::memory_order_acquire) != nullptr) {
        return true;
    }

    mWorkerStop.store(false, fl::memory_order_release);
    mWorkerRunning.store(false, fl::memory_order_release);

    TaskHandle_t handle = nullptr;
    const UBaseType_t priority = resolveLcdWorkerPriority(mWorkerConfig.priority);
    BaseType_t core = tskNO_AFFINITY;
    const Core workerCore = resolveLcdWorkerCore(mWorkerConfig.core);
    if (workerCore == Core::CallingCore) {
        core = xPortGetCoreID();
    } else if (workerCore == Core::Core0 || workerCore == Core::Core1) {
        core = static_cast<BaseType_t>(workerCore);
    }
    BaseType_t ok = xTaskCreatePinnedToCore(workerTaskEntry, "fl_lcd_clk",
                                          4096, this, priority, &handle, core);
    if (ok != pdPASS || handle == nullptr) {
        mWorkerTaskHandle.store(nullptr, fl::memory_order_release);
        return false;
    }

    mWorkerCore = core == tskNO_AFFINITY ? -1 : static_cast<i8>(core);
    mWorkerTaskHandle.store(static_cast<void *>(handle), fl::memory_order_release);
    return true;
#else
    if (mWorkerCore == -2) {
        const Core core = resolveLcdWorkerCore(mWorkerConfig.core);
        mWorkerCore = core == Core::Core0 || core == Core::Core1
                          ? static_cast<i8>(core) : -1;
    }
    return true;
#endif
}

void ChannelDriverLcdClockless::stopWorkerTask() FL_NO_EXCEPT {
#if defined(FL_IS_ESP32) && !defined(FASTLED_STUB_IMPL)
    TaskHandle_t handle = static_cast<TaskHandle_t>(
        mWorkerTaskHandle.load(fl::memory_order_acquire));
    if (handle == nullptr) {
        return;
    }

    mWorkerStop.store(true, fl::memory_order_release);
    xTaskNotifyGive(handle);

    for (int i = 0; i < 100 &&
         mWorkerTaskHandle.load(fl::memory_order_acquire) != nullptr; i++) {
        vTaskDelay(pdMS_TO_TICKS(1));
    }

    handle = static_cast<TaskHandle_t>(
        mWorkerTaskHandle.load(fl::memory_order_acquire));
    if (handle != nullptr) {
        vTaskDelete(handle);
        mWorkerTaskHandle.store(nullptr, fl::memory_order_release);
        mWorkerRunning.store(false, fl::memory_order_release);
    }
#else
    mWorkerStop.store(true, fl::memory_order_release);
    mWorkerTaskHandle.store(nullptr, fl::memory_order_release);
    mWorkerRunning.store(false, fl::memory_order_release);
#endif
}

void ChannelDriverLcdClockless::workerTaskEntry(void *arg) FL_NO_EXCEPT {
#if defined(FL_IS_ESP32) && !defined(FASTLED_STUB_IMPL)
    auto *self = static_cast<ChannelDriverLcdClockless *>(arg);
    if (self == nullptr) {
        vTaskDelete(nullptr);
        return;
    }

    self->mWorkerRunning.store(true, fl::memory_order_release);
    self->mWorkerObservedCore.store(xPortGetCoreID(), fl::memory_order_release);
    for (;;) {
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
        if (self->mWorkerStop.load(fl::memory_order_acquire)) {
            break;
        }
        self->mWorkerObservedCore.store(xPortGetCoreID(), fl::memory_order_release);
        self->processWorkerQueue();
    }

    self->mWorkerRunning.store(false, fl::memory_order_release);
    self->mWorkerTaskHandle.store(nullptr, fl::memory_order_release);
    vTaskDelete(nullptr);
#else
    (void)arg;
#endif
}

bool ChannelDriverLcdClockless::notifyWorker() FL_NO_EXCEPT {
#if defined(FL_IS_ESP32) && !defined(FASTLED_STUB_IMPL)
    TaskHandle_t handle = static_cast<TaskHandle_t>(
        mWorkerTaskHandle.load(fl::memory_order_acquire));
    if (handle == nullptr) {
        return false;
    }

    xTaskNotifyGive(handle);
    return true;
#else
    processWorkerQueue();
    return true;
#endif
}

bool ChannelDriverLcdClockless::notifyWorkerFromIsr() FL_NO_EXCEPT {
#if defined(FL_IS_ESP32) && !defined(FASTLED_STUB_IMPL)
    TaskHandle_t handle = static_cast<TaskHandle_t>(
        mWorkerTaskHandle.load(fl::memory_order_acquire));
    if (handle == nullptr) {
        return false;
    }

    BaseType_t higherPriorityTaskWoken = pdFALSE;
    vTaskNotifyGiveFromISR(handle, &higherPriorityTaskWoken);
    return higherPriorityTaskWoken == pdTRUE;
#else
    processWorkerQueue();
    return false;
#endif
}

bool ChannelDriverLcdClockless::submitChunk(size_t chunkIndex,
                                            bool waitForSlot) FL_NO_EXCEPT {
    size_t ringIdx = chunkIndex % kRingBufferCount;
    size_t startByte = chunkIndex * mIsrCtx.mChunkInputBytes;
    u16 *buf = mRingBuffers[ringIdx];
    if (buf == nullptr) {
        return false;
    }

    const u64 encodeStart = mPeripheral->getMicroseconds();
    size_t dmaBytes = encodeChunk(mTransmittingChannels, buf, startByte,
                                  mIsrCtx.mChunkInputBytes);
    recordWorkerMaximum(mWorkerMaxEncodeUs,
                        mPeripheral->getMicroseconds() - encodeStart);

    FL_MEMORY_BARRIER;

    const u64 submitStart = mPeripheral->getMicroseconds();
    const bool submitted = waitForSlot
                               ? mPeripheral->transmit(buf, dmaBytes)
                               : mPeripheral->queueTransmit(buf, dmaBytes);
    recordWorkerMaximum(mWorkerMaxSubmitUs,
                        mPeripheral->getMicroseconds() - submitStart);
    return submitted;
}

void ChannelDriverLcdClockless::processWorkerQueue() FL_NO_EXCEPT {
    IsrContext &ctx = mIsrCtx;

    while (mBusy && !ctx.mStreamComplete && !ctx.mStreamError) {
        size_t submitted = ctx.mSubmittedChunks.load(fl::memory_order_acquire);
        size_t completed = ctx.mCompletedChunks.load(fl::memory_order_acquire);

        if (submitted >= ctx.mTotalChunks) {
            break;
        }

        if (submitted > completed &&
            (submitted - completed) >= 2) {
            break;
        }

        if (!submitChunk(submitted, false)) {
            mWorkerStreamError.store(true, fl::memory_order_release);
            ctx.mStreamError = true;
            ctx.mStreamComplete = true;
            mBusy = false;
            break;
        }

        ctx.mSubmittedChunks.store(submitted + 1, fl::memory_order_release);
        FL_MEMORY_BARRIER;
    }
}

//=============================================================================
// IChannelDriver Interface
//=============================================================================

bool ChannelDriverLcdClockless::canHandle(
    const ChannelDataPtr &data) const FL_NO_EXCEPT {
    if (!data) {
        return false;
    }
    return !data->isSpi(); // clockless only
}

void ChannelDriverLcdClockless::enqueue(
    ChannelDataPtr channelData) FL_NO_EXCEPT {
    mEnqueuedChannels.push_back(fl::move(channelData));
}

void ChannelDriverLcdClockless::show() FL_NO_EXCEPT {
    poll();

    if (mEnqueuedChannels.empty()) {
        return;
    }

    // A failed start owns no active transfer. Retry it directly rather than
    // waiting for its latched ERROR state to become READY by itself.
    if (mStartError == StartError::None) {
        (void)waitForReady(2000);
    }
    if (mBusy) {
        FL_WARN("ChannelDriverLcdClockless: DMA hung â€” forcing release");
        mBusy = false;
        for (auto &channel : mTransmittingChannels) {
            channel->setInUse(false);
        }
        mTransmittingChannels.clear();
    }

    mTransmittingChannels = fl::move(mEnqueuedChannels);

    if (!beginTransmission(mTransmittingChannels)) {
        for (auto &channel : mTransmittingChannels) {
            channel->setInUse(false);
        }
        mTransmittingChannels.clear();
    }
}

IChannelDriver::DriverState ChannelDriverLcdClockless::poll() FL_NO_EXCEPT {
    switch (mStartError) {
    case StartError::InvalidConfig:
        return DriverState(DriverState::ERROR,
                           fl::string::from_literal("LCD_CLOCKLESS: invalid transmission configuration"));
    case StartError::WorkerCreation:
        return DriverState(DriverState::ERROR,
                           fl::string::from_literal("LCD_CLOCKLESS: worker task creation failed"));
    case StartError::PeripheralInitialization:
        return DriverState(DriverState::ERROR,
                           fl::string::from_literal("LCD_CLOCKLESS: peripheral initialization failed"));
    case StartError::BufferAllocation:
        return DriverState(DriverState::ERROR,
                           fl::string::from_literal("LCD_CLOCKLESS: DMA ring buffer allocation failed"));
    case StartError::CallbackRegistration:
        return DriverState(DriverState::ERROR,
                           fl::string::from_literal("LCD_CLOCKLESS: transmit callback registration failed"));
    case StartError::InitialTransmit:
        return DriverState(DriverState::ERROR,
                           fl::string::from_literal("LCD_CLOCKLESS: initial transmit failed"));
    case StartError::None:
        break;
    }

    if (mTransmittingChannels.empty()) {
        return DriverState::READY;
    }

#if !defined(FL_IS_ESP32) || defined(FASTLED_STUB_IMPL)
    processWorkerQueue();
#endif

    bool streamDone = mIsrCtx.mStreamComplete;
    bool peripheralIdle = mPeripheral && !mPeripheral->isBusy();

    // Only declare complete when the ISR has signaled all chunks are done.
    // peripheralIdle alone is insufficient: between the first DMA send and
    // the ISR callback registration, the peripheral can briefly appear idle.
    //
    // Require peripheralIdle on the error path as well: queueTransmit() keeps
    // up to two LCD transactions in flight, so a failed follow-up submission
    // can leave an earlier chunk still completing asynchronously in
    // LcdSpiPeripheralEsp. Clearing mIsrCtx and releasing the channels before
    // that final ISR callback would let the stale callback poke into a torn-
    // down or newly-started stream.
    if ((streamDone || mIsrCtx.mStreamError) && peripheralIdle) {
        mBusy = false;
        mIsrCtx.mStreamComplete = false;
        mIsrCtx.mStreamError = false;
        for (auto &channel : mTransmittingChannels) {
            channel->setInUse(false);
        }
        mTransmittingChannels.clear();
        return DriverState::READY;
    }

    return mBusy ? DriverState::DRAINING : DriverState::READY;
}

//=============================================================================
// Encoding â€” wave3 or wave8 transpose into u16 DMA words
//=============================================================================

void ChannelDriverLcdClockless::setPollNeededCallback(PollNeededCallback callback) FL_NO_EXCEPT {
    mPollNeededCallback.set(callback);
}

// FL_IRAM matches the declaration in channel_driver_lcd_clockless.h. Keep the
// encoder IRAM-safe because worker wake-up is triggered by the LCD ISR and this
// code is part of the time-critical refill path.
FL_OPTIMIZE_FUNCTION
size_t FL_IRAM ChannelDriverLcdClockless::encodeChunk(
    fl::span<const ChannelDataPtr> channels, u16 *output,
    size_t startByte, size_t byteCount) FL_NO_EXCEPT {
    if (!mUseWave3) {
        const u8 zeroPulse = mWave8Lut.lut[0][0].data;
        const u8 xorPulse = zeroPulse ^ mWave8Lut.lut[15][0].data;
        u32 zeroPairs[4];
        u32 xorPairs[4];
        for (size_t pair = 0; pair < 4; ++pair) {
            const u32 first = 0x80u >> (pair * 2);
            const u32 second = first >> 1;
            // ESP32-S3 is little-endian: the low half is the earlier tick.
            zeroPairs[pair] = (zeroPulse & first ? 0xffffu : 0u) |
                              (zeroPulse & second ? 0xffff0000u : 0u);
            xorPairs[pair] = (xorPulse & first ? 0xffffu : 0u) |
                             (xorPulse & second ? 0xffff0000u : 0u);
        }
        auto *dmaOutput =
            reinterpret_cast<u16 (*)[64]>(output); // ok reinterpret cast
        size_t offset = 0;
        for (; byteCount - offset >= 4; offset += 4) {
            u8 lanes[4][16] = {};
            for (size_t lane = 0; lane < channels.size() && lane < 16; ++lane) {
                const auto &data = channels[lane]->getData();
                for (size_t position = 0; position < 4; ++position) {
                    const size_t input = startByte + offset + position;
                    if (input < data.size()) {
                        lanes[position][lane] = data[input];
                    }
                }
            }
            for (size_t position = 0; position < 4; ++position) {
                encodeWave8Words(lanes[position], zeroPairs, xorPairs,
                                 dmaOutput[offset + position]);
            }
        }
        for (; offset < byteCount; ++offset) {
            u8 lanes[16] = {};
            const size_t input = startByte + offset;
            for (size_t lane = 0; lane < channels.size() && lane < 16; ++lane) {
                const auto &data = channels[lane]->getData();
                if (input < data.size()) {
                    lanes[lane] = data[input];
                }
            }
            encodeWave8Words(lanes, zeroPairs, xorPairs, dmaOutput[offset]);
        }
        return byteCount * 16 * sizeof(Wave8Byte);
    }

    // Eligible wave3 timings use one or two leading high ticks for each bit.
    // Transpose the input bits once; every bit then emits high/data/low words.
    const u16 inversionMask = (mWave3Lut.lut[0] & 0x2u) ? 0xffffu : 0;
    size_t outputIdx = 0; // u16 words written

    for (size_t byteIdx = startByte; byteIdx < startByte + byteCount;
         byteIdx++) {
        // Gather one byte from each lane (zero-fill unused lanes)
        u8 lanes[16] = {};
        for (size_t lane = 0; lane < channels.size() && lane < 16; lane++) {
            const auto &data = channels[lane]->getData();
            if (byteIdx < data.size()) {
                lanes[lane] = data[byteIdx];
            }
        }

        u8 columns[16];
        detail::spread_transpose16_symbol(lanes, columns);
        for (size_t bit = 0; bit < 8; ++bit) {
            const u16 mask = static_cast<u16>(
                columns[bit * 2] |
                (static_cast<u16>(columns[bit * 2 + 1]) << 8));
            output[outputIdx++] = 0xffffu;
            output[outputIdx++] = static_cast<u16>(mask ^ inversionMask);
            output[outputIdx++] = 0;
        }
    }

    return outputIdx * sizeof(u16);
}

//=============================================================================
// ISR Callback
//=============================================================================

// FL_IRAM matches the declaration in channel_driver_lcd_clockless.h â€” this
// callback runs in ISR context and MUST live in IRAM so it can execute while
// flash cache is suspended. Without it, a concurrent flash operation (NVS
// commit, esp_flash_erase_region, etc.) keeps the ISR stalled past the 300 ms
// interrupt watchdog window and the device panics with "Interrupt wdt timeout
// on CPU1" in ISR context.
bool FL_IRAM ChannelDriverLcdClockless::isrChunkDone(void *panel_io,
                                              const void *edata,
                                              void *user_ctx) FL_NO_EXCEPT {
    (void)panel_io;
    (void)edata;

    auto *self = static_cast<ChannelDriverLcdClockless *>(user_ctx);
    IsrContext &ctx = self->mIsrCtx;

    const size_t submitted = ctx.mSubmittedChunks.load(fl::memory_order_acquire);
    // This LCD ISR is the only completion writer during a transfer.
    const size_t completed =
        ctx.mCompletedChunks.load(fl::memory_order_relaxed) + 1;
    ctx.mCompletedChunks.store(completed, fl::memory_order_release);

    if (completed >= ctx.mTotalChunks) {
        ctx.mStreamComplete = true;
        self->mBusy = false;
        self->mPollNeededCallback.invoke();
        return false;
    }

    if (submitted < ctx.mTotalChunks) {
        return self->notifyWorkerFromIsr();
    }

    return false;
}

//=============================================================================
// Begin Transmission
//=============================================================================

bool ChannelDriverLcdClockless::beginTransmission(
    fl::span<const ChannelDataPtr> channels) FL_NO_EXCEPT {
    mStartError = StartError::None;
    mWorkerMaxEncodeUs.store(0, fl::memory_order_release);
    mWorkerMaxSubmitUs.store(0, fl::memory_order_release);
    mWorkerStreamError.store(false, fl::memory_order_release);
    if (channels.empty() || !mPeripheral) {
        mStartError = StartError::InvalidConfig;
        return false;
    }

    if (channels.size() > 16) {
        mStartError = StartError::InvalidConfig;
        FL_WARN("ChannelDriverLcdClockless: too many channels (" << channels.size() << "), max 16 supported");
        return false;
    }

    if (!ensureWorkerTask()) {
        mStartError = StartError::WorkerCreation;
        FL_WARN("ChannelDriverLcdClockless: worker task creation failed");
        return false;
    }

    // Find max data size and extract timing from first channel
    size_t maxSize = 0;
    ChipsetTiming chipsetTiming = {};
    bool timingFound = false;

    for (const auto &channel : channels) {
        size_t sz = channel->getSize();
        if (sz > maxSize) {
            maxSize = sz;
        }
        if (!timingFound) {
            auto timingCfg = channel->getTiming();
            chipsetTiming.T1 = timingCfg.t1_ns;
            chipsetTiming.T2 = timingCfg.t2_ns;
            chipsetTiming.T3 = timingCfg.t3_ns;
            chipsetTiming.RESET = timingCfg.reset_us;
            chipsetTiming.name = timingCfg.name;
            timingFound = true;
        }
    }

    if (maxSize == 0 || !timingFound) {
        mStartError = StartError::InvalidConfig;
        return false;
    }

    int numLanes = static_cast<int>(channels.size());

    // Auto-select wave3 or wave8 based on chipset timing
    mUseWave3 = canUseWave3(chipsetTiming);
    if (mUseWave3) {
        mClockHz = wave3ClockFrequencyHz(chipsetTiming);
        mWave3Lut = buildWave3ExpansionLUT(chipsetTiming);
        mOutputBytesPerInputByte = 16 * sizeof(Wave3Byte); // 48
    } else {
        // wave8 clock: 8 ticks per bit period
        u32 period = chipsetTiming.T1 + chipsetTiming.T2 + chipsetTiming.T3;
        mClockHz = period > 0 ? static_cast<u32>(8000000000ULL / period) : 8000000;
        mWave8Lut = buildWave8ExpansionLUT(chipsetTiming);
        mOutputBytesPerInputByte = 16 * sizeof(Wave8Byte); // 128
    }

    // Chunk sizing.
    //
    // The LCD I80 peripheral wrapper tears down and recreates the ESP-IDF
    // panel_io handle whenever the transaction size changes. Keep every chunk
    // at the same DMA byte count so the worker never needs to recreate panel IO
    // while a stream is active.
    size_t requestedChunkInputBytes =
        mChunkInputBytesOverride > 0 ? mChunkInputBytesOverride
                                     : kDefaultChunkInputBytes;
    if (requestedChunkInputBytes > maxSize) {
        requestedChunkInputBytes = maxSize;
    }

    // Keep the initial chunk count from the requested size, then round the
    // chunk size up to cover maxSize. Every submitted LCD transaction uses this
    // rounded byte count; the last chunk zero-fills past the real source bytes
    // so ESP-IDF panel IO never has to be recreated mid-stream.
    size_t numChunks =
        (maxSize + requestedChunkInputBytes - 1) / requestedChunkInputBytes;
    if (numChunks == 0) {
        numChunks = 1;
    }
    size_t chunkInputBytes = (maxSize + numChunks - 1) / numChunks;
    if (chunkInputBytes == 0) {
        chunkInputBytes = maxSize;
    }

    size_t slotCapacityBytes = chunkInputBytes * mOutputBytesPerInputByte;

    // A different DMA chunk size needs a fresh bus as well as fresh panel IO:
    // ESP-IDF retains the exclusive-device flag after deleting panel IO.
    bool needPeripheralInit = !mInitialized || (numLanes != mNumLanes) ||
                              (mClockHz != 0 && mClockHz != mPeripheral->getConfig().clock_hz) ||
                              slotCapacityBytes != mPeripheral->getConfig().max_transfer_bytes;
    if (!needPeripheralInit) {
        const auto &currentConfig = mPeripheral->getConfig();
        for (size_t lane = 0; lane < channels.size(); ++lane) {
            if (currentConfig.data_gpios[lane] != channels[lane]->getPin()) {
                needPeripheralInit = true;
                break;
            }
        }
    }

    if (needPeripheralInit) {
        freeRingBuffers();

        if (mInitialized) {
            mPeripheral->deinitialize();
            mInitialized = false;
        }

        detail::LcdSpiConfig config;
        config.num_lanes = numLanes;
        config.max_transfer_bytes = slotCapacityBytes;
        config.use_psram = true;
        config.clock_hz = mClockHz;
        // Issue #2270: tag the shared LCD_CAM singleton with this driver
        // so it can detect a cross-driver switch and force full teardown.
        config.owner = detail::LcdSpiOwnerDriver::LCD_CLOCKLESS;

        for (size_t i = 0; i < channels.size() && i < 16; i++) {
            config.data_gpios[i] = channels[i]->getPin();
        }

        // PCLK and DC pins are required by the LCD I80 bus API but the
        // signals are not connected to LEDs for clockless output (data
        // streams out on data_gpios). Use GPIO 0 for both (same convention
        // as the legacy I2S LCD_CAM driver this replaces â€” proven safe on
        // ESP32-S3 because the strapping pin tolerates being multiplexed
        // to unused peripheral functions).
        if (config.clock_gpio < 0) {
            config.clock_gpio = 0;
        }
        if (config.dc_gpio < 0) {
            config.dc_gpio = 0;
        }

        if (!mPeripheral->initialize(config)) {
            mStartError = StartError::PeripheralInitialization;
            FL_WARN("ChannelDriverLcdClockless: Failed to init peripheral");
            return false;
        }

        mNumLanes = numLanes;
        mInitialized = true;
    }

    if (!allocateRingBuffers(slotCapacityBytes)) {
        mStartError = StartError::BufferAllocation;
        return false;
    }

    // Register ISR callback
    if (!mPeripheral->registerTransmitCallback(
            reinterpret_cast<void *>(&isrChunkDone), this)) { // ok reinterpret cast
        mStartError = StartError::CallbackRegistration;
        FL_WARN("ChannelDriverLcdClockless: registerTransmitCallback failed");
        return false;
    }

    // Initialize ISR context
    mIsrCtx.reset();
    mIsrCtx.mTotalBytes = maxSize;
    mIsrCtx.mChunkInputBytes = chunkInputBytes;
    mIsrCtx.mTotalChunks = numChunks;

    for (const auto &channel : channels) {
        channel->setInUse(true);
    }

    mBusy = true;

    // A short first DMA transfer can complete before transmit() returns.
    // Publish the complete frame context before its ISR can wake the worker.
    mIsrCtx.mSubmittedChunks.store(1, fl::memory_order_release);
    if (!submitChunk(0, true)) {
        mIsrCtx.mSubmittedChunks.store(0, fl::memory_order_release);
        mStartError = StartError::InitialTransmit;
        mBusy = false;
        for (const auto &channel : channels) {
            channel->setInUse(false);
        }
        // No DMA was started. Destroy the failed SDK session as well: panel
        // IO creation can claim an exclusive bus before its allocation fails.
        mPeripheral->deinitialize();
        mInitialized = false;
        freeRingBuffers();
        FL_WARN("ChannelDriverLcdClockless: Initial transmit failed");
        return false;
    }
    notifyWorker();

    return true;
}

//=============================================================================
// Factory
//=============================================================================

fl::shared_ptr<IChannelDriver> createLcdClocklessEngine() FL_NO_EXCEPT {
#if FL_LCD_CLOCKLESS_HAS_ESP_PERIPHERAL
    class EspWrapper : public detail::ILcdSpiPeripheral {
      public:
        bool initialize(const detail::LcdSpiConfig &c) FL_NO_EXCEPT override {
            return detail::LcdSpiPeripheralEsp::instance().initialize(c);
        }
        void deinitialize() FL_NO_EXCEPT override {
            detail::LcdSpiPeripheralEsp::instance().deinitialize();
        }
        bool isInitialized() const FL_NO_EXCEPT override {
            return detail::LcdSpiPeripheralEsp::instance().isInitialized();
        }
        u16 *allocateBuffer(size_t s) FL_NO_EXCEPT override {
            return detail::LcdSpiPeripheralEsp::instance().allocateBuffer(s);
        }
        void freeBuffer(u16 *b) FL_NO_EXCEPT override {
            detail::LcdSpiPeripheralEsp::instance().freeBuffer(b);
        }
        // FL_IRAM: forwarded into LcdSpiPeripheralEsp::transmit(); keep the
        // thunk IRAM-safe because the shared peripheral also services LCD_SPI
        // ISR re-arm paths.
        bool FL_IRAM transmit(const u16 *b, size_t s) FL_NO_EXCEPT override {
            return detail::LcdSpiPeripheralEsp::instance().transmit(b, s);
        }
        bool FL_IRAM queueTransmit(const u16 *b,
                                   size_t s) FL_NO_EXCEPT override {
            return detail::LcdSpiPeripheralEsp::instance().queueTransmit(b, s);
        }
        bool waitTransmitDone(u32 t) FL_NO_EXCEPT override {
            return detail::LcdSpiPeripheralEsp::instance().waitTransmitDone(t);
        }
        bool isBusy() const FL_NO_EXCEPT override {
            return detail::LcdSpiPeripheralEsp::instance().isBusy();
        }
        bool registerTransmitCallback(void *cb, void *ctx) FL_NO_EXCEPT override {
            return detail::LcdSpiPeripheralEsp::instance()
                .registerTransmitCallback(cb, ctx);
        }
        const detail::LcdSpiConfig &getConfig() const FL_NO_EXCEPT override {
            return detail::LcdSpiPeripheralEsp::instance().getConfig();
        }
        u64 getMicroseconds() FL_NO_EXCEPT override {
            return detail::LcdSpiPeripheralEsp::instance().getMicroseconds();
        }
        void delay(u32 ms) FL_NO_EXCEPT override {
            detail::LcdSpiPeripheralEsp::instance().delay(ms);
        }
    };
    auto wrapper = fl::make_shared<EspWrapper>();
    return fl::make_shared<ChannelDriverLcdClockless>(wrapper);
#else
    return nullptr;
#endif
}

} // namespace fl

#undef FL_LCD_CLOCKLESS_COMPILE

#endif // FL_LCD_CLOCKLESS_COMPILE
