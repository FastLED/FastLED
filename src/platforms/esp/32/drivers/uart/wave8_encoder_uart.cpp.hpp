#pragma once

// IWYU pragma: private

/// @file wave8_encoder_uart.cpp
/// @brief Implementation of wave10 UART encoder
///
/// Implements dynamic LUT generation from chipset timing and the timing
/// feasibility predicate. The wave10 model treats the 10-bit UART frame
/// as two 5-pulse LED bit slots.

#include "platforms/esp/32/drivers/uart/wave8_encoder_uart.h"
#include "fl/stl/noexcept.h"

FL_OPTIMIZATION_LEVEL_O3_BEGIN

namespace fl {

namespace {

/// @brief Absolute value of difference (unsigned-safe)
u32 absDiff(u32 a, u32 b) FL_NO_EXCEPT {
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
u8 computePulseCount(u32 timing_ns, u32 pulse_width_ns) FL_NO_EXCEPT {
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
u8 buildUartByte(u8 pulses_a, u8 pulses_b, u8 pulses_per_bit = 5) FL_NO_EXCEPT {
    // Build the desired 2P-bit wire pattern (with TX inversion), where
    // P = pulses_per_bit. Wire positions:
    //   [0=START] [1=~D0] ... [2P-2=~D(2P-3)] [2P-1=STOP]
    //
    // LED bit A (pos 0..P-1): pulses_a HIGH bits, then LOW
    // LED bit B (pos P..2P-1): pulses_b HIGH bits, then LOW (STOP=L)

    const int P = static_cast<int>(pulses_per_bit);
    int wire[10]; // max frame = 10 (P=5)

    // LED bit A: pulses_a HIGH bits starting from position 0
    for (int i = 0; i < P; i++) {
        wire[i] = (i < static_cast<int>(pulses_a)) ? 1 : 0;
    }

    // LED bit B: pulses_b HIGH bits starting from position P
    for (int i = 0; i < P; i++) {
        wire[P + i] = (i < static_cast<int>(pulses_b)) ? 1 : 0;
    }

    // Convert wire pattern back to UART data byte
    // Wire[0] = START (inverted) = always H → no control needed
    // Wire[1..2P-2] = ~D0..~D(2P-3) (inverted data, LSB first)
    // Wire[2P-1] = STOP (inverted) = always L → no control needed
    //
    // So: wire[1+i] = ~D_i → D_i = ~wire[1+i] = wire[1+i] ? 0 : 1
    u8 byte_val = 0;
    const int data_bits = 2 * P - 2;
    for (int i = 0; i < data_bits; i++) {
        int data_bit = wire[1 + i] ? 0 : 1; // Invert: wire HIGH → data 0
        byte_val |= static_cast<u8>(data_bit << i); // LSB first
    }

    return byte_val;
}

/// @brief Fit result for one UART wave geometry (P pulses per LED bit)
struct UartWaveFit {
    bool ok;
    u8 pulses_0;
    u8 pulses_1;
    u32 total_err_ns; ///< |T0H err| + |T1H err| vs nominal, post-bump
};

/// @brief Evaluate whether P pulses/LED-bit can represent the timing.
///
/// Shared by buildWave10Lut() and canRepresentTiming() so the LUT that
/// gets built is always judged by the same rules that admitted it.
UartWaveFit fitUartWave(const ChipsetTimingConfig& timing,
                        u8 pulses_per_bit,
                        u32 max_baud_rate) FL_NO_EXCEPT {
    UartWaveFit fit = {};
    const u32 period_ns = timing.total_period_ns();
    if (period_ns == 0) return fit;

    // Baud = one UART bit per pulse = P / period. Must fit the ESP32
    // UART ceiling.
    const u64 baud =
        static_cast<u64>(pulses_per_bit) * 1000000000ULL / period_ns;
    if (baud == 0 || baud > max_baud_rate) return fit;

    const u32 pulse_width_ns = period_ns / pulses_per_bit;
    if (pulse_width_ns == 0) return fit;

    const u32 t0h_ns = timing.t1_ns;
    const u32 t1h_ns = timing.t1_ns + timing.t2_ns;
    const u8 max_pulses = static_cast<u8>(pulses_per_bit - 1);

    u8 pulses_0 = computePulseCount(t0h_ns, pulse_width_ns);
    u8 pulses_1 = computePulseCount(t1h_ns, pulse_width_ns);
    if (pulses_0 < 1 || pulses_0 > max_pulses) return fit;
    if (pulses_1 < 1 || pulses_1 > max_pulses) return fit;
    if (pulses_0 == pulses_1) return fit;

    // FastLED#3569/#3572 — enforce a minimum ABSOLUTE wire separation
    // between the 0 and 1 symbols, widening T1H upward (a longer HIGH
    // still reads as 1). Best-fit rounding alone can land them one
    // pulse apart, and at high baud one pulse is too little: WS2812B-V5
    // at P=5 rounds to 245 vs 490 ns — inside real WS281x parts'
    // sampling-threshold band (~550-625 ns); it flapped the WROOM bench
    // decoder. Slow chipsets (WS2811-400: 500+ ns pulses) already
    // exceed the floor with single-pulse separation.
    constexpr u32 kMinSymbolSeparationNs = 400;
    const u8 pulses_1_raw = pulses_1;
    while (pulses_1 < max_pulses &&
           static_cast<u32>(pulses_1 - pulses_0) * pulse_width_ns <
               kMinSymbolSeparationNs) {
        ++pulses_1;
    }

    // Quantized timing must be within half a pulse of nominal — the
    // natural error bound for best-fit rounding. When the separation
    // bump deliberately widened T1H, allow exactly the pulses it added
    // on top of that bound.
    const u32 tolerance_ns = pulse_width_ns / 2;
    const u32 actual_t0h = static_cast<u32>(pulses_0) * pulse_width_ns;
    const u32 actual_t1h = static_cast<u32>(pulses_1) * pulse_width_ns;
    const u32 t1h_tolerance_ns =
        tolerance_ns +
        static_cast<u32>(pulses_1 - pulses_1_raw) * pulse_width_ns;
    const u32 err_t0h = absDiff(actual_t0h, t0h_ns);
    const u32 err_t1h = absDiff(actual_t1h, t1h_ns);
    if (err_t0h > tolerance_ns) return fit;
    if (err_t1h > t1h_tolerance_ns) return fit;

    fit.ok = true;
    fit.pulses_0 = pulses_0;
    fit.pulses_1 = pulses_1;
    fit.total_err_ns = err_t0h + err_t1h;
    return fit;
}

} // anonymous namespace

Wave10Lut buildWave10Lut(const ChipsetTimingConfig& timing) FL_NO_EXCEPT {
    return buildWave10LutForMaxBaud(timing, kMaxUartBaudRate);
}

Wave10Lut buildWave10LutForMaxBaud(const ChipsetTimingConfig& timing,
                                   u32 max_baud_rate) FL_NO_EXCEPT {
    Wave10Lut result = {};

    // Evaluate both frame geometries (FastLED#3572 follow-up):
    //   P=5 — wave10: 8 data bits, baud = 5/period (the classic shape)
    //   P=4 — wave8-frame: 6 data bits, baud = 4/period (lower baud →
    //         reaches faster chipsets under the 5 Mbps cap; different
    //         quantization grid — e.g. SK6812 is exact at period/4)
    // Pick the feasible geometry with the smaller total quantization
    // error. P=5 wins ties and near-ties (50 ns hysteresis) so the
    // long-proven wave10 shapes stay stable for existing chipsets.
    const UartWaveFit fit5 = fitUartWave(timing, 5, max_baud_rate);
    const UartWaveFit fit4 = fitUartWave(timing, 4, max_baud_rate);

    u8 P = 0;
    UartWaveFit fit = {};
    constexpr u32 kHysteresisNs = 50;
    if (fit5.ok && fit4.ok) {
        if (fit4.total_err_ns + kHysteresisNs < fit5.total_err_ns) {
            P = 4;
            fit = fit4;
        } else {
            P = 5;
            fit = fit5;
        }
    } else if (fit5.ok) {
        P = 5;
        fit = fit5;
    } else if (fit4.ok) {
        P = 4;
        fit = fit4;
    } else {
        return result; // infeasible — pulses_per_bit stays 0
    }

    result.pulses_per_bit = P;
    result.lut[0] = buildUartByte(fit.pulses_0, fit.pulses_0, P); // "00"
    result.lut[1] = buildUartByte(fit.pulses_0, fit.pulses_1, P); // "01"
    result.lut[2] = buildUartByte(fit.pulses_1, fit.pulses_0, P); // "10"
    result.lut[3] = buildUartByte(fit.pulses_1, fit.pulses_1, P); // "11"

    return result;
}

bool canRepresentTiming(const ChipsetTimingConfig& timing) FL_NO_EXCEPT {
    return canRepresentTimingForMaxBaud(timing, kMaxUartBaudRate);
}

bool canRepresentTimingForMaxBaud(const ChipsetTimingConfig& timing,
                                  u32 max_baud_rate) FL_NO_EXCEPT {
    // Feasible if EITHER frame geometry fits — same helper as
    // buildWave10Lut(), so admission and construction can't drift.
    return fitUartWave(timing, 5, max_baud_rate).ok ||
           fitUartWave(timing, 4, max_baud_rate).ok;
}

FL_IRAM FL_OPTIMIZE_FUNCTION
size_t encodeLedsToUart(const u8* input,
                        size_t input_size,
                        u8* output,
                        size_t output_capacity,
                        const Wave10Lut& lut) FL_NO_EXCEPT {
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
                        size_t output_capacity) FL_NO_EXCEPT {
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

FL_OPTIMIZATION_LEVEL_O3_END
