/// @file parlio_driver_impl.h
/// @brief ESP32-P4 PARLIO LED driver template implementation

#pragma once

#include "parlio_driver.h"
#include "esp_err.h"

// Compile-time debug logging control
// Enable with: build_flags = -DFASTLED_ESP32_PARLIO_DLOGGING
#ifdef FASTLED_ESP32_PARLIO_DLOGGING
    #define PARLIO_DLOG(msg) FASTLED_DBG("PARLIO: " << msg)
#else
    #define PARLIO_DLOG(msg) do { } while(0)
#endif

// Helper function to convert esp_err_t to readable string
namespace detail {
inline const char* parlio_err_to_str(esp_err_t err) {
    return esp_err_to_name(err);
}
} // namespace detail

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
    PARLIO_DLOG("begin() called - DATA_WIDTH=" << int(DATA_WIDTH) << ", num_leds=" << num_leds);

    if (config.num_lanes != DATA_WIDTH) {
        FL_WARN("PARLIO: Configuration error - num_lanes (" << int(config.num_lanes)
                << ") does not match DATA_WIDTH (" << int(DATA_WIDTH) << ")");
        return false;
    }

    config_ = config;
    num_leds_ = num_leds;

    // Set default clock frequency if not specified
    if (config_.clock_freq_hz == 0) {
        config_.clock_freq_hz = DEFAULT_CLOCK_FREQ_HZ;
        PARLIO_DLOG("Using default clock frequency: " << DEFAULT_CLOCK_FREQ_HZ << " Hz");
    } else {
        PARLIO_DLOG("Using configured clock frequency: " << config_.clock_freq_hz << " Hz");
    }

    // Calculate buffer size: num_leds * 24 bytes (1 byte per bit-time)
    // Each LED has 24 bits (GRB), we send 1 byte per bit-time regardless of DATA_WIDTH
    buffer_size_ = num_leds * 24;
    PARLIO_DLOG("Calculated buffer_size: " << buffer_size_ << " bytes");

    // Allocate DMA buffers based on strategy
    if (config_.buffer_strategy == ParlioBufferStrategy::BREAK_PER_COLOR) {
        PARLIO_DLOG("Using BREAK_PER_COLOR buffer strategy");
        // Allocate 3 sub-buffers (one for each color component: G, R, B)
        // Each sub-buffer holds 8 bytes per LED (1 byte per bit-time)
        sub_buffer_size_ = num_leds * 8;
        PARLIO_DLOG("Allocating 3 sub-buffers of " << sub_buffer_size_ << " bytes each");

        for (int i = 0; i < 3; i++) {
            dma_sub_buffers_[i] = (uint8_t*)heap_caps_malloc(sub_buffer_size_, MALLOC_CAP_DMA);
            if (!dma_sub_buffers_[i]) {
                FL_WARN("PARLIO: Failed to allocate DMA sub-buffer " << i << " (" << sub_buffer_size_ << " bytes)");
                // Clean up previously allocated buffers
                for (int j = 0; j < i; j++) {
                    heap_caps_free(dma_sub_buffers_[j]);
                    dma_sub_buffers_[j] = nullptr;
                }
                return false;
            }
            memset(dma_sub_buffers_[i], 0, sub_buffer_size_);
            PARLIO_DLOG("Sub-buffer " << i << " allocated successfully at " << (void*)dma_sub_buffers_[i]);
        }
    } else {
        PARLIO_DLOG("Using MONOLITHIC buffer strategy");
        // Monolithic buffer (original implementation)
        dma_buffer_ = (uint8_t*)heap_caps_malloc(buffer_size_, MALLOC_CAP_DMA);
        if (!dma_buffer_) {
            FL_WARN("PARLIO: Failed to allocate DMA buffer (" << buffer_size_ << " bytes)");
            return false;
        }
        memset(dma_buffer_, 0, buffer_size_);
        PARLIO_DLOG("Monolithic buffer allocated successfully at " << (void*)dma_buffer_);
    }

    // Create semaphore for transfer completion
    xfer_done_sem_ = xSemaphoreCreateBinary();
    if (!xfer_done_sem_) {
        FL_WARN("PARLIO: Failed to create semaphore");
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

    // Configure PARLIO TX unit
    PARLIO_DLOG("Configuring PARLIO TX unit:");
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

    PARLIO_DLOG("  data_width: " << int(DATA_WIDTH));
    PARLIO_DLOG("  output_clk_freq_hz: " << config_.clock_freq_hz);
    PARLIO_DLOG("  max_transfer_size: " << parlio_config.max_transfer_size);
    PARLIO_DLOG("  clk_gpio: " << config_.clk_gpio);

    // Copy GPIO numbers
    for (int i = 0; i < DATA_WIDTH; i++) {
        parlio_config.data_gpio_nums[i] = (gpio_num_t)config_.data_gpios[i];
        PARLIO_DLOG("  data_gpio[" << i << "]: " << config_.data_gpios[i]);
    }

    // Create PARLIO TX unit
    esp_err_t err = parlio_new_tx_unit(&parlio_config, &tx_unit_);
    if (err != ESP_OK) {
        FL_WARN("PARLIO: parlio_new_tx_unit() failed with error: " << detail::parlio_err_to_str(err) << " (" << int(err) << ")");
        FL_WARN("  Check GPIO pins - clk:" << config_.clk_gpio << ", data:["
                << config_.data_gpios[0] << "," << config_.data_gpios[1] << "," << config_.data_gpios[2] << "]");

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

    // Register event callbacks
    PARLIO_DLOG("Registering PARLIO event callbacks");
    parlio_tx_event_callbacks_t cbs = {
        .on_trans_done = parlio_tx_done_callback,
    };
    parlio_tx_unit_register_event_callbacks(tx_unit_, &cbs, this);

    // Enable PARLIO TX unit
    PARLIO_DLOG("Enabling PARLIO TX unit");
    err = parlio_tx_unit_enable(tx_unit_);
    if (err != ESP_OK) {
        FL_WARN("PARLIO: parlio_tx_unit_enable() failed with error: " << detail::parlio_err_to_str(err) << " (" << int(err) << ")");

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

    PARLIO_DLOG("PARLIO driver initialization successful!");
    return true;
}

template <uint8_t DATA_WIDTH, typename CHIPSET>
void ParlioLedDriver<DATA_WIDTH, CHIPSET>::end() {
    PARLIO_DLOG("end() called - cleaning up resources");
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
        PARLIO_DLOG("set_strip() - channel " << int(channel) << " registered at " << (void*)leds);
    } else {
        FL_WARN("PARLIO: set_strip() - invalid channel " << int(channel) << " (DATA_WIDTH=" << int(DATA_WIDTH) << ")");
    }
}

template <uint8_t DATA_WIDTH, typename CHIPSET>
template<EOrder RGB_ORDER>
void ParlioLedDriver<DATA_WIDTH, CHIPSET>::show() {
    PARLIO_DLOG("show() called");

    if (!tx_unit_) {
        FL_WARN("PARLIO: show() called but tx_unit_ not initialized");
        return;
    }

    // Verify buffers are allocated
    if (config_.buffer_strategy == ParlioBufferStrategy::BREAK_PER_COLOR) {
        if (!dma_sub_buffers_[0] || !dma_sub_buffers_[1] || !dma_sub_buffers_[2]) {
            FL_WARN("PARLIO: show() called but DMA sub-buffers not allocated");
            return;
        }
    } else {
        if (!dma_buffer_) {
            FL_WARN("PARLIO: show() called but DMA buffer not allocated");
            return;
        }
    }

    // Wait for previous transfer to complete
    PARLIO_DLOG("Waiting for previous transfer to complete...");
    xSemaphoreTake(xfer_done_sem_, portMAX_DELAY);
    dma_busy_ = true;

    // Pack LED data into DMA buffer(s)
    PARLIO_DLOG("Packing LED data...");
    pack_data<RGB_ORDER>();

    // Configure transmission
    parlio_transmit_config_t tx_config = {};
    tx_config.idle_value = 0x00000000;  // Lines idle low between frames
    tx_config.flags.queue_nonblocking = 0;

    if (config_.buffer_strategy == ParlioBufferStrategy::BREAK_PER_COLOR) {
        PARLIO_DLOG("Transmitting 3 sub-buffers sequentially...");
        // Transmit 3 sub-buffers sequentially (G, R, B)
        // This ensures DMA gaps only occur at color component boundaries
        for (int color = 0; color < 3; color++) {
            size_t total_bits = sub_buffer_size_ * 8;
            PARLIO_DLOG("  Transmitting color " << color << " (" << sub_buffer_size_ << " bytes, " << total_bits << " bits)");
            esp_err_t err = parlio_tx_unit_transmit(tx_unit_, dma_sub_buffers_[color], total_bits, &tx_config);

            if (err != ESP_OK) {
                FL_WARN("PARLIO: parlio_tx_unit_transmit() failed for color " << color << ": " << detail::parlio_err_to_str(err) << " (" << int(err) << ")");
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
        PARLIO_DLOG("Transmitting monolithic buffer (" << buffer_size_ << " bytes, " << total_bits << " bits)");
        esp_err_t err = parlio_tx_unit_transmit(tx_unit_, dma_buffer_, total_bits, &tx_config);

        if (err != ESP_OK) {
            FL_WARN("PARLIO: parlio_tx_unit_transmit() failed: " << detail::parlio_err_to_str(err) << " (" << int(err) << ")");
            dma_busy_ = false;
            xSemaphoreGive(xfer_done_sem_);
        }
        // Callback will give semaphore when done
    }
    PARLIO_DLOG("show() completed - transmission started");
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
    // Always show basic pack_data info (not just debug mode)
    FASTLED_DBG("PARLIO pack_data:");
    FASTLED_DBG("  DATA_WIDTH: " << int(DATA_WIDTH));
    FASTLED_DBG("  num_leds: " << num_leds_);
    FASTLED_DBG("  buffer_size: " << buffer_size_);
    FASTLED_DBG("  sub_buffer_size: " << sub_buffer_size_);

    PARLIO_DLOG("pack_data() - Packing " << num_leds_ << " LEDs across " << int(DATA_WIDTH) << " channels");

    if (config_.buffer_strategy == ParlioBufferStrategy::BREAK_PER_COLOR) {
        PARLIO_DLOG("Using BREAK_PER_COLOR packing strategy");
        // Pack data into 3 separate sub-buffers (one per color component)
        // This ensures DMA gaps only occur at color boundaries
        for (uint8_t output_pos = 0; output_pos < 3; output_pos++) {
            // Get which CRGB byte to output at this position
            uint8_t crgb_offset = get_crgb_byte_offset<RGB_ORDER>(output_pos);
            size_t byte_idx = 0;
            PARLIO_DLOG("  Color component " << int(output_pos) << " (CRGB offset " << int(crgb_offset) << ")");

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
                            // Position bit based on DATA_WIDTH (use upper bits for < 8)
                            if (DATA_WIDTH >= 8) {
                                // MSB mode: bit N → GPIO N
                                output_byte |= (bit_val << channel);
                            } else if (DATA_WIDTH == 4) {
                                // Use upper nibble (bits 4-7): bit 4 → GPIO 0, bit 5 → GPIO 1, etc.
                                output_byte |= (bit_val << (4 + channel));
                            } else if (DATA_WIDTH == 2) {
                                // Use bits 6-7
                                output_byte |= (bit_val << (6 + channel));
                            } else if (DATA_WIDTH == 1) {
                                // Use bit 7
                                output_byte |= (bit_val << 7);
                            }
                        }
                    }

                    dma_sub_buffers_[output_pos][byte_idx++] = output_byte;

                    // Log first few bytes for debugging
                    #ifdef FASTLED_ESP32_PARLIO_DLOGGING
                    if (led == 0 && bit >= 5) {
                        char buf[64];
                        fl::snprintf(buf, sizeof(buf), "    LED[0] bit[%d]: byte=0x%02x", bit, output_byte);
                        FASTLED_DBG(buf);
                    }
                    #endif
                }
            }
        }
    } else {
        PARLIO_DLOG("Using MONOLITHIC buffer packing strategy");
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
                            // Position bit based on DATA_WIDTH (use upper bits for < 8)
                            if (DATA_WIDTH >= 8) {
                                // MSB mode: bit N → GPIO N
                                output_byte |= (bit_val << channel);
                            } else if (DATA_WIDTH == 4) {
                                // Use upper nibble (bits 4-7): bit 4 → GPIO 0, bit 5 → GPIO 1, etc.
                                output_byte |= (bit_val << (4 + channel));
                            } else if (DATA_WIDTH == 2) {
                                // Use bits 6-7
                                output_byte |= (bit_val << (6 + channel));
                            } else if (DATA_WIDTH == 1) {
                                // Use bit 7
                                output_byte |= (bit_val << 7);
                            }
                        }
                    }

                    dma_buffer_[byte_idx++] = output_byte;

                    // Log first few bytes for debugging
                    #ifdef FASTLED_ESP32_PARLIO_DLOGGING
                    if (led == 0 && output_pos == 0 && bit >= 5) {
                        char buf[64];
                        fl::snprintf(buf, sizeof(buf), "    LED[0] color[%d] bit[%d]: byte=0x%02x", output_pos, bit, output_byte);
                        FASTLED_DBG(buf);
                    }
                    #endif
                }
            }
        }
    }
    PARLIO_DLOG("pack_data() completed - packed " << byte_idx << " bytes");
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

    // Note: We can't use PARLIO_DLOG in ISR context - it uses FASTLED_DBG which may allocate
    driver->dma_busy_ = false;
    xSemaphoreGiveFromISR(driver->xfer_done_sem_, &high_priority_task_awoken);

    return high_priority_task_awoken == pdTRUE;
}
