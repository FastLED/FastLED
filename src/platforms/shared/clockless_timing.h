/// @file clockless_timing.h
/// @brief Platform-agnostic timing calculation for clockless LED protocols
///
/// This module provides compile-time and runtime calculation of optimal clock
/// frequencies and bit patterns for WS28xx-style LED protocols. The algorithms
/// are designed to minimize memory usage while maintaining timing accuracy.

#pragma once

#include "fl/stl/stdint.h"
#include "fl/math_macros.h"
#include "fl/compiler_control.h"

namespace fl {

// ============================================================================
// Timing Calculation
// ============================================================================

/// @brief Result of timing calculation
struct ClocklessTimingResult {
    uint32_t pclk_hz;        ///< Calculated PCLK frequency (Hz)
    uint32_t slot_ns;        ///< Duration of each time slot (nanoseconds)
    uint32_t n_bit;          ///< Number of words (slots) per bit
    uint32_t actual_T1_ns;   ///< Actual T1 timing after quantization (ns)
    uint32_t actual_T2_ns;   ///< Actual T2 timing after quantization (ns)
    uint32_t actual_T3_ns;   ///< Actual T3 timing after quantization (ns)
    float error_T1;          ///< Timing error for T1 (fraction, not percent)
    float error_T2;          ///< Timing error for T1+T2 combined (fraction)
    float error_T3;          ///< Timing error for T3 (fraction)
    bool valid;              ///< True if calculation succeeded
};

/// @brief Clockless LED timing calculator
///
/// This class provides both compile-time (constexpr) and runtime calculation
/// of optimal PCLK frequencies for memory-efficient clockless LED driving.
///
/// Algorithm:
/// 1. Calculate total bit period: Tbit = T1 + T2 + T3
/// 2. For N-word encoding, calculate ideal slot: slot = Tbit / N
/// 3. Convert to PCLK frequency: PCLK = 1,000,000,000 / slot
/// 4. Round to nearest MHz for clean clock division
/// 5. Clamp to supported frequency range
class ClocklessTiming {
public:
    /// @brief Calculate optimal PCLK for N-word-per-bit encoding
    ///
    /// @param T1_ns High time for bit 0 (nanoseconds)
    /// @param T2_ns Additional high time for bit 1 (nanoseconds)
    /// @param T3_ns Low tail duration (nanoseconds)
    /// @param n_words_per_bit Number of words to use per bit (e.g., 3 for memory efficiency)
    /// @param min_pclk_hz Minimum PCLK frequency (Hz)
    /// @param max_pclk_hz Maximum PCLK frequency (Hz)
    /// @param round_to_mhz If true, round result to nearest MHz
    /// @return ClocklessTimingResult with calculated values and errors
    static FL_CONSTEXPR14 ClocklessTimingResult calculate_optimal_pclk(
        uint32_t T1_ns,
        uint32_t T2_ns,
        uint32_t T3_ns,
        uint32_t n_words_per_bit = 3,
        uint32_t min_pclk_hz = 1000000,      // 1 MHz min
        uint32_t max_pclk_hz = 80000000,     // 80 MHz max
        bool round_to_mhz = true
    ) {
        ClocklessTimingResult result = {};

        // Validate inputs
        if (T1_ns == 0 || T2_ns == 0 || T3_ns == 0 || n_words_per_bit == 0) {
            result.valid = false;
            return result;
        }

        // Calculate total bit period
        uint32_t tbit_ns = T1_ns + T2_ns + T3_ns;

        // Calculate ideal slot duration for N-word encoding
        uint32_t ideal_slot_ns = tbit_ns / n_words_per_bit;

        if (ideal_slot_ns == 0) {
            result.valid = false;
            return result;
        }

        // Convert to PCLK frequency
        uint32_t ideal_pclk_hz = 1000000000UL / ideal_slot_ns;

        // Round to nearest MHz if requested
        uint32_t pclk_hz = ideal_pclk_hz;
        if (round_to_mhz) {
            uint32_t rounded_mhz = (ideal_pclk_hz + 500000) / 1000000;
            if (rounded_mhz < (min_pclk_hz / 1000000)) {
                rounded_mhz = (min_pclk_hz + 999999) / 1000000;
            }
            if (rounded_mhz > (max_pclk_hz / 1000000)) {
                rounded_mhz = max_pclk_hz / 1000000;
            }
            pclk_hz = rounded_mhz * 1000000UL;
        } else {
            // Clamp without rounding
            if (pclk_hz < min_pclk_hz) pclk_hz = min_pclk_hz;
            if (pclk_hz > max_pclk_hz) pclk_hz = max_pclk_hz;
        }

        // Calculate actual slot duration
        uint32_t slot_ns = 1000000000UL / pclk_hz;

        // Calculate actual timing values based on 3-word pattern:
        // Bit-0: [HIGH, LOW, LOW]   - 1 slot HIGH, 2 slots LOW
        // Bit-1: [HIGH, HIGH, LOW]  - 2 slots HIGH, 1 slot LOW
        //
        // For WS28xx timing convention:
        // - T1 = minimum HIGH time (for bit 0)
        // - T2 = additional HIGH time for bit 1 (so bit-1 HIGH = T1 + T2)
        // - T3 = LOW tail duration
        uint32_t actual_bit0_high_ns = slot_ns;      // Bit-0: 1 slot HIGH
        uint32_t actual_bit1_high_ns = 2 * slot_ns;  // Bit-1: 2 slots HIGH
        uint32_t actual_low_ns = slot_ns;            // Low tail: 1 slot

        // Map to T1/T2/T3 convention
        uint32_t actual_T1_ns = actual_bit0_high_ns;  // Minimum high (bit 0)
        uint32_t actual_T2_ns = actual_bit1_high_ns - actual_bit0_high_ns;  // Additional high for bit 1
        uint32_t actual_T3_ns = actual_low_ns;        // Low tail

        // Calculate timing errors
        // Error for T1 (bit-0 high time)
        float error_T1 = calculate_error(actual_T1_ns, T1_ns);

        // Error for T1+T2 combined (bit-1 total high time)
        uint32_t target_T1_T2 = T1_ns + T2_ns;
        uint32_t actual_T1_T2 = actual_T1_ns + actual_T2_ns;
        float error_T2 = calculate_error(actual_T1_T2, target_T1_T2);

        // Error for T3 (low tail)
        float error_T3 = calculate_error(actual_T3_ns, T3_ns);

        // Fill result
        result.pclk_hz = pclk_hz;
        result.slot_ns = slot_ns;
        result.n_bit = n_words_per_bit;
        result.actual_T1_ns = actual_T1_ns;
        result.actual_T2_ns = actual_T2_ns;
        result.actual_T3_ns = actual_T3_ns;
        result.error_T1 = error_T1;
        result.error_T2 = error_T2;
        result.error_T3 = error_T3;
        result.valid = true;

        return result;
    }

    /// @brief Calculate memory usage for LED buffer
    ///
    /// @param num_leds Number of LEDs in the strip
    /// @param bits_per_led Number of bits per LED (typically 24 for RGB, 32 for RGBW)
    /// @param words_per_bit Number of words (uint16_t) per bit
    /// @param latch_us Latch gap duration in microseconds
    /// @param slot_ns Duration of each slot in nanoseconds
    /// @return Buffer size in bytes
    static FL_CONSTEXPR14 size_t calculate_buffer_size(
        uint32_t num_leds,
        uint32_t bits_per_led,
        uint32_t words_per_bit,
        uint32_t latch_us,
        uint32_t slot_ns
    ) {
        // Data size: num_leds × bits_per_led × words_per_bit × 2 bytes
        size_t data_size = num_leds * bits_per_led * words_per_bit * 2;

        // Latch gap: convert microseconds to slots, then to bytes
        size_t latch_slots = (latch_us * 1000) / slot_ns;
        size_t latch_size = latch_slots * 2;

        return data_size + latch_size;
    }

    /// @brief Calculate estimated frame time in microseconds
    ///
    /// @param num_leds Number of LEDs in the strip
    /// @param bits_per_led Number of bits per LED (typically 24 for RGB)
    /// @param words_per_bit Number of words per bit
    /// @param slot_ns Duration of each slot in nanoseconds
    /// @param latch_us Latch gap duration in microseconds
    /// @return Frame time in microseconds
    static FL_CONSTEXPR14 uint32_t calculate_frame_time_us(
        uint32_t num_leds,
        uint32_t bits_per_led,
        uint32_t words_per_bit,
        uint32_t slot_ns,
        uint32_t latch_us
    ) {
        // Transmission time: num_leds × bits × words × slot_ns
        uint32_t transmission_ns = num_leds * bits_per_led * words_per_bit * slot_ns;
        uint32_t transmission_us = transmission_ns / 1000;

        return transmission_us + latch_us;
    }

    /// @brief Check if timing is within acceptable tolerance
    ///
    /// @param result ClocklessTimingResult to validate
    /// @param max_error_fraction Maximum acceptable error (e.g., 0.05 for 5%)
    /// @return True if all timing errors are within tolerance
    static FL_CONSTEXPR14 bool is_timing_acceptable(
        const ClocklessTimingResult& result,
        float max_error_fraction = 0.30f  // 30% default (WS28xx are tolerant)
    ) {
        if (!result.valid) return false;

        return (result.error_T1 <= max_error_fraction &&
                result.error_T2 <= max_error_fraction &&
                result.error_T3 <= max_error_fraction);
    }

private:
    /// @brief Calculate relative error between actual and target values
    ///
    /// @param actual Actual measured/calculated value
    /// @param target Target desired value
    /// @return Relative error as fraction (e.g., 0.05 = 5% error)
    static FL_CONSTEXPR14 float calculate_error(uint32_t actual, uint32_t target) {
        if (target == 0) return 0.0f;

        int32_t diff = (int32_t)actual - (int32_t)target;
        if (diff < 0) diff = -diff;  // abs()

        return (float)diff / (float)target;
    }
};

}  // namespace fl
