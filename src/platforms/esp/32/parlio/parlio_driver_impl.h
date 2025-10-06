/// @file parlio_driver_impl.h
/// @brief ESP32-P4 PARLIO LED driver template implementation

#pragma once

#include "parlio_driver.h"
#include "esp_err.h"

namespace {
    /// @brief Convert ESP-IDF error code to human-readable string
    const char* esp_err_to_name_safe(esp_err_t err) {
        switch (err) {
            case ESP_OK: return "ESP_OK (Success)";
            case ESP_FAIL: return "ESP_FAIL (Generic failure)";
            case ESP_ERR_NO_MEM: return "ESP_ERR_NO_MEM (Out of memory)";
            case ESP_ERR_INVALID_ARG: return "ESP_ERR_INVALID_ARG (Invalid argument)";
            case ESP_ERR_INVALID_STATE: return "ESP_ERR_INVALID_STATE (Invalid state)";
            case ESP_ERR_INVALID_SIZE: return "ESP_ERR_INVALID_SIZE (Invalid size)";
            case ESP_ERR_NOT_FOUND: return "ESP_ERR_NOT_FOUND (Resource not found / All units exhausted)";
            case ESP_ERR_NOT_SUPPORTED: return "ESP_ERR_NOT_SUPPORTED (Feature not supported by hardware)";
            case ESP_ERR_TIMEOUT: return "ESP_ERR_TIMEOUT (Operation timeout)";
            case ESP_ERR_INVALID_RESPONSE: return "ESP_ERR_INVALID_RESPONSE (Invalid response)";
            case ESP_ERR_INVALID_CRC: return "ESP_ERR_INVALID_CRC (CRC error)";
            case ESP_ERR_INVALID_VERSION: return "ESP_ERR_INVALID_VERSION (Version mismatch)";
            case ESP_ERR_INVALID_MAC: return "ESP_ERR_INVALID_MAC (Invalid MAC address)";
            default: return "UNKNOWN_ERROR";
        }
    }
}  // anonymous namespace

template <uint8_t DATA_WIDTH, typename CHIPSET>
ParlioLedDriver<DATA_WIDTH, CHIPSET>::ParlioLedDriver()
    : config_{}
    , num_leds_(0)
    , strips_{}
    , tx_unit_(nullptr)
    , dma_buffer_(nullptr)
    , dma_sub_buffers_{}
    , buffer_size_(0)
    , sub_buffer_size_(0)
    , xfer_done_sem_(nullptr)
    , dma_busy_(false)
{
    for (int i = 0; i < 16; i++) {
        strips_[i] = nullptr;
    }
    for (int i = 0; i < 3; i++) {
        dma_sub_buffers_[i] = nullptr;
    }
}

template <uint8_t DATA_WIDTH, typename CHIPSET>
ParlioLedDriver<DATA_WIDTH, CHIPSET>::~ParlioLedDriver() {
    end();
}

template <uint8_t DATA_WIDTH, typename CHIPSET>
bool ParlioLedDriver<DATA_WIDTH, CHIPSET>::begin(const ParlioDriverConfig& config, uint16_t num_leds) {
    FASTLED_DBG("========================================");
    FASTLED_DBG("PARLIO DRIVER INITIALIZATION STARTING");
    FASTLED_DBG("========================================");
    FASTLED_DBG("Configuration:");
    FASTLED_DBG("  DATA_WIDTH (parallel lanes): " << (int)DATA_WIDTH);
    FASTLED_DBG("  num_leds per strip: " << num_leds);
    FASTLED_DBG("  num_lanes requested: " << config.num_lanes);
    FASTLED_DBG("  clock_freq_hz: " << config.clock_freq_hz);
    FASTLED_DBG("  buffer_strategy: " << (int)config.buffer_strategy
                << (config.buffer_strategy == ParlioBufferStrategy::BREAK_PER_COLOR ? " (BREAK_PER_COLOR)" : " (MONOLITHIC)"));
    FASTLED_DBG("GPIO Configuration:");
    FASTLED_DBG("  clk_gpio (clock output): " << config.clk_gpio);
    for (int i = 0; i < config.num_lanes; i++) {
        FASTLED_DBG("  data_gpio[" << i << "]: " << config.data_gpios[i]);
    }
    FASTLED_DBG("========================================");

    // Validate configuration
    if (config.num_lanes != DATA_WIDTH) {
        FASTLED_DBG("*** FATAL ERROR: Lane count mismatch! ***");
        FASTLED_DBG("  Expected DATA_WIDTH: " << (int)DATA_WIDTH);
        FASTLED_DBG("  Got num_lanes: " << config.num_lanes);
        FASTLED_DBG("  This is a driver bug - please report to FastLED developers");
        return false;
    }

    config_ = config;
    num_leds_ = num_leds;

    // Set default clock frequency if not specified
    if (config_.clock_freq_hz == 0) {
        config_.clock_freq_hz = DEFAULT_CLOCK_FREQ_HZ;
    }

    // Calculate buffer size: num_leds * 24 bits * DATA_WIDTH bytes
    // Each LED has 24 bits (GRB), and each bit position requires DATA_WIDTH bytes
    buffer_size_ = num_leds * 24 * DATA_WIDTH;

    // Check available memory before allocation
    FASTLED_DBG("Memory Status Before Allocation:");
    size_t free_dma = heap_caps_get_free_size(MALLOC_CAP_DMA);
    size_t largest_dma_block = heap_caps_get_largest_free_block(MALLOC_CAP_DMA);
    size_t free_internal = heap_caps_get_free_size(MALLOC_CAP_INTERNAL);
    size_t free_spiram = heap_caps_get_free_size(MALLOC_CAP_SPIRAM);

    FASTLED_DBG("  DMA-capable memory free: " << free_dma << " bytes");
    FASTLED_DBG("  Largest DMA block: " << largest_dma_block << " bytes");
    FASTLED_DBG("  Internal RAM free: " << free_internal << " bytes");
    FASTLED_DBG("  SPIRAM free: " << free_spiram << " bytes");

    // Allocate DMA buffers based on strategy
    FASTLED_DBG("Allocating DMA buffers:");
    if (config_.buffer_strategy == ParlioBufferStrategy::BREAK_PER_COLOR) {
        // Allocate 3 sub-buffers (one for each color component: G, R, B)
        // Each sub-buffer holds 8 bits * DATA_WIDTH bytes per LED
        sub_buffer_size_ = num_leds * 8 * DATA_WIDTH;
        size_t total_needed = sub_buffer_size_ * 3;

        FASTLED_DBG("  Strategy: BREAK_PER_COLOR (3 sub-buffers)");
        FASTLED_DBG("  Sub-buffer size: " << sub_buffer_size_ << " bytes");
        FASTLED_DBG("  Total memory needed: " << total_needed << " bytes");

        if (total_needed > free_dma) {
            FASTLED_DBG("*** FATAL ERROR: Insufficient DMA memory! ***");
            FASTLED_DBG("  Required: " << total_needed << " bytes");
            FASTLED_DBG("  Available: " << free_dma << " bytes");
            FASTLED_DBG("  Shortfall: " << (total_needed - free_dma) << " bytes");
            FASTLED_DBG("Suggestion: Reduce NUM_LEDS or use fewer strips");
            return false;
        }

        for (int i = 0; i < 3; i++) {
            FASTLED_DBG("  Allocating sub-buffer " << i << " (" << sub_buffer_size_ << " bytes)...");
            dma_sub_buffers_[i] = (uint8_t*)heap_caps_malloc(sub_buffer_size_, MALLOC_CAP_DMA);
            if (!dma_sub_buffers_[i]) {
                FASTLED_DBG("*** FATAL ERROR: DMA sub-buffer allocation failed! ***");
                FASTLED_DBG("  Buffer index: " << i);
                FASTLED_DBG("  Requested size: " << sub_buffer_size_ << " bytes");
                FASTLED_DBG("  Free DMA memory: " << heap_caps_get_free_size(MALLOC_CAP_DMA) << " bytes");
                FASTLED_DBG("  Largest free block: " << heap_caps_get_largest_free_block(MALLOC_CAP_DMA) << " bytes");
                FASTLED_DBG("Possible cause: Memory fragmentation");

                // Clean up previously allocated buffers
                for (int j = 0; j < i; j++) {
                    heap_caps_free(dma_sub_buffers_[j]);
                    dma_sub_buffers_[j] = nullptr;
                }
                return false;
            }
            memset(dma_sub_buffers_[i], 0, sub_buffer_size_);
            FASTLED_DBG("    Success! (" << (3-i-1) << " more to go)");
        }
        FASTLED_DBG("  All 3 sub-buffers allocated successfully!");
    } else {
        // Monolithic buffer (original implementation)
        FASTLED_DBG("  Strategy: MONOLITHIC (single buffer)");
        FASTLED_DBG("  Buffer size needed: " << buffer_size_ << " bytes");

        if (buffer_size_ > free_dma) {
            FASTLED_DBG("*** FATAL ERROR: Insufficient DMA memory! ***");
            FASTLED_DBG("  Required: " << buffer_size_ << " bytes");
            FASTLED_DBG("  Available: " << free_dma << " bytes");
            FASTLED_DBG("  Shortfall: " << (buffer_size_ - free_dma) << " bytes");
            FASTLED_DBG("Suggestion: Reduce NUM_LEDS or use fewer strips");
            return false;
        }

        FASTLED_DBG("  Allocating buffer...");
        dma_buffer_ = (uint8_t*)heap_caps_malloc(buffer_size_, MALLOC_CAP_DMA);
        if (!dma_buffer_) {
            FASTLED_DBG("*** FATAL ERROR: DMA buffer allocation failed! ***");
            FASTLED_DBG("  Requested size: " << buffer_size_ << " bytes");
            FASTLED_DBG("  Free DMA memory: " << heap_caps_get_free_size(MALLOC_CAP_DMA) << " bytes");
            FASTLED_DBG("  Largest free block: " << heap_caps_get_largest_free_block(MALLOC_CAP_DMA) << " bytes");
            FASTLED_DBG("Possible cause: Memory fragmentation");
            return false;
        }
        memset(dma_buffer_, 0, buffer_size_);
        FASTLED_DBG("  Buffer allocated successfully!");
    }

    FASTLED_DBG("Memory Status After Allocation:");
    FASTLED_DBG("  DMA-capable memory free: " << heap_caps_get_free_size(MALLOC_CAP_DMA) << " bytes");
    FASTLED_DBG("  Memory used for buffers: " << (free_dma - heap_caps_get_free_size(MALLOC_CAP_DMA)) << " bytes");

    // Create semaphore for transfer completion
    FASTLED_DBG("Creating synchronization semaphore...");
    xfer_done_sem_ = xSemaphoreCreateBinary();
    if (!xfer_done_sem_) {
        FASTLED_DBG("*** FATAL ERROR: Semaphore creation failed! ***");
        FASTLED_DBG("  This usually indicates system resource exhaustion");
        FASTLED_DBG("  Free heap: " << heap_caps_get_free_size(MALLOC_CAP_INTERNAL) << " bytes");
        FASTLED_DBG("Suggestion: Simplify sketch or reduce memory usage");

        if (config_.buffer_strategy == ParlioBufferStrategy::BREAK_PER_COLOR) {
            for (int i = 0; i < 3; i++) {
                heap_caps_free(dma_sub_buffers_[i]);
                dma_sub_buffers_[i] = nullptr;
            }
        } else {
            heap_caps_free(dma_buffer_);
            dma_buffer_ = nullptr;
        }
        return false;
    }
    xSemaphoreGive(xfer_done_sem_);
    FASTLED_DBG("  Semaphore created successfully!");

    // Configure PARLIO TX unit
    FASTLED_DBG("Configuring PARLIO TX peripheral...");
    parlio_tx_unit_config_t parlio_config = {};
    parlio_config.clk_src = PARLIO_CLK_SRC_DEFAULT;
    parlio_config.clk_in_gpio_num = (gpio_num_t)-1;  // Use internal clock
    parlio_config.input_clk_src_freq_hz = 0;  // Not used when clk_in_gpio_num is -1
    parlio_config.output_clk_freq_hz = config_.clock_freq_hz;
    parlio_config.data_width = DATA_WIDTH;
    parlio_config.clk_out_gpio_num = (gpio_num_t)config_.clk_gpio;
    parlio_config.valid_gpio_num = (gpio_num_t)-1;  // No separate valid signal
    parlio_config.trans_queue_depth = 4;
    // Use sub-buffer size if breaking per color, otherwise use full buffer size
    parlio_config.max_transfer_size = (config_.buffer_strategy == ParlioBufferStrategy::BREAK_PER_COLOR)
                                       ? sub_buffer_size_
                                       : buffer_size_;
    parlio_config.dma_burst_size = 64;  // Standard DMA burst size
    parlio_config.sample_edge = PARLIO_SAMPLE_EDGE_POS;
    parlio_config.bit_pack_order = PARLIO_BIT_PACK_ORDER_MSB;
    parlio_config.flags.clk_gate_en = 0;
    parlio_config.flags.io_loop_back = 0;
    parlio_config.flags.allow_pd = 0;

    FASTLED_DBG("PARLIO Configuration Details:");
    FASTLED_DBG("  clk_src: PARLIO_CLK_SRC_DEFAULT");
    FASTLED_DBG("  clk_in_gpio_num: -1 (using internal clock)");
    FASTLED_DBG("  clk_out_gpio_num: " << config_.clk_gpio << " (GPIO for clock output)");
    if (config_.clk_gpio == 9) {
        FASTLED_DBG("    NOTE: GPIO 9 may be used for flash/PSRAM on some boards!");
    }
    FASTLED_DBG("  output_clk_freq_hz: " << config_.clock_freq_hz << " Hz");
    FASTLED_DBG("  data_width: " << (int)DATA_WIDTH << " lanes");
    FASTLED_DBG("  valid_gpio_num: -1 (disabled)");
    FASTLED_DBG("  trans_queue_depth: 4");
    FASTLED_DBG("  max_transfer_size: " << parlio_config.max_transfer_size << " bytes");
    FASTLED_DBG("  dma_burst_size: 64 bytes");
    FASTLED_DBG("  sample_edge: PARLIO_SAMPLE_EDGE_POS");
    FASTLED_DBG("  bit_pack_order: PARLIO_BIT_PACK_ORDER_MSB");
    FASTLED_DBG("  flags: clk_gate_en=0, io_loop_back=0, allow_pd=0");

    // Copy GPIO numbers
    FASTLED_DBG("Assigning data lane GPIOs:");
    for (int i = 0; i < DATA_WIDTH; i++) {
        parlio_config.data_gpio_nums[i] = (gpio_num_t)config_.data_gpios[i];
        FASTLED_DBG("  Lane " << i << ": GPIO " << config_.data_gpios[i]);
    }

    // Create PARLIO TX unit
    FASTLED_DBG("========================================");
    FASTLED_DBG("CALLING parlio_new_tx_unit()...");
    FASTLED_DBG("This is the critical ESP-IDF API call that allocates the PARLIO peripheral");
    esp_err_t err = parlio_new_tx_unit(&parlio_config, &tx_unit_);

    if (err != ESP_OK) {
        FASTLED_DBG("========================================");
        FASTLED_DBG("*** FATAL ERROR: parlio_new_tx_unit() FAILED! ***");
        FASTLED_DBG("========================================");
        FASTLED_DBG("Error Code: " << (int)err << " (hex: 0x" << (unsigned int)err << ") - " << esp_err_to_name_safe(err));
        FASTLED_DBG("");

        // Provide detailed diagnosis based on error code
        switch (err) {
            case ESP_ERR_INVALID_ARG:
                FASTLED_DBG("DIAGNOSIS: Invalid Configuration Parameter");
                FASTLED_DBG("  One or more PARLIO configuration parameters is invalid.");
                FASTLED_DBG("  Common causes:");
                FASTLED_DBG("    - Invalid GPIO number (reserved, doesn't exist, or in use)");
                FASTLED_DBG("    - clk_out_gpio_num conflict (GPIO " << config_.clk_gpio << ")");
                FASTLED_DBG("    - data_gpio conflict (GPIOs: 2, 5, 4)");
                FASTLED_DBG("    - Invalid data_width or frequency");
                FASTLED_DBG("");
                FASTLED_DBG("SUGGESTED FIX:");
                FASTLED_DBG("  1. Check if GPIO " << config_.clk_gpio << " is available on your board");
                FASTLED_DBG("  2. Try different clock GPIO (10, 11, or 12)");
                FASTLED_DBG("  3. Check ESP32-P4 board schematic for reserved pins");
                FASTLED_DBG("  4. Try setting clk_gpio to -1 (if supported)");
                break;

            case ESP_ERR_NO_MEM:
                FASTLED_DBG("DIAGNOSIS: Insufficient Memory");
                FASTLED_DBG("  ESP-IDF could not allocate memory for PARLIO TX unit.");
                FASTLED_DBG("  Free heap: " << heap_caps_get_free_size(MALLOC_CAP_INTERNAL) << " bytes");
                FASTLED_DBG("");
                FASTLED_DBG("SUGGESTED FIX:");
                FASTLED_DBG("  1. Reduce NUM_LEDS or number of strips");
                FASTLED_DBG("  2. Initialize FastLED earlier in setup()");
                FASTLED_DBG("  3. Reduce memory usage in other parts of sketch");
                break;

            case ESP_ERR_NOT_FOUND:
                FASTLED_DBG("DIAGNOSIS: All PARLIO TX Units Exhausted");
                FASTLED_DBG("  ESP32-P4 has a limited number of PARLIO TX units (likely 1).");
                FASTLED_DBG("  All units are currently in use or not properly released.");
                FASTLED_DBG("");
                FASTLED_DBG("SUGGESTED FIX:");
                FASTLED_DBG("  1. Ensure no other code is using PARLIO");
                FASTLED_DBG("  2. Power cycle the ESP32-P4 board");
                FASTLED_DBG("  3. Check for driver cleanup issues");
                FASTLED_DBG("  4. This may indicate a driver bug - please report");
                break;

            case ESP_ERR_NOT_SUPPORTED:
                FASTLED_DBG("DIAGNOSIS: Feature Not Supported");
                FASTLED_DBG("  The requested PARLIO configuration is not supported by hardware.");
                FASTLED_DBG("");
                FASTLED_DBG("SUGGESTED FIX:");
                FASTLED_DBG("  1. Check ESP32-P4 datasheet for PARLIO capabilities");
                FASTLED_DBG("  2. Reduce clock frequency");
                FASTLED_DBG("  3. Change data_width or other parameters");
                break;

            case ESP_FAIL:
                FASTLED_DBG("DIAGNOSIS: Generic ESP-IDF Failure");
                FASTLED_DBG("  An unspecified error occurred in the ESP-IDF driver.");
                FASTLED_DBG("");
                FASTLED_DBG("SUGGESTED FIX:");
                FASTLED_DBG("  1. Check ESP-IDF version (currently: dirty/modified)");
                FASTLED_DBG("  2. Try official ESP-IDF v5.5.1");
                FASTLED_DBG("  3. Check board power supply (unstable power can cause issues)");
                FASTLED_DBG("  4. Review ESP-IDF logs (if CONFIG_LOG_LEVEL is set)");
                break;

            default:
                FASTLED_DBG("DIAGNOSIS: Unknown Error Code");
                FASTLED_DBG("  This error code is not recognized.");
                FASTLED_DBG("");
                FASTLED_DBG("SUGGESTED FIX:");
                FASTLED_DBG("  1. Check ESP-IDF documentation for error code " << (int)err);
                FASTLED_DBG("  2. Update ESP-IDF to latest version");
                FASTLED_DBG("  3. Report this error to FastLED developers");
                break;
        }

        FASTLED_DBG("");
        FASTLED_DBG("CONFIGURATION SUMMARY (for bug report):");
        FASTLED_DBG("  Board: ESP32-P4");
        FASTLED_DBG("  Clock GPIO: " << config_.clk_gpio);
        FASTLED_DBG("  Data GPIOs: " << config_.data_gpios[0] << ", "
                    << config_.data_gpios[1] << ", " << config_.data_gpios[2]);
        FASTLED_DBG("  Frequency: " << config_.clock_freq_hz << " Hz");
        FASTLED_DBG("  Lanes: " << (int)DATA_WIDTH);
        FASTLED_DBG("  LEDs per strip: " << num_leds);
        FASTLED_DBG("========================================");

        // Cleanup
        vSemaphoreDelete(xfer_done_sem_);
        if (config_.buffer_strategy == ParlioBufferStrategy::BREAK_PER_COLOR) {
            for (int i = 0; i < 3; i++) {
                heap_caps_free(dma_sub_buffers_[i]);
                dma_sub_buffers_[i] = nullptr;
            }
        } else {
            heap_caps_free(dma_buffer_);
            dma_buffer_ = nullptr;
        }
        xfer_done_sem_ = nullptr;
        return false;
    }

    FASTLED_DBG("  SUCCESS! PARLIO TX unit created!");

    // Register event callbacks
    parlio_tx_event_callbacks_t cbs = {
        .on_trans_done = parlio_tx_done_callback,
    };
    parlio_tx_unit_register_event_callbacks(tx_unit_, &cbs, this);

    // Enable PARLIO TX unit
    FASTLED_DBG("Enabling PARLIO TX unit...");
    err = parlio_tx_unit_enable(tx_unit_);
    if (err != ESP_OK) {
        FASTLED_DBG("========================================");
        FASTLED_DBG("*** FATAL ERROR: parlio_tx_unit_enable() FAILED! ***");
        FASTLED_DBG("========================================");
        FASTLED_DBG("Error Code: " << (int)err << " (hex: 0x" << (unsigned int)err << ") - " << esp_err_to_name_safe(err));
        FASTLED_DBG("");
        FASTLED_DBG("DIAGNOSIS:");
        FASTLED_DBG("  PARLIO TX unit was created but failed to enable.");
        FASTLED_DBG("  This usually indicates a hardware or resource conflict.");
        FASTLED_DBG("");
        FASTLED_DBG("SUGGESTED FIX:");
        FASTLED_DBG("  1. Power cycle the ESP32-P4 board");
        FASTLED_DBG("  2. Check for GPIO conflicts with other peripherals");
        FASTLED_DBG("  3. Ensure no other PARLIO instances are running");
        FASTLED_DBG("  4. This may be an ESP-IDF bug - try different version");
        FASTLED_DBG("========================================");

        parlio_del_tx_unit(tx_unit_);
        vSemaphoreDelete(xfer_done_sem_);
        if (config_.buffer_strategy == ParlioBufferStrategy::BREAK_PER_COLOR) {
            for (int i = 0; i < 3; i++) {
                heap_caps_free(dma_sub_buffers_[i]);
                dma_sub_buffers_[i] = nullptr;
            }
        } else {
            heap_caps_free(dma_buffer_);
            dma_buffer_ = nullptr;
        }
        tx_unit_ = nullptr;
        xfer_done_sem_ = nullptr;
        return false;
    }

    FASTLED_DBG("  SUCCESS! PARLIO TX unit enabled!");
    FASTLED_DBG("========================================");
    FASTLED_DBG("PARLIO DRIVER INITIALIZATION COMPLETE!");
    FASTLED_DBG("========================================");
    FASTLED_DBG("Driver is ready to display LEDs.");
    FASTLED_DBG("Configuration:");
    FASTLED_DBG("  " << (int)DATA_WIDTH << " lanes @ " << config_.clock_freq_hz << " Hz");
    FASTLED_DBG("  " << num_leds << " LEDs per strip");
    FASTLED_DBG("  Total: " << (num_leds * DATA_WIDTH) << " LEDs");
    FASTLED_DBG("========================================");

    return true;
}

template <uint8_t DATA_WIDTH, typename CHIPSET>
void ParlioLedDriver<DATA_WIDTH, CHIPSET>::end() {
    if (tx_unit_) {
        parlio_tx_unit_disable(tx_unit_);
        parlio_del_tx_unit(tx_unit_);
        tx_unit_ = nullptr;
    }

    if (dma_buffer_) {
        heap_caps_free(dma_buffer_);
        dma_buffer_ = nullptr;
    }

    for (int i = 0; i < 3; i++) {
        if (dma_sub_buffers_[i]) {
            heap_caps_free(dma_sub_buffers_[i]);
            dma_sub_buffers_[i] = nullptr;
        }
    }

    if (xfer_done_sem_) {
        vSemaphoreDelete(xfer_done_sem_);
        xfer_done_sem_ = nullptr;
    }

    num_leds_ = 0;
}

template <uint8_t DATA_WIDTH, typename CHIPSET>
void ParlioLedDriver<DATA_WIDTH, CHIPSET>::set_strip(uint8_t channel, CRGB* leds) {
    if (channel < DATA_WIDTH) {
        strips_[channel] = leds;
    }
}

template <uint8_t DATA_WIDTH, typename CHIPSET>
template<EOrder RGB_ORDER>
void ParlioLedDriver<DATA_WIDTH, CHIPSET>::show() {
    if (!tx_unit_) {
        return;
    }

    // Verify buffers are allocated
    if (config_.buffer_strategy == ParlioBufferStrategy::BREAK_PER_COLOR) {
        if (!dma_sub_buffers_[0] || !dma_sub_buffers_[1] || !dma_sub_buffers_[2]) {
            return;
        }
    } else {
        if (!dma_buffer_) {
            return;
        }
    }

    // Wait for previous transfer to complete
    xSemaphoreTake(xfer_done_sem_, portMAX_DELAY);
    dma_busy_ = true;

    // Pack LED data into DMA buffer(s)
    pack_data<RGB_ORDER>();

    // Configure transmission
    parlio_transmit_config_t tx_config = {};
    tx_config.idle_value = 0x00000000;  // Lines idle low between frames
    tx_config.flags.queue_nonblocking = 0;

    if (config_.buffer_strategy == ParlioBufferStrategy::BREAK_PER_COLOR) {
        // Transmit 3 sub-buffers sequentially (G, R, B)
        // This ensures DMA gaps only occur at color component boundaries
        for (int color = 0; color < 3; color++) {
            size_t total_bits = sub_buffer_size_ * 8;
            esp_err_t err = parlio_tx_unit_transmit(tx_unit_, dma_sub_buffers_[color], total_bits, &tx_config);

            if (err != ESP_OK) {
                dma_busy_ = false;
                xSemaphoreGive(xfer_done_sem_);
                return;
            }

            // Wait for this buffer to complete before transmitting next
            // This is necessary because we're reusing the completion callback
            if (color < 2) {
                xSemaphoreTake(xfer_done_sem_, portMAX_DELAY);
            }
        }
        // Last callback will give semaphore when done
    } else {
        // Monolithic buffer (original implementation)
        size_t total_bits = buffer_size_ * 8;
        esp_err_t err = parlio_tx_unit_transmit(tx_unit_, dma_buffer_, total_bits, &tx_config);

        if (err != ESP_OK) {
            dma_busy_ = false;
            xSemaphoreGive(xfer_done_sem_);
        }
        // Callback will give semaphore when done
    }
}

template <uint8_t DATA_WIDTH, typename CHIPSET>
void ParlioLedDriver<DATA_WIDTH, CHIPSET>::wait() {
    if (xfer_done_sem_) {
        xSemaphoreTake(xfer_done_sem_, portMAX_DELAY);
        xSemaphoreGive(xfer_done_sem_);
    }
}

template <uint8_t DATA_WIDTH, typename CHIPSET>
bool ParlioLedDriver<DATA_WIDTH, CHIPSET>::is_initialized() const {
    return tx_unit_ != nullptr;
}

template <uint8_t DATA_WIDTH, typename CHIPSET>
void ParlioLedDriver<DATA_WIDTH, CHIPSET>::show_grb() {
    show<GRB>();
}

template <uint8_t DATA_WIDTH, typename CHIPSET>
void ParlioLedDriver<DATA_WIDTH, CHIPSET>::show_rgb() {
    show<RGB>();
}

template <uint8_t DATA_WIDTH, typename CHIPSET>
void ParlioLedDriver<DATA_WIDTH, CHIPSET>::show_bgr() {
    show<BGR>();
}

template <uint8_t DATA_WIDTH, typename CHIPSET>
template<EOrder RGB_ORDER>
void ParlioLedDriver<DATA_WIDTH, CHIPSET>::pack_data() {
    if (config_.buffer_strategy == ParlioBufferStrategy::BREAK_PER_COLOR) {
        // Pack data into 3 separate sub-buffers (one per color component)
        // This ensures DMA gaps only occur at color boundaries
        for (uint8_t output_pos = 0; output_pos < 3; output_pos++) {
            // Get which CRGB byte to output at this position
            uint8_t crgb_offset = get_crgb_byte_offset<RGB_ORDER>(output_pos);
            size_t byte_idx = 0;

            for (uint16_t led = 0; led < num_leds_; led++) {
                // Process 8 bits of this color byte (MSB first)
                for (int8_t bit = 7; bit >= 0; bit--) {
                    uint8_t output_byte = 0;

                    // Pack same bit position from all DATA_WIDTH channels
                    for (uint8_t channel = 0; channel < DATA_WIDTH; channel++) {
                        if (strips_[channel]) {
                            const uint8_t* channel_data = reinterpret_cast<const uint8_t*>(&strips_[channel][led]);
                            uint8_t channel_byte = channel_data[crgb_offset];
                            uint8_t bit_val = (channel_byte >> bit) & 0x01;
                            // MSB of output byte corresponds to channel 0
                            output_byte |= (bit_val << (7 - channel));
                        }
                    }

                    dma_sub_buffers_[output_pos][byte_idx++] = output_byte;
                }
            }
        }
    } else {
        // Monolithic buffer (original implementation)
        size_t byte_idx = 0;

        for (uint16_t led = 0; led < num_leds_; led++) {
            // Process each of 3 color bytes in the specified output order
            for (uint8_t output_pos = 0; output_pos < 3; output_pos++) {
                // Get which CRGB byte to output at this position
                uint8_t crgb_offset = get_crgb_byte_offset<RGB_ORDER>(output_pos);

                // Process 8 bits of this byte (MSB first)
                for (int8_t bit = 7; bit >= 0; bit--) {
                    uint8_t output_byte = 0;

                    // Pack same bit position from all DATA_WIDTH channels
                    for (uint8_t channel = 0; channel < DATA_WIDTH; channel++) {
                        if (strips_[channel]) {
                            const uint8_t* channel_data = reinterpret_cast<const uint8_t*>(&strips_[channel][led]);
                            uint8_t channel_byte = channel_data[crgb_offset];
                            uint8_t bit_val = (channel_byte >> bit) & 0x01;
                            // MSB of output byte corresponds to channel 0
                            output_byte |= (bit_val << (7 - channel));
                        }
                    }

                    dma_buffer_[byte_idx++] = output_byte;
                }
            }
        }
    }
}

template <uint8_t DATA_WIDTH, typename CHIPSET>
template<EOrder RGB_ORDER>
constexpr uint8_t ParlioLedDriver<DATA_WIDTH, CHIPSET>::get_crgb_byte_offset(uint8_t output_pos) {
    if constexpr (RGB_ORDER == GRB) {
        // Output: G, R, B -> byte offsets: 1, 0, 2
        return (output_pos == 0) ? 1 : (output_pos == 1) ? 0 : 2;
    } else if constexpr (RGB_ORDER == RGB) {
        // Output: R, G, B -> byte offsets: 0, 1, 2
        return output_pos;
    } else if constexpr (RGB_ORDER == BGR) {
        // Output: B, G, R -> byte offsets: 2, 1, 0
        return (output_pos == 0) ? 2 : (output_pos == 1) ? 1 : 0;
    } else {
        // Default to RGB
        return output_pos;
    }
}

template <uint8_t DATA_WIDTH, typename CHIPSET>
bool IRAM_ATTR ParlioLedDriver<DATA_WIDTH, CHIPSET>::parlio_tx_done_callback(
    parlio_tx_unit_handle_t tx_unit,
    const parlio_tx_done_event_data_t* edata,
    void* user_ctx)
{
    ParlioLedDriver* driver = static_cast<ParlioLedDriver*>(user_ctx);
    BaseType_t high_priority_task_awoken = pdFALSE;

    driver->dma_busy_ = false;
    xSemaphoreGiveFromISR(driver->xfer_done_sem_, &high_priority_task_awoken);

    return high_priority_task_awoken == pdTRUE;
}
