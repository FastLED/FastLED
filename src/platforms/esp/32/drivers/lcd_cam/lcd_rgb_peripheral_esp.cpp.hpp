/// @file lcd_rgb_peripheral_esp.cpp
/// @brief ESP32-P4 LCD RGB peripheral implementation

#if defined(ESP32)

#include "sdkconfig.h"

#if defined(CONFIG_IDF_TARGET_ESP32P4) && __has_include("esp_lcd_panel_rgb.h")

#include "lcd_rgb_peripheral_esp.h"
#include "fl/singleton.h"
#include "fl/warn.h"

#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "esp_lcd_panel_rgb.h"
#include "esp_heap_caps.h"
#include "esp_timer.h"
#include "platforms/esp/esp_version.h"

// Alignment for DMA buffers
#ifndef LCD_DRIVER_PSRAM_DATA_ALIGNMENT
#define LCD_DRIVER_PSRAM_DATA_ALIGNMENT 64
#endif

namespace fl {
namespace detail {

//=============================================================================
// Singleton Instance
//=============================================================================

LcdRgbPeripheralEsp& LcdRgbPeripheralEsp::instance() {
    return Singleton<LcdRgbPeripheralEsp>::instance();
}

//=============================================================================
// Constructor / Destructor
//=============================================================================

LcdRgbPeripheralEsp::LcdRgbPeripheralEsp()
    : mInitialized(false),
      mConfig(),
      mPanelHandle(nullptr),
      mCallback(nullptr),
      mUserCtx(nullptr),
      mBusy(false) {
}

LcdRgbPeripheralEsp::~LcdRgbPeripheralEsp() {
    deinitialize();
}

//=============================================================================
// Lifecycle Methods
//=============================================================================

bool LcdRgbPeripheralEsp::initialize(const LcdRgbPeripheralConfig& config) {
    if (mInitialized) {
        FL_WARN("LcdRgbPeripheralEsp: Already initialized");
        return false;
    }

    mConfig = config;

    // Validate configuration
    if (config.num_lanes < 1 || config.num_lanes > 16) {
        FL_WARN("LcdRgbPeripheralEsp: Invalid num_lanes: " << config.num_lanes);
        return false;
    }

    if (config.pclk_hz == 0) {
        FL_WARN("LcdRgbPeripheralEsp: Invalid pclk_hz: 0");
        return false;
    }

    // Create RGB LCD panel configuration
    esp_lcd_rgb_panel_config_t panel_config = {};
    panel_config.clk_src = LCD_CLK_SRC_DEFAULT;
    panel_config.data_width = 16;
    panel_config.bits_per_pixel = 16;

    // DMA configuration (IDF version dependent)
#if ESP_IDF_VERSION < ESP_IDF_VERSION_VAL(5, 3, 0)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
    panel_config.psram_trans_align = LCD_DRIVER_PSRAM_DATA_ALIGNMENT;
#pragma GCC diagnostic pop
#else
    panel_config.dma_burst_size = 64;
#endif

    panel_config.num_fbs = 0;  // We manage our own frame buffers

    // Timing parameters
    panel_config.timings.pclk_hz = config.pclk_hz;
    panel_config.timings.h_res = config.h_res;
    panel_config.timings.v_res = config.v_res;
    panel_config.timings.hsync_pulse_width = 1;
    panel_config.timings.hsync_back_porch = 0;
    panel_config.timings.hsync_front_porch = 0;
    panel_config.timings.vsync_pulse_width = 1;
    panel_config.timings.vsync_back_porch = 1;
    panel_config.timings.vsync_front_porch = config.vsync_front_porch;

    // GPIO configuration
    panel_config.pclk_gpio_num = config.pclk_gpio;
    panel_config.hsync_gpio_num = config.hsync_gpio;
    panel_config.vsync_gpio_num = config.vsync_gpio;
    panel_config.de_gpio_num = config.de_gpio;
    panel_config.disp_gpio_num = config.disp_gpio;

    // Data GPIO pins
    for (int i = 0; i < 16; i++) {
        if (i < static_cast<int>(config.num_lanes)) {
            panel_config.data_gpio_nums[i] = config.data_gpios[i];
        } else {
            panel_config.data_gpio_nums[i] = -1;
        }
    }

    // Flags
    panel_config.flags.fb_in_psram = config.use_psram ? 1 : 0;
    panel_config.flags.refresh_on_demand = 1;

    // Create RGB panel
    esp_err_t err = esp_lcd_new_rgb_panel(&panel_config, &mPanelHandle);
    if (err != ESP_OK) {
        FL_WARN("LcdRgbPeripheralEsp: Failed to create RGB panel: " << err);
        return false;
    }

    // Initialize panel
    err = esp_lcd_panel_init(mPanelHandle);
    if (err != ESP_OK) {
        FL_WARN("LcdRgbPeripheralEsp: Failed to initialize panel: " << err);
        esp_lcd_panel_del(mPanelHandle);
        mPanelHandle = nullptr;
        return false;
    }

    mInitialized = true;
    return true;
}

void LcdRgbPeripheralEsp::deinitialize() {
    if (mPanelHandle != nullptr) {
        esp_lcd_panel_del(mPanelHandle);
        mPanelHandle = nullptr;
    }
    mInitialized = false;
    mCallback = nullptr;
    mUserCtx = nullptr;
    mBusy = false;
}

bool LcdRgbPeripheralEsp::isInitialized() const {
    return mInitialized;
}

//=============================================================================
// Buffer Management
//=============================================================================

uint16_t* LcdRgbPeripheralEsp::allocateFrameBuffer(size_t size_bytes) {
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

void LcdRgbPeripheralEsp::freeFrameBuffer(uint16_t* buffer) {
    if (buffer != nullptr) {
        heap_caps_free(buffer);
    }
}

//=============================================================================
// Transmission Methods
//=============================================================================

bool LcdRgbPeripheralEsp::drawFrame(const uint16_t* buffer, size_t size_bytes) {
    if (!mInitialized || mPanelHandle == nullptr) {
        return false;
    }

    mBusy = true;

    // Calculate width in pixels (each pixel is 2 bytes)
    size_t width = size_bytes / 2;

    esp_err_t err = esp_lcd_panel_draw_bitmap(
        mPanelHandle,
        0, 0,           // x, y offset
        width, 1,       // width (pixels), height
        buffer
    );

    if (err != ESP_OK) {
        mBusy = false;
        return false;
    }

    return true;
}

bool LcdRgbPeripheralEsp::waitFrameDone(uint32_t timeout_ms) {
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

bool LcdRgbPeripheralEsp::isBusy() const {
    return mBusy;
}

//=============================================================================
// Callback Registration
//=============================================================================

bool LcdRgbPeripheralEsp::registerDrawCallback(void* callback, void* user_ctx) {
    if (!mInitialized || mPanelHandle == nullptr) {
        return false;
    }

    mCallback = callback;
    mUserCtx = user_ctx;

    // Wrapper callback that calls user callback and clears busy flag
    struct CallbackData {
        LcdRgbPeripheralEsp* self;
        void* user_callback;
        void* user_ctx;
    };

    // Store callback info (static to persist across calls)
    static CallbackData cb_data;
    cb_data.self = this;
    cb_data.user_callback = callback;
    cb_data.user_ctx = user_ctx;

    // Register with ESP-IDF
    esp_lcd_rgb_panel_event_callbacks_t cbs = {};
    cbs.on_vsync = [](esp_lcd_panel_handle_t panel, const esp_lcd_rgb_panel_event_data_t* edata, void* ctx) -> bool {
        CallbackData* data = static_cast<CallbackData*>(ctx);
        data->self->mBusy = false;

        // Call user callback if registered
        if (data->user_callback) {
            using CallbackType = bool (*)(void*, const void*, void*);
            auto fn = reinterpret_cast<CallbackType>(data->user_callback); // ok reinterpret cast
            return fn(panel, edata, data->user_ctx);
        }
        return false;
    };

    esp_err_t err = esp_lcd_rgb_panel_register_event_callbacks(mPanelHandle, &cbs, &cb_data);
    return err == ESP_OK;
}

//=============================================================================
// State Inspection
//=============================================================================

const LcdRgbPeripheralConfig& LcdRgbPeripheralEsp::getConfig() const {
    return mConfig;
}

//=============================================================================
// Platform Utilities
//=============================================================================

uint64_t LcdRgbPeripheralEsp::getMicroseconds() {
    return esp_timer_get_time();
}

void LcdRgbPeripheralEsp::delay(uint32_t ms) {
    vTaskDelay(pdMS_TO_TICKS(ms));
}

} // namespace detail
} // namespace fl

#endif // CONFIG_IDF_TARGET_ESP32P4 && esp_lcd_panel_rgb.h
#endif // ESP32
