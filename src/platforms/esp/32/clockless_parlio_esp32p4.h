/// @file clockless_parlio_esp32p4.h
/// @brief ESP32-P4 Parallel IO (PARLIO) LED driver for simultaneous multi-strip output
///
/// This driver uses the ESP32-P4 PARLIO TX peripheral to drive up to 8 or 16
/// identical WS28xx-style LED strips in parallel with DMA-based hardware timing.
///
/// Supported platforms:
/// - ESP32-P4: PARLIO TX peripheral (requires driver/parlio_tx.h)
///
/// Key features:
/// - Simultaneous output to 8 or 16 LED strips
/// - DMA-based transmission (minimal CPU overhead)
/// - Hardware timing control (no CPU bit-banging)
/// - High performance (+ FPS for 256-pixel strips)
/// - Template-parameterized for 8 or 16 channels

#pragma once

#if !defined(CONFIG_IDF_TARGET_ESP32P4)
#error "This file is only for ESP32-P4"
#endif

#include "sdkconfig.h"

#include <stdint.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "esp_heap_caps.h"
#include "driver/parlio_tx.h"
#include "driver/gpio.h"
#include "platforms/esp/esp_version.h"

#include "crgb.h"
#include "cpixel_ledcontroller.h"
#include "eorder.h"
#include "pixel_iterator.h"
#include "platforms/shared/clockless_timing.h"
#include "fl/namespace.h"

namespace fl {

/// @brief Configuration structure for PARLIO LED driver
struct ParlioDriverConfig {
    int clk_gpio;                ///< GPIO number for clock output
    int data_gpios[16];          ///< GPIO numbers for data lanes (up to 16)
    int num_lanes;               ///< Active lane count (8 or 16)
    uint32_t clock_freq_hz;      ///< PARLIO clock frequency (e.g., 12000000 for 12MHz)
};

/// @brief Abstract base for PARLIO driver (enables runtime polymorphism)
class ParlioLedDriverBase {
public:
    virtual ~ParlioLedDriverBase() = default;
    virtual bool begin(const ParlioDriverConfig& config, uint16_t num_leds) = 0;
    virtual void end() = 0;
    virtual void set_strip(uint8_t channel, CRGB* leds) = 0;
    virtual void show_grb() = 0;
    virtual void show_rgb() = 0;
    virtual void show_bgr() = 0;
    virtual void wait() = 0;
    virtual bool is_initialized() const = 0;
};

/// @brief PARLIO parallel LED driver with template-based configuration
///
/// This driver uses the ESP32-P4's Parallel IO TX peripheral to simultaneously
/// drive multiple LED strips with hardware-timed output and DMA transfers.
///
/// @tparam DATA_WIDTH Number of parallel data lanes (1, 2, 4, 8, or 16)
/// @tparam CHIPSET Chipset timing trait (e.g., WS2812ChipsetTiming)
template <uint8_t DATA_WIDTH, typename CHIPSET>
class ParlioLedDriver : public ParlioLedDriverBase {
public:
    static_assert(DATA_WIDTH >= 1 && DATA_WIDTH <= 16, "DATA_WIDTH must be 1-16");
    static_assert((DATA_WIDTH == 1) || (DATA_WIDTH == 2) || (DATA_WIDTH == 4) ||
                  (DATA_WIDTH == 8) || (DATA_WIDTH == 16),
                  "DATA_WIDTH must be power of 2 (1, 2, 4, 8, or 16)");

    /// @brief Default clock frequency for WS2812 timing
    static constexpr uint32_t DEFAULT_CLOCK_FREQ_HZ = 12000000;  // 12 MHz

    /// @brief Constructor
    ParlioLedDriver()
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

    /// @brief Destructor
    ~ParlioLedDriver() {
        end();
    }

    /// @brief Initialize driver with GPIO pins and LED count
    ///
    /// @param config Driver configuration (pins, lane count, clock frequency)
    /// @param num_leds Number of LEDs per strip
    /// @return true if initialization succeeded
    bool begin(const ParlioDriverConfig& config, uint16_t num_leds) override {
        if (config.num_lanes != DATA_WIDTH) {
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

        // Allocate DMA buffer
        dma_buffer_ = (uint8_t*)heap_caps_malloc(buffer_size_, MALLOC_CAP_DMA);
        if (!dma_buffer_) {
            return false;
        }
        memset(dma_buffer_, 0, buffer_size_);

        // Create semaphore for transfer completion
        xfer_done_sem_ = xSemaphoreCreateBinary();
        if (!xfer_done_sem_) {
            heap_caps_free(dma_buffer_);
            dma_buffer_ = nullptr;
            return false;
        }
        xSemaphoreGive(xfer_done_sem_);

        // Configure PARLIO TX unit
        parlio_tx_unit_config_t parlio_config = {};
        parlio_config.clk_src = PARLIO_CLK_SRC_DEFAULT;
        parlio_config.clk_in_gpio_num = (gpio_num_t)-1;  // Use internal clock
        parlio_config.input_clk_src_freq_hz = 0;  // Not used when clk_in_gpio_num is -1
        parlio_config.output_clk_freq_hz = config_.clock_freq_hz;
        parlio_config.data_width = DATA_WIDTH;
        parlio_config.clk_out_gpio_num = (gpio_num_t)config_.clk_gpio;
        parlio_config.valid_gpio_num = (gpio_num_t)-1;  // No separate valid signal
        parlio_config.trans_queue_depth = 4;
        parlio_config.max_transfer_size = buffer_size_;
        parlio_config.dma_burst_size = 64;  // Standard DMA burst size
        parlio_config.sample_edge = PARLIO_SAMPLE_EDGE_POS;
        parlio_config.bit_pack_order = PARLIO_BIT_PACK_ORDER_MSB;
        parlio_config.flags.clk_gate_en = 0;
        parlio_config.flags.io_loop_back = 0;
        parlio_config.flags.allow_pd = 0;

        // Copy GPIO numbers
        for (int i = 0; i < DATA_WIDTH; i++) {
            parlio_config.data_gpio_nums[i] = (gpio_num_t)config_.data_gpios[i];
        }

        // Create PARLIO TX unit
        esp_err_t err = parlio_new_tx_unit(&parlio_config, &tx_unit_);
        if (err != ESP_OK) {
            vSemaphoreDelete(xfer_done_sem_);
            heap_caps_free(dma_buffer_);
            dma_buffer_ = nullptr;
            xfer_done_sem_ = nullptr;
            return false;
        }

        // Register event callbacks
        parlio_tx_event_callbacks_t cbs = {
            .on_trans_done = parlio_tx_done_callback,
        };
        parlio_tx_unit_register_event_callbacks(tx_unit_, &cbs, this);

        // Enable PARLIO TX unit
        err = parlio_tx_unit_enable(tx_unit_);
        if (err != ESP_OK) {
            parlio_del_tx_unit(tx_unit_);
            vSemaphoreDelete(xfer_done_sem_);
            heap_caps_free(dma_buffer_);
            tx_unit_ = nullptr;
            dma_buffer_ = nullptr;
            xfer_done_sem_ = nullptr;
            return false;
        }

        return true;
    }

    /// @brief Shutdown driver and free resources
    void end() override {
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

    /// @brief Set LED strip data pointer for a specific channel
    ///
    /// @param channel Channel index (0 to DATA_WIDTH-1)
    /// @param leds Pointer to LED data array
    void set_strip(uint8_t channel, CRGB* leds) override {
        if (channel < DATA_WIDTH) {
            strips_[channel] = leds;
        }
    }

    /// @brief Show LED data - transmit to all strips
    ///
    /// @tparam RGB_ORDER Color order (e.g., GRB, RGB, BGR)
    template<EOrder RGB_ORDER = GRB>
    void show() {
        if (!tx_unit_ || !dma_buffer_) {
            return;
        }

        // Wait for previous transfer to complete
        xSemaphoreTake(xfer_done_sem_, portMAX_DELAY);
        dma_busy_ = true;

        // Pack LED data into DMA buffer
        pack_data<RGB_ORDER>();

        // Configure transmission
        parlio_transmit_config_t tx_config = {};
        tx_config.idle_value = 0x00000000;  // Lines idle low between frames
        tx_config.flags.queue_nonblocking = 0;

        // Transmit (non-blocking DMA)
        size_t total_bits = buffer_size_ * 8;
        esp_err_t err = parlio_tx_unit_transmit(tx_unit_, dma_buffer_, total_bits, &tx_config);

        if (err != ESP_OK) {
            dma_busy_ = false;
            xSemaphoreGive(xfer_done_sem_);
        }

        // Callback will give semaphore when done
    }

    /// @brief Wait for current transmission to complete
    void wait() override {
        if (xfer_done_sem_) {
            xSemaphoreTake(xfer_done_sem_, portMAX_DELAY);
            xSemaphoreGive(xfer_done_sem_);
        }
    }

    /// @brief Check if driver is initialized
    bool is_initialized() const override {
        return tx_unit_ != nullptr;
    }

    /// @brief Virtual show methods for base class interface
    void show_grb() override { show<GRB>(); }
    void show_rgb() override { show<RGB>(); }
    void show_bgr() override { show<BGR>(); }

private:
    /// @brief Pack LED data into PARLIO format
    ///
    /// For each LED position and each of 24 color bits (in specified order, MSB-first):
    /// - Collect the same bit position from all DATA_WIDTH strips
    /// - Pack into single output byte
    ///
    /// CRGB data is always stored as RGB in memory (r, g, b fields).
    /// This function outputs in the specified order (e.g., GRB for WS2812).
    ///
    /// @tparam RGB_ORDER Color order for output (GRB for WS2812)
    template<EOrder RGB_ORDER>
    void pack_data() {
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

    /// @brief Map output position to CRGB byte offset
    ///
    /// CRGB is stored in memory as: struct { uint8_t r, g, b; }
    /// So byte offsets are: r=0, g=1, b=2
    ///
    /// @tparam RGB_ORDER Desired output color order
    /// @param output_pos Position in output stream (0, 1, or 2)
    /// @return Byte offset into CRGB structure
    template<EOrder RGB_ORDER>
    static constexpr uint8_t get_crgb_byte_offset(uint8_t output_pos) {
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

    /// @brief PARLIO TX completion callback
    static bool IRAM_ATTR parlio_tx_done_callback(parlio_tx_unit_handle_t tx_unit,
                                                   const parlio_tx_done_event_data_t* edata,
                                                   void* user_ctx) {
        ParlioLedDriver* driver = static_cast<ParlioLedDriver*>(user_ctx);
        BaseType_t high_priority_task_awoken = pdFALSE;

        driver->dma_busy_ = false;
        xSemaphoreGiveFromISR(driver->xfer_done_sem_, &high_priority_task_awoken);

        return high_priority_task_awoken == pdTRUE;
    }

    ParlioDriverConfig config_;     ///< Driver configuration
    uint16_t num_leds_;             ///< Number of LEDs per strip
    CRGB* strips_[16];              ///< Pointers to LED data for each strip
    parlio_tx_unit_handle_t tx_unit_; ///< PARLIO TX unit handle
    uint8_t* dma_buffer_;           ///< DMA buffer for bit-packed data
    size_t buffer_size_;            ///< Size of DMA buffer in bytes
    SemaphoreHandle_t xfer_done_sem_; ///< Semaphore for transfer completion
    volatile bool dma_busy_;        ///< Flag indicating DMA transfer in progress
};

// ===== Proxy Controller System (matches I2S pattern) =====

/// @brief Helper object for PARLIO proxy controllers
class Parlio_Esp32P4 {
public:
    void beginShowLeds(int data_pin, int nleds);
    void showPixels(uint8_t data_pin, PixelIterator& pixel_iterator);
    void endShowLeds();
};

/// @brief Base proxy controller with dynamic pin
template <EOrder RGB_ORDER = RGB>
class ClocklessController_Parlio_Esp32P4_WS2812Base
    : public CPixelLEDController<RGB_ORDER> {
private:
    typedef CPixelLEDController<RGB_ORDER> Base;
    Parlio_Esp32P4 mParlio_Esp32P4;
    int mPin;

public:
    ClocklessController_Parlio_Esp32P4_WS2812Base(int pin) : mPin(pin) {}

    void init() override {}

    uint16_t getMaxRefreshRate() const override { return 800; }

protected:
    void *beginShowLeds(int nleds) override {
        void *data = Base::beginShowLeds(nleds);
        mParlio_Esp32P4.beginShowLeds(mPin, nleds);
        return data;
    }

    void showPixels(PixelController<RGB_ORDER> &pixels) override {
        auto pixel_iterator = pixels.as_iterator(this->getRgbw());
        mParlio_Esp32P4.showPixels(mPin, pixel_iterator);
    }

    void endShowLeds(void *data) override {
        Base::endShowLeds(data);
        mParlio_Esp32P4.endShowLeds();
    }
};

/// @brief Template version with compile-time pin
template <int DATA_PIN, EOrder RGB_ORDER = RGB>
class ClocklessController_Parlio_Esp32P4_WS2812
    : public ClocklessController_Parlio_Esp32P4_WS2812Base<RGB_ORDER> {
private:
    typedef ClocklessController_Parlio_Esp32P4_WS2812Base<RGB_ORDER> Base;

public:
    ClocklessController_Parlio_Esp32P4_WS2812() : Base(DATA_PIN) {}

    void init() override {}

    uint16_t getMaxRefreshRate() const override { return 800; }
};

}  // namespace fl
