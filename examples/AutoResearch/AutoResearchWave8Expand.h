/// @file AutoResearchWave8Expand.h
/// @brief Boot-time micro-benchmark for the Wave8 expansion (#2526).
///
/// Compares the current nibble-LUT expansion against the byte-indexed (256x8)
/// LUT proposed in #2526, plus a batched-byte variant (S3). Output goes through
/// esp_rom_printf so it reaches the USB-Serial-JTAG console (= COM25) at boot
/// — the testSimd RPC path is currently blocked by a UART0-vs-USB-JTAG routing
/// issue on P4 (CDC On Boot = 0; see #2536/#2538). Capturing boot serial on
/// COM25 sidesteps that.

#pragma once

#include "fl/channels/detail/wave8.hpp"
#include "fl/channels/wave8.h"
#include "fl/chipsets/led_timing.h"
#include "fl/stl/bit_cast.h"
#include "fl/stl/int.h"

#if defined(ARDUINO_ARCH_ESP32) || defined(ESP_PLATFORM)
#include <Arduino.h>     // micros()
#include <esp_rom_sys.h> // esp_rom_printf -> USB-Serial-JTAG console
#define FL_BENCH_PRINTF esp_rom_printf
#define FL_BENCH_ENABLED 1
#else
#define FL_BENCH_ENABLED 0
#endif

namespace autoresearch {
namespace wave8_bench {

#if FL_BENCH_ENABLED

inline void runWave8ExpandBenchmark() {
    // Representative WS2812B-ish timing (ratios that yield non-degenerate
    // expansion patterns; the absolute values don't matter for this micro-bench).
    fl::ChipsetTiming timing;
    timing.T1 = 400;
    timing.T2 = 450;
    timing.T3 = 400;

    fl::Wave8BitExpansionLut nibLut = fl::buildWave8ExpansionLUT(timing);
    fl::Wave8ByteExpansionLut byteLut = fl::buildWave8ByteExpansionLUT(nibLut);

    // 16 lane inputs, mixed values; output buffer matches the production
    // wave8Transpose_16() stack layout (16 Wave8Byte = 128 bytes).
    fl::u8 lanes[16];
    for (int i = 0; i < 16; ++i) {
        lanes[i] = static_cast<fl::u8>(i * 17 + 3);
    }
    fl::Wave8Byte out[16];

    volatile fl::u32 sink = 0;

    // One iter == one "byte-position" of encode = 16 lane expansions.
    // Production encodes 768 byte-positions per frame (256 LEDs * 3 channels).
    // Pick ITERS large enough that micros() jitter is <1% (~hundreds of ms total).
    const int ITERS = 30000; // 30000 * 16 = 480,000 expansions per measurement

    // Warm caches / icache.
    for (int i = 0; i < 16; ++i) {
        fl::detail::wave8_convert_byte_to_wave8byte(lanes[i], nibLut, &out[i]);
        fl::detail::wave8_expand_byte(lanes[i], byteLut, &out[i]);
    }

    // --- Nibble path (current production) ---
    fl::u32 t0 = micros();
    for (int it = 0; it < ITERS; ++it) {
        lanes[0] = static_cast<fl::u8>(it);
        lanes[8] = static_cast<fl::u8>(~it);
        for (int i = 0; i < 16; ++i) {
            fl::detail::wave8_convert_byte_to_wave8byte(lanes[i], nibLut, &out[i]);
        }
        sink ^= out[0].symbols[0].data ^ out[15].symbols[7].data;
    }
    fl::u32 nibble_us = micros() - t0;

    // --- Byte-LUT path (S1: byte-indexed 256x8) ---
    t0 = micros();
    for (int it = 0; it < ITERS; ++it) {
        lanes[0] = static_cast<fl::u8>(it);
        lanes[8] = static_cast<fl::u8>(~it);
        for (int i = 0; i < 16; ++i) {
            fl::detail::wave8_expand_byte(lanes[i], byteLut, &out[i]);
        }
        sink ^= out[0].symbols[0].data ^ out[15].symbols[7].data;
    }
    fl::u32 byte_us = micros() - t0;

    // --- Batched byte path (S3: load-all-then-store-all, ILP) ---
    t0 = micros();
    for (int it = 0; it < ITERS; ++it) {
        lanes[0] = static_cast<fl::u8>(it);
        lanes[8] = static_cast<fl::u8>(~it);
        fl::u32 lo[16];
        fl::u32 hi[16];
        for (int i = 0; i < 16; ++i) {
            const fl::u32 *src = fl::bit_cast_ptr<const fl::u32>(&byteLut.lut[lanes[i]]);
            lo[i] = src[0];
            hi[i] = src[1];
        }
        for (int i = 0; i < 16; ++i) {
            fl::u32 *dst = fl::bit_cast_ptr<fl::u32>(&out[i]);
            dst[0] = lo[i];
            dst[1] = hi[i];
        }
        sink ^= out[0].symbols[0].data ^ out[15].symbols[7].data;
    }
    fl::u32 batched_us = micros() - t0;

    // Frame-equivalent micros: per-frame is 768 byte-positions vs our ITERS.
    fl::u32 nibble_frame_us = static_cast<fl::u32>(static_cast<fl::u64>(nibble_us) * 768 / ITERS);
    fl::u32 byte_frame_us = static_cast<fl::u32>(static_cast<fl::u64>(byte_us) * 768 / ITERS);
    fl::u32 batched_frame_us = static_cast<fl::u32>(static_cast<fl::u64>(batched_us) * 768 / ITERS);

    // Speedups as percentage * 100 (integer math, avoid floats in esp_rom_printf).
    fl::u32 spd_byte_x100 = (byte_us > 0)
        ? static_cast<fl::u32>(static_cast<fl::u64>(nibble_us) * 100 / byte_us) : 0;
    fl::u32 spd_batched_x100 = (batched_us > 0)
        ? static_cast<fl::u32>(static_cast<fl::u64>(nibble_us) * 100 / batched_us) : 0;

    FL_BENCH_PRINTF("\nBENCH_EXPAND_START (issue #2526)\n");
    FL_BENCH_PRINTF("BENCH_EXPAND iters=%d  nibble_us=%u byte_us=%u batched_us=%u  sink=%u\n",
                    ITERS, nibble_us, byte_us, batched_us,
                    static_cast<fl::u32>(sink));
    FL_BENCH_PRINTF("BENCH_EXPAND frame_equiv_us  nibble=%u byte=%u batched=%u\n",
                    nibble_frame_us, byte_frame_us, batched_frame_us);
    FL_BENCH_PRINTF("BENCH_EXPAND speedup_x100  byte_vs_nibble=%u batched_vs_nibble=%u\n",
                    spd_byte_x100, spd_batched_x100);
    FL_BENCH_PRINTF("BENCH_EXPAND_END\n\n");
}

#else  // !FL_BENCH_ENABLED

inline void runWave8ExpandBenchmark() { /* no-op on non-ESP32 */ }

#endif

} // namespace wave8_bench
} // namespace autoresearch
