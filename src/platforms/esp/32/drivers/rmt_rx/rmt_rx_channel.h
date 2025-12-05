#pragma once

#include "ftl/stdint.h"
#include "ftl/shared_ptr.h"

// RMT symbol is a 32-bit value (union with duration0/level0/duration1/level1 bitfields)
// We expose uint32_t in the interface to avoid ESP-IDF header dependencies
// Implementation will cast between uint32_t and rmt_symbol_word_t (static_assert size match)
using RmtSymbol = uint32_t;

namespace fl {
namespace esp32 {

/**
 * @brief RMT RX Channel Interface
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
 * auto rx = RmtRxChannel::create(6, 40000000);  // GPIO 6, 40MHz resolution
 * if (!rx->begin()) {
 *     FL_WARN("RX channel init failed");
 *     return;
 * }
 *
 * rmt_symbol_word_t symbols[1024];
 * rx->startReceive(symbols, 1024);
 *
 * while (!rx->isReceiveDone()) {
 *     delayMicroseconds(10);
 * }
 *
 * size_t count = rx->getReceivedSymbols();
 * FL_DBG("Received " << count << " symbols");
 * @endcode
 */
class RmtRxChannel {
public:
    /**
     * @brief Create RX channel instance (does not initialize hardware)
     * @param pin GPIO pin number for receiving signals
     * @param resolution_hz RMT clock resolution (default 40MHz for 25ns ticks)
     * @return Shared pointer to RmtRxChannel interface
     */
    static fl::shared_ptr<RmtRxChannel> create(int pin, uint32_t resolution_hz = 40000000);

    /**
     * @brief Virtual destructor
     */
    virtual ~RmtRxChannel() = default;

    /**
     * @brief Initialize RMT RX channel
     * @return true on success, false on failure
     *
     * Creates RMT RX channel and registers ISR callback.
     * Must be called before startReceive().
     */
    virtual bool begin() = 0;

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
    virtual bool startReceive(RmtSymbol* buffer, size_t buffer_size) = 0;

    /**
     * @brief Check if receive operation is complete
     * @return true if receive finished, false if still in progress
     */
    virtual bool isReceiveDone() const = 0;

    /**
     * @brief Get number of symbols received
     * @return Symbol count (valid after isReceiveDone() returns true)
     */
    virtual size_t getReceivedSymbols() const = 0;

    /**
     * @brief Reset receive state (call before starting new receive)
     */
    virtual void reset() = 0;

    /**
     * @brief Re-enable RMT RX channel after receive completion
     * @return true on success, false on failure
     *
     * After a receive operation completes (or times out), the channel
     * is automatically disabled by the ESP-IDF RMT driver. This method
     * re-enables it for the next receive operation.
     */
    virtual bool enable() = 0;

protected:
    RmtRxChannel() = default;
};

} // namespace esp32
} // namespace fl
