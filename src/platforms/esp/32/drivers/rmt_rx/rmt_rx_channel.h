#pragma once

#include "fl/compiler_control.h"
#ifdef ESP32

#include "platforms/esp/32/feature_flags/enabled.h"

#if FASTLED_RMT5

#include "ftl/stdint.h"

FL_EXTERN_C_BEGIN
#include "driver/rmt_rx.h"
#include "driver/rmt_common.h"
#include "driver/gpio.h"
#include "esp_attr.h"
FL_EXTERN_C_END

namespace fl {
namespace esp32 {

/**
 * @brief RMT RX Channel Wrapper
 *
 * Provides RAII-based management of RMT receive channels.
 * Follows FastLED conventions:
 * - Uses fl:: namespace
 * - IRAM_ATTR for ISR callbacks
 * - FL_DBG for debug logging
 * - Boolean return values for error handling
 *
 * Example usage:
 * @code
 * RmtRxChannel rx(GPIO_NUM_6, 40000000);  // 40MHz resolution
 * if (!rx.begin()) {
 *     FL_WARN("RX channel init failed");
 *     return;
 * }
 *
 * rmt_symbol_word_t symbols[1024];
 * rx.startReceive(symbols, 1024);
 *
 * while (!rx.isReceiveDone()) {
 *     delayMicroseconds(10);
 * }
 *
 * size_t count = rx.getReceivedSymbols();
 * FL_DBG("Received " << count << " symbols");
 * @endcode
 */
class RmtRxChannel {
public:
    /**
     * @brief Construct RX channel (does not initialize hardware)
     * @param pin GPIO pin for receiving signals
     * @param resolution_hz RMT clock resolution (default 40MHz for 25ns ticks)
     */
    RmtRxChannel(gpio_num_t pin, uint32_t resolution_hz = 40000000);

    /**
     * @brief Destructor - releases RMT channel
     */
    ~RmtRxChannel();

    /**
     * @brief Initialize RMT RX channel
     * @return true on success, false on failure
     *
     * Creates RMT RX channel and registers ISR callback.
     * Must be called before startReceive().
     */
    bool begin();

    /**
     * @brief Start receiving RMT symbols
     * @param buffer Buffer to store received symbols (must remain valid until done)
     * @param buffer_size Number of symbols buffer can hold
     * @return true if receive started, false on error
     *
     * This is a non-blocking call. Use isReceiveDone() to poll for completion,
     * or rely on the ISR callback to set receive_done_ flag.
     *
     * Buffer must be large enough for expected transmission:
     * - WS2812: 24 symbols per LED (8 bits per color × 3 colors)
     * - For 100 LEDs: 2400 symbols minimum
     * - Recommended: Add 20% margin for reset pulses and overhead
     */
    bool startReceive(rmt_symbol_word_t* buffer, size_t buffer_size);

    /**
     * @brief Check if receive operation is complete
     * @return true if receive finished, false if still in progress
     */
    bool isReceiveDone() const { return receive_done_; }

    /**
     * @brief Get number of symbols received
     * @return Symbol count (valid after isReceiveDone() returns true)
     */
    size_t getReceivedSymbols() const { return symbols_received_; }

    /**
     * @brief Reset receive state (call before starting new receive)
     */
    void reset();

    /**
     * @brief Re-enable RMT RX channel after receive completion
     * @return true on success, false on failure
     *
     * After a receive operation completes (or times out), the channel
     * is automatically disabled by the ESP-IDF RMT driver. This method
     * re-enables it for the next receive operation.
     */
    bool enable();

private:
    /**
     * @brief ISR callback for receive completion
     * @param channel RMT channel handle
     * @param data Event data (contains received symbols)
     * @param user_data Pointer to RmtRxChannel instance
     * @return true if higher-priority task awakened
     */
    static bool IRAM_ATTR rxDoneCallback(rmt_channel_handle_t channel,
                                         const rmt_rx_done_event_data_t* data,
                                         void* user_data);

    rmt_channel_handle_t channel_;      ///< RMT channel handle
    gpio_num_t pin_;                    ///< GPIO pin for RX
    uint32_t resolution_hz_;            ///< Clock resolution in Hz
    volatile bool receive_done_;        ///< Set by ISR when receive complete
    volatile size_t symbols_received_;  ///< Number of symbols received (set by ISR)
};

} // namespace esp32
} // namespace fl

#endif // FASTLED_RMT5

#endif // ESP32
