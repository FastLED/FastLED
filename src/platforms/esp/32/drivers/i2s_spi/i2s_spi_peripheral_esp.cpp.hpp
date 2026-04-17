// IWYU pragma: private

/// @file i2s_spi_peripheral_esp.cpp
/// @brief ESP32dev native I2S parallel SPI peripheral implementation
///
/// Direct I2S register manipulation for clocked SPI LED output.
/// No dependency on Yves I2SClockBasedLedDriver.

#include "platforms/is_platform.h"
#if defined(FL_IS_ESP32)

#include "platforms/esp/32/feature_flags/enabled.h"

#if FASTLED_ESP32_HAS_I2S

#include "platforms/esp/32/drivers/i2s_spi/i2s_spi_peripheral_esp.h"
#include "fl/stl/singleton.h"
#include "fl/stl/cstring.h"
#include "fl/stl/noexcept.h"
#include "fl/system/log.h"
#include "fl/stl/compiler_control.h"

FL_EXTERN_C_BEGIN
// IWYU pragma: begin_keep
#include "freertos/FreeRTOS.h"
// IWYU pragma: end_keep
// IWYU pragma: begin_keep
#include "freertos/task.h"
// IWYU pragma: end_keep
// IWYU pragma: begin_keep
#include "freertos/semphr.h"
// IWYU pragma: end_keep
#include "esp_heap_caps.h"
#include "esp_timer.h"
#include "esp_intr_alloc.h"
// IWYU pragma: begin_keep
#include "driver/gpio.h"
// IWYU pragma: end_keep
// IWYU pragma: begin_keep
#include "soc/i2s_reg.h"
// IWYU pragma: end_keep
// IWYU pragma: begin_keep
#include "soc/i2s_struct.h"
// IWYU pragma: end_keep
// IWYU pragma: begin_keep
#include "soc/gpio_sig_map.h"
// IWYU pragma: end_keep
#include "platforms/esp/esp_version.h"
FL_EXTERN_C_END

#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 0, 0)
// IWYU pragma: begin_keep
#include "esp_private/periph_ctrl.h"
#include "esp_rom_gpio.h"
// IWYU pragma: end_keep
#define gpio_matrix_out esp_rom_gpio_connect_out_signal
#else
// IWYU pragma: begin_keep
#include "driver/periph_ctrl.h"
// IWYU pragma: end_keep
#endif

#define I2S_BASE_CLK 80000000L

// ESP32 lldesc_t has 12-bit size/length fields, max usable value = 4092
// (must be divisible by 4 for DMA alignment).
#define I2S_DMA_MAX_DATA_LEN 4092

namespace fl {
namespace detail {

//=============================================================================
// ISR Handler
//=============================================================================

void IRAM_ATTR I2sSpiPeripheralEsp::isrHandler(void *arg) {
    I2sSpiPeripheralEsp *self = static_cast<I2sSpiPeripheralEsp *>(arg);
    i2s_dev_t *i2s = self->mI2s;

    if (i2s->int_st.out_eof) {
        i2s->int_clr.val = i2s->int_raw.val;

        // Stop transmission
        i2s->conf.tx_start = 0;

        self->mBusy = false;

        // Signal completion
        BaseType_t xHigherPriorityTaskWoken = pdFALSE;
        xSemaphoreGiveFromISR(self->mCompleteSem,
                              &xHigherPriorityTaskWoken);

        // Invoke registered callback (must be IRAM_ATTR)
        if (self->mCallback) {
            using CallbackType = bool (*)(void *, const void *, void *);
            auto fn =
                reinterpret_cast<CallbackType>(self->mCallback); // ok reinterpret cast
            fn(nullptr, nullptr, self->mUserCtx);
        }

        if (xHigherPriorityTaskWoken) {
            portYIELD_FROM_ISR();
        }
    }
}

//=============================================================================
// Singleton
//=============================================================================

I2sSpiPeripheralEsp &I2sSpiPeripheralEsp::instance() FL_NOEXCEPT {
    return Singleton<I2sSpiPeripheralEsp>::instance();
}

I2sSpiPeripheralEsp::I2sSpiPeripheralEsp() FL_NOEXCEPT
    : mI2s(nullptr), mDmaDescs(nullptr), mDmaDescCount(0),
      mDmaBuffer(nullptr), mDmaBufferWords(0),
      mIntrHandle(nullptr), mCompleteSem(nullptr), mInitialized(false),
      mConfig(), mCallback(nullptr), mUserCtx(nullptr), mBusy(false) {}

I2sSpiPeripheralEsp::~I2sSpiPeripheralEsp() { deinitialize(); }

//=============================================================================
// Reset Helpers
//=============================================================================

void I2sSpiPeripheralEsp::resetI2s() FL_NOEXCEPT {
    const unsigned long lc_conf_reset_flags =
        I2S_IN_RST_M | I2S_OUT_RST_M | I2S_AHBM_RST_M |
        I2S_AHBM_FIFO_RST_M;
    mI2s->lc_conf.val |= lc_conf_reset_flags;
    mI2s->lc_conf.val &= ~lc_conf_reset_flags;

    const u32 conf_reset_flags = I2S_RX_RESET_M | I2S_RX_FIFO_RESET_M |
                                 I2S_TX_RESET_M | I2S_TX_FIFO_RESET_M;
    mI2s->conf.val |= conf_reset_flags;
    mI2s->conf.val &= ~conf_reset_flags;
}

void I2sSpiPeripheralEsp::resetDma() FL_NOEXCEPT {
    mI2s->lc_conf.out_rst = 1;
    mI2s->lc_conf.out_rst = 0;
}

void I2sSpiPeripheralEsp::resetFifo() FL_NOEXCEPT {
    mI2s->conf.tx_fifo_reset = 1;
    mI2s->conf.tx_fifo_reset = 0;
}

//=============================================================================
// Lifecycle
//=============================================================================

bool I2sSpiPeripheralEsp::initialize(const I2sSpiConfig &config) FL_NOEXCEPT {
    if (mInitialized) {
        if (mConfig.num_lanes == config.num_lanes &&
            mConfig.clock_hz == config.clock_hz &&
            mConfig.max_transfer_bytes >= config.max_transfer_bytes) {
            return true;
        }
        deinitialize();
    }

    if (config.num_lanes < 1 || config.num_lanes > 16) {
        FL_WARN("I2sSpiPeripheralEsp: Invalid num_lanes: "
                << config.num_lanes);
        return false;
    }

    if (config.clock_gpio < 0) {
        FL_WARN("I2sSpiPeripheralEsp: Invalid clock_gpio");
        return false;
    }

    mConfig = config;

    // Use I2S0
    mI2s = &I2S0;
    periph_module_enable(PERIPH_I2S0_MODULE);

    // Reset everything
    resetI2s();
    resetDma();
    resetFifo();

    // Main configuration
    mI2s->conf.tx_msb_right = 1;
    mI2s->conf.tx_mono = 0;
    mI2s->conf.tx_short_sync = 0;
    mI2s->conf.tx_msb_shift = 0;
    mI2s->conf.tx_right_first = 1;
    mI2s->conf.tx_slave_mod = 0;

    // Parallel/LCD mode
    mI2s->conf2.val = 0;
    mI2s->conf2.lcd_en = 1;
    mI2s->conf2.lcd_tx_wrx2_en = 0;
    mI2s->conf2.lcd_tx_sdx2_en = 0;

    // 16-bit parallel output (for up to 16 data lanes)
    mI2s->sample_rate_conf.val = 0;
    mI2s->sample_rate_conf.tx_bits_mod = 16;
    mI2s->sample_rate_conf.tx_bck_div_num = 1;

    // Clock configuration: base = 80MHz
    // Integer truncation is intentional — APA102/SK9822 use clean MHz
    // values (1, 2, 4, 8, etc.). Sub-MHz precision is not needed.
    int clockMHz = static_cast<int>(config.clock_hz / 1000000);
    if (clockMHz < 1) clockMHz = 1;
    if (clockMHz > 40) clockMHz = 40;

    int divN = I2S_BASE_CLK / (clockMHz * 1000000);
    int divB = (I2S_BASE_CLK % (clockMHz * 1000000)) / 1000000;
    int divA = clockMHz;

    if (divB == 0) {
        divA = 1;
        divB = 0;
    }

    mI2s->clkm_conf.val = 0;
    mI2s->clkm_conf.clka_en = 0;
    mI2s->clkm_conf.clkm_div_a = divA;
    mI2s->clkm_conf.clkm_div_b = divB;
    mI2s->clkm_conf.clkm_div_num = divN;

    // FIFO configuration
    mI2s->fifo_conf.val = 0;
    mI2s->fifo_conf.tx_fifo_mod_force_en = 1;
    mI2s->fifo_conf.tx_fifo_mod = 1; // 16-bit single channel
    mI2s->fifo_conf.tx_data_num = 32;
    mI2s->fifo_conf.dscr_en = 1;

    mI2s->conf1.val = 0;
    mI2s->conf1.tx_stop_en = 0;
    mI2s->conf1.tx_pcm_bypass = 1;

    mI2s->conf_chan.val = 0;
    mI2s->conf_chan.tx_chan_mod = 1; // Mono

    mI2s->timing.val = 0;

    // Route data GPIOs
    for (int i = 0; i < config.num_lanes; i++) {
        int pin = config.data_gpios[i];
        if (pin >= 0) {
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 0, 0)
            gpio_reset_pin(static_cast<gpio_num_t>(pin));
            gpio_set_direction(static_cast<gpio_num_t>(pin),
                               GPIO_MODE_OUTPUT);
#else
            PIN_FUNC_SELECT(GPIO_PIN_MUX_REG[pin], PIN_FUNC_GPIO);
            gpio_set_direction(static_cast<gpio_num_t>(pin),
                               static_cast<gpio_mode_t>(GPIO_MODE_DEF_OUTPUT));
#endif
            gpio_matrix_out(pin, I2S0O_DATA_OUT0_IDX + i + 8, false, false);
        }
    }

    // Route clock GPIO
    {
        int clkPin = config.clock_gpio;
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 0, 0)
        gpio_reset_pin(static_cast<gpio_num_t>(clkPin));
        gpio_set_direction(static_cast<gpio_num_t>(clkPin),
                           GPIO_MODE_OUTPUT);
#else
        PIN_FUNC_SELECT(GPIO_PIN_MUX_REG[clkPin], PIN_FUNC_GPIO);
        gpio_set_direction(static_cast<gpio_num_t>(clkPin),
                           static_cast<gpio_mode_t>(GPIO_MODE_DEF_OUTPUT));
#endif
        gpio_matrix_out(clkPin, I2S0O_BCK_OUT_IDX, false, false);
    }

    // Allocate DMA buffer
    // Each interleaved byte across all lanes is transposed: 8 bits per byte
    // produce 8 u16 output words (one per bit, MSB first).
    // Each byte per lane produces 8 u16 words (one per bit, MSB first)
    mDmaBufferWords = (config.max_transfer_bytes / config.num_lanes) * 8;
    size_t dmaBytes = mDmaBufferWords * sizeof(u16);

    mDmaBuffer = static_cast<u16 *>(
        heap_caps_malloc(dmaBytes, MALLOC_CAP_DMA | MALLOC_CAP_8BIT));
    if (mDmaBuffer == nullptr) {
        FL_WARN("I2sSpiPeripheralEsp: Failed to allocate DMA buffer");
        return false;
    }
    fl::memset(mDmaBuffer, 0, dmaBytes);

    // Set up DMA descriptor chain — each descriptor covers up to
    // I2S_DMA_MAX_DATA_LEN bytes (ESP32 lldesc_t has 12-bit size field).
    mDmaDescCount =
        static_cast<int>((dmaBytes + I2S_DMA_MAX_DATA_LEN - 1) /
                         I2S_DMA_MAX_DATA_LEN);
    if (mDmaDescCount < 1) {
        mDmaDescCount = 1;
    }
    mDmaDescs = static_cast<lldesc_t *>(heap_caps_malloc(
        mDmaDescCount * sizeof(lldesc_t), MALLOC_CAP_DMA));
    if (mDmaDescs == nullptr) {
        FL_WARN("I2sSpiPeripheralEsp: Failed to allocate DMA descriptors");
        heap_caps_free(mDmaBuffer);
        mDmaBuffer = nullptr;
        mDmaBufferWords = 0;
        return false;
    }
    fl::memset(mDmaDescs, 0, mDmaDescCount * sizeof(lldesc_t));

    u8 *bufPtr = reinterpret_cast<u8 *>(mDmaBuffer); // ok reinterpret cast
    size_t remaining = dmaBytes;
    for (int i = 0; i < mDmaDescCount; i++) {
        size_t chunkSize =
            (remaining > I2S_DMA_MAX_DATA_LEN) ? I2S_DMA_MAX_DATA_LEN
                                               : remaining;
        mDmaDescs[i].buf = bufPtr;
        mDmaDescs[i].length = static_cast<u32>(chunkSize);
        mDmaDescs[i].size = static_cast<u32>(chunkSize);
        mDmaDescs[i].owner = 1;
        mDmaDescs[i].sosf = (i == 0) ? 1 : 0;
        mDmaDescs[i].offset = 0;
        mDmaDescs[i].empty = 0;
        mDmaDescs[i].eof = (i == mDmaDescCount - 1) ? 1 : 0;
        mDmaDescs[i].qe.stqe_next =
            (i < mDmaDescCount - 1) ? &mDmaDescs[i + 1] : nullptr;
        bufPtr += chunkSize;
        remaining -= chunkSize;
    }

    // Register interrupt — ESP_INTR_FLAG_IRAM is required because
    // isrHandler is IRAM_ATTR and must remain callable when flash
    // cache is disabled (OTA, NVS writes, PSRAM flash collisions).
    SET_PERI_REG_BITS(I2S_INT_ENA_REG(0), I2S_OUT_EOF_INT_ENA_V, 1,
                      I2S_OUT_EOF_INT_ENA_S);
    esp_err_t intr_err = esp_intr_alloc(
        ETS_I2S0_INTR_SOURCE, ESP_INTR_FLAG_IRAM, &isrHandler, this,
        &mIntrHandle);
    if (intr_err != ESP_OK) {
        FL_WARN("I2sSpiPeripheralEsp: Failed to allocate interrupt: "
                << intr_err);
        heap_caps_free(mDmaDescs);
        mDmaDescs = nullptr;
        mDmaDescCount = 0;
        heap_caps_free(mDmaBuffer);
        mDmaBuffer = nullptr;
        mDmaBufferWords = 0;
        return false;
    }

    // Create completion semaphore
    if (mCompleteSem == nullptr) {
        mCompleteSem = xSemaphoreCreateBinary();
    }

    mInitialized = true;
    FL_DBG("I2sSpiPeripheralEsp: Native I2S init with "
           << config.num_lanes << " lanes, " << clockMHz << " MHz clock");
    return true;
}

void I2sSpiPeripheralEsp::deinitialize() FL_NOEXCEPT {
    if (mIntrHandle != nullptr) {
        esp_intr_disable(mIntrHandle);
        esp_intr_free(mIntrHandle);
        mIntrHandle = nullptr;
    }
    if (mDmaBuffer != nullptr) {
        heap_caps_free(mDmaBuffer);
        mDmaBuffer = nullptr;
    }
    if (mDmaDescs != nullptr) {
        heap_caps_free(mDmaDescs);
        mDmaDescs = nullptr;
    }
    mDmaDescCount = 0;
    if (mCompleteSem != nullptr) {
        vSemaphoreDelete(mCompleteSem);
        mCompleteSem = nullptr;
    }
    mDmaBufferWords = 0;

    // Disable the I2S peripheral module (matches periph_module_enable
    // in initialize()). Guarded to avoid ref-count underflow if
    // deinitialize() is called when already deinitialized.
    if (mInitialized) {
        periph_module_disable(PERIPH_I2S0_MODULE);
    }

    mInitialized = false;
    mCallback = nullptr;
    mUserCtx = nullptr;
    mBusy = false;
}

bool I2sSpiPeripheralEsp::isInitialized() const FL_NOEXCEPT {
    return mInitialized;
}

//=============================================================================
// Buffer Management
//=============================================================================

u8 *I2sSpiPeripheralEsp::allocateBuffer(size_t size_bytes) FL_NOEXCEPT {
    size_t aligned_size = ((size_bytes + 3) / 4) * 4;
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(4, 3, 0)
    void *buffer = heap_caps_aligned_calloc(4, aligned_size, 1,
                                            MALLOC_CAP_DMA | MALLOC_CAP_8BIT);
#else
    // ESP-IDF prior to 4.3 (including 3.3-LTS) lacks heap_caps_aligned_calloc.
    // aligned_size is already a multiple of 4, and ESP32 heap allocations are
    // naturally 4-byte aligned, so heap_caps_calloc gives us what we need.
    void *buffer = heap_caps_calloc(1, aligned_size,
                                    MALLOC_CAP_DMA | MALLOC_CAP_8BIT);
#endif
    return static_cast<u8 *>(buffer);
}

void I2sSpiPeripheralEsp::freeBuffer(u8 *buffer) FL_NOEXCEPT {
    if (buffer != nullptr) {
        heap_caps_free(buffer);
    }
}

//=============================================================================
// Transpose
//=============================================================================

void I2sSpiPeripheralEsp::transposeToI2sBuffer(const u8 *src,
                                                size_t srcSize) FL_NOEXCEPT {
    // Input: interleaved bytes [lane0_b0, lane1_b0, ..., lane0_b1, ...]
    // Output: 16-bit words where each bit position = one lane
    //
    // For clocked SPI, each byte position across all lanes becomes 8
    // output words (one per bit, MSB first). Each output word has one bit
    // per active lane.
    //
    // But wait — for clocked SPI, the data is sent in parallel on the
    // clock edge. Each clock cycle outputs one 16-bit word. So each
    // SOURCE bit across lanes = one clock cycle = one u16 word.

    int numLanes = mConfig.num_lanes;
    if (numLanes <= 0) {
        fl::memset(mDmaBuffer, 0, mDmaBufferWords * sizeof(u16));
        return;
    }
    size_t bytesPerLane = srcSize / numLanes;
    size_t wordIdx = 0;

    for (size_t bytePos = 0; bytePos < bytesPerLane && wordIdx < mDmaBufferWords; bytePos++) {
        // Gather this byte from all lanes
        u8 laneBytes[16];
        fl::memset(laneBytes, 0, sizeof(laneBytes));
        for (int lane = 0; lane < numLanes; lane++) {
            laneBytes[lane] = src[bytePos * numLanes + lane];
        }

        // Transpose: 8 bits per byte -> 8 u16 words (MSB first)
        for (int bit = 7; bit >= 0 && wordIdx < mDmaBufferWords; bit--) {
            u16 word = 0;
            for (int lane = 0; lane < numLanes; lane++) {
                if (laneBytes[lane] & (1 << bit)) {
                    word |= (1 << lane);
                }
            }
            mDmaBuffer[wordIdx++] = word;
        }
    }

    // Zero remaining buffer
    while (wordIdx < mDmaBufferWords) {
        mDmaBuffer[wordIdx++] = 0;
    }
}

//=============================================================================
// Transmission
//=============================================================================

bool I2sSpiPeripheralEsp::transmit(const u8 *buffer,
                                   size_t size_bytes) FL_NOEXCEPT {
    if (!mInitialized || mDmaBuffer == nullptr) {
        return false;
    }

    // Transpose interleaved bytes into I2S parallel words
    transposeToI2sBuffer(buffer, size_bytes);

    // Update DMA descriptor chain for actual transfer size
    size_t bytesPerLane = size_bytes / mConfig.num_lanes;
    size_t outputWords = bytesPerLane * 8; // 8 bits per byte
    if (outputWords > mDmaBufferWords) {
        outputWords = mDmaBufferWords;
    }
    size_t dmaBytes = outputWords * sizeof(u16);

    // Distribute dmaBytes across descriptor chain
    size_t remaining = dmaBytes;
    u8 *bufPtr = reinterpret_cast<u8 *>(mDmaBuffer); // ok reinterpret cast
    for (int i = 0; i < mDmaDescCount; i++) {
        if (remaining == 0) {
            // Deactivate unused descriptors
            mDmaDescs[i].owner = 0;
            mDmaDescs[i].length = 0;
            mDmaDescs[i].size = 0;
            mDmaDescs[i].eof = 0;
            mDmaDescs[i].qe.stqe_next = nullptr;
            continue;
        }
        size_t chunkSize =
            (remaining > I2S_DMA_MAX_DATA_LEN) ? I2S_DMA_MAX_DATA_LEN
                                               : remaining;
        mDmaDescs[i].buf = bufPtr;
        mDmaDescs[i].length = static_cast<u32>(chunkSize);
        mDmaDescs[i].size = static_cast<u32>(chunkSize);
        mDmaDescs[i].owner = 1;
        mDmaDescs[i].sosf = (i == 0) ? 1 : 0;
        mDmaDescs[i].offset = 0;
        mDmaDescs[i].empty = 0;

        bufPtr += chunkSize;
        remaining -= chunkSize;

        if (remaining == 0) {
            // Last active descriptor
            mDmaDescs[i].eof = 1;
            mDmaDescs[i].qe.stqe_next = nullptr;
        } else {
            mDmaDescs[i].eof = 0;
            mDmaDescs[i].qe.stqe_next = &mDmaDescs[i + 1];
        }
    }

    mBusy = true;

    // Reset and start I2S DMA
    resetI2s();
    resetDma();
    resetFifo();

    mI2s->lc_conf.val |=
        I2S_OUT_DATA_BURST_EN | I2S_OUTDSCR_BURST_EN;
    mI2s->out_link.addr =
        reinterpret_cast<u32>(&mDmaDescs[0]); // ok reinterpret cast
    mI2s->out_link.start = 1;

    mI2s->int_clr.val = mI2s->int_raw.val;
    mI2s->int_ena.val = 0;
    mI2s->int_ena.out_eof = 1;
    esp_intr_enable(mIntrHandle);

    // Start transmission
    mI2s->conf.tx_start = 1;

    return true;
}

bool I2sSpiPeripheralEsp::waitTransmitDone(u32 timeout_ms) FL_NOEXCEPT {
    if (!mInitialized) {
        return false;
    }

    TickType_t ticks = (timeout_ms == 0)
                           ? portMAX_DELAY
                           : pdMS_TO_TICKS(timeout_ms);
    if (xSemaphoreTake(mCompleteSem, ticks) == pdTRUE) {
        mBusy = false;
        return true;
    }
    return false;
}

bool I2sSpiPeripheralEsp::isBusy() const FL_NOEXCEPT { return mBusy; }

bool I2sSpiPeripheralEsp::registerTransmitCallback(
    void *callback, void *user_ctx) FL_NOEXCEPT {
    if (!mInitialized) {
        return false;
    }
    mCallback = callback;
    mUserCtx = user_ctx;
    return true;
}

const I2sSpiConfig &I2sSpiPeripheralEsp::getConfig() const FL_NOEXCEPT {
    return mConfig;
}

u64 I2sSpiPeripheralEsp::getMicroseconds() FL_NOEXCEPT {
    return esp_timer_get_time();
}

void I2sSpiPeripheralEsp::delay(u32 ms) FL_NOEXCEPT {
    vTaskDelay(pdMS_TO_TICKS(ms));
}

} // namespace detail
} // namespace fl

#endif // FASTLED_ESP32_HAS_I2S
#endif // FL_IS_ESP32
