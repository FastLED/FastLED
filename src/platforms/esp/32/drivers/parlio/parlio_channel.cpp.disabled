/// @file parlio_channel.cpp
/// @brief ESP32-P4 PARLIO LED channel implementation (runtime, non-template)

#if defined(ESP32)
#include "sdkconfig.h"

#if defined(CONFIG_IDF_TARGET_ESP32P4)

#include "parlio_channel.h"
#include "parlio_timing.h"
#include "parlio_engine.h"
#include "crgb.h"
#include "esp_err.h"
#include "fl/cstring.h"
#include "fl/log.h"
#include "fl/unique_ptr.h"
#include "driver/parlio_tx.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "esp_heap_caps.h"

// Compile-time debug logging control
// Enable with: build_flags = -DFASTLED_ESP32_PARLIO_DLOGGING
#ifdef FASTLED_ESP32_PARLIO_DLOGGING
    #define PARLIO_DLOG(msg) FASTLED_DBG("PARLIO: " << msg)
#else
    #define PARLIO_DLOG(msg) do { } while(0)
#endif

// ============================================================================
// WLED-MM-P4 Style Buffer Breaking Strategy - IMPLEMENTED
// ============================================================================
// This implementation breaks DMA buffers at LED boundaries after the LSB
// (bit 0) of each color component byte. This strategic placement minimizes
// visual artifacts from DMA timing gaps:
//
// Benefits:
// - DMA gaps only affect the least significant bit (minimal visual impact)
// - Worst case artifact: 0,0,0 becomes 0,0,1 (imperceptible)
// - Keeps each transmission under timing threshold (<20μs gap tolerance)
// - Prevents mid-component corruption that causes ±128 brightness errors
//
// Implementation:
// - Each LED's color data is transmitted in 3 chunks (G, R, B components)
// - Each chunk consists of 32 bits (8 bits × 4 waveform expansion)
// - Break points occur naturally at component boundaries after LSB
// - For large LED counts, additional breaks occur at LED boundaries
//
// Reference: https://github.com/FastLED/FastLED/issues/2095#issuecomment-3369337632
// ============================================================================

namespace fl {

// Helper functions
namespace detail {

inline const char* parlio_err_to_str(esp_err_t err) {
    return esp_err_to_name(err);
}

// Runtime waveform generator using chipset timing configuration
// Each LED bit is encoded as a 4-clock pattern, and each nibble (4 bits)
// is encoded as a 16-bit pattern. An 8-bit color value is split into two
// nibbles and expanded into a 32-bit waveform.
//
// @param value 8-bit color value (0-255)
// @param patterns Runtime-generated bitpattern lookup table
// @return 32-bit pattern encoding 8 LED bits as timing waveforms
inline uint32_t generate_waveform(uint8_t value, const uint16_t* patterns) {
    const uint16_t p1 = patterns[value >> 4];    // High nibble
    const uint16_t p2 = patterns[value & 0x0F];  // Low nibble
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

/// Calculate optimal clock frequency based on LED count
///
/// Strategy (from WLED Moon Modules):
/// - ≤256 LEDs: 150% base frequency (DMA overhead dominates, can go faster)
/// - ≤512 LEDs: 137.5% base frequency (balanced performance)
/// - >512 LEDs: 100% base frequency (DMA bandwidth constrained)
///
/// @param num_leds Total LED count
/// @param base_freq Chipset base frequency (from timing trait)
/// @return Adjusted clock frequency in Hz
inline uint32_t calculate_optimal_clock_freq(uint16_t num_leds, uint32_t base_freq) {
    if (num_leds <= 256) {
        // Small counts: 150% speed (e.g., 3.2 MHz → 4.8 MHz)
        return (base_freq * 3) / 2;
    } else if (num_leds <= 512) {
        // Medium counts: 137.5% speed (e.g., 3.2 MHz → 4.4 MHz)
        return (base_freq * 11) / 8;
    } else {
        // Large counts: 100% speed (e.g., 3.2 MHz → 3.2 MHz)
        return base_freq;
    }
}

/// Validate adjusted frequency is within PARLIO and chipset limits
inline bool validate_adjusted_freq(uint32_t adjusted_freq, uint32_t base_freq) {
    // PARLIO hardware limits: 1-40 MHz
    if (adjusted_freq < 1000000 || adjusted_freq > 40000000) {
        return false;
    }

    // Don't adjust more than 2× base frequency (safety margin)
    if (adjusted_freq > base_freq * 2) {
        return false;
    }

    return true;
}

/// Format GPIO status indicator
inline const char* gpio_status_string(gpio_num_t gpio, bool is_active, bool is_valid) {
    if (!is_active) {
        return "[unused]";
    }
    if (!is_valid || gpio < 0) {
        return "[INVALID]";
    }
    return "[active]";
}

/// Log detailed GPIO configuration
void log_gpio_configuration(uint8_t data_width, const ParlioChannelConfig& config, uint32_t clock_freq) {
    FL_LOG_PARLIO("╔═══════════════════════════════════════════════╗");
    FL_LOG_PARLIO("║   PARLIO Configuration Summary                ║");
    FL_LOG_PARLIO("╠═══════════════════════════════════════════════╣");
    FL_LOG_PARLIO("  Data width:        " << int(data_width) << " bits");
    FL_LOG_PARLIO("  Active lanes:      " << int(config.num_lanes));
    FL_LOG_PARLIO("  LED mode:          " << (config.is_rgbw ? "RGBW (4-component)" : "RGB (3-component)"));
    FL_LOG_PARLIO("  Clock frequency:   " << clock_freq << " Hz (" << (clock_freq / 1000) << " kHz)");
    FL_LOG_PARLIO("  Clock source:      Internal (PARLIO_CLK_SRC_DEFAULT)");
    FL_LOG_PARLIO("  Auto-clocking:     " << (config.auto_clock_adjustment ? "Enabled" : "Disabled"));
    FL_LOG_PARLIO("╠═══════════════════════════════════════════════╣");
    FL_LOG_PARLIO("║   GPIO Pin Mapping                            ║");
    FL_LOG_PARLIO("╠═══════════════════════════════════════════════╣");

    for (int i = 0; i < 16; i++) {
        bool is_active = (i < config.num_lanes);
        bool is_valid = (config.data_gpios[i] >= 0);
        const char* status = gpio_status_string(
            (gpio_num_t)config.data_gpios[i],
            is_active,
            is_valid
        );

        // Only show lanes up to data_width + inactive lanes if configured
        if (is_active || (i < data_width && config.data_gpios[i] != 0)) {
            char buf[64];
            fl::snprintf(buf, sizeof(buf), "  Lane %2d:          GPIO %2d  %s",
                        i, config.data_gpios[i], status);
            FL_LOG_PARLIO(buf);
        }
    }

    FL_LOG_PARLIO("╚═══════════════════════════════════════════════╝");
}

} // namespace detail

// ============================================================================
// ParlioChannelImpl - Private implementation struct
// ============================================================================

// Concrete implementation - all platform-specific types and data inline
class ParlioChannel : public IParlioChannel {
public:
    explicit ParlioChannel(const ChipsetTimingConfig& timing, uint8_t data_width);
    ~ParlioChannel() override;

    bool begin(const ParlioChannelConfig& config, uint16_t num_leds) override;
    void end() override;
    void set_strip(uint8_t channel, CRGB* leds) override;
    void show() override;
    void wait() override;
    bool is_initialized() const override;

private:
    void generate_bitpatterns();
    bool needs_reconfiguration(const ParlioChannelConfig& new_config, uint16_t new_num_leds) const;
    void print_status() const;
    void pack_data(uint8_t* output_buffer);

    static bool IRAM_ATTR parlio_tx_done_callback(
        parlio_tx_unit_handle_t tx_unit,
        const parlio_tx_done_event_data_t* edata,
        void* user_ctx);

    // All data members inline (no pimpl indirection)
    ChipsetTimingConfig timing_;
    uint16_t bitpatterns_[16];
    uint8_t data_width_;
    ParlioChannelConfig config_;
    uint16_t num_leds_;
    CRGB* strips_[16];
    parlio_tx_unit_handle_t tx_unit_;
    uint8_t* dma_buffer_[2];
    uint8_t current_buffer_idx_;
    size_t buffer_size_;
    SemaphoreHandle_t xfer_done_sem_;
    bool dma_busy_;
    ParlioChannelConfig last_config_;
    uint16_t last_num_leds_;
};

// ============================================================================
// ParlioChannel Implementation
// ============================================================================

ParlioChannel::ParlioChannel(const ChipsetTimingConfig& timing, uint8_t data_width)
    : timing_(timing)
    , bitpatterns_{}
    , data_width_(data_width)
    , config_{}
    , num_leds_(0)
    , strips_{}
    , tx_unit_(nullptr)
    , dma_buffer_{nullptr, nullptr}
    , current_buffer_idx_(0)
    , buffer_size_(0)
    , xfer_done_sem_(nullptr)
    , dma_busy_(false)
    , last_config_{}
    , last_num_leds_(0)
{
    for (int i = 0; i < 16; i++) {
        strips_[i] = nullptr;
    }

    // Generate bitpatterns from runtime timing configuration
    generate_bitpatterns();
}

ParlioChannel::~ParlioChannel() {
    end();
}

void ParlioChannel::generate_bitpatterns() {
    // Runtime version of parlio_detail::ParlioTimingGenerator
    // Generate bitpatterns based on timing_ configuration

    static constexpr uint32_t CLOCKS_PER_BIT = 4;
    static constexpr uint32_t CLOCK_FREQ_HZ = 3200000;  // 3.2 MHz for WS2812
    static constexpr uint32_t CLOCK_PERIOD_NS = 1000000000UL / CLOCK_FREQ_HZ;

    // Convert timing to clock counts (same logic as compile-time version)
    const uint32_t T0H_NS = timing_.t1_ns;
    const uint32_t T1H_NS = timing_.t1_ns + timing_.t2_ns;

    const uint32_t T0H_CLOCKS = (T0H_NS + CLOCK_PERIOD_NS / 2) / CLOCK_PERIOD_NS;
    const uint32_t T1H_CLOCKS = (T1H_NS + CLOCK_PERIOD_NS / 2) / CLOCK_PERIOD_NS;

    // Lambda: Generate 4-bit pattern for a single LED bit
    auto generate_bit_pattern = [T0H_CLOCKS, T1H_CLOCKS](uint8_t bit_value) -> uint16_t {
        uint16_t pattern = 0;
        uint32_t high_clocks = bit_value ? T1H_CLOCKS : T0H_CLOCKS;

        // Set bits to 1 for high duration
        for (uint32_t i = 0; i < high_clocks && i < CLOCKS_PER_BIT; ++i) {
            pattern |= (1 << i);
        }

        return pattern;
    };

    // Lambda: Generate 16-bit pattern for a 4-bit nibble
    auto generate_nibble_pattern = [&generate_bit_pattern](uint8_t nibble) -> uint16_t {
        uint16_t pattern = 0;

        // Process each of 4 LED bits in the nibble (MSB-first)
        for (int bit_pos = 3; bit_pos >= 0; --bit_pos) {
            uint8_t led_bit = (nibble >> bit_pos) & 1;
            uint16_t bit_pattern = generate_bit_pattern(led_bit);

            // Shift and combine (LSB of pattern is first in time)
            pattern = (pattern << CLOCKS_PER_BIT) | bit_pattern;
        }

        return pattern;
    };

    // Generate all 16 nibble patterns
    for (uint8_t i = 0; i < 16; ++i) {
        bitpatterns_[i] = generate_nibble_pattern(i);
    }
}

bool ParlioChannel::begin(const ParlioChannelConfig& config, uint16_t num_leds) {
    PARLIO_DLOG("begin() called - data_width=" << int(data_width_) << ", num_leds=" << num_leds);

    // Check if reconfiguration needed
    if (is_initialized()) {
        if (!needs_reconfiguration(config, num_leds)) {
            PARLIO_DLOG("Configuration unchanged, skipping reinitialization");
            return true;  // Already configured correctly
        }
        FL_LOG_PARLIO("Reconfiguring PARLIO driver...");
        end();  // Clean teardown before reconfiguration
    }

    if (config.num_lanes != data_width_) {
        FL_LOG_PARLIO("Configuration error - num_lanes (" << int(config.num_lanes)
                << ") does not match data_width (" << int(data_width_) << ")");
        return false;
    }

    config_ = config;
    num_leds_ = num_leds;

    // Set clock frequency from chipset timing if not specified
    if (config_.clock_freq_hz == 0) {
        // Use chipset-derived clock frequency from timing configuration
        // Base frequency is T1 + T2 + T3 (total bit period) → clock = 1 / (total_period_ns / 4)
        uint32_t total_period_ns = timing_.t1_ns + timing_.t2_ns + timing_.t3_ns;
        uint32_t base_freq = (1000000000UL * 4) / total_period_ns;

        // Apply auto-clock adjustment if enabled
        if (config_.auto_clock_adjustment) {
            uint32_t adjusted_freq = detail::calculate_optimal_clock_freq(num_leds, base_freq);
            if (detail::validate_adjusted_freq(adjusted_freq, base_freq)) {
                config_.clock_freq_hz = adjusted_freq;
                PARLIO_DLOG("Auto-adjusted clock frequency: " << adjusted_freq << " Hz "
                           << "(base: " << base_freq << " Hz, " << num_leds << " LEDs, "
                           << "scaling: " << (adjusted_freq * 100 / base_freq) << "%)");
            } else {
                config_.clock_freq_hz = base_freq;
                FL_LOG_PARLIO("Clock adjustment out of range, using base frequency: " << base_freq << " Hz");
            }
        } else {
            config_.clock_freq_hz = base_freq;
            PARLIO_DLOG("Using base clock frequency: " << base_freq << " Hz");
        }
        PARLIO_DLOG("Chipset timing: T1=" << timing_.t1_ns << "ns, T2=" << timing_.t2_ns << "ns, T3=" << timing_.t3_ns << "ns");
    } else {
        PARLIO_DLOG("Using configured clock frequency: " << config_.clock_freq_hz << " Hz (manual override)");
    }

    // Calculate expanded buffer size for waveform encoding
    // RGB: 3 components × 32 bits = 96 bits per LED
    // RGBW: 4 components × 32 bits = 128 bits per LED
    const uint32_t COMPONENTS_PER_LED = config_.is_rgbw ? 4 : 3;
    const uint32_t SYMBOLS_PER_LED = COMPONENTS_PER_LED * 32;  // Each component = 32-bit waveform
    const uint32_t BITS_PER_LED = SYMBOLS_PER_LED * data_width_;
    buffer_size_ = (num_leds * BITS_PER_LED + 7) / 8;  // Convert to bytes
    PARLIO_DLOG("Calculated buffer_size: " << buffer_size_ << " bytes "
               << "(mode: " << (config_.is_rgbw ? "RGBW" : "RGB") << ", "
               << "components: " << COMPONENTS_PER_LED << ", "
               << "SYMBOLS_PER_LED=" << SYMBOLS_PER_LED << ", BITS_PER_LED=" << BITS_PER_LED << ")");

    // Allocate TWO DMA buffers for ping-pong operation (double buffering)
    for (int i = 0; i < 2; i++) {
        dma_buffer_[i] = (uint8_t*)heap_caps_malloc(buffer_size_, MALLOC_CAP_DMA);
        if (!dma_buffer_[i]) {
            FL_LOG_PARLIO("Failed to allocate DMA buffer " << i << " (" << buffer_size_ << " bytes)");
            // Clean up previously allocated buffers
            for (int j = 0; j < i; j++) {
                heap_caps_free(dma_buffer_[j]);
                dma_buffer_[j] = nullptr;
            }
            return false;
        }
        fl::memset(dma_buffer_[i], 0, buffer_size_);
        PARLIO_DLOG("DMA buffer " << i << " allocated at " << (void*)dma_buffer_[i]);
    }
    current_buffer_idx_ = 0;  // Start with buffer 0

    // Create semaphore for transfer completion
    xfer_done_sem_ = xSemaphoreCreateBinary();
    if (!xfer_done_sem_) {
        FL_LOG_PARLIO("Failed to create semaphore");
        for (int i = 0; i < 2; i++) {
            heap_caps_free(dma_buffer_[i]);
            dma_buffer_[i] = nullptr;
        }
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
    parlio_config.data_width = data_width_;
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

    PARLIO_DLOG("  data_width: " << int(data_width_));
    PARLIO_DLOG("  output_clk_freq_hz: " << config_.clock_freq_hz);
    PARLIO_DLOG("  max_transfer_size: " << parlio_config.max_transfer_size);
    PARLIO_DLOG("  clk_out_gpio: -1 (internal clock)");

    // Copy GPIO numbers
    for (int i = 0; i < data_width_; i++) {
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
        for (int i = 0; i < 2; i++) {
            heap_caps_free(dma_buffer_[i]);
            dma_buffer_[i] = nullptr;
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
        FL_LOG_PARLIO("parlio_tx_unit_enable() failed with error: " << detail::parlio_err_to_str(err) << " (" << int(err) << ")");

        parlio_del_tx_unit(tx_unit_);
        vSemaphoreDelete(xfer_done_sem_);
        for (int i = 0; i < 2; i++) {
            heap_caps_free(dma_buffer_[i]);
            dma_buffer_[i] = nullptr;
        }
        tx_unit_ = nullptr;
        xfer_done_sem_ = nullptr;
        return false;
    }

    // Store configuration for reconfiguration detection
    last_config_ = config_;
    last_num_leds_ = num_leds;

    // Log GPIO configuration summary
    detail::log_gpio_configuration(data_width_, config_, config_.clock_freq_hz);

    PARLIO_DLOG("PARLIO driver initialization successful!");
    PARLIO_DLOG("  DMA buffer:            " << buffer_size_ << " bytes × 2 (double buffered)");
    PARLIO_DLOG("  Buffer 0 address:      " << (void*)dma_buffer_[0]);
    PARLIO_DLOG("  Buffer 1 address:      " << (void*)dma_buffer_[1]);
    PARLIO_DLOG("  Transfer queue depth:  " << int(parlio_config.trans_queue_depth));
    PARLIO_DLOG("  DMA burst size:        " << int(parlio_config.dma_burst_size));
    PARLIO_DLOG("  Max transfer size:     " << parlio_config.max_transfer_size);

    return true;
}

void ParlioChannel::end() {
    if (!is_initialized()) {
        return;  // Already ended, idempotent
    }

    PARLIO_DLOG("end() called - cleaning up resources");

    if (tx_unit_) {
        parlio_tx_unit_disable(tx_unit_);
        parlio_del_tx_unit(tx_unit_);
        tx_unit_ = nullptr;
    }

    // Free both DMA buffers
    for (int i = 0; i < 2; i++) {
        if (dma_buffer_[i]) {
            heap_caps_free(dma_buffer_[i]);
            dma_buffer_[i] = nullptr;
        }
    }

    if (xfer_done_sem_) {
        vSemaphoreDelete(xfer_done_sem_);
        xfer_done_sem_ = nullptr;
    }

    // Clear state tracking
    fl::memset(&last_config_, 0, sizeof(last_config_));
    last_num_leds_ = 0;
    num_leds_ = 0;
}

void ParlioChannel::set_strip(uint8_t channel, CRGB* leds) {
    if (channel < data_width_) {
        strips_[channel] = leds;
        PARLIO_DLOG("set_strip() - channel " << int(channel) << " registered at " << (void*)leds);
    } else {
        FL_LOG_PARLIO("set_strip() - invalid channel " << int(channel) << " (data_width=" << int(data_width_) << ")");
    }
}

void ParlioChannel::show() {
    // Acquire hardware resource (blocks if another driver is using PARLIO)
    auto& resource_mgr = IParlioEngine::getInstance();
    resource_mgr.acquire(this);
    PARLIO_DLOG("Acquired PARLIO hardware resource");

    PARLIO_DLOG("show() called");

    if (!tx_unit_) {
        FL_LOG_PARLIO("show() called but tx_unit_ not initialized");
        resource_mgr.release(this);
        return;
    }

    // Verify buffers are allocated
    if (!dma_buffer_[0] || !dma_buffer_[1]) {
        FL_LOG_PARLIO("show() called but DMA buffers not allocated");
        resource_mgr.release(this);
        return;
    }

    // Wait for previous transfer to complete
    PARLIO_DLOG("Waiting for previous transfer to complete...");
    xSemaphoreTake(xfer_done_sem_, portMAX_DELAY);

    // Swap to next buffer for packing (double buffering)
    uint8_t pack_buffer_idx = 1 - current_buffer_idx_;
    uint8_t* pack_buffer = dma_buffer_[pack_buffer_idx];

    // Pack LED data into the next buffer
    PARLIO_DLOG("Packing LED data into buffer " << int(pack_buffer_idx) << "...");
    pack_data(pack_buffer);

    // Swap to this buffer for transmission
    current_buffer_idx_ = pack_buffer_idx;
    dma_busy_ = true;

    // Get buffer to transmit
    const uint8_t* tx_buffer = dma_buffer_[current_buffer_idx_];

    // Configure transmission
    parlio_transmit_config_t tx_config = {};
    tx_config.idle_value = 0x00000000;  // Lines idle low between frames
    tx_config.flags.queue_nonblocking = 0;

    // Calculate component-level chunking parameters (WLED-MM-P4 strategy)
    // Each color component = 32 bits × data_width = one transmission unit
    const uint32_t BITS_PER_COMPONENT = 32 * data_width_;
    const uint32_t BYTES_PER_COMPONENT = (BITS_PER_COMPONENT + 7) / 8;
    const uint32_t COMPONENTS_PER_LED = config_.is_rgbw ? 4 : 3;  // G, R, B, W (if RGBW)
    constexpr uint32_t MAX_BYTES_PER_CHUNK = 65535;  // PARLIO hardware limit

    // Calculate max LEDs per chunk (must break at LED boundaries)
    // Each LED = COMPONENTS_PER_LED components × BYTES_PER_COMPONENT bytes
    const uint32_t BYTES_PER_LED = COMPONENTS_PER_LED * BYTES_PER_COMPONENT;
    const uint16_t MAX_LEDS_PER_CHUNK = MAX_BYTES_PER_CHUNK / BYTES_PER_LED;

    // Transmit using WLED-MM-P4 buffer breaking strategy:
    // Break at LED boundaries, with each LED's components transmitted together
    uint16_t leds_remaining = num_leds_;
    const uint8_t* chunk_ptr = tx_buffer;
    uint16_t led_offset = 0;

    PARLIO_DLOG("Transmitting with WLED-MM-P4 buffer breaking strategy");
    PARLIO_DLOG("  Mode: " << (config_.is_rgbw ? "RGBW" : "RGB"));
    PARLIO_DLOG("  BYTES_PER_COMPONENT=" << BYTES_PER_COMPONENT
               << ", COMPONENTS_PER_LED=" << COMPONENTS_PER_LED
               << ", BYTES_PER_LED=" << BYTES_PER_LED
               << ", MAX_LEDS_PER_CHUNK=" << MAX_LEDS_PER_CHUNK);

    while (leds_remaining > 0) {
        // Determine how many LEDs to transmit in this chunk
        uint16_t leds_in_chunk = (leds_remaining < MAX_LEDS_PER_CHUNK)
                                  ? leds_remaining : MAX_LEDS_PER_CHUNK;

        // Calculate chunk parameters
        size_t chunk_bytes = leds_in_chunk * BYTES_PER_LED;
        size_t chunk_bits = leds_in_chunk * COMPONENTS_PER_LED * BITS_PER_COMPONENT;

        PARLIO_DLOG("  LED chunk [" << led_offset << ".." << (led_offset + leds_in_chunk - 1) << "]: "
                   << leds_in_chunk << " LEDs, " << chunk_bytes << " bytes, " << chunk_bits << " bits");

        // Transmit this LED chunk (contains all components for leds_in_chunk LEDs)
        esp_err_t err = parlio_tx_unit_transmit(tx_unit_, chunk_ptr, chunk_bits, &tx_config);

        if (err != ESP_OK) {
            FL_LOG_PARLIO("parlio_tx_unit_transmit() failed for LED chunk at offset " << led_offset
                          << ": " << detail::parlio_err_to_str(err) << " (" << int(err) << ")");
            dma_busy_ = false;
            xSemaphoreGive(xfer_done_sem_);
            resource_mgr.release(this);
            return;
        }

        // Advance pointers
        chunk_ptr += chunk_bytes;
        leds_remaining -= leds_in_chunk;
        led_offset += leds_in_chunk;

        // Wait for completion if not the last chunk
        // This ensures DMA gaps occur at LED boundaries (after LSB of last component)
        if (leds_remaining > 0) {
            xSemaphoreTake(xfer_done_sem_, portMAX_DELAY);
        }
    }

    // Release hardware resource for next driver (async DMA continues)
    resource_mgr.release(this);
    // Last callback will give semaphore when done
    PARLIO_DLOG("show() completed - transmission started from buffer " << int(current_buffer_idx_));
}

void ParlioChannel::wait() {
    if (xfer_done_sem_) {
        xSemaphoreTake(xfer_done_sem_, portMAX_DELAY);
        xSemaphoreGive(xfer_done_sem_);
    }
}

bool ParlioChannel::is_initialized() const {
    return tx_unit_ != nullptr;
}

bool ParlioChannel::needs_reconfiguration(const ParlioChannelConfig& new_config, uint16_t new_num_leds) const {
    if (!is_initialized()) return true;  // Not initialized yet

    bool changed = false;

    // Check LED count
    if (last_num_leds_ != new_num_leds) {
        FL_LOG_PARLIO("LED count changed: " << last_num_leds_ << " → " << new_num_leds);
        changed = true;
    }

    // Check RGBW flag
    if (last_config_.is_rgbw != new_config.is_rgbw) {
        FL_LOG_PARLIO("LED mode changed: "
                     << (last_config_.is_rgbw ? "RGBW" : "RGB") << " → "
                     << (new_config.is_rgbw ? "RGBW" : "RGB"));
        changed = true;
    }

    // Check lane count
    if (last_config_.num_lanes != new_config.num_lanes) {
        FL_LOG_PARLIO("Lane count changed: " << int(last_config_.num_lanes)
                     << " → " << int(new_config.num_lanes));
        changed = true;
    }

    // Check GPIO pins (only active lanes)
    for (int i = 0; i < new_config.num_lanes; i++) {
        if (last_config_.data_gpios[i] != new_config.data_gpios[i]) {
            FL_LOG_PARLIO("Lane " << i << " GPIO changed: "
                         << last_config_.data_gpios[i] << " → "
                         << new_config.data_gpios[i]);
            changed = true;
        }
    }

    // Check clock frequency
    if (last_config_.clock_freq_hz != new_config.clock_freq_hz) {
        FL_LOG_PARLIO("Clock frequency changed: "
                     << last_config_.clock_freq_hz << " Hz → "
                     << new_config.clock_freq_hz << " Hz");
        changed = true;
    }

    // Check auto-clock adjustment flag
    if (last_config_.auto_clock_adjustment != new_config.auto_clock_adjustment) {
        FL_LOG_PARLIO("Auto-clock adjustment changed: "
                     << (last_config_.auto_clock_adjustment ? "Enabled" : "Disabled") << " → "
                     << (new_config.auto_clock_adjustment ? "Enabled" : "Disabled"));
        changed = true;
    }

    return changed;
}

void ParlioChannel::print_status() const {
    if (!is_initialized()) {
        FL_LOG_PARLIO("PARLIO driver is NOT initialized");
        return;
    }

    detail::log_gpio_configuration(data_width_, config_, config_.clock_freq_hz);

    FL_LOG_PARLIO("╔═══════════════════════════════════════════════╗");
    FL_LOG_PARLIO("║   Runtime Status                              ║");
    FL_LOG_PARLIO("╠═══════════════════════════════════════════════╣");
    FL_LOG_PARLIO("  Initialized:       Yes");
    FL_LOG_PARLIO("  LED count:         " << num_leds_);
    FL_LOG_PARLIO("  DMA busy:          " << (dma_busy_ ? "Yes" : "No"));
    FL_LOG_PARLIO("  Current buffer:    " << int(current_buffer_idx_));
    FL_LOG_PARLIO("  Buffer size:       " << buffer_size_ << " bytes (each)");

    // Show active strips
    FL_LOG_PARLIO("  Active strips:");
    for (uint8_t i = 0; i < data_width_; i++) {
        if (strips_[i]) {
            FL_LOG_PARLIO("    Lane " << int(i) << ":        " << (void*)strips_[i] << " [SET]");
        } else {
            FL_LOG_PARLIO("    Lane " << int(i) << ":        nullptr [NOT SET]");
        }
    }

    FL_LOG_PARLIO("╚═══════════════════════════════════════════════╝");
}

void ParlioChannel::pack_data(uint8_t* output_buffer) {
    // Runtime dispatch on data_width_
    uint32_t bytes_per_component = 0;
    switch (data_width_) {
        case 1: bytes_per_component = 4; break;
        case 2: bytes_per_component = 8; break;
        case 4: bytes_per_component = 16; break;
        case 8: bytes_per_component = 32; break;
        case 16: bytes_per_component = 64; break;
        default:
            FL_LOG_PARLIO("Invalid data_width: " << int(data_width_));
            return;
    }

    const uint8_t num_components = config_.is_rgbw ? 4 : 3;

    // Always show basic pack_data info (not just debug mode)
    FASTLED_DBG("PARLIO pack_data:");
    FASTLED_DBG("  data_width: " << int(data_width_));
    FASTLED_DBG("  num_leds: " << num_leds_);
    FASTLED_DBG("  buffer_size: " << buffer_size_);
    FASTLED_DBG("  bytes_per_component: " << bytes_per_component);
    FASTLED_DBG("  mode: " << (config_.is_rgbw ? "RGBW" : "RGB"));

    PARLIO_DLOG("pack_data() - Packing " << num_leds_ << " LEDs across " << int(data_width_)
               << " channels (" << (config_.is_rgbw ? "RGBW" : "RGB") << " mode)");
    uint8_t* out_ptr = output_buffer;

    // Process each LED
    for (uint16_t led = 0; led < num_leds_; led++) {
        // Process each color component (G, R, B, W in order)
        // G=1, R=0, B=2, W=3 in CRGB (W is always last for RGBW)
        const uint8_t component_order[4] = {1, 0, 2, 3};

        for (uint8_t comp_idx = 0; comp_idx < num_components; comp_idx++) {
            const uint8_t component = component_order[comp_idx];

            // Initialize 32 time-slices (each holds bits for all strips)
            uint32_t transposed_slices[32] = {0};

            // Generate and transpose waveforms for all active strips
            for (uint8_t channel = 0; channel < data_width_; channel++) {
                if (!strips_[channel]) continue;

                // Get color component value from strip buffer
                const uint8_t* led_bytes = reinterpret_cast<const uint8_t*>(&strips_[channel][led]);

                // For RGBW, white component (index 3) needs special handling
                // CRGB only has r, g, b, so white would be at offset 3 if we had CRGBW
                // For now, white defaults to 0 (users need to provide CRGBW data)
                const uint8_t component_value = (component < 3) ? led_bytes[component] : 0;

                // Generate 32-bit waveform for this component value using runtime bitpatterns
                const uint32_t waveform = detail::generate_waveform(component_value, bitpatterns_);
                const uint32_t pin_bit = (1u << channel);

                // Transpose: Extract each of 32 bits from waveform
                // and distribute to corresponding time-slices
                for (int slice = 0; slice < 32; slice++) {
                    if ((waveform >> slice) & 1) {
                        transposed_slices[slice] |= pin_bit;
                    }
                }
            }

            // Pack transposed slices based on data_width (runtime dispatch)
            switch (data_width_) {
                case 1:
                    detail::process_1bit(out_ptr, transposed_slices);
                    break;
                case 2:
                    detail::process_2bit(out_ptr, transposed_slices);
                    break;
                case 4:
                    detail::process_4bit(out_ptr, transposed_slices);
                    break;
                case 8:
                    detail::process_8bit(out_ptr, transposed_slices);
                    break;
                case 16:
                    detail::process_16bit(out_ptr, transposed_slices);
                    break;
            }

            out_ptr += bytes_per_component;
        }
    }
    PARLIO_DLOG("pack_data() completed");
}

bool IRAM_ATTR ParlioChannel::parlio_tx_done_callback(
    parlio_tx_unit_handle_t tx_unit,
    const parlio_tx_done_event_data_t* edata,
    void* user_ctx)
{
    ParlioChannel* driver = static_cast<ParlioChannel*>(user_ctx);
    BaseType_t high_priority_task_awoken = pdFALSE;

    // Note: We can't use PARLIO_DLOG in ISR context - it uses FASTLED_DBG which may allocate
    driver->dma_busy_ = false;
    xSemaphoreGiveFromISR(driver->xfer_done_sem_, &high_priority_task_awoken);

    return high_priority_task_awoken == pdTRUE;
}

// ============================================================================
// Factory Function
// ============================================================================

fl::unique_ptr<IParlioChannel> IParlioChannel::create(const ChipsetTimingConfig& timing, uint8_t data_width) {
    return fl::unique_ptr<IParlioChannel>(new ParlioChannel(timing, data_width));
}

}  // namespace fl

#endif // CONFIG_IDF_TARGET_ESP32P4
#endif // ESP32
