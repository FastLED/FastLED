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
#include "fl/channels/detail/wave3.hpp"
#include "fl/channels/detail/wave8.hpp"
#include "fl/chipsets/led_timing.h"
#include "fl/log/log.h"
#include "fl/task/executor.h"
#include "fl/stl/cstring.h"
#include "fl/stl/noexcept.h"
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

//=============================================================================
// Constructor / Destructor
//=============================================================================

ChannelDriverLcdClockless::ChannelDriverLcdClockless(
    fl::shared_ptr<detail::ILcdSpiPeripheral> peripheral) FL_NOEXCEPT
    : mPeripheral(fl::move(peripheral)), mInitialized(false),
      mEnqueuedChannels(), mTransmittingChannels(),
      mRingBuffers{nullptr, nullptr, nullptr}, mRingCapacity(0),
      mNumLanes(0), mBusy(false), mIsrCtx(), mChunkInputBytesOverride(0),
      mWorkerTaskHandle(nullptr), mWorkerStop(false), mWorkerRunning(false),
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
            FL_WARN("ChannelDriverLcdClockless: DMA wait timed out — "
                    "freeing ring buffers anyway");
        }
    }
    stopWorkerTask();
    freeRingBuffers();
}

//=============================================================================
// Ring Buffer Management
//=============================================================================

void ChannelDriverLcdClockless::freeRingBuffers() FL_NOEXCEPT {
    for (size_t i = 0; i < kRingBufferCount; i++) {
        if (mPeripheral && mRingBuffers[i] != nullptr) {
            mPeripheral->freeBuffer(mRingBuffers[i]);
            mRingBuffers[i] = nullptr;
        }
    }
    mRingCapacity = 0;
}

bool ChannelDriverLcdClockless::allocateRingBuffers(
    size_t slotCapacityBytes) FL_NOEXCEPT {
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

bool ChannelDriverLcdClockless::ensureWorkerTask() FL_NOEXCEPT {
#if defined(FL_IS_ESP32) && !defined(FASTLED_STUB_IMPL)
    if (mWorkerTaskHandle != nullptr) {
        return true;
    }

    mWorkerStop = false;
    mWorkerRunning = false;

    TaskHandle_t handle = nullptr;
    UBaseType_t priority =
        (configMAX_PRIORITIES > 2) ? (configMAX_PRIORITIES - 2) : 1;
    BaseType_t ok = xTaskCreate(workerTaskEntry, "fl_lcd_clk",
                                4096, this, priority, &handle);
    if (ok != pdPASS || handle == nullptr) {
        mWorkerTaskHandle = nullptr;
        return false;
    }

    mWorkerTaskHandle = static_cast<void *>(handle);
    return true;
#else
    return true;
#endif
}

void ChannelDriverLcdClockless::stopWorkerTask() FL_NOEXCEPT {
#if defined(FL_IS_ESP32) && !defined(FASTLED_STUB_IMPL)
    TaskHandle_t handle = static_cast<TaskHandle_t>(mWorkerTaskHandle);
    if (handle == nullptr) {
        return;
    }

    mWorkerStop = true;
    xTaskNotifyGive(handle);

    for (int i = 0; i < 100 && mWorkerTaskHandle != nullptr; i++) {
        vTaskDelay(pdMS_TO_TICKS(1));
    }

    if (mWorkerTaskHandle != nullptr) {
        vTaskDelete(static_cast<TaskHandle_t>(mWorkerTaskHandle));
        mWorkerTaskHandle = nullptr;
        mWorkerRunning = false;
    }
#else
    mWorkerStop = true;
    mWorkerTaskHandle = nullptr;
    mWorkerRunning = false;
#endif
}

void ChannelDriverLcdClockless::workerTaskEntry(void *arg) FL_NOEXCEPT {
#if defined(FL_IS_ESP32) && !defined(FASTLED_STUB_IMPL)
    auto *self = static_cast<ChannelDriverLcdClockless *>(arg);
    if (self == nullptr) {
        vTaskDelete(nullptr);
        return;
    }

    self->mWorkerRunning = true;
    for (;;) {
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
        if (self->mWorkerStop) {
            break;
        }
        self->processWorkerQueue();
    }

    self->mWorkerRunning = false;
    self->mWorkerTaskHandle = nullptr;
    vTaskDelete(nullptr);
#else
    (void)arg;
#endif
}

bool ChannelDriverLcdClockless::notifyWorker() FL_NOEXCEPT {
#if defined(FL_IS_ESP32) && !defined(FASTLED_STUB_IMPL)
    TaskHandle_t handle = static_cast<TaskHandle_t>(mWorkerTaskHandle);
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

bool ChannelDriverLcdClockless::notifyWorkerFromIsr() FL_NOEXCEPT {
#if defined(FL_IS_ESP32) && !defined(FASTLED_STUB_IMPL)
    TaskHandle_t handle = static_cast<TaskHandle_t>(mWorkerTaskHandle);
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
                                            bool waitForSlot) FL_NOEXCEPT {
    size_t ringIdx = chunkIndex % kRingBufferCount;
    size_t startByte = chunkIndex * mIsrCtx.mChunkInputBytes;
    u16 *buf = mRingBuffers[ringIdx];
    if (buf == nullptr) {
        return false;
    }

    size_t dmaBytes = encodeChunk(mTransmittingChannels, buf, startByte,
                                  mIsrCtx.mChunkInputBytes);

    FL_MEMORY_BARRIER;

    if (waitForSlot) {
        return mPeripheral->transmit(buf, dmaBytes);
    }
    return mPeripheral->queueTransmit(buf, dmaBytes);
}

void ChannelDriverLcdClockless::processWorkerQueue() FL_NOEXCEPT {
    IsrContext &ctx = mIsrCtx;

    while (mBusy && !ctx.mStreamComplete && !ctx.mStreamError) {
        size_t submitted = ctx.mSubmittedChunks;
        size_t completed = ctx.mCompletedChunks;

        if (submitted >= ctx.mTotalChunks) {
            break;
        }

        if (submitted > completed &&
            (submitted - completed) >= 2) {
            break;
        }

        if (!submitChunk(submitted, false)) {
            ctx.mStreamError = true;
            ctx.mStreamComplete = true;
            mBusy = false;
            break;
        }

        ctx.mSubmittedChunks = submitted + 1;
        FL_MEMORY_BARRIER;
    }
}

//=============================================================================
// IChannelDriver Interface
//=============================================================================

bool ChannelDriverLcdClockless::canHandle(
    const ChannelDataPtr &data) const FL_NOEXCEPT {
    if (!data) {
        return false;
    }
    return !data->isSpi(); // clockless only
}

void ChannelDriverLcdClockless::enqueue(
    ChannelDataPtr channelData) FL_NOEXCEPT {
    mEnqueuedChannels.push_back(fl::move(channelData));
}

void ChannelDriverLcdClockless::show() FL_NOEXCEPT {
    poll();

    if (mEnqueuedChannels.empty()) {
        return;
    }

    (void)waitForReady(2000);
    if (mBusy) {
        FL_WARN("ChannelDriverLcdClockless: DMA hung — forcing release");
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

IChannelDriver::DriverState ChannelDriverLcdClockless::poll() FL_NOEXCEPT {
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
// Encoding — wave3 or wave8 transpose into u16 DMA words
//=============================================================================

void ChannelDriverLcdClockless::setPollNeededCallback(PollNeededCallback callback) FL_NOEXCEPT {
    mPollNeededCallback.set(callback);
}

// FL_IRAM matches the declaration in channel_driver_lcd_clockless.h. Keep the
// encoder IRAM-safe because worker wake-up is triggered by the LCD ISR and this
// code is part of the time-critical refill path.
size_t FL_IRAM ChannelDriverLcdClockless::encodeChunk(
    fl::span<const ChannelDataPtr> channels, u16 *output,
    size_t startByte, size_t byteCount) FL_NOEXCEPT {
    size_t outputIdx = 0; // bytes written

    for (size_t byteIdx = startByte; byteIdx < startByte + byteCount;
         byteIdx++) {
        // Gather one byte from each lane (zero-fill unused lanes)
        u8 lanes[16];
        fl::memset(lanes, 0, sizeof(lanes));
        for (size_t lane = 0; lane < channels.size() && lane < 16; lane++) {
            const auto &data = channels[lane]->getData();
            if (byteIdx < data.size()) {
                lanes[lane] = data[byteIdx];
            }
        }

        if (mUseWave3) {
            u8 transposed[16 * sizeof(Wave3Byte)]; // 48 bytes = 24 u16 words
            wave3Transpose_16(
                reinterpret_cast<const u8 (&)[16]>(lanes), // ok reinterpret cast
                mWave3Lut,
                reinterpret_cast<u8 (&)[16 * sizeof(Wave3Byte)]>( // ok reinterpret cast
                    transposed));
            fl::memcpy(reinterpret_cast<u8 *>(output) + outputIdx, // ok reinterpret cast
                       transposed, sizeof(transposed));
            outputIdx += sizeof(transposed);
        } else {
            u8 transposed[16 * sizeof(Wave8Byte)]; // 128 bytes = 64 u16 words
            wave8Transpose_16(
                reinterpret_cast<const u8 (&)[16]>(lanes), // ok reinterpret cast
                mWave8Lut,
                reinterpret_cast<u8 (&)[16 * sizeof(Wave8Byte)]>( // ok reinterpret cast
                    transposed));
            fl::memcpy(reinterpret_cast<u8 *>(output) + outputIdx, // ok reinterpret cast
                       transposed, sizeof(transposed));
            outputIdx += sizeof(transposed);
        }
    }

    return outputIdx;
}

//=============================================================================
// ISR Callback
//=============================================================================

// FL_IRAM matches the declaration in channel_driver_lcd_clockless.h — this
// callback runs in ISR context and MUST live in IRAM so it can execute while
// flash cache is suspended. Without it, a concurrent flash operation (NVS
// commit, esp_flash_erase_region, etc.) keeps the ISR stalled past the 300 ms
// interrupt watchdog window and the device panics with "Interrupt wdt timeout
// on CPU1" in ISR context.
bool FL_IRAM ChannelDriverLcdClockless::isrChunkDone(void *panel_io,
                                              const void *edata,
                                              void *user_ctx) FL_NOEXCEPT {
    (void)panel_io;
    (void)edata;

    auto *self = static_cast<ChannelDriverLcdClockless *>(user_ctx);
    IsrContext &ctx = self->mIsrCtx;

    ctx.mCompletedChunks = ctx.mCompletedChunks + 1;

    if (ctx.mCompletedChunks >= ctx.mTotalChunks) {
        ctx.mStreamComplete = true;
        self->mBusy = false;
        self->mPollNeededCallback.invoke();
        return false;
    }

    if (ctx.mSubmittedChunks < ctx.mTotalChunks) {
        return self->notifyWorkerFromIsr();
    }

    return false;
}

//=============================================================================
// Begin Transmission
//=============================================================================

bool ChannelDriverLcdClockless::beginTransmission(
    fl::span<const ChannelDataPtr> channels) FL_NOEXCEPT {
    if (channels.empty() || !mPeripheral) {
        return false;
    }

    if (channels.size() > 16) {
        FL_WARN("ChannelDriverLcdClockless: too many channels ("
                << channels.size() << "), max 16 supported");
        return false;
    }

    if (!ensureWorkerTask()) {
        FL_WARN("ChannelDriverLcdClockless: worker task creation failed");
        return false;
    }

    // Find max data size and extract timing from first channel
    size_t maxSize = 0;
    ChipsetTiming chipsetTiming = {};
    bool timingFound = false;
    int firstPin = -1;

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
        if (firstPin < 0) {
            firstPin = channel->getPin();
        }
    }

    if (maxSize == 0 || !timingFound) {
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

    // Peripheral init when lane count, clock rate, or pin assignment changes
    bool needPeripheralInit = !mInitialized || (numLanes != mNumLanes) ||
                              (mClockHz != 0 && mClockHz != mPeripheral->getConfig().clock_hz) ||
                              (firstPin >= 0 && mInitialized &&
                               mPeripheral->getConfig().data_gpios[0] != firstPin);

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
        // as the legacy I2S LCD_CAM driver this replaces — proven safe on
        // ESP32-S3 because the strapping pin tolerates being multiplexed
        // to unused peripheral functions).
        if (config.clock_gpio < 0) {
            config.clock_gpio = 0;
        }
        if (config.dc_gpio < 0) {
            config.dc_gpio = 0;
        }

        if (!mPeripheral->initialize(config)) {
            FL_WARN("ChannelDriverLcdClockless: Failed to init peripheral");
            return false;
        }

        mNumLanes = numLanes;
        mInitialized = true;
    }

    if (!allocateRingBuffers(slotCapacityBytes)) {
        return false;
    }

    // Register ISR callback
    if (!mPeripheral->registerTransmitCallback(
            reinterpret_cast<void *>(&isrChunkDone), this)) { // ok reinterpret cast
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

    if (!submitChunk(0, true)) {
        mBusy = false;
        for (const auto &channel : channels) {
            channel->setInUse(false);
        }
        FL_WARN("ChannelDriverLcdClockless: Initial transmit failed");
        return false;
    }
    mIsrCtx.mSubmittedChunks = 1;

    notifyWorker();

    return true;
}

//=============================================================================
// Factory
//=============================================================================

fl::shared_ptr<IChannelDriver> createLcdClocklessEngine() FL_NOEXCEPT {
#if FL_LCD_CLOCKLESS_HAS_ESP_PERIPHERAL
    class EspWrapper : public detail::ILcdSpiPeripheral {
      public:
        bool initialize(const detail::LcdSpiConfig &c) FL_NOEXCEPT override {
            return detail::LcdSpiPeripheralEsp::instance().initialize(c);
        }
        void deinitialize() FL_NOEXCEPT override {
            detail::LcdSpiPeripheralEsp::instance().deinitialize();
        }
        bool isInitialized() const FL_NOEXCEPT override {
            return detail::LcdSpiPeripheralEsp::instance().isInitialized();
        }
        u16 *allocateBuffer(size_t s) FL_NOEXCEPT override {
            return detail::LcdSpiPeripheralEsp::instance().allocateBuffer(s);
        }
        void freeBuffer(u16 *b) FL_NOEXCEPT override {
            detail::LcdSpiPeripheralEsp::instance().freeBuffer(b);
        }
        // FL_IRAM: forwarded into LcdSpiPeripheralEsp::transmit(); keep the
        // thunk IRAM-safe because the shared peripheral also services LCD_SPI
        // ISR re-arm paths.
        bool FL_IRAM transmit(const u16 *b, size_t s) FL_NOEXCEPT override {
            return detail::LcdSpiPeripheralEsp::instance().transmit(b, s);
        }
        bool FL_IRAM queueTransmit(const u16 *b,
                                   size_t s) FL_NOEXCEPT override {
            return detail::LcdSpiPeripheralEsp::instance().queueTransmit(b, s);
        }
        bool waitTransmitDone(u32 t) FL_NOEXCEPT override {
            return detail::LcdSpiPeripheralEsp::instance().waitTransmitDone(t);
        }
        bool isBusy() const FL_NOEXCEPT override {
            return detail::LcdSpiPeripheralEsp::instance().isBusy();
        }
        bool registerTransmitCallback(void *cb, void *ctx) FL_NOEXCEPT override {
            return detail::LcdSpiPeripheralEsp::instance()
                .registerTransmitCallback(cb, ctx);
        }
        const detail::LcdSpiConfig &getConfig() const FL_NOEXCEPT override {
            return detail::LcdSpiPeripheralEsp::instance().getConfig();
        }
        u64 getMicroseconds() FL_NOEXCEPT override {
            return detail::LcdSpiPeripheralEsp::instance().getMicroseconds();
        }
        void delay(u32 ms) FL_NOEXCEPT override {
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
