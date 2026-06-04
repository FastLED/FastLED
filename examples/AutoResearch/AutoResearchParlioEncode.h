/// @file AutoResearchParlioEncode.h
/// @brief Full PARLIO encode bench for ESP32-P4 (post-byte-LUT, #2526 landed).
///
/// Measures the engine's per-byte-position hot path — **16-lane gather +
/// `wave8Transpose_16` + memcpy 128 B to DMA output** — with the source scratch
/// and the DMA output each in SRAM or PSRAM (4 combinations). Frame-equivalent
/// is x768 byte-positions, which matches the 16-channel × 256-LED × 3-byte
/// production frame size on the attached P4 EV board.
///
/// Two questions this bench answers:
///   1. **Is encode now fast enough for ISR-driven chunked streaming?**
///      At 800 kHz WS2812B with 16 lanes in parallel, one frame transmits in
///      `256 * 24 * 1.25 us = 7680 us`. If frame encode < 7680 us, the CPU can
///      keep ahead of the DMA stream and chunked streaming works.
///   2. **Is the PSRAM hypothesis (PSRAM is the bottleneck) correct?**
///      Compare SRAM-scratch / SRAM-output vs PSRAM variants. Prior memory
///      indicates the P4 L2 cache fully hides PSRAM latency for this access
///      pattern (×1.00 SRAM vs PSRAM); this rechecks on the byte-LUT version.
///
/// **Invocation:** RPC-only via `parlioEncodeBenchmark` in AutoResearchRemote.cpp.
/// The result struct is JSON-serialized by the RPC handler; this header no
/// longer prints directly.
///
/// Current implementation times the production BF1 pipe4 encode path and skips
/// PSRAM-only placements when PSRAM is not present.

#pragma once

#include "fl/channels/detail/wave8.hpp"
#include "fl/channels/wave8.h"
#include "fl/chipsets/led_timing.h"
#include "fl/log/log.h"  // FL_WARN
#include "fl/stl/bit_cast.h"
#include "fl/stl/int.h"

#include "fl/stl/cstring.h"  // fl::memcpy / fl::memset

#if defined(ARDUINO_ARCH_ESP32) || defined(ESP_PLATFORM)
#include <Arduino.h>     // micros()
#include <esp_heap_caps.h>
#define FL_PARLIO_BENCH_ENABLED 1
#else
#define FL_PARLIO_BENCH_ENABLED 0
#endif

namespace autoresearch {
namespace parlio_bench {

struct ParlioEncodeResult {
    fl::u32 iters;             // byte-positions per measurement
    fl::u32 lanes;             // 16
    fl::u32 leds_per_lane;     // 256 (matches the standard P4 test config)

    // Per-byte-position hot loop = 16-lane gather + BF1 pipe4 direct encode.
    // 4 placements of {scratch, output}: SRAM/SRAM, SRAM/PSRAM, PSRAM/SRAM, PSRAM/PSRAM.
    fl::u32 perpos_ss_us;      // scratch SRAM, output SRAM
    fl::u32 perpos_sp_us;      // scratch SRAM, output PSRAM
    fl::u32 perpos_ps_us;      // scratch PSRAM, output SRAM
    fl::u32 perpos_pp_us;      // scratch PSRAM, output PSRAM

    bool scratch_psram_ok;
    bool output_psram_ok;
    fl::u32 sink;
};

#if FL_PARLIO_BENCH_ENABLED

namespace {

// Mirror of the engine's clockless hot loop for one byte-position, 16-lane
// Wave8 path (parlio_engine.cpp.hpp ≈line 981-994). lane_stride matches the
// production layout: lane k reads from scratch[k * lane_stride + byte_offset].
FASTLED_FORCE_INLINE FL_IRAM
fl::u32 fl_parlio_inner_one_byte_position_bf1(const fl::u8 *scratch,
                                              fl::size_t lane_stride,
                                              fl::size_t byte_offset,
                                              const fl::Wave8ByteExpansionLut &byte_lut,
                                              fl::u8 *output,
                                              fl::size_t output_idx) {
    fl::u8 lanes[16];
    for (fl::size_t lane = 0; lane < 16; lane++) {
        lanes[lane] = scratch[lane * lane_stride + byte_offset];
    }
    fl::wave8Transpose_16_bf1(
        reinterpret_cast<const fl::u8(&)[16]>(lanes), byte_lut,
        *reinterpret_cast<fl::u8(*)[16 * sizeof(fl::Wave8Byte)]>(output + output_idx));
    return static_cast<fl::u32>(output[output_idx] ^ output[output_idx + 127]);
}

FASTLED_FORCE_INLINE FL_IRAM
fl::u32 fl_parlio_inner_four_byte_positions_bf1_pipe4(const fl::u8 *scratch,
                                                      fl::size_t lane_stride,
                                                      fl::size_t byte_offset,
                                                      const fl::Wave8ByteExpansionLut &byte_lut,
                                                      fl::u8 *output) {
    fl::u8 lanes_a[16];
    fl::u8 lanes_b[16];
    fl::u8 lanes_c[16];
    fl::u8 lanes_d[16];
    for (fl::size_t lane = 0; lane < 16; lane++) {
        const fl::u8 *base = scratch + lane * lane_stride + byte_offset;
        lanes_a[lane] = base[0];
        lanes_b[lane] = base[1];
        lanes_c[lane] = base[2];
        lanes_d[lane] = base[3];
    }

    constexpr fl::size_t BLOCK_SIZE = 16 * sizeof(fl::Wave8Byte);
    fl::u8 *out_a = output + byte_offset * BLOCK_SIZE;
    fl::wave8Transpose_16x4_bf1_pipe4(
        reinterpret_cast<const fl::u8(&)[16]>(lanes_a),
        reinterpret_cast<const fl::u8(&)[16]>(lanes_b),
        reinterpret_cast<const fl::u8(&)[16]>(lanes_c),
        reinterpret_cast<const fl::u8(&)[16]>(lanes_d),
        byte_lut,
        *reinterpret_cast<fl::u8(*)[BLOCK_SIZE]>(out_a),
        *reinterpret_cast<fl::u8(*)[BLOCK_SIZE]>(out_a + BLOCK_SIZE),
        *reinterpret_cast<fl::u8(*)[BLOCK_SIZE]>(out_a + 2 * BLOCK_SIZE),
        *reinterpret_cast<fl::u8(*)[BLOCK_SIZE]>(out_a + 3 * BLOCK_SIZE));

    return static_cast<fl::u32>(out_a[0] ^ out_a[BLOCK_SIZE * 4 - 1]);
}

// Time one configuration: scratch + output buffers (caller pre-allocates).
inline fl::u32 fl_parlio_measure(const fl::u8 *scratch, fl::size_t scratch_size,
                                 fl::u8 *output, fl::size_t output_size,
                                 const fl::Wave8ByteExpansionLut &byte_lut,
                                 int iters_byte_positions,
                                 volatile fl::u32 *sink) {
    constexpr fl::size_t LANES = 16;
    constexpr fl::size_t BYTES_PER_LANE = 768; // 256 LEDs × 3
    const fl::size_t lane_stride = BYTES_PER_LANE;

    const fl::size_t required_scratch = LANES * BYTES_PER_LANE;
    const fl::size_t required_output = BYTES_PER_LANE * LANES * sizeof(fl::Wave8Byte);
    if (scratch_size < required_scratch || output_size < required_output) {
        FL_WARN("fl_parlio_measure: undersized buffer "
                "(scratch=" << scratch_size << " need=" << required_scratch
                << ", output=" << output_size << " need=" << required_output << ")");
        return 0u;
    }

    constexpr fl::size_t BLOCK_SIZE = LANES * sizeof(fl::Wave8Byte);
    fl::u32 t0 = micros();
    int it = 0;
    while (it < iters_byte_positions) {
        const fl::size_t byte_offset =
            static_cast<fl::size_t>(it) % BYTES_PER_LANE;
        if (byte_offset + 3 < BYTES_PER_LANE && it + 3 < iters_byte_positions) {
            *sink ^= fl_parlio_inner_four_byte_positions_bf1_pipe4(
                scratch, lane_stride, byte_offset, byte_lut, output);
            it += 4;
        } else {
            const fl::size_t output_idx = byte_offset * BLOCK_SIZE;
            *sink ^= fl_parlio_inner_one_byte_position_bf1(
                scratch, lane_stride, byte_offset, byte_lut, output, output_idx);
            it += 1;
        }
    }
    return micros() - t0;
}

inline fl::u8 *fl_parlio_alloc(fl::size_t size, bool psram) {
    return reinterpret_cast<fl::u8 *>(heap_caps_malloc(
        size,
        psram ? (MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT)
              : (MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT)));
}

} // anonymous namespace

inline ParlioEncodeResult measureParlioEncode(int iters_in = 12000) {
    ParlioEncodeResult result{};
    if (iters_in < 1) iters_in = 1;
    if (iters_in > 200000) iters_in = 200000;
    result.iters = static_cast<fl::u32>(iters_in);
    result.lanes = 16;
    result.leds_per_lane = 256;

    constexpr fl::size_t LANES = 16;
    constexpr fl::size_t BYTES_PER_LANE = 768;
    constexpr fl::size_t SCRATCH_BYTES = LANES * BYTES_PER_LANE;        // 12 KB
    constexpr fl::size_t OUTPUT_BYTES = BYTES_PER_LANE * LANES * sizeof(fl::Wave8Byte); // 96 KB

    fl::ChipsetTiming timing;
    timing.T1 = 400;
    timing.T2 = 450;
    timing.T3 = 400;
    fl::Wave8BitExpansionLut nib_lut = fl::buildWave8ExpansionLUT(timing);
    fl::Wave8ByteExpansionLut byte_lut = fl::buildWave8ByteExpansionLUT(nib_lut);

    fl::u8 *scratch_sram = fl_parlio_alloc(SCRATCH_BYTES, /*psram=*/false);
    fl::u8 *output_sram = fl_parlio_alloc(OUTPUT_BYTES, /*psram=*/false);

    if (!scratch_sram || !output_sram) {
        heap_caps_free(scratch_sram);
        heap_caps_free(output_sram);
        result.iters = 0;
        return result;
    }

    fl::u8 *scratch_psram = fl_parlio_alloc(SCRATCH_BYTES, /*psram=*/true);
    fl::u8 *output_psram = fl_parlio_alloc(OUTPUT_BYTES, /*psram=*/true);
    result.scratch_psram_ok = scratch_psram != nullptr;
    result.output_psram_ok = output_psram != nullptr;

    if (!scratch_psram) {
        scratch_psram = scratch_sram;
    }
    if (!output_psram) {
        output_psram = output_sram;
    }

    // Fill scratch buffers with representative LED data.
    for (fl::size_t i = 0; i < SCRATCH_BYTES; ++i) {
        const fl::u8 v = static_cast<fl::u8>((i * 31 + 7) & 0xFF);
        scratch_sram[i] = v;
        if (result.scratch_psram_ok) {
            scratch_psram[i] = v;
        }
    }
    fl::memset(output_sram, 0, OUTPUT_BYTES);
    if (result.output_psram_ok) {
        fl::memset(output_psram, 0, OUTPUT_BYTES);
    }

    volatile fl::u32 sink = 0;

    // Warm caches / icache.
    fl_parlio_measure(scratch_sram, SCRATCH_BYTES, output_sram, OUTPUT_BYTES,
                      byte_lut, 64, &sink);

    result.perpos_ss_us = fl_parlio_measure(
        scratch_sram, SCRATCH_BYTES, output_sram, OUTPUT_BYTES,
        byte_lut, iters_in, &sink);
    if (result.output_psram_ok) {
        result.perpos_sp_us = fl_parlio_measure(
            scratch_sram, SCRATCH_BYTES, output_psram, OUTPUT_BYTES,
            byte_lut, iters_in, &sink);
    }
    if (result.scratch_psram_ok) {
        result.perpos_ps_us = fl_parlio_measure(
            scratch_psram, SCRATCH_BYTES, output_sram, OUTPUT_BYTES,
            byte_lut, iters_in, &sink);
    }
    if (result.scratch_psram_ok && result.output_psram_ok) {
        result.perpos_pp_us = fl_parlio_measure(
            scratch_psram, SCRATCH_BYTES, output_psram, OUTPUT_BYTES,
            byte_lut, iters_in, &sink);
    }

    result.sink = static_cast<fl::u32>(sink);

    heap_caps_free(scratch_sram);
    if (result.scratch_psram_ok) {
        heap_caps_free(scratch_psram);
    }
    heap_caps_free(output_sram);
    if (result.output_psram_ok) {
        heap_caps_free(output_psram);
    }
    return result;
}

#else // !FL_PARLIO_BENCH_ENABLED

inline ParlioEncodeResult measureParlioEncode(int /*iters*/ = 12000) { return {}; }

#endif

} // namespace parlio_bench
} // namespace autoresearch
