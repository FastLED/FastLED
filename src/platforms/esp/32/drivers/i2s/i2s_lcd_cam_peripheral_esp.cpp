/// @file i2s_lcd_cam_peripheral_esp.cpp
/// @brief ESP32-S3 I2S LCD_CAM peripheral implementation

#if defined(ESP32)

#include "sdkconfig.h"

#if defined(CONFIG_IDF_TARGET_ESP32S3) && __has_include("esp_lcd_panel_io.h")

#include "i2s_lcd_cam_peripheral_esp.h"
#include "fl/singleton.h"
#include "fl/warn.h"

#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "esp_lcd_panel_io.h"
#include "esp_heap_caps.h"
#include "esp_timer.h"
#include "platforms/esp/esp_version.h"

// Alignment for DMA buffers
#ifndef LCD_DRIVER_PSRAM_DATA_ALIGNMENT
#define LCD_DRIVER_PSRAM_DATA_ALIGNMENT 64
#endif

// Default I2S clock frequency (2.4 MHz - standard for WS2812)
#ifndef FASTLED_ESP32S3_I2S_CLOCK_HZ
#define FASTLED_ESP32S3_I2S_CLOCK_HZ uint32_t(24 * 100 * 1000)
#endif

namespace fl {
namespace detail {

//=============================================================================
// ISR Callback Wrapper
//=============================================================================

/// @brief ISR callback for transfer complete
/// @param panel_io Panel IO handle
/// @param edata Event data
/// @param user_ctx User context (I2sLcdCamPeripheralEsp*)
/// @return true if a higher priority task was woken
bool IRAM_ATTR i2s_lcd_cam_flush_ready(
    esp_lcd_panel_io_handle_t panel_io,
    esp_lcd_panel_io_event_data_t* edata,
    void* user_ctx) {

    I2sLcdCamPeripheralEsp* self = static_cast<I2sLcdCamPeripheralEsp*>(user_ctx);

    // Clear busy flag
    self->mBusy = false;

    // Call user callback if registered
    if (self->mCallback) {
        using CallbackType = bool (*)(void*, const void*, void*);
        auto fn = reinterpret_cast<CallbackType>(self->mCallback); // ok reinterpret cast
        return fn(panel_io, edata, self->mUserCtx);
    }

    return false;
}

//=============================================================================
// Singleton Instance
//=============================================================================

I2sLcdCamPeripheralEsp& I2sLcdCamPeripheralEsp::instance() {
    return Singleton<I2sLcdCamPeripheralEsp>::instance();
}

//=============================================================================
// Constructor / Destructor
//=============================================================================

I2sLcdCamPeripheralEsp::I2sLcdCamPeripheralEsp()
    : mInitialized(false),
      mConfig(),
      mI80Bus(nullptr),
      mPanelIo(nullptr),
      mCallback(nullptr),
      mUserCtx(nullptr),
      mBusy(false) {
}

I2sLcdCamPeripheralEsp::~I2sLcdCamPeripheralEsp() {
    deinitialize();
}

//=============================================================================
// Lifecycle Methods
//=============================================================================

bool I2sLcdCamPeripheralEsp::initialize(const I2sLcdCamConfig& config) {
    if (mInitialized) {
        FL_WARN("I2sLcdCamPeripheralEsp: Already initialized");
        return false;
    }

    mConfig = config;

    // Validate configuration
    if (config.num_lanes < 1 || config.num_lanes > 16) {
        FL_WARN("I2sLcdCamPeripheralEsp: Invalid num_lanes: " << config.num_lanes);
        return false;
    }

    if (config.pclk_hz == 0) {
        FL_WARN("I2sLcdCamPeripheralEsp: Invalid pclk_hz: 0");
        return false;
    }

    // Create I80 bus configuration
    esp_lcd_i80_bus_config_t bus_config = {};
    bus_config.clk_src = LCD_CLK_SRC_PLL160M;
    bus_config.dc_gpio_num = 0;  // Not used for LED driving
    bus_config.wr_gpio_num = 0;  // Not used for LED driving
    bus_config.bus_width = 16;
    bus_config.max_transfer_bytes = config.max_transfer_bytes;

    // DMA configuration (IDF version dependent)
#if ESP_IDF_VERSION < ESP_IDF_VERSION_VAL(5, 3, 0)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
    bus_config.psram_trans_align = LCD_DRIVER_PSRAM_DATA_ALIGNMENT;
    bus_config.sram_trans_align = 4;
#pragma GCC diagnostic pop
#else
    bus_config.dma_burst_size = 64;
#endif

    // Data GPIO pins
    for (int i = 0; i < 16; i++) {
        if (i < config.num_lanes) {
            bus_config.data_gpio_nums[i] = config.data_gpios[i];
        } else {
            bus_config.data_gpio_nums[i] = 0;  // Unused pins set to 0
        }
    }

    // Create I80 bus
    esp_err_t err = esp_lcd_new_i80_bus(&bus_config, &mI80Bus);
    if (err != ESP_OK) {
        FL_WARN("I2sLcdCamPeripheralEsp: Failed to create I80 bus: " << err);
        return false;
    }

    // Create panel IO configuration
    esp_lcd_panel_io_i80_config_t io_config = {};
    io_config.cs_gpio_num = -1;  // No CS pin
    io_config.pclk_hz = config.pclk_hz > 0 ? config.pclk_hz : FASTLED_ESP32S3_I2S_CLOCK_HZ;
    io_config.trans_queue_depth = 1;
    io_config.dc_levels = {
        .dc_idle_level = 0,
        .dc_cmd_level = 0,
        .dc_dummy_level = 0,
        .dc_data_level = 1,
    };
    io_config.lcd_cmd_bits = 0;
    io_config.lcd_param_bits = 0;
    io_config.user_ctx = this;
    io_config.on_color_trans_done = i2s_lcd_cam_flush_ready;

    // Create panel IO
    err = esp_lcd_new_panel_io_i80(mI80Bus, &io_config, &mPanelIo);
    if (err != ESP_OK) {
        FL_WARN("I2sLcdCamPeripheralEsp: Failed to create panel IO: " << err);
        esp_lcd_del_i80_bus(mI80Bus);
        mI80Bus = nullptr;
        return false;
    }

    mInitialized = true;
    return true;
}

void I2sLcdCamPeripheralEsp::deinitialize() {
    if (mPanelIo != nullptr) {
        esp_lcd_panel_io_del(mPanelIo);
        mPanelIo = nullptr;
    }
    if (mI80Bus != nullptr) {
        esp_lcd_del_i80_bus(mI80Bus);
        mI80Bus = nullptr;
    }
    mInitialized = false;
    mCallback = nullptr;
    mUserCtx = nullptr;
    mBusy = false;
}

bool I2sLcdCamPeripheralEsp::isInitialized() const {
    return mInitialized;
}

//=============================================================================
// Buffer Management
//=============================================================================

uint16_t* I2sLcdCamPeripheralEsp::allocateBuffer(size_t size_bytes) {
    // Round up to 64-byte alignment
    size_t aligned_size = ((size_bytes + 63) / 64) * 64;

    uint32_t alloc_caps;
    if (mConfig.use_psram) {
        alloc_caps = MALLOC_CAP_SPIRAM | MALLOC_CAP_DMA | MALLOC_CAP_8BIT;
    } else {
        alloc_caps = MALLOC_CAP_DMA | MALLOC_CAP_8BIT;
    }

    void* buffer = heap_caps_aligned_alloc(
        LCD_DRIVER_PSRAM_DATA_ALIGNMENT,
        aligned_size,
        alloc_caps
    );

    // Fallback to internal DMA RAM if PSRAM allocation failed
    if (buffer == nullptr && mConfig.use_psram) {
        buffer = heap_caps_aligned_alloc(
            LCD_DRIVER_PSRAM_DATA_ALIGNMENT,
            aligned_size,
            MALLOC_CAP_DMA | MALLOC_CAP_8BIT
        );
    }

    return static_cast<uint16_t*>(buffer);
}

void I2sLcdCamPeripheralEsp::freeBuffer(uint16_t* buffer) {
    if (buffer != nullptr) {
        heap_caps_free(buffer);
    }
}

//=============================================================================
// Transmission Methods
//=============================================================================

bool I2sLcdCamPeripheralEsp::transmit(const uint16_t* buffer, size_t size_bytes) {
    if (!mInitialized || mPanelIo == nullptr) {
        return false;
    }

    mBusy = true;

    // Use esp_lcd_panel_io_tx_color to transmit data via LCD_CAM DMA
    // Command 0x2C is the standard "write memory" command for displays
    // The LCD_CAM peripheral will DMA the buffer to the data pins
    esp_err_t err = esp_lcd_panel_io_tx_color(mPanelIo, 0x2C, buffer, size_bytes);

    if (err != ESP_OK) {
        mBusy = false;
        return false;
    }

    return true;
}

bool I2sLcdCamPeripheralEsp::waitTransmitDone(uint32_t timeout_ms) {
    if (!mInitialized) {
        return false;
    }

    // Simple polling wait (callback will clear mBusy)
    uint32_t start = (uint32_t)(esp_timer_get_time() / 1000);
    while (mBusy) {
        if (timeout_ms > 0) {
            uint32_t now = (uint32_t)(esp_timer_get_time() / 1000);
            if ((now - start) >= timeout_ms) {
                return false;  // Timeout
            }
        }
        vTaskDelay(1);  // Yield
    }
    return true;
}

bool I2sLcdCamPeripheralEsp::isBusy() const {
    return mBusy;
}

//=============================================================================
// Callback Registration
//=============================================================================

bool I2sLcdCamPeripheralEsp::registerTransmitCallback(void* callback, void* user_ctx) {
    if (!mInitialized) {
        return false;
    }

    // Store callback info for ISR to invoke
    mCallback = callback;
    mUserCtx = user_ctx;
    return true;
}

//=============================================================================
// State Inspection
//=============================================================================

const I2sLcdCamConfig& I2sLcdCamPeripheralEsp::getConfig() const {
    return mConfig;
}

//=============================================================================
// Platform Utilities
//=============================================================================

uint64_t I2sLcdCamPeripheralEsp::getMicroseconds() {
    return esp_timer_get_time();
}

void I2sLcdCamPeripheralEsp::delay(uint32_t ms) {
    vTaskDelay(pdMS_TO_TICKS(ms));
}

} // namespace detail
} // namespace fl

#endif // CONFIG_IDF_TARGET_ESP32S3 && esp_lcd_panel_io.h
#endif // ESP32
