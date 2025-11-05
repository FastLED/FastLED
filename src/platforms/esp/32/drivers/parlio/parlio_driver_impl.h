/// @file parlio_driver_impl.h
/// @brief ESP32-P4 PARLIO LED driver template implementation

#pragma once

#include "parlio_driver.h"
#include "esp_err.h"
#include "fl/cstring.h"
#include "fl/log.h"

// Compile-time debug logging control
// Enable with: build_flags = -DFASTLED_ESP32_PARLIO_DLOGGING
#ifdef FASTLED_ESP32_PARLIO_DLOGGING
    #define PARLIO_DLOG(msg) FASTLED_DBG("PARLIO: " << msg)
#else
    #define PARLIO_DLOG(msg) do { } while(0)
#endif

// ============================================================================
// TODO: WLED-MM-P4 Style Buffer Breaking Strategy
// ============================================================================
// Current BREAK_PER_COLOR implementation breaks buffers between entire color
// components (all G bits → all R bits → all B bits). This can cause LEDs to
// latch prematurely during DMA gaps between color transmissions.
//
// WLED-MM-P4 uses a superior approach:
// - Break buffers at LED boundaries after the LSB (bit 0) of each color byte
// - DMA gaps only affect the least significant bit (minimal visual impact)
// - Worst case artifact: 0,0,0 becomes 0,0,1 (imperceptible)
// - Keeps each transmission under timing threshold (<20μs gap tolerance)
//
// Reference: https://github.com/FastLED/FastLED/issues/2095#issuecomment-3369337632
// ============================================================================

namespace fl {

// Helper function to convert esp_err_t to readable string
namespace detail {
inline const char* parlio_err_to_str(esp_err_t err) {
    return esp_err_to_name(err);
}

// WS2812 timing patterns (4 bits per LED bit at 3.2 MHz)
// Each 4-bit group: 1000 = T0H+T0L (0.4μs+0.85μs), 1110 = T1H+T1L (0.8μs+0.45μs)
static constexpr uint16_t WS2812_BITPATTERNS[16] = {
    0b1000100010001000, 0b1000100010001110, 0b1000100011101000, 0b1000100011101110,
    0b1000111010001000, 0b1000111010001110, 0b1000111011101000, 0b1000111011101110,
    0b1110100010001000, 0b1110100010001110, 0b1110100011101000, 0b1110100011101110,
    0b1110111010001000, 0b1110111010001110, 0b1110111011101000, 0b1110111011101110,
};

// Generate 32-bit waveform for 8-bit color value
// Input: color byte (0-255)
// Output: 32-bit pattern encoding 8 LED bits as timing waveforms
inline uint32_t generate_waveform(uint8_t value) {
    const uint16_t p1 = WS2812_BITPATTERNS[value >> 4];    // High nibble
    const uint16_t p2 = WS2812_BITPATTERNS[value & 0x0F];  // Low nibble
    return (uint32_t(p2) << 16) | p1;
}

// Pack 32 time-slices for 1-bit width (32 slices → 4 bytes)
inline void process_1bit(uint8_t* buffer, const uint32_t* slices) {
    uint32_t packed = 0;
    for (int i = 0; i < 32; ++i) {
        if (slices[i] & 0x01) packed |= (1 << i);
    }
    *reinterpret_cast<uint32_t*>(buffer) = packed;
}

// Pack 32 time-slices for 2-bit width (32 slices → 8 bytes)
inline void process_2bit(uint8_t* buffer, const uint32_t* slices) {
    uint32_t* out = reinterpret_cast<uint32_t*>(buffer);
    uint32_t word0 = 0, word1 = 0;
    for (int i = 0; i < 16; ++i) {
        word0 |= ((slices[i] & 0x03) << (i * 2));
        word1 |= ((slices[i+16] & 0x03) << (i * 2));
    }
    out[0] = word0;
    out[1] = word1;
}

// Pack 32 time-slices for 4-bit width (32 slices → 16 bytes)
inline void process_4bit(uint8_t* buffer, const uint32_t* slices) {
    uint32_t* out = reinterpret_cast<uint32_t*>(buffer);
    uint32_t word0 = 0, word1 = 0, word2 = 0, word3 = 0;
    for (int i = 0; i < 8; ++i) {
        word0 |= ((slices[i] & 0x0F) << (i * 4));
        word1 |= ((slices[i+8] & 0x0F) << (i * 4));
        word2 |= ((slices[i+16] & 0x0F) << (i * 4));
        word3 |= ((slices[i+24] & 0x0F) << (i * 4));
    }
    out[0] = word0; out[1] = word1; out[2] = word2; out[3] = word3;
}

// Pack 32 time-slices for 8-bit width (32 slices → 32 bytes)
inline void process_8bit(uint8_t* buffer, const uint32_t* slices) {
    uint32_t* out = reinterpret_cast<uint32_t*>(buffer);
    for (int i = 0; i < 8; ++i) {
        const int base = i * 4;
        out[i] = (slices[base] & 0xFF) |
                 ((slices[base+1] & 0xFF) << 8) |
                 ((slices[base+2] & 0xFF) << 16) |
                 ((slices[base+3] & 0xFF) << 24);
    }
}

// Pack 32 time-slices for 16-bit width (32 slices → 64 bytes)
inline void process_16bit(uint8_t* buffer, const uint32_t* slices) {
    uint32_t* out = reinterpret_cast<uint32_t*>(buffer);
    for (int i = 0; i < 16; ++i) {
        const int base = i * 2;
        out[i] = (slices[base] & 0xFFFF) |
                 ((slices[base+1] & 0xFFFF) << 16);
    }
}

} // namespace detail

template <uint8_t DATA_WIDTH, typename CHIPSET>
ParlioLedDriver<DATA_WIDTH, CHIPSET>::ParlioLedDriver()
    : config_{}
    , num_leds_(0)
    , strips_{}
    , tx_unit_(nullptr)
    , dma_buffer_(nullptr)
    , buffer_size_(0)
    , xfer_done_sem_(nullptr)
    , dma_busy_(false)
{
    for (int i = 0; i < 16; i++) {
        strips_[i] = nullptr;
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
        FL_LOG_PARLIO("Configuration error - num_lanes (" << int(config.num_lanes)
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

    // Calculate expanded buffer size for waveform encoding
    // 3 color components × 32-bit waveform each = 96 bits per LED
    // Then multiply by DATA_WIDTH for parallel strips
    const uint32_t SYMBOLS_PER_LED = 96;  // 3 components × 32 bits each
    const uint32_t BITS_PER_LED = SYMBOLS_PER_LED * DATA_WIDTH;
    buffer_size_ = (num_leds * BITS_PER_LED + 7) / 8;  // Convert to bytes
    PARLIO_DLOG("Calculated buffer_size: " << buffer_size_ << " bytes (SYMBOLS_PER_LED=" << SYMBOLS_PER_LED << ", BITS_PER_LED=" << BITS_PER_LED << ")");

    // Allocate single DMA buffer for continuous waveform transmission
    dma_buffer_ = (uint8_t*)heap_caps_malloc(buffer_size_, MALLOC_CAP_DMA);
    if (!dma_buffer_) {
        FL_LOG_PARLIO("Failed to allocate DMA buffer (" << buffer_size_ << " bytes)");
        return false;
    }
    fl::memset(dma_buffer_, 0, buffer_size_);
    PARLIO_DLOG("DMA buffer allocated successfully at " << (void*)dma_buffer_);

    // Create semaphore for transfer completion
    xfer_done_sem_ = xSemaphoreCreateBinary();
    if (!xfer_done_sem_) {
        FL_LOG_PARLIO("Failed to create semaphore");
        heap_caps_free(dma_buffer_);
        dma_buffer_ = nullptr;
        return false;
    }
    xSemaphoreGive(xfer_done_sem_);

    // Configure PARLIO TX unit
    PARLIO_DLOG("Configuring PARLIO TX unit:");
    parlio_tx_unit_config_t parlio_config = {};
    parlio_config.clk_src = PARLIO_CLK_SRC_DEFAULT;
    parlio_config.clk_in_gpio_num = (gpio_num_t)-1;  // Use internal clock source
    parlio_config.input_clk_src_freq_hz = 0;  // Not used when clk_in_gpio_num is -1
    parlio_config.output_clk_freq_hz = config_.clock_freq_hz;  // 3.2 MHz for WS2812
    parlio_config.data_width = DATA_WIDTH;
    parlio_config.clk_out_gpio_num = (gpio_num_t)-1;  // No external clock output needed
    parlio_config.valid_gpio_num = (gpio_num_t)-1;  // No separate valid signal
    parlio_config.trans_queue_depth = 4;
    parlio_config.max_transfer_size = buffer_size_;  // Maximum size for single continuous buffer
    parlio_config.dma_burst_size = 64;  // Standard DMA burst size
    parlio_config.sample_edge = PARLIO_SAMPLE_EDGE_POS;
    parlio_config.bit_pack_order = PARLIO_BIT_PACK_ORDER_MSB;
    parlio_config.flags.clk_gate_en = 0;
    parlio_config.flags.io_loop_back = 0;
    parlio_config.flags.allow_pd = 0;

    PARLIO_DLOG("  data_width: " << int(DATA_WIDTH));
    PARLIO_DLOG("  output_clk_freq_hz: " << config_.clock_freq_hz);
    PARLIO_DLOG("  max_transfer_size: " << parlio_config.max_transfer_size);
    PARLIO_DLOG("  clk_out_gpio: -1 (internal clock)");

    // Copy GPIO numbers
    for (int i = 0; i < DATA_WIDTH; i++) {
        parlio_config.data_gpio_nums[i] = (gpio_num_t)config_.data_gpios[i];
        PARLIO_DLOG("  data_gpio[" << i << "]: " << config_.data_gpios[i]);
    }

    // Create PARLIO TX unit
    esp_err_t err = parlio_new_tx_unit(&parlio_config, &tx_unit_);
    if (err != ESP_OK) {
        FL_LOG_PARLIO("parlio_new_tx_unit() failed with error: " << detail::parlio_err_to_str(err) << " (" << int(err) << ")");
        FL_LOG_PARLIO("  Check GPIO pins - data:["
                << config_.data_gpios[0] << "," << config_.data_gpios[1] << "," << config_.data_gpios[2] << ",...]");

        vSemaphoreDelete(xfer_done_sem_);
        heap_caps_free(dma_buffer_);
        dma_buffer_ = nullptr;
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
        FL_LOG_PARLIO("parlio_tx_unit_enable() failed with error: " << detail::parlio_err_to_str(err) << " (" << int(err) << ")");

        parlio_del_tx_unit(tx_unit_);
        vSemaphoreDelete(xfer_done_sem_);
        heap_caps_free(dma_buffer_);
        dma_buffer_ = nullptr;
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
        FL_LOG_PARLIO("set_strip() - invalid channel " << int(channel) << " (DATA_WIDTH=" << int(DATA_WIDTH) << ")");
    }
}

template <uint8_t DATA_WIDTH, typename CHIPSET>
void ParlioLedDriver<DATA_WIDTH, CHIPSET>::show() {
    PARLIO_DLOG("show() called");

    if (!tx_unit_) {
        FL_LOG_PARLIO("show() called but tx_unit_ not initialized");
        return;
    }

    // Verify buffer is allocated
    if (!dma_buffer_) {
        FL_LOG_PARLIO("show() called but DMA buffer not allocated");
        return;
    }

    // Wait for previous transfer to complete
    PARLIO_DLOG("Waiting for previous transfer to complete...");
    xSemaphoreTake(xfer_done_sem_, portMAX_DELAY);
    dma_busy_ = true;

    // Pack LED data into DMA buffer(s)
    PARLIO_DLOG("Packing LED data...");
    pack_data();

    // Configure transmission
    parlio_transmit_config_t tx_config = {};
    tx_config.idle_value = 0x00000000;  // Lines idle low between frames
    tx_config.flags.queue_nonblocking = 0;

    // Calculate chunking parameters
    constexpr uint32_t SYMBOLS_PER_LED = 96;  // 3 components × 32 bits each
    constexpr uint32_t BITS_PER_LED = SYMBOLS_PER_LED * DATA_WIDTH;
    constexpr uint32_t BYTES_PER_LED = (BITS_PER_LED + 7) / 8;
    constexpr uint32_t MAX_BYTES_PER_CHUNK = 65535;  // PARLIO hardware limit
    constexpr uint16_t MAX_LEDS_PER_CHUNK = MAX_BYTES_PER_CHUNK / BYTES_PER_LED;

    // Continuous buffer transmission with chunking
    const uint8_t num_chunks = (num_leds_ + MAX_LEDS_PER_CHUNK - 1) / MAX_LEDS_PER_CHUNK;
    uint16_t leds_remaining = num_leds_;
    const uint8_t* chunk_ptr = dma_buffer_;

    PARLIO_DLOG("Transmitting continuous buffer in " << int(num_chunks) << " chunk(s)");

    for (uint8_t chunk_idx = 0; chunk_idx < num_chunks; chunk_idx++) {
        uint16_t leds_in_chunk = (leds_remaining < MAX_LEDS_PER_CHUNK)
                                  ? leds_remaining : MAX_LEDS_PER_CHUNK;
        size_t chunk_bits = leds_in_chunk * BITS_PER_LED;
        size_t chunk_bytes = leds_in_chunk * BYTES_PER_LED;

        PARLIO_DLOG("  Chunk " << int(chunk_idx) << ": " << leds_in_chunk << " LEDs, "
                   << chunk_bytes << " bytes, " << chunk_bits << " bits");

        esp_err_t err = parlio_tx_unit_transmit(tx_unit_, chunk_ptr, chunk_bits, &tx_config);

        if (err != ESP_OK) {
            FL_LOG_PARLIO("parlio_tx_unit_transmit() failed for chunk " << int(chunk_idx)
                          << ": " << detail::parlio_err_to_str(err) << " (" << int(err) << ")");
            dma_busy_ = false;
            xSemaphoreGive(xfer_done_sem_);
            return;
        }

        chunk_ptr += chunk_bytes;
        leds_remaining -= leds_in_chunk;

        // Wait for completion if not the last chunk
        if (chunk_idx < num_chunks - 1) {
            xSemaphoreTake(xfer_done_sem_, portMAX_DELAY);
        }
    }
    // Last callback will give semaphore when done
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
void ParlioLedDriver<DATA_WIDTH, CHIPSET>::pack_data() {
    constexpr uint32_t BYTES_PER_COMPONENT = (DATA_WIDTH == 1) ? 4 :
                                             (DATA_WIDTH == 2) ? 8 :
                                             (DATA_WIDTH == 4) ? 16 :
                                             (DATA_WIDTH == 8) ? 32 : 64;

    // Always show basic pack_data info (not just debug mode)
    FASTLED_DBG("PARLIO pack_data:");
    FASTLED_DBG("  DATA_WIDTH: " << int(DATA_WIDTH));
    FASTLED_DBG("  num_leds: " << num_leds_);
    FASTLED_DBG("  buffer_size: " << buffer_size_);
    FASTLED_DBG("  BYTES_PER_COMPONENT: " << BYTES_PER_COMPONENT);

    PARLIO_DLOG("pack_data() - Packing " << num_leds_ << " LEDs across " << int(DATA_WIDTH) << " channels with waveform generation");
    uint8_t* out_ptr = dma_buffer_;

    // Process each LED
    for (uint16_t led = 0; led < num_leds_; led++) {
        // Process each color component (G, R, B in WS2812 order)
        const uint8_t component_order[3] = {1, 0, 2};  // G=1, R=0, B=2 in CRGB

        for (uint8_t comp_idx = 0; comp_idx < 3; comp_idx++) {
            const uint8_t component = component_order[comp_idx];

            // Initialize 32 time-slices (each holds bits for all strips)
            uint32_t transposed_slices[32] = {0};

            // Generate and transpose waveforms for all active strips
            for (uint8_t channel = 0; channel < DATA_WIDTH; channel++) {
                if (!strips_[channel]) continue;

                // Get color component value from strip buffer
                const uint8_t* led_bytes = reinterpret_cast<const uint8_t*>(&strips_[channel][led]);
                const uint8_t component_value = led_bytes[component];

                // Generate 32-bit waveform for this component value
                const uint32_t waveform = detail::generate_waveform(component_value);
                const uint32_t pin_bit = (1u << channel);

                // Transpose: Extract each of 32 bits from waveform
                // and distribute to corresponding time-slices
                for (int slice = 0; slice < 32; slice++) {
                    if ((waveform >> slice) & 1) {
                        transposed_slices[slice] |= pin_bit;
                    }
                }
            }

            // Pack transposed slices based on DATA_WIDTH
            if constexpr (DATA_WIDTH == 1) {
                detail::process_1bit(out_ptr, transposed_slices);
            } else if constexpr (DATA_WIDTH == 2) {
                detail::process_2bit(out_ptr, transposed_slices);
            } else if constexpr (DATA_WIDTH == 4) {
                detail::process_4bit(out_ptr, transposed_slices);
            } else if constexpr (DATA_WIDTH == 8) {
                detail::process_8bit(out_ptr, transposed_slices);
            } else if constexpr (DATA_WIDTH == 16) {
                detail::process_16bit(out_ptr, transposed_slices);
            }

            out_ptr += BYTES_PER_COMPONENT;
        }
    }
    PARLIO_DLOG("pack_data() completed");
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

}  // namespace fl
