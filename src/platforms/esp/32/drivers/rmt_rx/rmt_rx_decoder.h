#pragma once

#include "ftl/shared_ptr.h"
#include "ftl/stdint.h"
#include "platforms/esp/32/drivers/rmt_rx/rmt_rx_channel.h" // For RmtSymbol typedef

namespace fl {
namespace esp32 {

/**
 * @brief RX timing thresholds for chipset detection
 *
 * Defines acceptable timing ranges for decoding RMT symbols back to bits.
 * Uses min/max ranges to tolerate signal jitter and hardware variations.
 *
 * Thresholds should be ±150ns wider than nominal TX timing to account for:
 * - Clock drift between TX and RX
 * - Signal propagation delays
 * - LED capacitance effects
 * - GPIO sampling jitter
 */
struct ChipsetTimingRx {
    // Bit 0 timing thresholds
    uint32_t t0h_min_ns; ///< Bit 0 high time minimum (e.g., 250ns)
    uint32_t t0h_max_ns; ///< Bit 0 high time maximum (e.g., 550ns)
    uint32_t t0l_min_ns; ///< Bit 0 low time minimum (e.g., 700ns)
    uint32_t t0l_max_ns; ///< Bit 0 low time maximum (e.g., 1000ns)

    // Bit 1 timing thresholds
    uint32_t t1h_min_ns; ///< Bit 1 high time minimum (e.g., 650ns)
    uint32_t t1h_max_ns; ///< Bit 1 high time maximum (e.g., 950ns)
    uint32_t t1l_min_ns; ///< Bit 1 low time minimum (e.g., 300ns)
    uint32_t t1l_max_ns; ///< Bit 1 low time maximum (e.g., 600ns)

    // Reset pulse threshold
    uint32_t reset_min_us; ///< Reset pulse minimum duration (e.g., 50us)
};

/**
 * @brief WS2812B RX timing thresholds
 *
 * Based on datasheet specs with ±150ns tolerance:
 * - T0H: 400ns ±150ns → [250ns, 550ns]
 * - T0L: 850ns ±150ns → [700ns, 1000ns]
 * - T1H: 800ns ±150ns → [650ns, 950ns]
 * - T1L: 450ns ±150ns → [300ns, 600ns]
 * - RESET: 280us minimum (WS2812-V5B datasheet)
 */
constexpr ChipsetTimingRx CHIPSET_TIMING_WS2812B_RX = {.t0h_min_ns = 250,
                                                       .t0h_max_ns = 550,
                                                       .t0l_min_ns = 700,
                                                       .t0l_max_ns = 1000,
                                                       .t1h_min_ns = 650,
                                                       .t1h_max_ns = 950,
                                                       .t1l_min_ns = 300,
                                                       .t1l_max_ns = 600,
                                                       .reset_min_us = 280};

/**
 * @brief RMT Symbol to Byte Decoder Interface
 *
 * Converts received RMT symbols back to LED color bytes.
 *
 * Decoding process:
 * 1. For each symbol, extract high time (duration0) and low time (duration1)
 * 2. Convert tick counts to nanoseconds using resolution_hz
 * 3. Compare against timing thresholds to determine bit value (0 or 1)
 * 4. Accumulate 8 bits into a byte (MSB first, matching WS2812 protocol)
 * 5. Detect reset pulses (long low duration) to mark frame boundaries
 *
 * Error handling:
 * - Invalid symbols (timing out of range) increment error_count_
 * - Decoding continues but result may be corrupted
 * - Caller checks getErrorCount() after decode() to detect issues
 *
 * Example usage:
 * @code
 * ChipsetTimingRx timing = CHIPSET_TIMING_WS2812B_RX;
 * auto decoder = RmtRxDecoder::create(timing, 40000000);  // 40MHz resolution
 *
 * uint8_t bytes_out[300];  // 100 LEDs × 3 bytes per LED
 * size_t bytes_decoded = 0;
 *
 * bool ok = decoder->decode(symbols, symbol_count, bytes_out, &bytes_decoded);
 * if (!ok) {
 *     FL_WARN("High decode error rate: " << decoder->getErrorCount());
 * }
 * @endcode
 */
class RmtRxDecoder {
  public:
    /**
     * @brief Create decoder instance
     * @param timing Timing thresholds for bit detection
     * @param resolution_hz RMT clock resolution (must match RX channel)
     * @return Shared pointer to RmtRxDecoder interface
     */
    static fl::shared_ptr<RmtRxDecoder> create(const ChipsetTimingRx &timing,
                                               uint32_t resolution_hz);

    /**
     * @brief Virtual destructor
     */
    virtual ~RmtRxDecoder() = default;

    /**
     * @brief Decode RMT symbols to bytes
     * @param symbols Array of received RMT symbols
     * @param symbol_count Number of symbols in array
     * @param bytes_out Output buffer for decoded bytes (caller allocates)
     * @param bytes_decoded[out] Number of bytes successfully decoded
     * @return true if decode successful, false if too many errors
     *
     * Decoding stops at first reset pulse or end of symbol array.
     * Returns false if error_count exceeds 10% of symbol_count.
     */
    virtual bool decode(const RmtSymbol *symbols, size_t symbol_count,
                        uint8_t *bytes_out, size_t *bytes_decoded) = 0;

    /**
     * @brief Check if symbol is a reset pulse
     * @param symbol RMT symbol to check
     * @return true if symbol is reset pulse (long low duration)
     */
    virtual bool isResetPulse(RmtSymbol symbol) const = 0;

    /**
     * @brief Decode single symbol to bit value
     * @param symbol RMT symbol to decode
     * @return 0 for bit 0, 1 for bit 1, -1 for invalid symbol
     *
     * Checks high time and low time against timing thresholds.
     * Returns -1 if timing doesn't match either bit pattern.
     */
    virtual int decodeBit(RmtSymbol symbol) const = 0;

    /**
     * @brief Get decode error count
     * @return Number of invalid symbols encountered
     */
    virtual size_t getErrorCount() const = 0;

    /**
     * @brief Clear decoder state
     *
     * Clears error count and internal state.
     * Call before starting new decode operation.
     */
    virtual void clear() = 0;

  protected:
    RmtRxDecoder() = default;
};

} // namespace esp32
} // namespace fl
