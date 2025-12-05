#include "fl/compiler_control.h"
#ifdef ESP32

#include "platforms/esp/32/feature_flags/enabled.h"

#if FASTLED_RMT5

#include "rmt_rx_decoder.h"
#include "fl/dbg.h"
#include "fl/warn.h"

FL_EXTERN_C_BEGIN
#include "driver/rmt_common.h"
FL_EXTERN_C_END

namespace fl {
namespace esp32 {

// ============================================================================
// Constructor
// ============================================================================

RmtRxDecoder::RmtRxDecoder(const ChipsetTimingRx& timing, uint32_t resolution_hz)
    : timing_(timing)
    , resolution_hz_(resolution_hz)
    , error_count_(0)
    , ns_per_tick_(0)
{
    // Calculate nanoseconds per tick for efficient conversion
    // Example: 40MHz = 40,000,000 Hz → 1,000,000,000 / 40,000,000 = 25ns per tick
    ns_per_tick_ = 1000000000UL / resolution_hz_;

    FL_DBG("RmtRxDecoder constructed: resolution=" << resolution_hz_
           << "Hz, ns_per_tick=" << ns_per_tick_);
}

// ============================================================================
// Public Methods
// ============================================================================

bool RmtRxDecoder::decode(const RmtSymbol* symbols,
                          size_t symbol_count,
                          uint8_t* bytes_out,
                          size_t* bytes_decoded)
{
    if (!symbols || !bytes_out || !bytes_decoded) {
        FL_WARN("RmtRxDecoder::decode: null pointer argument");
        return false;
    }

    if (symbol_count == 0) {
        FL_WARN("RmtRxDecoder::decode: symbol_count is zero");
        *bytes_decoded = 0;
        return false;
    }

    // Reset state
    error_count_ = 0;
    *bytes_decoded = 0;

    uint8_t current_byte = 0;
    int bit_index = 0;  // 0-7, MSB first

    FL_DBG("RmtRxDecoder: decoding " << symbol_count << " symbols");

    // Cast RmtSymbol array to rmt_symbol_word_t for access to bitfields
    const auto* rmt_symbols = reinterpret_cast<const rmt_symbol_word_t*>(symbols);

    for (size_t i = 0; i < symbol_count; i++) {
        // Check for reset pulse (frame boundary)
        if (isResetPulse(symbols[i])) {
            FL_DBG("RmtRxDecoder: reset pulse detected at symbol " << i);

            // Flush partial byte if needed
            if (bit_index != 0) {
                FL_WARN("RmtRxDecoder: partial byte at reset (bit_index="
                       << bit_index << "), flushing");
                // Shift remaining bits to MSB position
                current_byte <<= (8 - bit_index);
                bytes_out[*bytes_decoded] = current_byte;
                (*bytes_decoded)++;
            }
            break;  // End of frame
        }

        // Decode symbol to bit
        int bit = decodeBit(symbols[i]);
        if (bit < 0) {
            error_count_++;
            FL_DBG("RmtRxDecoder: invalid symbol at index " << i
                  << " (duration0=" << rmt_symbols[i].duration0
                  << ", duration1=" << rmt_symbols[i].duration1 << ")");
            continue;  // Skip invalid symbol
        }

        // Accumulate bit into byte (MSB first)
        current_byte = (current_byte << 1) | static_cast<uint8_t>(bit);
        bit_index++;

        // Byte complete?
        if (bit_index == 8) {
            bytes_out[*bytes_decoded] = current_byte;
            (*bytes_decoded)++;
            current_byte = 0;
            bit_index = 0;
        }
    }

    // Flush partial byte if we reached end of symbols without reset
    if (bit_index != 0) {
        FL_WARN("RmtRxDecoder: partial byte at end (bit_index="
               << bit_index << "), flushing");
        // Shift remaining bits to MSB position
        current_byte <<= (8 - bit_index);
        bytes_out[*bytes_decoded] = current_byte;
        (*bytes_decoded)++;
    }

    FL_DBG("RmtRxDecoder: decoded " << *bytes_decoded << " bytes, "
           << error_count_ << " errors");

    // Decode successful if error rate < 10%
    bool success = error_count_ < (symbol_count / 10);

    if (!success) {
        FL_WARN("RmtRxDecoder: high error rate: " << error_count_
               << "/" << symbol_count << " symbols ("
               << (100 * error_count_ / symbol_count) << "%)");
    }

    return success;
}

bool RmtRxDecoder::isResetPulse(RmtSymbol symbol) const
{
    // Cast RmtSymbol to rmt_symbol_word_t to access bitfields
    const auto& rmt_sym = reinterpret_cast<const rmt_symbol_word_t&>(symbol);

    // Reset pulse characteristics:
    // - Long low duration (>50us)
    // - Either duration0 or duration1 can be the low period

    // Convert durations to nanoseconds
    uint32_t duration0_ns = ticksToNs(rmt_sym.duration0);
    uint32_t duration1_ns = ticksToNs(rmt_sym.duration1);

    // Check if either duration exceeds reset threshold
    uint32_t reset_min_ns = timing_.reset_min_us * 1000;

    // Reset pulse should have level=0 (low) for the long duration
    // Check duration0 with level0=0, or duration1 with level1=0
    if (rmt_sym.level0 == 0 && duration0_ns >= reset_min_ns) {
        return true;
    }
    if (rmt_sym.level1 == 0 && duration1_ns >= reset_min_ns) {
        return true;
    }

    return false;
}

int RmtRxDecoder::decodeBit(RmtSymbol symbol) const
{
    // Cast RmtSymbol to rmt_symbol_word_t to access bitfields
    const auto& rmt_sym = reinterpret_cast<const rmt_symbol_word_t&>(symbol);

    // Convert tick durations to nanoseconds
    uint32_t high_ns = ticksToNs(rmt_sym.duration0);
    uint32_t low_ns = ticksToNs(rmt_sym.duration1);

    // WS2812B protocol: first duration is high, second is low
    // Check if levels match expected pattern (high=1, low=0)
    if (rmt_sym.level0 != 1 || rmt_sym.level1 != 0) {
        // Unexpected level pattern - possibly inverted signal or noise
        return -1;
    }

    // Decision logic: check if timing matches bit 0 pattern
    bool t0h_match = (high_ns >= timing_.t0h_min_ns) && (high_ns <= timing_.t0h_max_ns);
    bool t0l_match = (low_ns >= timing_.t0l_min_ns) && (low_ns <= timing_.t0l_max_ns);

    if (t0h_match && t0l_match) {
        return 0;  // Bit 0
    }

    // Check if timing matches bit 1 pattern
    bool t1h_match = (high_ns >= timing_.t1h_min_ns) && (high_ns <= timing_.t1h_max_ns);
    bool t1l_match = (low_ns >= timing_.t1l_min_ns) && (low_ns <= timing_.t1l_max_ns);

    if (t1h_match && t1l_match) {
        return 1;  // Bit 1
    }

    // Timing doesn't match either pattern
    return -1;  // Invalid
}

void RmtRxDecoder::reset()
{
    error_count_ = 0;
    FL_DBG("RmtRxDecoder: state reset");
}

// ============================================================================
// Private Methods
// ============================================================================

uint32_t RmtRxDecoder::ticksToNs(uint32_t ticks) const
{
    // Convert RMT ticks to nanoseconds
    // Example: 16 ticks × 25 ns/tick = 400ns
    return ticks * ns_per_tick_;
}

} // namespace esp32
} // namespace fl

#endif // FASTLED_RMT5

#endif // ESP32
