#pragma once

#include "fl/compiler_control.h"
#ifdef ESP32

#include "third_party/espressif/led_strip/src/enabled.h"

#if FASTLED_RMT5

#include "ftl/stdint.h"
#include "common.h"

FL_EXTERN_C_BEGIN
#include "driver/rmt_encoder.h"
#include "driver/rmt_tx.h"
FL_EXTERN_C_END

namespace fl {

// Forward declarations
struct ChipsetTiming;

/**
 * FastLED RMT Encoder - Converts pixel bytes to RMT symbols
 *
 * Architecture:
 * - Uses ESP-IDF's official encoder pattern (rmt_encoder_t)
 * - Combines bytes_encoder (for pixel data) + copy_encoder (for reset pulse)
 * - State machine handles partial encoding when buffer fills
 * - Runs in ISR context - must be fast and ISR-safe
 *
 * Implementation based on ESP-IDF led_strip example:
 * https://github.com/espressif/esp-idf/tree/master/examples/peripherals/rmt/led_strip
 */
class FastLEDRmtEncoder {
public:
    FastLEDRmtEncoder();
    ~FastLEDRmtEncoder();

    /**
     * Initialize encoder with timing configuration
     * @param timing Chipset timing (T1/T2/T3/RESET in nanoseconds)
     * @param resolution_hz RMT clock resolution (typically 40MHz)
     * @return ESP_OK on success, error code on failure
     */
    esp_err_t initialize(const ChipsetTiming& timing, uint32_t resolution_hz);

    /**
     * Get the underlying encoder handle for RMT transmission
     */
    rmt_encoder_handle_t getHandle() const { return mEncoder; }

    /**
     * Check if encoder is initialized
     */
    bool isInitialized() const { return mEncoder != nullptr; }

    // Friend declaration for factory function
    friend esp_err_t fastled_rmt_new_encoder(const ChipsetTiming& timing,
                                              uint32_t resolution_hz,
                                              rmt_encoder_handle_t *ret_encoder);

private:
    // Main encoder handle (composite of bytes + copy encoders)
    rmt_encoder_handle_t mEncoder;

    // Sub-encoders
    rmt_encoder_handle_t mBytesEncoder;  // Converts bytes to RMT pulses
    rmt_encoder_handle_t mCopyEncoder;   // Copies reset pulse

    // Encoder state machine counter (0 = data, 1 = reset)
    int mState;

    // Reset pulse symbol (low signal for RESET microseconds)
    rmt_symbol_word_t mResetCode;

    // Timing configuration (stored for debugging)
    uint32_t mBit0HighTicks;
    uint32_t mBit0LowTicks;
    uint32_t mBit1HighTicks;
    uint32_t mBit1LowTicks;
    uint32_t mResetTicks;

    // Static callbacks for rmt_encoder_t interface
    static size_t encodeCallback(rmt_encoder_t *encoder,
                                 rmt_channel_handle_t channel,
                                 const void *primary_data, size_t data_size,
                                 rmt_encode_state_t *ret_state);

    static esp_err_t resetCallback(rmt_encoder_t *encoder);
    static esp_err_t delCallback(rmt_encoder_t *encoder);

    // Instance method called by static callback
    size_t encode(rmt_channel_handle_t channel,
                  const void *primary_data, size_t data_size,
                  rmt_encode_state_t *ret_state);

    // Helper: reset encoder state
    esp_err_t reset();

    // Helper: delete encoder and free resources
    esp_err_t del();
};

/**
 * Factory function to create FastLED RMT encoder
 * @param timing Chipset timing configuration
 * @param resolution_hz RMT clock resolution
 * @param[out] ret_encoder Output parameter for encoder handle
 * @return ESP_OK on success, error code on failure
 */
esp_err_t fastled_rmt_new_encoder(const ChipsetTiming& timing,
                                   uint32_t resolution_hz,
                                   rmt_encoder_handle_t *ret_encoder);

} // namespace fl

#endif // FASTLED_RMT5

#endif // ESP32
