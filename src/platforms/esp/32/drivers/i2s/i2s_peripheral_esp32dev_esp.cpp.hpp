// IWYU pragma: private

/// @file i2s_peripheral_esp32dev_esp.cpp.hpp
/// @brief Real-hardware impl of `II2sPeripheralEsp32Dev` (#3474 Stage 2).

#include "platforms/is_platform.h"
#ifdef FL_IS_ESP32

#include "fl/stl/compiler_control.h"
#include "platforms/esp/32/feature_flags/enabled.h"

#if FASTLED_ESP32_HAS_I2S

#include "platforms/esp/32/drivers/i2s/i2s_peripheral_esp32dev_esp.h"

#include "fl/log/log.h"
#include "fl/stl/noexcept.h"

FL_EXTERN_C_BEGIN
// IWYU pragma: begin_keep
#include "driver/gpio.h"
#include "driver/i2s.h"
#include "esp_err.h"
#include "esp_heap_caps.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
// IWYU pragma: end_keep
FL_EXTERN_C_END

namespace fl {

//=============================================================================
// Ctor / dtor
//=============================================================================

I2sPeripheralEsp32DevEsp::I2sPeripheralEsp32DevEsp() FL_NO_EXCEPT
    : mConfig(),
      mInitialized(false),
      mBusy(false),
      mCallback(nullptr),
      mCallbackUserCtx(nullptr) {}

I2sPeripheralEsp32DevEsp::~I2sPeripheralEsp32DevEsp() {
    if (mInitialized) {
        deinitialize();
    }
}

//=============================================================================
// Lifecycle
//=============================================================================

bool I2sPeripheralEsp32DevEsp::initialize(
    const I2sEsp32DevPeripheralConfig &cfg) FL_NO_EXCEPT {
    if (mInitialized) {
        FL_WARN_F("I2sPeripheralEsp32DevEsp: already initialized");
        return false;
    }
    mConfig = cfg;

    // IDF v4 I2S TX mode with DMA. The classic-ESP32 I2S peripheral
    // has 8 DMA buffers per channel by default; use 4×1024 for a
    // healthy TX window. `dma_buf_len` is in samples, not bytes.
    i2s_config_t i2s_cfg = {};
    i2s_cfg.mode = static_cast<i2s_mode_t>(I2S_MODE_MASTER | I2S_MODE_TX);
    i2s_cfg.sample_rate = cfg.mPixelClockHz > 0 ? cfg.mPixelClockHz : 2400000u;
    i2s_cfg.bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT;
    i2s_cfg.channel_format = I2S_CHANNEL_FMT_ONLY_RIGHT;
    i2s_cfg.communication_format = I2S_COMM_FORMAT_STAND_I2S;
    i2s_cfg.intr_alloc_flags = 0;
    i2s_cfg.dma_buf_count = 4;
    i2s_cfg.dma_buf_len = 1024;
    i2s_cfg.use_apll = false;
    i2s_cfg.tx_desc_auto_clear = true;
    i2s_cfg.fixed_mclk = 0;

    const i2s_port_t port = static_cast<i2s_port_t>(cfg.mI2sPort == 0 ? 0 : 1);
    esp_err_t err = i2s_driver_install(port, &i2s_cfg, 0, nullptr);
    if (err != ESP_OK) {
        FL_WARN_F("I2sPeripheralEsp32DevEsp: i2s_driver_install failed (%s)",
                  err);
        return false;
    }

    // Leave the pin config empty — a follow-up wires the parallel-out
    // pin assignments through the config struct. For Stage 2 v1 we
    // just want a DMA queue that we can push bytes into for
    // architectural validation. Real WS2812 output on device requires
    // the parallel-out register poke, which lives in the next PR.
    mInitialized = true;
    mBusy = false;
    return true;
}

void I2sPeripheralEsp32DevEsp::deinitialize() FL_NO_EXCEPT {
    if (!mInitialized) {
        return;
    }
    const i2s_port_t port =
        static_cast<i2s_port_t>(mConfig.mI2sPort == 0 ? 0 : 1);
    (void)i2s_driver_uninstall(port);
    mInitialized = false;
    mBusy = false;
}

bool I2sPeripheralEsp32DevEsp::isInitialized() const FL_NO_EXCEPT {
    return mInitialized;
}

//=============================================================================
// Buffer management
//=============================================================================

u8 *I2sPeripheralEsp32DevEsp::allocateBuffer(size_t size_bytes) FL_NO_EXCEPT {
    if (size_bytes == 0) {
        return nullptr;
    }
    void *p = heap_caps_malloc(size_bytes, MALLOC_CAP_DMA);
    return static_cast<u8 *>(p);
}

void I2sPeripheralEsp32DevEsp::freeBuffer(u8 *buffer) FL_NO_EXCEPT {
    if (buffer != nullptr) {
        heap_caps_free(buffer);
    }
}

//=============================================================================
// Transmission
//=============================================================================

bool I2sPeripheralEsp32DevEsp::transmit(const u8 *buffer,
                                        size_t size_bytes) FL_NO_EXCEPT {
    if (!mInitialized || buffer == nullptr || size_bytes == 0) {
        return false;
    }
    if (mBusy) {
        FL_WARN_F("I2sPeripheralEsp32DevEsp: transmit while busy");
        return false;
    }
    mBusy = true;

    const i2s_port_t port =
        static_cast<i2s_port_t>(mConfig.mI2sPort == 0 ? 0 : 1);
    size_t bytes_written = 0;
    // The IDF driver's own DMA queue absorbs the write; the ISR
    // drains the FIFO onward. `i2s_write` blocks the caller until
    // every byte has landed in the DMA ring, but the actual output
    // continues asynchronously in the driver's ISR chain.
    esp_err_t err =
        i2s_write(port, buffer, size_bytes, &bytes_written, portMAX_DELAY);
    if (err != ESP_OK) {
        FL_WARN_F("I2sPeripheralEsp32DevEsp: i2s_write failed (%s)", err);
        mBusy = false;
        return false;
    }

    // Fire the completion callback synchronously here, mirroring the
    // "buffer accepted by DMA, treat as done for engine bookkeeping"
    // semantics the engine expects. The engine's `poll()` picks up
    // the flag on the next tick and drops the driver back to READY.
    // Stage 3 replaces this with a real DMA-done ISR trampoline.
    mBusy = false;
    if (mCallback != nullptr) {
        (*mCallback)(mCallbackUserCtx);
    }
    return true;
}

bool I2sPeripheralEsp32DevEsp::waitTransmitDone(u32 timeout_ms) FL_NO_EXCEPT {
    (void)timeout_ms;
    // `transmit()` is synchronous relative to the DMA queue, so this
    // is a straight status query.
    return !mBusy;
}

bool I2sPeripheralEsp32DevEsp::isBusy() const FL_NO_EXCEPT { return mBusy; }

//=============================================================================
// Callback registration
//=============================================================================

bool I2sPeripheralEsp32DevEsp::registerTransmitCallback(
    I2sEsp32DevTxDoneCallback cb, void *user_ctx) FL_NO_EXCEPT {
    mCallback = cb;
    mCallbackUserCtx = user_ctx;
    return true;
}

//=============================================================================
// Introspection
//=============================================================================

const I2sEsp32DevPeripheralConfig &
I2sPeripheralEsp32DevEsp::getConfig() const FL_NO_EXCEPT {
    return mConfig;
}

} // namespace fl

#endif // FASTLED_ESP32_HAS_I2S
#endif // FL_IS_ESP32
