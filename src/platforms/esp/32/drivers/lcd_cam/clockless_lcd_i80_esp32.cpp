
#if defined(ESP32)
#include "sdkconfig.h"

// I80 LCD driver is only available on ESP32-S3 and ESP32-P4
#if defined(CONFIG_IDF_TARGET_ESP32S3) || defined(CONFIG_IDF_TARGET_ESP32P4)

// Feature-based detection: compile I80 LCD driver if platform supports it
// The lcd_driver_i80.h header will provide compile-time errors if headers are missing
#if __has_include("hal/lcd_hal.h") && __has_include("soc/lcd_periph.h")

#define FASTLED_INTERNAL
#include "fl/fastled.h"

#include "crgb.h"
#include "eorder.h"
#include "fl/map.h"
#include "fl/singleton.h"
#include "fl/vector.h"
#include "fl/warn.h"
#include "fl/math_macros.h"
#include "fl/cstring.h"
#include "pixel_iterator.h"
#include "fl/rectangular_draw_buffer.h"
#include "cpixel_ledcontroller.h"
#include "platforms/assert_defs.h"
#include "clockless_lcd_i80_esp32.h"
#include "lcd_i80_registry.h"
#include "esp_log.h"

#define LCD_TAG "FastLED_LCD"

namespace fl {

// Forward declare for use in anonymous namespace
class LCDI80Esp32GroupBase;

} // namespace fl

namespace { // anonymous namespace

typedef fl::FixedVector<int, 16> PinList16;
typedef uint8_t LCDPin;

} // anonymous namespace

namespace fl {

// Concrete (non-template) base class for LCD I80 groups
// Accepts runtime timing configuration instead of compile-time templates
class LCDI80Esp32GroupBase {
  public:

    fl::unique_ptr<fl::LcdI80DriverBase> mDriver;
    fl::RectangularDrawBuffer mRectDrawBuffer;
    bool mDrawn = false;
    fl::LcdI80TimingConfig mTiming;

    LCDI80Esp32GroupBase(const fl::LcdI80TimingConfig& timing)
        : mTiming(timing) {}

    ~LCDI80Esp32GroupBase() { mDriver.reset(); }

    void onQueuingStart() {
        mRectDrawBuffer.onQueuingStart();
        mDrawn = false;
    }

    void onQueuingDone() {
        mRectDrawBuffer.onQueuingDone();
    }

    void addObject(uint8_t pin, uint16_t numLeds, bool is_rgbw) {
        mRectDrawBuffer.queue(fl::DrawItem(pin, numLeds, is_rgbw));
    }


    void showPixelsOnceThisFrame() {
        if (mDrawn) {
            return;
        }
        mDrawn = true;
        if (!mRectDrawBuffer.mAllLedsBufferUint8Size) {
            return;
        }
        bool drawlist_changed = mRectDrawBuffer.mDrawListChangedThisFrame;
        bool needs_validation = !mDriver.get() || drawlist_changed;
        if (needs_validation) {
            mDriver.reset();
            mDriver.reset(new fl::LcdI80DriverBase(mTiming));

            // Build pin list and config
            fl::LcdDriverConfig config;
            config.num_lanes = 0;
            for (auto it = mRectDrawBuffer.mDrawList.begin(); it != mRectDrawBuffer.mDrawList.end(); ++it) {
                // Check for invalid USB-JTAG pins (ESP32-S2/S3)
                if (it->mPin == 19 || it->mPin == 20) {
                    FASTLED_ASSERT(false, "GPIO19 and GPIO20 are reserved for USB-JTAG on ESP32-S2/S3 and CANNOT be used for LED output. "
                                          "Using these pins WILL BREAK USB flashing capability. Please choose a different pin.");
                    return; // Don't continue if assertion doesn't halt
                }

                // Check for Flash/PSRAM pins (GPIO26-32)
                if (it->mPin >= 26 && it->mPin <= 32) {
                    FASTLED_ASSERT(false, "GPIO26-32 are reserved for SPI Flash/PSRAM and CANNOT be used for LED output. "
                                          "Using these pins WILL BREAK flash/PSRAM functionality. Please choose a different pin.");
                    return;
                }

                // Error for strapping pins (GPIO0, 3, 45, 46) - can be suppressed with FASTLED_ESP32_ALLOW_STRAPPING_PINS
                if (it->mPin == 0 || it->mPin == 3 || it->mPin == 45 || it->mPin == 46) {
                    #ifndef FASTLED_ESP32_ALLOW_STRAPPING_PINS
                    FASTLED_ASSERT(false, "GPIO" << int(it->mPin) << " is a strapping pin used for boot configuration. "
                                          "Using this pin may affect boot behavior and requires careful external circuit design. "
                                          "Define FASTLED_ESP32_ALLOW_STRAPPING_PINS to suppress this error if you know what you're doing.");
                    return;
                    #else
                    FL_WARN("GPIO" << int(it->mPin) << " is a strapping pin used for boot configuration. "
                            "Using this pin may affect boot behavior and requires careful external circuit design. "
                            "(Warning shown because FASTLED_ESP32_ALLOW_STRAPPING_PINS is defined)");
                    #endif
                }

                #if defined(CONFIG_SPIRAM_MODE_OCT) || defined(CONFIG_ESPTOOLPY_FLASHMODE_OPI)
                // Check for Octal Flash/PSRAM pins (GPIO33-37)
                if (it->mPin >= 33 && it->mPin <= 37) {
                    FASTLED_ASSERT(false, "GPIO33-37 are reserved for Octal Flash/PSRAM (SPIIO4-7, SPIDQS) and CANNOT be used for LED output. "
                                          "Using these pins WILL BREAK Octal flash/PSRAM functionality. Please choose a different pin.");
                    return;
                }
                #endif

                config.gpio_pins[config.num_lanes++] = it->mPin;
            }

            uint32_t num_strips = 0;
            uint32_t bytes_per_strip = 0;
            uint32_t total_bytes = 0;
            mRectDrawBuffer.getBlockInfo(&num_strips, &bytes_per_strip, &total_bytes);
            int num_leds_per_strip = bytes_per_strip / 3;

            // Initialize PSRAM if not already done
            // Skip PSRAM initialization in QEMU environment (not emulated)
            #ifndef FASTLED_ESP32_IS_QEMU
            static bool gPsramInited = false;
            if (!gPsramInited) {
                gPsramInited = true;
                bool psram_ok = psramInit();
                if (!psram_ok) {
                    log_e("PSRAM initialization failed, LCD driver will use internal RAM.");
                }
            }
            #endif

            // Initialize the driver
            bool ok = mDriver->begin(config, num_leds_per_strip);
            if (!ok) {
                FASTLED_ASSERT(false, "Failed to initialize LCD driver");
                return;
            }

            // Attach strips to the driver
            CRGB* strips[16];
            for (int i = 0; i < config.num_lanes; i++) {
                // Get the LED buffer for this pin and convert to CRGB*
                fl::span<uint8_t> pin_buffer = mRectDrawBuffer.getLedsBufferBytesForPin(
                    config.gpio_pins[i], false);
                strips[i] = reinterpret_cast<CRGB*>(pin_buffer.data());
            }
            mDriver->attachStrips(strips);
        }
        mDriver->show();
    }
};

} // namespace fl

namespace fl {

/// Templated singleton wrapper - one instance per chipset type
/// Thin wrapper that converts compile-time TIMING to runtime config and delegates to base
/// This is now in fl namespace so it can be used from the header
template <typename TIMING>
class LCDI80Esp32Group {
public:
    static LCDI80Esp32Group& getInstance() {
        return fl::Singleton<LCDI80Esp32Group<TIMING>>::instance();
    }

    LCDI80Esp32Group()
        : mBase(fl::LcdI80TimingConfig{
            TIMING::T1,        // T1
            TIMING::T2,        // T2
            TIMING::T3,        // T3
            TIMING::RESET,     // reset_us
            0,                 // pclk_hz (calculated by driver)
            0,                 // slot_ns (calculated by driver)
            0,                 // actual_T1 (calculated by driver)
            0,                 // actual_T2 (calculated by driver)
            0,                 // actual_T3 (calculated by driver)
            0.0f,              // error_T1 (calculated by driver)
            0.0f,              // error_T2 (calculated by driver)
            0.0f,              // error_T3 (calculated by driver)
            TIMING::name()     // chipset_name
        })
    {
        // Auto-register with global registry on construction
        fl::LCDI80Esp32Registry::getInstance().registerGroup(
            this,
            [](void* ptr) {
                static_cast<LCDI80Esp32Group*>(ptr)->flush();
            }
        );
    }

    void onQueuingStart() { mBase.onQueuingStart(); }
    void onQueuingDone() { mBase.onQueuingDone(); }
    void addObject(uint8_t pin, uint16_t numLeds, bool is_rgbw) {
        mBase.addObject(pin, numLeds, is_rgbw);
    }
    void showPixelsOnceThisFrame() { mBase.showPixelsOnceThisFrame(); }
    void flush() { mBase.showPixelsOnceThisFrame(); }  // Alias for registry

    LCDI80Esp32GroupBase mBase;  // Make public for proxy access

private:
};

void LCD_I80_Esp32::beginShowLeds(int datapin, int nleds) {
    // Temporary: Use WS2812 timing for backward compatibility
    // This will be replaced by template-based routing in Phase 3
    LCDI80Esp32Group<TIMING_WS2812_800KHZ> &group = LCDI80Esp32Group<TIMING_WS2812_800KHZ>::getInstance();
    group.onQueuingStart();
    group.addObject(datapin, nleds, false);
}

void LCD_I80_Esp32::showPixels(uint8_t data_pin, PixelIterator& pixel_iterator) {
    // Temporary: Use WS2812 timing for backward compatibility
    LCDI80Esp32Group<TIMING_WS2812_800KHZ> &group = LCDI80Esp32Group<TIMING_WS2812_800KHZ>::getInstance();
    group.onQueuingDone();
    const Rgbw rgbw = pixel_iterator.get_rgbw();

    fl::span<uint8_t> strip_bytes = group.mBase.mRectDrawBuffer.getLedsBufferBytesForPin(data_pin, true);
    if (rgbw.active()) {
        uint8_t r, g, b, w;
        while (pixel_iterator.has(1)) {
            pixel_iterator.loadAndScaleRGBW(&r, &g, &b, &w);
            strip_bytes[0] = r;
            strip_bytes[1] = g;
            strip_bytes[2] = b;
            strip_bytes[3] = w;
            strip_bytes.pop_front();
            strip_bytes.pop_front();
            strip_bytes.pop_front();
            strip_bytes.pop_front();
            pixel_iterator.advanceData();
            pixel_iterator.stepDithering();
        }
    } else {
        uint8_t r, g, b;
        while (pixel_iterator.has(1)) {
            pixel_iterator.loadAndScaleRGB(&r, &g, &b);
            strip_bytes[0] = r;
            strip_bytes[1] = g;
            strip_bytes[2] = b;
            strip_bytes.pop_front();
            strip_bytes.pop_front();
            strip_bytes.pop_front();
            pixel_iterator.advanceData();
            pixel_iterator.stepDithering();
        }
    }
}

void LCD_I80_Esp32::endShowLeds() {
    // First one to call this draws everything, every other call this frame
    // is ignored.
    // Temporary: Use WS2812 timing for backward compatibility
    LCDI80Esp32Group<TIMING_WS2812_800KHZ>::getInstance().showPixelsOnceThisFrame();
}

// ============================================================================
// LcdI80DriverBase implementation (concrete, non-template)
// ============================================================================

void LcdI80DriverBase::generateTemplates() {
    // 3-word encoding:
    // Bit-0: [HIGH, LOW, LOW]   (1 slot HIGH, 2 slots LOW)
    // Bit-1: [HIGH, HIGH, LOW]  (2 slots HIGH, 1 slot LOW)

    // All lanes transmit bit 0
    template_bit0_[0] = 0xFFFF;  // Slot 0: HIGH
    template_bit0_[1] = 0x0000;  // Slot 1: LOW
    template_bit0_[2] = 0x0000;  // Slot 2: LOW

    // All lanes transmit bit 1
    template_bit1_[0] = 0xFFFF;  // Slot 0: HIGH
    template_bit1_[1] = 0xFFFF;  // Slot 1: HIGH
    template_bit1_[2] = 0x0000;  // Slot 2: LOW
}

bool LcdI80DriverBase::begin(const LcdDriverConfig& config, int leds_per_strip) {
    config_ = config;
    num_leds_ = leds_per_strip;

    // Use chipset default reset time if not specified
    if (config_.latch_us == 0) {
        config_.latch_us = timing_.reset_us;
    }

    // Validate configuration
    if (config_.num_lanes < 1 || config_.num_lanes > 16) {
        ESP_LOGE(LCD_TAG, "Invalid num_lanes: %d (must be 1-16)", config_.num_lanes);
        return false;
    }

    if (num_leds_ < 1) {
        ESP_LOGE(LCD_TAG, "Invalid leds_per_strip: %d", num_leds_);
        return false;
    }

    // Validate GPIO pins
    for (int i = 0; i < config_.num_lanes; i++) {
        int pin = config_.gpio_pins[i];

        // Check for invalid USB-JTAG pins
        if (pin == 19 || pin == 20) {
            ESP_LOGE(LCD_TAG, "GPIO%d is reserved for USB-JTAG and cannot be used for LED output", pin);
            return false;
        }

        // Check for Flash/PSRAM pins
        if (pin >= 26 && pin <= 32) {
            ESP_LOGE(LCD_TAG, "GPIO%d is reserved for SPI Flash/PSRAM and cannot be used for LED output", pin);
            return false;
        }

        // Warning for strapping pins
        if (pin == 0 || pin == 3 || pin == 45 || pin == 46) {
            ESP_LOGW(LCD_TAG, "GPIO%d is a strapping pin - use with caution, may affect boot behavior", pin);
        }

        #if defined(CONFIG_SPIRAM_MODE_OCT) || defined(CONFIG_ESPTOOLPY_FLASHMODE_OPI)
        // Check for Octal Flash/PSRAM pins
        if (pin >= 33 && pin <= 37) {
            ESP_LOGE(LCD_TAG, "GPIO%d is reserved for Octal Flash/PSRAM and cannot be used for LED output", pin);
            return false;
        }
        #endif
    }

    // Generate bit templates
    generateTemplates();

    // Log timing information
    ESP_LOGI(LCD_TAG, "Chipset: %s", timing_.chipset_name);
    ESP_LOGI(LCD_TAG, "Target timing: T1=%u ns, T2=%u ns, T3=%u ns",
             timing_.T1, timing_.T2, timing_.T3);
    ESP_LOGI(LCD_TAG, "Optimized PCLK: %u Hz (%u MHz)", timing_.pclk_hz, timing_.pclk_hz / 1000000);
    ESP_LOGI(LCD_TAG, "Slot duration: %u ns", timing_.slot_ns);
    ESP_LOGI(LCD_TAG, "Slots per bit: %u", N_BIT);

    ESP_LOGI(LCD_TAG, "Actual timing: T1=%u ns, T1+T2=%u ns, T3=%u ns",
             timing_.actual_T1, timing_.actual_T1 + timing_.actual_T2, timing_.actual_T3);

    // Calculate buffer size
    // Data: num_leds * 24 bits * N_BIT words * 2 bytes
    // Latch: latch_us converted to slots, then to bytes
    size_t data_size = num_leds_ * 24 * N_BIT * 2;
    size_t latch_slots = (config_.latch_us * 1000) / timing_.slot_ns;
    size_t latch_size = latch_slots * 2;
    buffer_size_ = data_size + latch_size;

    ESP_LOGI(LCD_TAG, "Buffer size: %u bytes (%u KB)", buffer_size_, buffer_size_ / 1024);
    ESP_LOGI(LCD_TAG, "Frame time (estimated): %u us", getFrameTimeUs());

    // Allocate double buffers
    // LCD peripheral requires DMA-capable memory
    // - Internal DMA RAM: Always works but limited size (~160KB on ESP32-S3)
    // - PSRAM: Requires EDMA (cache-DMA) support, available on ESP32-S3 with proper config
    uint32_t alloc_caps;
    if (config_.use_psram) {
        // Try PSRAM with DMA capability (EDMA on ESP32-S3)
        // MALLOC_CAP_SPIRAM alone isn't DMA-capable, need DMA flag too
        alloc_caps = MALLOC_CAP_SPIRAM | MALLOC_CAP_DMA | MALLOC_CAP_8BIT;
    } else {
        // Internal DMA RAM
        alloc_caps = MALLOC_CAP_DMA | MALLOC_CAP_8BIT;
    }

    for (int i = 0; i < 2; i++) {
        buffers_[i] = (uint16_t*)heap_caps_aligned_alloc(
            LCD_DRIVER_PSRAM_DATA_ALIGNMENT,
            buffer_size_,
            alloc_caps
        );

        // Fallback: If PSRAM+DMA allocation failed, try internal DMA RAM only
        if (buffers_[i] == nullptr && config_.use_psram) {
            ESP_LOGW(LCD_TAG, "PSRAM+DMA allocation failed for buffer %d, falling back to internal DMA RAM", i);
            buffers_[i] = (uint16_t*)heap_caps_aligned_alloc(
                LCD_DRIVER_PSRAM_DATA_ALIGNMENT,
                buffer_size_,
                MALLOC_CAP_DMA | MALLOC_CAP_8BIT
            );
        }

        if (buffers_[i] == nullptr) {
            ESP_LOGE(LCD_TAG, "Failed to allocate buffer %d (%u bytes)", i, buffer_size_);
            ESP_LOGE(LCD_TAG, "Free DMA heap: %u bytes, largest block: %u bytes",
                     heap_caps_get_free_size(MALLOC_CAP_DMA),
                     heap_caps_get_largest_free_block(MALLOC_CAP_DMA));
            end();
            return false;
        }

        // Initialize buffer with zeros (latch gap pre-filled)
        fl::memset(buffers_[i], 0, buffer_size_);
    }

    ESP_LOGI(LCD_TAG, "Allocated 2 buffers at %p, %p", buffers_[0], buffers_[1]);

    // Create semaphore for DMA synchronization
    xfer_done_sem_ = xSemaphoreCreateBinary();
    if (xfer_done_sem_ == nullptr) {
        ESP_LOGE(LCD_TAG, "Failed to create semaphore");
        end();
        return false;
    }
    xSemaphoreGive(xfer_done_sem_);  // Start in "transfer done" state

    // Initialize I80 bus
    esp_lcd_i80_bus_config_t bus_config = {};
    bus_config.clk_src = LCD_CLK_SRC_PLL160M;
    bus_config.dc_gpio_num = -1;  // Not used
    bus_config.wr_gpio_num = -1;  // Not used - WS2812 doesn't need external WR signal
    bus_config.bus_width = 16;
    bus_config.max_transfer_bytes = buffer_size_;

    for (int i = 0; i < 16; i++) {
        if (i < config_.num_lanes) {
            bus_config.data_gpio_nums[i] = config_.gpio_pins[i];
        } else {
            bus_config.data_gpio_nums[i] = -1;  // Unused lanes
        }
    }

    // IDF 5.0-5.2 use alignment fields; IDF 5.3+ use dma_burst_size
    #if ESP_IDF_VERSION < ESP_IDF_VERSION_VAL(5, 3, 0)
    bus_config.psram_trans_align = LCD_DRIVER_PSRAM_DATA_ALIGNMENT;
    bus_config.sram_trans_align = 4;
    #else
    // IDF 5.3+ uses new dma_burst_size field (replaces both psram/sram alignment)
    bus_config.dma_burst_size = 64;
    #endif

    esp_err_t bus_err = esp_lcd_new_i80_bus(&bus_config, &bus_handle_);
    if (bus_err != ESP_OK) {
        ESP_LOGE(LCD_TAG, "Failed to create I80 bus: %d", bus_err);
        end();
        return false;
    }

    // Initialize panel IO
    esp_lcd_panel_io_i80_config_t io_config = {};
    io_config.cs_gpio_num = -1;  // Not used
    io_config.pclk_hz = timing_.pclk_hz;
    io_config.trans_queue_depth = 1;  // Critical: single transfer to avoid gaps
    io_config.dc_levels = {
        .dc_idle_level = 0,
        .dc_cmd_level = 0,
        .dc_dummy_level = 0,
        .dc_data_level = 1,
    };
    io_config.lcd_cmd_bits = 0;
    io_config.lcd_param_bits = 0;
    io_config.on_color_trans_done = dmaCallback;
    io_config.user_ctx = this;

    esp_err_t panel_err = esp_lcd_new_panel_io_i80(bus_handle_, &io_config, &io_handle_);
    if (panel_err != ESP_OK) {
        ESP_LOGE(LCD_TAG, "Failed to create panel IO: %d", panel_err);
        end();
        return false;
    }

    ESP_LOGI(LCD_TAG, "LCD driver initialized successfully");
    return true;
}

void LcdI80DriverBase::end() {
    // Wait for any pending transfer
    if (dma_busy_) {
        wait();
    }

    // Release ESP-LCD resources
    if (io_handle_ != nullptr) {
        esp_lcd_panel_io_del(io_handle_);
        io_handle_ = nullptr;
    }

    if (bus_handle_ != nullptr) {
        esp_lcd_del_i80_bus(bus_handle_);
        bus_handle_ = nullptr;
    }

    // Free buffers
    for (int i = 0; i < 2; i++) {
        if (buffers_[i] != nullptr) {
            heap_caps_free(buffers_[i]);
            buffers_[i] = nullptr;
        }
    }

    // Delete semaphore
    if (xfer_done_sem_ != nullptr) {
        vSemaphoreDelete(xfer_done_sem_);
        xfer_done_sem_ = nullptr;
    }
}

void LcdI80DriverBase::encodeFrame(int buffer_index) {
    uint16_t* output = buffers_[buffer_index];
    uint8_t pixel_bytes[16];  // One byte per lane for current color component
    uint16_t lane_bits[8];    // Transposed bits (8 words for 8 bit positions)

    // Encode all LEDs
    for (int led_idx = 0; led_idx < num_leds_; led_idx++) {
        // Process color components in GRB order (WS28xx standard)
        const int color_order[3] = {1, 0, 2};  // G, R, B indices into CRGB

        for (int color = 0; color < 3; color++) {
            int component = color_order[color];

            // Gather bytes for this color component across all lanes
            for (int lane = 0; lane < config_.num_lanes; lane++) {
                if (strips_[lane] != nullptr) {
                    pixel_bytes[lane] = strips_[lane][led_idx].raw[component];
                } else {
                    pixel_bytes[lane] = 0;
                }
            }

            // Fill unused lanes with zeros
            for (int lane = config_.num_lanes; lane < 16; lane++) {
                pixel_bytes[lane] = 0;
            }

            // Transpose 16 bytes into 8 words (one bit per lane)
            LcdDriverBase::transpose16x1(pixel_bytes, lane_bits);

            // Encode each bit (MSB first: bit 7 down to bit 0)
            for (int bit_idx = 7; bit_idx >= 0; bit_idx--) {
                uint16_t current_bit_mask = lane_bits[bit_idx];

                // Apply templates with bit masking
                // For each slot, select bit0 or bit1 template based on lane bits
                for (uint32_t slot = 0; slot < N_BIT; slot++) {
                    output[slot] = (template_bit0_[slot] & ~current_bit_mask) |
                                   (template_bit1_[slot] & current_bit_mask);
                }

                output += N_BIT;  // Advance to next bit
            }
        }
    }

    // Latch gap already pre-filled with zeros during buffer allocation
}

bool LcdI80DriverBase::show() {
    // Check if previous transfer is still running
    if (dma_busy_) {
        return false;  // Busy, cannot start new transfer
    }

    // Take semaphore (should be available immediately if not busy)
    if (xSemaphoreTake(xfer_done_sem_, 0) != pdTRUE) {
        return false;  // Race condition, still busy
    }

    // Encode frame into back buffer
    int back_buffer = 1 - front_buffer_;
    encodeFrame(back_buffer);

    // Mark as busy before starting transfer
    dma_busy_ = true;

    // Start DMA transfer
    // Note: tx_color expects (panel_io, command, color_data, color_size)
    // We use command=-1 for no command and pass entire buffer as color data
    esp_err_t err = esp_lcd_panel_io_tx_color(
        io_handle_,
        -1,  // No command byte
        buffers_[back_buffer],
        buffer_size_
    );

    if (err != ESP_OK) {
        ESP_LOGE(LCD_TAG, "DMA transfer failed: %d", err);
        dma_busy_ = false;
        xSemaphoreGive(xfer_done_sem_);  // Release semaphore
        return false;
    }

    // Swap buffers
    front_buffer_ = back_buffer;
    frame_counter_++;

    return true;
}

// IRAM_ATTR callback - safe because it's in the .cpp file (always compiled)
bool IRAM_ATTR LcdI80DriverBase::dmaCallback(esp_lcd_panel_io_handle_t panel_io,
                                              esp_lcd_panel_io_event_data_t* edata,
                                              void* user_ctx) {
    LcdI80DriverBase* driver = static_cast<LcdI80DriverBase*>(user_ctx);

    // Mark transfer as complete
    driver->dma_busy_ = false;

    // Signal semaphore
    BaseType_t higher_priority_task_woken = pdFALSE;
    xSemaphoreGiveFromISR(driver->xfer_done_sem_, &higher_priority_task_woken);

    return higher_priority_task_woken == pdTRUE;
}

} // namespace fl

#endif  // __has_include("hal/lcd_hal.h") && __has_include("soc/lcd_periph.h")
#endif  // CONFIG_IDF_TARGET_ESP32S3 || CONFIG_IDF_TARGET_ESP32P4
#endif  // ESP32
