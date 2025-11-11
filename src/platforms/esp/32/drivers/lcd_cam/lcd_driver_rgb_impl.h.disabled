/// @file lcd_driver_rgb_impl.h
/// @brief Implementation of ESP32 RGB LCD parallel LED driver template methods
///
/// NOTE: This file should only be included from lcd_driver_rgb.h
///       Do not include this file directly in other translation units.

#pragma once

#include "esp_log.h"
#include "esp_lcd_panel_rgb.h"
#include "platforms/assert_defs.h"
#include "fl/cstring.h"

#define LCD_P4_TAG "FastLED_LCD_P4"

namespace fl {

template <typename LED_CHIPSET>
void LcdRgbDriver<LED_CHIPSET>::generateTemplates() {
    // 4-pixel encoding for WS2812 at 3.2 MHz PCLK (312.5ns per pixel):
    // Bit-0: [HI, LO, LO, LO]   (312ns high, 938ns low)
    // Bit-1: [HI, HI, LO, LO]   (625ns high, 625ns low)

    // All lanes transmit bit 0
    template_bit0_[0] = 0xFFFF;  // Pixel 0: HIGH
    template_bit0_[1] = 0x0000;  // Pixel 1: LOW
    template_bit0_[2] = 0x0000;  // Pixel 2: LOW
    template_bit0_[3] = 0x0000;  // Pixel 3: LOW

    // All lanes transmit bit 1
    template_bit1_[0] = 0xFFFF;  // Pixel 0: HIGH
    template_bit1_[1] = 0xFFFF;  // Pixel 1: HIGH
    template_bit1_[2] = 0x0000;  // Pixel 2: LOW
    template_bit1_[3] = 0x0000;  // Pixel 3: LOW
}

template <typename LED_CHIPSET>
bool LcdRgbDriver<LED_CHIPSET>::begin(const LcdRgbDriverConfig& config, int leds_per_strip) {
    config_ = config;
    num_leds_ = leds_per_strip;

    // Use chipset default reset time if not specified
    if (config_.latch_us == 0) {
        config_.latch_us = LED_CHIPSET::RESET();
    }

    // Validate configuration
    if (config_.num_lanes < 1 || config_.num_lanes > 16) {
        ESP_LOGE(LCD_P4_TAG, "Invalid num_lanes: %d (must be 1-16)", config_.num_lanes);
        return false;
    }

    if (num_leds_ < 1) {
        ESP_LOGE(LCD_P4_TAG, "Invalid leds_per_strip: %d", num_leds_);
        return false;
    }

    // Validate GPIO pins using P4-specific validation
    for (int i = 0; i < config_.num_lanes; i++) {
        int pin = config_.data_gpios[i];
        auto result = validate_esp32p4_lcd_pin(pin);
        if (!result.valid) {
            ESP_LOGE(LCD_P4_TAG, "GPIO%d validation failed: %s", pin, result.error_message);
            return false;
        }
    }

    // Generate bit templates
    generateTemplates();

    // Log timing information
    ESP_LOGI(LCD_P4_TAG, "Chipset: %s", LED_CHIPSET::name());
    ESP_LOGI(LCD_P4_TAG, "Target timing: T1=%u ns, T2=%u ns, T3=%u ns",
             LED_CHIPSET::T1(), LED_CHIPSET::T2(), LED_CHIPSET::T3());
    ESP_LOGI(LCD_P4_TAG, "Optimized PCLK: %u Hz (%u MHz)", PCLK_HZ, PCLK_HZ / 1000000);
    ESP_LOGI(LCD_P4_TAG, "Pixel duration: %u ns", PIXEL_NS);
    ESP_LOGI(LCD_P4_TAG, "Pixels per bit: %u", N_PIXELS);

    uint32_t T1_act, T2_act, T3_act;
    getActualTiming(T1_act, T2_act, T3_act);
    ESP_LOGI(LCD_P4_TAG, "Actual timing: T1=%u ns, T1+T2=%u ns, T3=%u ns",
             T1_act, T1_act + T2_act, T3_act);

    // Calculate buffer size
    // Data: num_leds * 24 bits * N_PIXELS * 2 bytes per pixel
    // Latch: latch_us converted to pixels, then to bytes
    size_t data_size = num_leds_ * 24 * N_PIXELS * 2;
    // Use ceiling division to ensure reset time meets or exceeds target
    size_t latch_pixels = ((config_.latch_us * 1000) + PIXEL_NS - 1) / PIXEL_NS;
    size_t latch_size = latch_pixels * 2;
    buffer_size_ = data_size + latch_size;

    ESP_LOGI(LCD_P4_TAG, "Buffer size: %u bytes (%u KB)", buffer_size_, buffer_size_ / 1024);
    ESP_LOGI(LCD_P4_TAG, "Frame time (estimated): %u us", getFrameTimeUs());

    // Allocate double buffers
    uint32_t alloc_caps;
    if (config_.use_psram) {
        // Try PSRAM with DMA capability
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
            ESP_LOGW(LCD_P4_TAG, "PSRAM+DMA allocation failed for buffer %d, falling back to internal DMA RAM", i);
            buffers_[i] = (uint16_t*)heap_caps_aligned_alloc(
                LCD_DRIVER_PSRAM_DATA_ALIGNMENT,
                buffer_size_,
                MALLOC_CAP_DMA | MALLOC_CAP_8BIT
            );
        }

        if (buffers_[i] == nullptr) {
            ESP_LOGE(LCD_P4_TAG, "Failed to allocate buffer %d (%u bytes)", i, buffer_size_);
            ESP_LOGE(LCD_P4_TAG, "Free DMA heap: %u bytes, largest block: %u bytes",
                     heap_caps_get_free_size(MALLOC_CAP_DMA),
                     heap_caps_get_largest_free_block(MALLOC_CAP_DMA));
            end();
            return false;
        }

        // Initialize buffer with zeros (latch gap pre-filled)
        fl::memset(buffers_[i], 0, buffer_size_);
    }

    ESP_LOGI(LCD_P4_TAG, "Allocated 2 buffers at %p, %p", buffers_[0], buffers_[1]);

    // Create semaphore for DMA synchronization
    xfer_done_sem_ = xSemaphoreCreateBinary();
    if (xfer_done_sem_ == nullptr) {
        ESP_LOGE(LCD_P4_TAG, "Failed to create semaphore");
        end();
        return false;
    }
    xSemaphoreGive(xfer_done_sem_);  // Start in "transfer done" state

    // Calculate horizontal resolution and VSYNC front porch for reset gap
    // h_res = number of pixels per line = bits per strip
    uint32_t h_res = num_leds_ * 24 * N_PIXELS;

    // Calculate VSYNC front porch to create reset gap
    // Need at least 50us idle time for WS2812 latch
    uint32_t reset_pixels = latch_pixels;

    // Initialize RGB LCD panel
    esp_lcd_rgb_panel_config_t panel_config = {};
    panel_config.clk_src = LCD_CLK_SRC_DEFAULT;
    panel_config.data_width = 16;  // Use 16 data lines
    panel_config.bits_per_pixel = 16;  // 16 bits per pixel output
    // IDF 5.4 and earlier use deprecated psram_trans_align
    #if ESP_IDF_VERSION < ESP_IDF_VERSION_VAL(5, 3, 0)
    #pragma GCC diagnostic push
    #pragma GCC diagnostic ignored "-Wdeprecated-declarations"
    panel_config.psram_trans_align = LCD_DRIVER_PSRAM_DATA_ALIGNMENT;
    #else
    // IDF 5.3+ uses new dma_burst_size field
    panel_config.dma_burst_size = 64;
    #endif
    panel_config.num_fbs = 0;  // We manage our own frame buffers

    // Timing parameters
    panel_config.timings.pclk_hz = PCLK_HZ;
    panel_config.timings.h_res = h_res;
    panel_config.timings.v_res = 1;  // Single line per frame
    panel_config.timings.hsync_pulse_width = 1;
    panel_config.timings.hsync_back_porch = 0;
    panel_config.timings.hsync_front_porch = 0;
    panel_config.timings.vsync_pulse_width = 1;
    panel_config.timings.vsync_back_porch = 1;
    panel_config.timings.vsync_front_porch = reset_pixels;  // Reset gap via VSYNC

    // GPIO configuration
    panel_config.pclk_gpio_num = config_.pclk_gpio;
    panel_config.hsync_gpio_num = config_.hsync_gpio;
    panel_config.vsync_gpio_num = config_.vsync_gpio;
    panel_config.de_gpio_num = config_.de_gpio;
    panel_config.disp_gpio_num = config_.disp_gpio;

    // Data GPIO pins
    for (int i = 0; i < 16; i++) {
        if (i < config_.num_lanes) {
            panel_config.data_gpio_nums[i] = config_.data_gpios[i];
        } else {
            panel_config.data_gpio_nums[i] = -1;  // Unused lanes
        }
    }

    // Flags
    panel_config.flags.fb_in_psram = config_.use_psram ? 1 : 0;
    panel_config.flags.refresh_on_demand = 1;  // Manual refresh control

    // Create RGB panel
    esp_err_t err = esp_lcd_new_rgb_panel(&panel_config, &panel_handle_);
    if (err != ESP_OK) {
        ESP_LOGE(LCD_P4_TAG, "Failed to create RGB panel: %d (%s)", err, esp_err_to_name(err));
        end();
        return false;
    }

    // Initialize panel
    err = esp_lcd_panel_init(panel_handle_);
    if (err != ESP_OK) {
        ESP_LOGE(LCD_P4_TAG, "Failed to initialize panel: %d (%s)", err, esp_err_to_name(err));
        end();
        return false;
    }

    // Register DMA callback for proper synchronization
    esp_lcd_rgb_panel_event_callbacks_t cbs = {};
    cbs.on_vsync = drawCallback;
    err = esp_lcd_rgb_panel_register_event_callbacks(panel_handle_, &cbs, this);
    if (err != ESP_OK) {
        ESP_LOGE(LCD_P4_TAG, "Failed to register event callbacks: %d (%s)", err, esp_err_to_name(err));
        end();
        return false;
    }

    ESP_LOGI(LCD_P4_TAG, "RGB LCD driver initialized successfully");
    return true;
}

template <typename LED_CHIPSET>
void LcdRgbDriver<LED_CHIPSET>::end() {
    // Wait for any pending transfer
    if (dma_busy_) {
        wait();
    }

    // Release RGB panel
    if (panel_handle_ != nullptr) {
        esp_lcd_panel_del(panel_handle_);
        panel_handle_ = nullptr;
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

template <typename LED_CHIPSET>
void LcdRgbDriver<LED_CHIPSET>::encodeFrame(int buffer_index) {
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
                // For each pixel, select bit0 or bit1 template based on lane bits
                for (uint32_t pixel = 0; pixel < N_PIXELS; pixel++) {
                    output[pixel] = (template_bit0_[pixel] & ~current_bit_mask) |
                                    (template_bit1_[pixel] & current_bit_mask);
                }

                output += N_PIXELS;  // Advance to next bit
            }
        }
    }

    // Latch gap already pre-filled with zeros during buffer allocation
}

template <typename LED_CHIPSET>
bool LcdRgbDriver<LED_CHIPSET>::show() {
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

    // Start DMA transfer using RGB panel draw_bitmap
    esp_err_t err = esp_lcd_panel_draw_bitmap(
        panel_handle_,
        0, 0,  // x, y offset
        buffer_size_ / 2, 1,  // width (in pixels), height
        buffers_[back_buffer]
    );

    if (err != ESP_OK) {
        ESP_LOGE(LCD_P4_TAG, "DMA transfer failed: %d (%s)", err, esp_err_to_name(err));
        dma_busy_ = false;
        xSemaphoreGive(xfer_done_sem_);  // Release semaphore
        return false;
    }

    // Swap buffers
    front_buffer_ = back_buffer;
    frame_counter_++;

    // DMA callback (drawCallback) will mark transfer as complete and release semaphore
    return true;
}

template <typename LED_CHIPSET>
bool IRAM_ATTR LcdRgbDriver<LED_CHIPSET>::drawCallback(esp_lcd_panel_handle_t panel,
                                                       void* edata,
                                                       void* user_ctx) {
    LcdRgbDriver<LED_CHIPSET>* driver = static_cast<LcdRgbDriver<LED_CHIPSET>*>(user_ctx);

    // Mark transfer as complete
    driver->dma_busy_ = false;

    // Signal semaphore
    BaseType_t higher_priority_task_woken = pdFALSE;
    xSemaphoreGiveFromISR(driver->xfer_done_sem_, &higher_priority_task_woken);

    return higher_priority_task_woken == pdTRUE;
}

}  // namespace fl
