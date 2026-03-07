// IWYU pragma: private

/// @file wave8_encoder_uart.cpp
/// @brief Implementation of wave10 UART encoder
///
/// Implements dynamic LUT generation from chipset timing and the timing
/// feasibility predicate. The wave10 model treats the 10-bit UART frame
/// as two 5-pulse LED bit slots.

#include "platforms/esp/32/drivers/uart/wave8_encoder_uart.h"

namespace fl {

namespace {

/// @brief Absolute value of difference (unsigned-safe)
u32 absDiff(u32 a, u32 b) {
    return (a > b) ? (a - b) : (b - a);
}

/// @brief Compute HIGH pulse count for a given timing value
/// @param timing_ns Timing value in nanoseconds (T0H or T1H)
/// @param pulse_width_ns Width of each pulse in nanoseconds
/// @return Number of HIGH pulses (best-fit: floor or ceil, whichever is closer)
///
/// Tries both floor and ceil, picks the one with smaller error.
/// Ties are broken by preferring the lower (floor) value, which keeps the
/// HIGH time shorter — safer for most LED protocols. For example, WS2812
/// T1H=875ns at 250ns/pulse: floor=3 (750ns, err=125), ceil=4 (1000ns,
/// err=125) → picks 3 (shorter HIGH time).
u8 computePulseCount(u32 timing_ns, u32 pulse_width_ns) {
    if (pulse_width_ns == 0) return 0;
    u8 lo = static_cast<u8>(timing_ns / pulse_width_ns);
    u8 hi = lo + 1;
    u32 err_lo = absDiff(static_cast<u32>(lo) * pulse_width_ns, timing_ns);
    u32 err_hi = absDiff(static_cast<u32>(hi) * pulse_width_ns, timing_ns);
    // Prefer lo on tie (shorter HIGH time is safer)
    return (err_hi < err_lo) ? hi : lo;
}

/// @brief Build a UART data byte from two LED bit pulse counts
///
/// Constructs the 8-bit UART data value that, with TX inversion and LSB-first
/// transmission, produces the desired wire waveform.
///
/// Wire model (10 bits with TX inversion):
///   [START=H] [~D0] [~D1] [~D2] [~D3] [~D4] [~D5] [~D6] [~D7] [STOP=L]
///
/// LED bit A occupies positions 0-4 (START + D0..D3):
///   START is always HIGH (1 pulse guaranteed)
///   D0..D3 set HIGH for (pulses_a - 1) additional pulses, then LOW
///
/// LED bit B occupies positions 5-9 (D4..D7 + STOP):
///   D4 must be HIGH for leading edge (1 pulse guaranteed)
///   D5..D7 set HIGH for (pulses_b - 1) additional pulses, then LOW
///   STOP is always LOW
///
/// @param pulses_a HIGH pulse count for LED bit A [1, 4]
/// @param pulses_b HIGH pulse count for LED bit B [1, 4]
/// @return UART data byte value
u8 buildUartByte(u8 pulses_a, u8 pulses_b) {
    // Build the desired 10-bit wire pattern (with TX inversion)
    // Wire positions: [0=START] [1=~D0] ... [8=~D7] [9=STOP]
    //
    // LED bit A (pos 0-4): pulses_a HIGH bits, then LOW
    // LED bit B (pos 5-9): pulses_b HIGH bits, then LOW (STOP=L)

    int wire[10];

    // LED bit A: pulses_a HIGH bits starting from position 0
    for (int i = 0; i < 5; i++) {
        wire[i] = (i < static_cast<int>(pulses_a)) ? 1 : 0;
    }

    // LED bit B: pulses_b HIGH bits starting from position 5
    for (int i = 0; i < 5; i++) {
        wire[5 + i] = (i < static_cast<int>(pulses_b)) ? 1 : 0;
    }

    // Convert wire pattern back to UART data byte
    // Wire[0] = START (inverted) = always H → no control needed
    // Wire[1..8] = ~D0..~D7 (inverted data, LSB first)
    // Wire[9] = STOP (inverted) = always L → no control needed
    //
    // So: wire[1+i] = ~D_i → D_i = ~wire[1+i] = wire[1+i] ? 0 : 1
    u8 byte_val = 0;
    for (int i = 0; i < 8; i++) {
        int data_bit = wire[1 + i] ? 0 : 1; // Invert: wire HIGH → data 0
        byte_val |= static_cast<u8>(data_bit << i); // LSB first
    }

    return byte_val;
}

} // anonymous namespace

Wave10Lut buildWave10Lut(const ChipsetTimingConfig& timing) {
    Wave10Lut result = {};

    const u32 period_ns = timing.total_period_ns();
    if (period_ns == 0) return result;

    // Pulse width: each LED bit gets 5 pulses within its half-frame
    const u32 pulse_width_ns = period_ns / 5;
    if (pulse_width_ns == 0) return result;

    // T0H and T1H in nanoseconds
    const u32 t0h_ns = timing.t1_ns;           // T0H = T1
    const u32 t1h_ns = timing.t1_ns + timing.t2_ns; // T1H = T1 + T2

    // Compute pulse counts
    u8 pulses_0 = computePulseCount(t0h_ns, pulse_width_ns);
    u8 pulses_1 = computePulseCount(t1h_ns, pulse_width_ns);

    // Clamp to valid range [1, 4]
    if (pulses_0 < 1) pulses_0 = 1;
    if (pulses_0 > 4) pulses_0 = 4;
    if (pulses_1 < 1) pulses_1 = 1;
    if (pulses_1 > 4) pulses_1 = 4;

    // Build LUT entries for all 4 two-bit combinations
    result.lut[0] = buildUartByte(pulses_0, pulses_0); // "00"
    result.lut[1] = buildUartByte(pulses_0, pulses_1); // "01"
    result.lut[2] = buildUartByte(pulses_1, pulses_0); // "10"
    result.lut[3] = buildUartByte(pulses_1, pulses_1); // "11"

    return result;
}

bool canRepresentTiming(const ChipsetTimingConfig& timing) {
    const u32 period_ns = timing.total_period_ns();
    if (period_ns == 0) return false;

    // Check 1: Baud rate must be within ESP32 UART limits
    const u32 baud_rate = Wave10Lut::computeBaudRate(timing);
    if (baud_rate == 0 || baud_rate > kMaxUartBaudRate) return false;

    // Pulse width for 5-pulse-per-bit model
    const u32 pulse_width_ns = period_ns / 5;
    if (pulse_width_ns == 0) return false;

    // T0H and T1H timing
    const u32 t0h_ns = timing.t1_ns;
    const u32 t1h_ns = timing.t1_ns + timing.t2_ns;

    // Compute pulse counts
    u8 pulses_0 = computePulseCount(t0h_ns, pulse_width_ns);
    u8 pulses_1 = computePulseCount(t1h_ns, pulse_width_ns);

    // Check 2 & 3: Pulse counts must be in valid range [1, 4]
    if (pulses_0 < 1 || pulses_0 > 4) return false;
    if (pulses_1 < 1 || pulses_1 > 4) return false;

    // Check 4: Pulse counts must be distinguishable
    if (pulses_0 == pulses_1) return false;

    // Check 5 & 6: Quantized timing must be within half a pulse width of nominal.
    // This is the natural quantization error bound for best-fit rounding.
    // For chipsets with wider pulses (lower baud), tolerance scales accordingly.
    const u32 tolerance_ns = pulse_width_ns / 2;
    const u32 actual_t0h = static_cast<u32>(pulses_0) * pulse_width_ns;
    const u32 actual_t1h = static_cast<u32>(pulses_1) * pulse_width_ns;

    if (absDiff(actual_t0h, t0h_ns) > tolerance_ns) return false;
    if (absDiff(actual_t1h, t1h_ns) > tolerance_ns) return false;

    return true;
}

FL_IRAM FL_OPTIMIZE_FUNCTION
size_t encodeLedsToUart(const u8* input,
                        size_t input_size,
                        u8* output,
                        size_t output_capacity,
                        const Wave10Lut& lut) {
    size_t required_size = input_size * 4;

    if (required_size > output_capacity) {
        return 0;
    }

    for (size_t i = 0; i < input_size; ++i) {
        detail::encodeUartByte(input[i], &output[i * 4], lut);
    }

    return required_size;
}

FL_IRAM FL_OPTIMIZE_FUNCTION
size_t encodeLedsToUart(const u8* input,
                        size_t input_size,
                        u8* output,
                        size_t output_capacity) {
    size_t required_size = input_size * 4;

    if (required_size > output_capacity) {
        return 0;
    }

    for (size_t i = 0; i < input_size; ++i) {
        detail::encodeUartByte(input[i], &output[i * 4]);
    }

    return required_size;
}

} // namespace fl
