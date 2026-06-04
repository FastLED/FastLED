/// @file AutoResearchWave8Expand.h
/// @brief Wave8 expansion micro-benchmark for #2526.
///
/// Compares the nibble-LUT expansion path against the byte-indexed (256x8)
/// LUT and a batched-byte variant (S3), AND times the full per-byte-position
/// production cost (expansion + 16-lane transpose) for both LUTs.
///
/// **Invocation:** RPC-only. Call via the `wave8ExpandBenchmark` handler
/// registered in AutoResearchRemote.cpp. The result struct is JSON-serialized
/// by the RPC handler; this header no longer prints directly.

#pragma once

#include "fl/channels/detail/wave8.hpp"
#include "fl/channels/wave8.h"
#include "fl/chipsets/led_timing.h"
#include "fl/stl/bit_cast.h"
#include "fl/stl/int.h"

#if defined(ARDUINO_ARCH_ESP32) || defined(ESP_PLATFORM)
#include <Arduino.h>     // micros()
#endif

namespace autoresearch {
namespace wave8_bench {

struct Wave8ExpandResult {
    fl::u32 iters;
    // Expansion in isolation (the #2526 strategies side-by-side).
    fl::u32 expand_nibble_us;     // current production
    fl::u32 expand_byte_us;       // S1: byte-indexed 256x8 LUT
    fl::u32 expand_batched_us;    // S3: byte LUT, load-all-then-store-all
    // Full per-byte-position cost (expansion + 16-lane transpose). This is
    // what the parlio engine actually pays per byte-position * 768/frame.
    fl::u32 transpose16_nibble_us;
    fl::u32 transpose16_byte_us;
    fl::u32 sink;
};

#if defined(ARDUINO_ARCH_ESP32) || defined(ESP_PLATFORM)

/// Pure compute: builds both LUTs, runs all five timed paths, returns numbers.
/// No printing — the caller (RPC handler or boot-time bridge) decides.
inline Wave8ExpandResult measureWave8Expand(int iters_in = 30000) {
    Wave8ExpandResult result{};
    if (iters_in < 1) {
        iters_in = 1;
    }
    if (iters_in > 200000) {
        iters_in = 200000;
    }
    result.iters = static_cast<fl::u32>(iters_in);

    // Representative WS2812B-ish timing; absolutes don't matter for the bench.
    fl::ChipsetTiming timing;
    timing.T1 = 400;
    timing.T2 = 450;
    timing.T3 = 400;

    fl::Wave8BitExpansionLut nibLut = fl::buildWave8ExpansionLUT(timing);
    fl::Wave8ByteExpansionLut byteLut = fl::buildWave8ByteExpansionLUT(nibLut);

    fl::u8 lanes[16];
    for (int i = 0; i < 16; ++i) {
        lanes[i] = static_cast<fl::u8>(i * 17 + 3);
    }
    fl::Wave8Byte out[16];
    volatile fl::u32 sink = 0;

    // Warm caches / icache.
    for (int i = 0; i < 16; ++i) {
        fl::detail::wave8_convert_byte_to_wave8byte(lanes[i], nibLut, &out[i]);
        fl::detail::wave8_expand_byte(lanes[i], byteLut, &out[i]);
    }
    fl::u8 transposed[16 * sizeof(fl::Wave8Byte)];
    fl::wave8Transpose_16(reinterpret_cast<const fl::u8(&)[16]>(lanes), nibLut,
                          reinterpret_cast<fl::u8(&)[16 * sizeof(fl::Wave8Byte)]>(transposed));
    fl::wave8Transpose_16(reinterpret_cast<const fl::u8(&)[16]>(lanes), byteLut,
                          reinterpret_cast<fl::u8(&)[16 * sizeof(fl::Wave8Byte)]>(transposed));

    const int iters = iters_in;

    // --- Expansion only: nibble (current production path) ---
    {
        fl::u32 t0 = micros();
        for (int it = 0; it < iters; ++it) {
            lanes[0] = static_cast<fl::u8>(it);
            lanes[8] = static_cast<fl::u8>(~it);
            for (int i = 0; i < 16; ++i) {
                fl::detail::wave8_convert_byte_to_wave8byte(lanes[i], nibLut, &out[i]);
            }
            sink ^= out[0].symbols[0].data ^ out[15].symbols[7].data;
        }
        result.expand_nibble_us = micros() - t0;
    }

    // --- Expansion only: byte-LUT (S1) ---
    {
        fl::u32 t0 = micros();
        for (int it = 0; it < iters; ++it) {
            lanes[0] = static_cast<fl::u8>(it);
            lanes[8] = static_cast<fl::u8>(~it);
            for (int i = 0; i < 16; ++i) {
                fl::detail::wave8_expand_byte(lanes[i], byteLut, &out[i]);
            }
            sink ^= out[0].symbols[0].data ^ out[15].symbols[7].data;
        }
        result.expand_byte_us = micros() - t0;
    }

    // --- Expansion only: batched byte-LUT (S3: load-all-then-store-all) ---
    {
        fl::u32 t0 = micros();
        for (int it = 0; it < iters; ++it) {
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
        result.expand_batched_us = micros() - t0;
    }

    // --- Full per-byte-position (expansion + 16-lane transpose), nibble path ---
    {
        fl::u32 t0 = micros();
        for (int it = 0; it < iters; ++it) {
            lanes[0] = static_cast<fl::u8>(it);
            lanes[8] = static_cast<fl::u8>(~it);
            fl::wave8Transpose_16(reinterpret_cast<const fl::u8(&)[16]>(lanes), nibLut,
                                  reinterpret_cast<fl::u8(&)[16 * sizeof(fl::Wave8Byte)]>(transposed));
            sink ^= transposed[0] ^ transposed[127];
        }
        result.transpose16_nibble_us = micros() - t0;
    }

    // --- Full per-byte-position (expansion + 16-lane transpose), byte-LUT path ---
    {
        fl::u32 t0 = micros();
        for (int it = 0; it < iters; ++it) {
            lanes[0] = static_cast<fl::u8>(it);
            lanes[8] = static_cast<fl::u8>(~it);
            fl::wave8Transpose_16(reinterpret_cast<const fl::u8(&)[16]>(lanes), byteLut,
                                  reinterpret_cast<fl::u8(&)[16 * sizeof(fl::Wave8Byte)]>(transposed));
            sink ^= transposed[0] ^ transposed[127];
        }
        result.transpose16_byte_us = micros() - t0;
    }

    result.sink = static_cast<fl::u32>(sink);
    return result;
}

#else // non-ESP32

inline Wave8ExpandResult measureWave8Expand(int /*iters*/ = 30000) { return {}; }

#endif

} // namespace wave8_bench
} // namespace autoresearch
