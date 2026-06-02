// IWYU pragma: private

/// @file lcd_spi_peripheral_esp.cpp
/// @brief ESP32-S3 LCD_CAM SPI peripheral implementation

#include "platforms/is_platform.h"
#if defined(FL_IS_ESP32)

#include "fl/stl/has_include.h"
#include "sdkconfig.h"

#if defined(FL_IS_ESP_32S3) && FL_HAS_INCLUDE("esp_lcd_panel_io.h")

#include "platforms/esp/32/drivers/lcd_spi/lcd_spi_peripheral_esp.h"
#include "fl/stl/singleton.h"
#include "fl/stl/noexcept.h"
#include "fl/log/log.h"

// IWYU pragma: begin_keep
#include "freertos/FreeRTOS.h"
// IWYU pragma: end_keep
// IWYU pragma: begin_keep
#include "freertos/semphr.h"
// IWYU pragma: end_keep
#include "esp_lcd_panel_io.h"
#include "esp_heap_caps.h"
#include "esp_timer.h"
#include "platforms/esp/esp_version.h"

// Cache sync for PSRAM DMA buffers (IDF 5.0+)
#if FL_HAS_INCLUDE("esp_cache.h")
#include "esp_cache.h"
#define FL_HAS_ESP_CACHE_MSYNC 1
#else
#define FL_HAS_ESP_CACHE_MSYNC 0
#endif

// Backfills `ESP_CACHE_MSYNC_FLAG_DIR_C2M = 0` on IDF < 5.2 (the flag was
// added in 5.2; older esp_cache_msync() defaulted to that direction). See
// FastLED #2619: previously rmt_5/rmt5_peripheral_esp.cpp.hpp owned this
// shim and lcd_spi relied on unity-build include order putting rmt_5
// first, which broke esp_extra_libs.
#include "platforms/esp/esp_cache_compat.h"

#ifndef LCD_SPI_PSRAM_ALIGNMENT
#define LCD_SPI_PSRAM_ALIGNMENT 64
#endif

namespace fl {
namespace detail {

//=============================================================================
// ISR Callback
//=============================================================================

/// @note This runs in ISR context. Any function registered via
/// registerTransmitCallback() MUST be placed in IRAM (IRAM_ATTR).
bool IRAM_ATTR lcd_spi_flush_ready(
    esp_lcd_panel_io_handle_t panel_io,
    esp_lcd_panel_io_event_data_t *edata,
    void *user_ctx) { // ok no noexcept — matches friend decl
    LcdSpiPeripheralEsp *self =
        static_cast<LcdSpiPeripheralEsp *>(user_ctx);
    self->mBusy = false;

    // Signal completion semaphore
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    if (self->mCompleteSem) {
        xSemaphoreGiveFromISR(self->mCompleteSem,
                              &xHigherPriorityTaskWoken);
    }

    bool result = false;
    if (self->mCallback) {
        // WARNING: callback MUST be in IRAM — called from ISR context.
        using CallbackType = bool (*)(void *, const void *, void *);
        auto fn =
            reinterpret_cast<CallbackType>(self->mCallback); // ok reinterpret cast
        result = fn(panel_io, edata, self->mUserCtx);
    }

    if (xHigherPriorityTaskWoken) {
        portYIELD_FROM_ISR();
    }
    // Return true if any higher-priority task was woken (by our semaphore
    // or by the user callback) so ESP-IDF LCD driver can yield correctly.
    return result || (xHigherPriorityTaskWoken == pdTRUE);
}

//=============================================================================
// Singleton
//=============================================================================

LcdSpiPeripheralEsp &LcdSpiPeripheralEsp::instance() FL_NOEXCEPT {
    return Singleton<LcdSpiPeripheralEsp>::instance();
}

LcdSpiPeripheralEsp::LcdSpiPeripheralEsp() FL_NOEXCEPT
    : mInitialized(false), mConfig(), mI80Bus(nullptr), mPanelIo(nullptr),
      mCompleteSem(nullptr), mCallback(nullptr), mUserCtx(nullptr),
      mBusy(false), mLastTransmitSize(0),
      mOwner(LcdSpiOwnerDriver::NONE) {}

LcdSpiPeripheralEsp::~LcdSpiPeripheralEsp() { deinitialize(); }

//=============================================================================
// Lifecycle
//=============================================================================

bool LcdSpiPeripheralEsp::initialize(const LcdSpiConfig &config) FL_NOEXCEPT {
    // Issue #2270: when the owning driver changes between calls (e.g. the
    // clocked-SPI driver handed the peripheral over to the clockless
    // driver, or vice-versa) we MUST fully tear down even if the lane
    // count / clock / transfer size happen to match. Otherwise the
    // previous driver's ISR callback, ring buffers, and semaphore state
    // leak into the new driver's session and the second cross-driver
    // switch silently produces no DMA output.
    const bool sameOwner =
        (config.owner == mOwner) ||
        (config.owner == LcdSpiOwnerDriver::NONE);

    if (mInitialized) {
        const bool sameShape =
            mConfig.num_lanes == config.num_lanes &&
            mConfig.clock_hz == config.clock_hz &&
            mConfig.max_transfer_bytes >= config.max_transfer_bytes;

        if (sameShape && sameOwner) {
            // Fast path - same driver, compatible config. Record the
            // (possibly NONE) owner from the caller in case it now
            // reports a concrete identity that was unset before.
            if (config.owner != LcdSpiOwnerDriver::NONE) {
                mOwner = config.owner;
            }
            return true;
        }

        // Slow path - either the shape changed or the owning driver
        // changed. Both cases require a full tear-down so we don't
        // carry stale callback / semaphore / panel_io state forward.
        teardownLocked();
    }

    if (config.num_lanes < 1 || config.num_lanes > 16) {
        FL_WARN("LcdSpiPeripheralEsp: Invalid num_lanes: "
                << config.num_lanes);
        return false;
    }

    if (config.clock_gpio < 0) {
        FL_WARN("LcdSpiPeripheralEsp: Invalid clock_gpio");
        return false;
    }

    if (config.clock_hz == 0) {
        FL_WARN("LcdSpiPeripheralEsp: Invalid clock_hz: 0");
        return false;
    }

    mConfig = config;

    // Create I80 bus configuration
    esp_lcd_i80_bus_config_t bus_config = {};
    bus_config.clk_src = LCD_CLK_SRC_PLL160M;
    // DC pin is not functionally needed for LED data streaming but
    // the I80 bus API requires a valid GPIO.  GPIO 0 is used as a
    // dummy (matching i2s_lcd_cam_peripheral_esp.cpp.hpp convention).
    // NOTE: GPIO 0 is a strapping pin — if your board uses GPIO 0 for
    // boot-mode selection, set dc_gpio in LcdSpiConfig to an unused pin.
    if (config.dc_gpio >= 0) {
        bus_config.dc_gpio_num = static_cast<gpio_num_t>(config.dc_gpio);
    } else {
        bus_config.dc_gpio_num = static_cast<gpio_num_t>(0);
        FL_WARN("LcdSpiPeripheralEsp: dc_gpio not set, defaulting to "
                "GPIO 0 (strapping pin). Set LcdSpiConfig::dc_gpio to "
                "an unused pin to avoid boot issues.");
    }
    bus_config.wr_gpio_num = static_cast<gpio_num_t>(config.clock_gpio); // PCLK as SPI clock
    bus_config.bus_width = 16;
    bus_config.max_transfer_bytes = config.max_transfer_bytes;

#if ESP_IDF_VERSION < ESP_IDF_VERSION_VAL(5, 3, 0)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
    bus_config.psram_trans_align = LCD_SPI_PSRAM_ALIGNMENT;
    bus_config.sram_trans_align = 4;
#pragma GCC diagnostic pop
#else
    bus_config.dma_burst_size = 64;
#endif

    for (int i = 0; i < 16; i++) {
        if (i < config.num_lanes) {
            bus_config.data_gpio_nums[i] = static_cast<gpio_num_t>(config.data_gpios[i]);
        } else {
            // I80 bus requires valid GPIOs for all bus_width pins.
            // Use GPIO 0 for unused lanes (matches I2S LCD_CAM convention).
            bus_config.data_gpio_nums[i] = static_cast<gpio_num_t>(0);
        }
    }

    esp_err_t err = esp_lcd_new_i80_bus(&bus_config, &mI80Bus);
    if (err != ESP_OK) {
        FL_WARN("LcdSpiPeripheralEsp: Failed to create I80 bus: " << err);
        return false;
    }

    // Panel IO is created lazily in transmit() so it can be recreated
    // when the transaction size changes. This avoids stale GDMA
    // descriptor state between different-sized transfers.

    // Create completion semaphore and prime it so the first transmit()
    // can take it without blocking. Subsequent transmits wait for the
    // ISR to give the semaphore after DMA completion.
    if (mCompleteSem == nullptr) {
        mCompleteSem = xSemaphoreCreateBinary();
        xSemaphoreGive(mCompleteSem);
    }

    mLastTransmitSize = 0;
    mInitialized = true;
    mOwner = config.owner;
    FL_DBG("LcdSpiPeripheralEsp: Initialized with " << config.num_lanes
           << " lanes, " << config.clock_hz << " Hz clock, owner="
           << static_cast<int>(config.owner));
    return true;
}

void LcdSpiPeripheralEsp::teardownLocked() FL_NOEXCEPT {
    // Clear the ISR callback BEFORE deleting the panel_io so a late
    // DMA-done interrupt from the previous owner can't dispatch into a
    // stale context. The ESP-IDF LCD driver invokes our callback via
    // the on_color_trans_done hook set when we create the panel_io, so
    // nulling our user-level callback pointers here stops the trampoline
    // in lcd_spi_flush_ready() from fanning out to the previous driver.
    mCallback = nullptr;
    mUserCtx = nullptr;

    if (mPanelIo != nullptr) {
        esp_lcd_panel_io_del(mPanelIo);
        mPanelIo = nullptr;
    }
    if (mI80Bus != nullptr) {
        esp_lcd_del_i80_bus(mI80Bus);
        mI80Bus = nullptr;
    }
    if (mCompleteSem != nullptr) {
        // Drain any stale "given" state from the previous owner, then
        // delete the semaphore. Subsequent initialize() calls recreate
        // and re-prime it so the new owner starts from a known-good
        // state (given, ready to be taken by the first transmit()).
        xSemaphoreTake(mCompleteSem, 0);
        vSemaphoreDelete(mCompleteSem);
        mCompleteSem = nullptr;
    }
    mInitialized = false;
    mBusy = false;
    mLastTransmitSize = 0;
    mOwner = LcdSpiOwnerDriver::NONE;
}

void LcdSpiPeripheralEsp::deinitialize() FL_NOEXCEPT {
    teardownLocked();
}

bool LcdSpiPeripheralEsp::isInitialized() const FL_NOEXCEPT {
    return mInitialized;
}

//=============================================================================
// Buffer Management
//=============================================================================

u16 *LcdSpiPeripheralEsp::allocateBuffer(size_t size_bytes) FL_NOEXCEPT {
    size_t aligned_size = ((size_bytes + 63) / 64) * 64;
    void *buffer = nullptr;

    if (mConfig.use_psram) {
        buffer = heap_caps_aligned_calloc(LCD_SPI_PSRAM_ALIGNMENT,
                                          aligned_size, 1,
                                          MALLOC_CAP_SPIRAM | MALLOC_CAP_DMA |
                                              MALLOC_CAP_8BIT);
    }

    if (buffer == nullptr) {
        buffer = heap_caps_aligned_calloc(LCD_SPI_PSRAM_ALIGNMENT,
                                          aligned_size, 1,
                                          MALLOC_CAP_DMA | MALLOC_CAP_8BIT);
    }

    return static_cast<u16 *>(buffer);
}

void LcdSpiPeripheralEsp::freeBuffer(u16 *buffer) FL_NOEXCEPT {
    if (buffer != nullptr) {
        heap_caps_free(buffer);
    }
}

//=============================================================================
// Transmission
//=============================================================================

bool FL_IRAM LcdSpiPeripheralEsp::transmit(const u16 *buffer,
                                   size_t size_bytes) FL_NOEXCEPT {
    if (!mInitialized || mI80Bus == nullptr) {
        return false;
    }

    // Block until previous DMA is truly complete. The semaphore is
    // primed during initialize() so the first call returns immediately.
    //
    // ISR safety: when ChannelDriverLcdClockless::isrChunkDone re-arms the
    // next chunk from the on_color_trans_done callback, transmit() runs in
    // interrupt context. lcd_spi_flush_ready() already did xSemaphoreGiveFromISR
    // *before* invoking our user callback, so the take is guaranteed to be
    // non-blocking — but we MUST use the FromISR variant because the regular
    // xSemaphoreTake() asserts (configASSERT) when called from ISR.
    if (mCompleteSem) {
        if (xPortInIsrContext()) {
            BaseType_t hpw = pdFALSE;
            if (xSemaphoreTakeFromISR(mCompleteSem, &hpw) != pdTRUE) {
                // Semaphore should always be available here because
                // lcd_spi_flush_ready already gave it before invoking our
                // callback. If not, abort the re-arm — the next CPU-side
                // transmit() will pick up where we left off.
                mBusy = false;
                return false;
            }
            if (hpw) {
                portYIELD_FROM_ISR();
            }
        } else {
            if (xSemaphoreTake(mCompleteSem, pdMS_TO_TICKS(2000)) != pdTRUE) {
                esp_rom_printf("LcdSpiPeripheralEsp: Timed out waiting for previous transmit to complete\n");  // ok esp_rom_printf - FL_IRAM transmit() context, FL_WARN unsafe
                mBusy = false;
                return false;
            }
        }
    }

    // Recreate panel IO when transaction size changes. The ESP-IDF I80
    // bus driver builds a GDMA descriptor chain sized for each
    // transaction. Reusing the same panel_io handle with a different
    // size can leave stale DMA descriptor state that causes the second
    // transmit to produce no output (ESP_OK but no DMA activity).
    // Recreating panel_io is cheap (descriptor alloc + config) and does
    // not touch the I80 bus GPIO routing.
    if (size_bytes != mLastTransmitSize && mPanelIo != nullptr) {
        esp_lcd_panel_io_del(mPanelIo);
        mPanelIo = nullptr;
    }

    if (mPanelIo == nullptr) {
        esp_lcd_panel_io_i80_config_t io_config = {};
        io_config.cs_gpio_num = GPIO_NUM_NC;
        io_config.pclk_hz = mConfig.clock_hz;
        // trans_queue_depth=2: ChannelDriverLcdClockless::isrChunkDone calls
        // back into tx_color() from on_color_trans_done. With depth=1 the
        // currently-running transaction has not been retired from the I80
        // queue yet, so the re-arm blocks waiting for a queue slot — from
        // ISR context — and trips the interrupt watchdog. Depth=2 leaves
        // a free slot for the ISR-driven re-arm.
        io_config.trans_queue_depth = 2;
        io_config.dc_levels = {
            .dc_idle_level = 0,
            .dc_cmd_level = 0,
            .dc_dummy_level = 0,
            .dc_data_level = 1,
        };
        io_config.lcd_cmd_bits = 0;
        io_config.lcd_param_bits = 0;
        io_config.user_ctx = this;
        io_config.on_color_trans_done = lcd_spi_flush_ready;

        esp_err_t err =
            esp_lcd_new_panel_io_i80(mI80Bus, &io_config, &mPanelIo);
        if (err != ESP_OK) {
            esp_rom_printf("LcdSpiPeripheralEsp: Failed to recreate panel IO: %d\n", static_cast<int>(err));  // ok esp_rom_printf - FL_IRAM transmit() context, FL_WARN unsafe
            return false;
        }
    }

    // Flush CPU cache to PSRAM so DMA sees the latest buffer contents.
    // esp_lcd_panel_io_tx_color() should handle this internally in
    // IDF 5.2+, but explicit sync guards against edge cases where the
    // same buffer pointer is reused with different data.
#if FL_HAS_ESP_CACHE_MSYNC
    {
        // Round up to cache-line boundary (64 bytes). Safe because the
        // buffer allocation is always 64-byte aligned and oversized.
        size_t sync_size = ((size_bytes + 63) & ~(size_t)63);
        esp_cache_msync((void *)buffer, sync_size,
                        ESP_CACHE_MSYNC_FLAG_DIR_C2M);
    }
#endif

    mBusy = true;
    mLastTransmitSize = size_bytes;

    esp_err_t err =
        esp_lcd_panel_io_tx_color(mPanelIo, 0x2C, buffer, size_bytes);

    if (err != ESP_OK) {
        mBusy = false;
        esp_rom_printf("LcdSpiPeripheralEsp: tx_color failed: %d\n", static_cast<int>(err));  // ok esp_rom_printf - FL_IRAM transmit() context, FL_WARN unsafe
        return false;
    }

    return true;
}

bool LcdSpiPeripheralEsp::waitTransmitDone(u32 timeout_ms) FL_NOEXCEPT {
    if (!mInitialized || mCompleteSem == nullptr) {
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

bool LcdSpiPeripheralEsp::isBusy() const FL_NOEXCEPT { return mBusy; }

//=============================================================================
// Callback
//=============================================================================

bool LcdSpiPeripheralEsp::registerTransmitCallback(
    void *callback, void *user_ctx) FL_NOEXCEPT {
    if (!mInitialized) {
        return false;
    }
    mCallback = callback;
    mUserCtx = user_ctx;
    return true;
}

const LcdSpiConfig &LcdSpiPeripheralEsp::getConfig() const FL_NOEXCEPT {
    return mConfig;
}

u64 LcdSpiPeripheralEsp::getMicroseconds() FL_NOEXCEPT {
    return esp_timer_get_time();
}

void LcdSpiPeripheralEsp::delay(u32 ms) FL_NOEXCEPT {
    vTaskDelay(pdMS_TO_TICKS(ms));
}

} // namespace detail
} // namespace fl

#endif // FL_IS_ESP_32S3 && esp_lcd_panel_io.h
#endif // FL_IS_ESP32
