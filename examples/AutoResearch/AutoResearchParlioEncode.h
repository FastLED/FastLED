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
/// Output: `BENCH_PARLIO_*` lines via `esp_rom_printf` -> USB-Serial-JTAG (COM25)
/// so capture works without the broken testSimd RPC routing (#2541). Gated by
/// `-DFL_BENCH_PARLIO_AT_BOOT=1` per #2542 — RPC-invokable from
/// `parlioEncodeBenchmark` in AutoResearchRemote.cpp.

#pragma once

#include "fl/channels/detail/wave8.hpp"
#include "fl/channels/wave8.h"
#include "fl/chipsets/led_timing.h"
#include "fl/stl/bit_cast.h"
#include "fl/stl/int.h"
#include "fl/stl/span.h"

#include "fl/stl/cstring.h"  // fl::memcpy / fl::memset

#if defined(ARDUINO_ARCH_ESP32) || defined(ESP_PLATFORM)
#include <Arduino.h>     // micros()
#include <esp_heap_caps.h>
#include <esp_rom_sys.h> // esp_rom_printf -> USB-Serial-JTAG console
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

    // Per-byte-position hot loop = 16-lane gather + wave8Transpose_16 + memcpy.
    // 4 placements of {scratch, output}: SRAM/SRAM, SRAM/PSRAM, PSRAM/SRAM, PSRAM/PSRAM.
    fl::u32 perpos_ss_us;      // scratch SRAM, output SRAM
    fl::u32 perpos_sp_us;      // scratch SRAM, output PSRAM
    fl::u32 perpos_ps_us;      // scratch PSRAM, output SRAM
    fl::u32 perpos_pp_us;      // scratch PSRAM, output PSRAM

    // #2548 algorithmic variants (all SRAM scratch + SRAM output; PSRAM proven
    // equivalent above, so we hold those constant and isolate the algorithm).
    fl::u32 var_l2_us;         // L2: direct write to output, skip 128-B memcpy
    fl::u32 var_l1l2_us;       // L1+L2: fused expand+transpose + direct write
    fl::u32 var_l1l2l3_us;     // L1+L2+L3: fused + direct write + 4-byte-position tiling

    // #2548 L4 prototype — lazy / PSRAM-allocated combined byte+lane+symbol LUT
    // (512 KB). Built once at engine init from byte_lut; lives in PSRAM (zero
    // SRAM footprint outside the engine pointer). 0 = build failed or not run.
    fl::u32 var_l4_us;
    fl::u32 l4_lut_bytes;      // for sanity-check: 524288 expected

    fl::u32 sink;
};

#if FL_PARLIO_BENCH_ENABLED

namespace {

// Mirror of the engine's clockless hot loop for one byte-position, 16-lane
// Wave8 path (parlio_engine.cpp.hpp ≈line 981-994). lane_stride matches the
// production layout: lane k reads from scratch[k * lane_stride + byte_offset].
FASTLED_FORCE_INLINE FL_IRAM
fl::u32 fl_parlio_inner_one_byte_position(const fl::u8 *scratch,
                                          fl::size_t lane_stride,
                                          fl::size_t byte_offset,
                                          const fl::Wave8ByteExpansionLut &byte_lut,
                                          fl::u8 *output,
                                          fl::size_t output_idx) {
    fl::u8 lanes[16];
    for (fl::size_t lane = 0; lane < 16; lane++) {
        lanes[lane] = scratch[lane * lane_stride + byte_offset];
    }
    fl::u8 transposed[16 * sizeof(fl::Wave8Byte)];
    fl::wave8Transpose_16(reinterpret_cast<const fl::u8(&)[16]>(lanes), byte_lut,
                          reinterpret_cast<fl::u8(&)[16 * sizeof(fl::Wave8Byte)]>(transposed));
    fl::memcpy(output + output_idx, transposed, sizeof(transposed));
    // Sink to defeat DCE: cheap reduction on a few output bytes.
    return static_cast<fl::u32>(output[output_idx] ^ output[output_idx + 127]);
}

// --- #2548 L1: fused expand+transpose. Cache the 16 byte_lut row pointers,
// then symbol-major loop reads `e[lane]->symbols[s]` directly — no 128-B
// intermediate `laneWaveformSymbols` buffer.
FASTLED_FORCE_INLINE FL_IRAM FL_OPTIMIZE_FUNCTION
void wave8Transpose_16_fused(const fl::u8 lanes[16],
                             const fl::Wave8ByteExpansionLut &lut,
                             fl::u8 *output /*128 bytes*/) {
    const fl::Wave8Byte *e[16];
    for (int lane = 0; lane < 16; ++lane) {
        e[lane] = &lut.lut[lanes[lane]];
    }
    for (int s = 0; s < 8; ++s) {
        fl::u8 l[16];
        for (int lane = 0; lane < 16; ++lane) {
            l[lane] = e[lane]->symbols[s].data;
        }
        fl::detail::spread_transpose16_symbol(l, output + s * 16);
    }
}

// --- #2548 L2 inner: same call shape as baseline, but pass the DMA output
// straight in (skip the 128-B stack `transposed[]` + memcpy).
FASTLED_FORCE_INLINE FL_IRAM
fl::u32 fl_parlio_inner_l2(const fl::u8 *scratch,
                           fl::size_t lane_stride,
                           fl::size_t byte_offset,
                           const fl::Wave8ByteExpansionLut &byte_lut,
                           fl::u8 *output,
                           fl::size_t output_idx) {
    fl::u8 lanes[16];
    for (fl::size_t lane = 0; lane < 16; lane++) {
        lanes[lane] = scratch[lane * lane_stride + byte_offset];
    }
    fl::wave8Transpose_16(
        reinterpret_cast<const fl::u8(&)[16]>(lanes), byte_lut,
        *reinterpret_cast<fl::u8(*)[16 * sizeof(fl::Wave8Byte)]>(output + output_idx));
    return static_cast<fl::u32>(output[output_idx] ^ output[output_idx + 127]);
}

// --- #2548 L1+L2 inner: fused transpose into the DMA output directly.
FASTLED_FORCE_INLINE FL_IRAM
fl::u32 fl_parlio_inner_l1l2(const fl::u8 *scratch,
                             fl::size_t lane_stride,
                             fl::size_t byte_offset,
                             const fl::Wave8ByteExpansionLut &byte_lut,
                             fl::u8 *output,
                             fl::size_t output_idx) {
    fl::u8 lanes[16];
    for (fl::size_t lane = 0; lane < 16; lane++) {
        lanes[lane] = scratch[lane * lane_stride + byte_offset];
    }
    wave8Transpose_16_fused(lanes, byte_lut, output + output_idx);
    return static_cast<fl::u32>(output[output_idx] ^ output[output_idx + 127]);
}

// --- #2548 L4 (lazy/PSRAM combined byte+lane+symbol LUT) ---
// Layout: lut[symbol_idx][lane_position][input_byte][output_byte_offset].
// Each (s, l, b) 16-byte slot pre-encodes the byte's full contribution to
// symbol s's 16-byte output area when expanded+transposed at lane position l.
// Total: 8 × 16 × 256 × 16 = 524288 bytes = 512 KB — lives in PSRAM, not SRAM.
// Per byte_position kernel = 8 symbols × 16 lanes × 4-u32 OR-reduce = the
// algorithmic ceiling for single-core / single-Wave8 (#2548 deferred entry,
// brought back via lazy allocation).
constexpr fl::size FL_L4_LUT_BYTES = 8u * 16u * 256u * 16u;
using Wave8L4LutSpan = fl::span<fl::u8, FL_L4_LUT_BYTES>;
using Wave8L4LutConstSpan = fl::span<const fl::u8, FL_L4_LUT_BYTES>;

// Build the L4 LUT by calling the existing spread-LUT transpose on dummy lanes
// (one lane non-zero at a time) — correct by construction. The dst span is a
// fixed-extent compile-time-sized view of FL_L4_LUT_BYTES bytes, so callers
// must back it with an allocation of exactly that size.
inline void buildWave8L4Lut(const fl::Wave8ByteExpansionLut &byte_lut,
                            Wave8L4LutSpan dst) {
    fl::u8 *base = dst.data();
    // base[s * 16*256*16 + lane * 256*16 + byte * 16 + i]
    for (int s = 0; s < 8; ++s) {
        for (int lane = 0; lane < 16; ++lane) {
            for (int b = 0; b < 256; ++b) {
                fl::u8 dummy[16] = {0};
                dummy[lane] = byte_lut.lut[b].symbols[s].data;
                fl::u8 contrib[16];
                fl::detail::spread_transpose16_symbol(dummy, contrib);
                fl::u8 *entry = base + ((static_cast<fl::size>(s) * 16 + lane) * 256 + b) * 16;
                for (int i = 0; i < 16; ++i) entry[i] = contrib[i];
            }
        }
    }
}

FASTLED_FORCE_INLINE FL_IRAM FL_OPTIMIZE_FUNCTION
void wave8Transpose_16_l4(const fl::u8 lanes[16],
                          Wave8L4LutConstSpan l4_lut,
                          fl::u8 *output /*128 bytes*/) {
    const fl::u8 *base = l4_lut.data();
    for (int s = 0; s < 8; ++s) {
        fl::u32 acc0 = 0, acc1 = 0, acc2 = 0, acc3 = 0;
        const fl::u8 *sym_base = base + static_cast<fl::size>(s) * 16 * 256 * 16;
        for (int lane = 0; lane < 16; ++lane) {
            const fl::u8 *entry =
                sym_base + (static_cast<fl::size>(lane) * 256 + lanes[lane]) * 16;
            const fl::u32 *u = fl::bit_cast_ptr<const fl::u32>(entry);
            acc0 |= u[0];
            acc1 |= u[1];
            acc2 |= u[2];
            acc3 |= u[3];
        }
        fl::u32 *out_u = fl::bit_cast_ptr<fl::u32>(output + s * 16);
        out_u[0] = acc0;
        out_u[1] = acc1;
        out_u[2] = acc2;
        out_u[3] = acc3;
    }
}

// --- #2548 L1+L2+L3 inner: process 4 byte-positions per call. Strided gather
// pulls 4 consecutive bytes per lane, then 4 fused transposes back-to-back.
FASTLED_FORCE_INLINE FL_IRAM
fl::u32 fl_parlio_inner_l1l2l3_4tile(const fl::u8 *scratch,
                                     fl::size_t lane_stride,
                                     fl::size_t byte_offset_base,
                                     const fl::Wave8ByteExpansionLut &byte_lut,
                                     fl::u8 *output,
                                     fl::size_t output_idx_base) {
    fl::u8 lanes0[16], lanes1[16], lanes2[16], lanes3[16];
    for (fl::size_t lane = 0; lane < 16; lane++) {
        const fl::u8 *p = &scratch[lane * lane_stride + byte_offset_base];
        lanes0[lane] = p[0];
        lanes1[lane] = p[1];
        lanes2[lane] = p[2];
        lanes3[lane] = p[3];
    }
    wave8Transpose_16_fused(lanes0, byte_lut, output + output_idx_base + 0);
    wave8Transpose_16_fused(lanes1, byte_lut, output + output_idx_base + 128);
    wave8Transpose_16_fused(lanes2, byte_lut, output + output_idx_base + 256);
    wave8Transpose_16_fused(lanes3, byte_lut, output + output_idx_base + 384);
    return static_cast<fl::u32>(
        output[output_idx_base + 0] ^ output[output_idx_base + 511]);
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
    (void)scratch_size;
    (void)output_size;

    fl::u32 t0 = micros();
    for (int it = 0; it < iters_byte_positions; ++it) {
        const fl::size_t byte_offset =
            static_cast<fl::size_t>(it) % BYTES_PER_LANE;
        const fl::size_t output_idx =
            byte_offset * LANES * sizeof(fl::Wave8Byte);
        *sink ^= fl_parlio_inner_one_byte_position(
            scratch, lane_stride, byte_offset, byte_lut, output, output_idx);
    }
    return micros() - t0;
}

// Variant measure for L2 and L1+L2 (same per-iter structure as baseline).
template <fl::u32 (*Inner)(const fl::u8 *, fl::size_t, fl::size_t,
                            const fl::Wave8ByteExpansionLut &, fl::u8 *,
                            fl::size_t)>
inline fl::u32 fl_parlio_measure_variant(const fl::u8 *scratch,
                                         fl::u8 *output,
                                         const fl::Wave8ByteExpansionLut &byte_lut,
                                         int iters_byte_positions,
                                         volatile fl::u32 *sink) {
    constexpr fl::size_t LANES = 16;
    constexpr fl::size_t BYTES_PER_LANE = 768;
    const fl::size_t lane_stride = BYTES_PER_LANE;

    fl::u32 t0 = micros();
    for (int it = 0; it < iters_byte_positions; ++it) {
        const fl::size_t byte_offset =
            static_cast<fl::size_t>(it) % BYTES_PER_LANE;
        const fl::size_t output_idx =
            byte_offset * LANES * sizeof(fl::Wave8Byte);
        *sink ^=
            Inner(scratch, lane_stride, byte_offset, byte_lut, output, output_idx);
    }
    return micros() - t0;
}

// L4 measure: SRAM scratch + SRAM output; LUT in PSRAM (already proven to be
// transparent to encode time, #2547).
inline fl::u32 fl_parlio_measure_l4(const fl::u8 *scratch,
                                    fl::u8 *output,
                                    Wave8L4LutConstSpan l4_lut,
                                    int iters_byte_positions,
                                    volatile fl::u32 *sink) {
    constexpr fl::size LANES = 16;
    constexpr fl::size BYTES_PER_LANE = 768;
    const fl::size lane_stride = BYTES_PER_LANE;

    fl::u32 t0 = micros();
    for (int it = 0; it < iters_byte_positions; ++it) {
        const fl::size byte_offset =
            static_cast<fl::size>(it) % BYTES_PER_LANE;
        const fl::size output_idx =
            byte_offset * LANES * sizeof(fl::Wave8Byte);
        fl::u8 lanes[16];
        for (fl::size lane = 0; lane < 16; lane++) {
            lanes[lane] = scratch[lane * lane_stride + byte_offset];
        }
        wave8Transpose_16_l4(lanes, l4_lut, output + output_idx);
        *sink ^= static_cast<fl::u32>(
            output[output_idx] ^ output[output_idx + 127]);
    }
    return micros() - t0;
}

// L1+L2+L3 measure: each iter does 4 byte-positions. iters_byte_positions is
// the total byte-positions to cover, so the outer loop runs iters/4 times.
inline fl::u32 fl_parlio_measure_l1l2l3(const fl::u8 *scratch,
                                        fl::u8 *output,
                                        const fl::Wave8ByteExpansionLut &byte_lut,
                                        int iters_byte_positions,
                                        volatile fl::u32 *sink) {
    constexpr fl::size_t LANES = 16;
    constexpr fl::size_t BYTES_PER_LANE = 768;
    const fl::size_t lane_stride = BYTES_PER_LANE;
    const int outer_iters = iters_byte_positions / 4;

    fl::u32 t0 = micros();
    for (int it = 0; it < outer_iters; ++it) {
        const fl::size_t byte_offset_base =
            (static_cast<fl::size_t>(it) * 4) % BYTES_PER_LANE;
        const fl::size_t output_idx_base =
            byte_offset_base * LANES * sizeof(fl::Wave8Byte);
        *sink ^= fl_parlio_inner_l1l2l3_4tile(
            scratch, lane_stride, byte_offset_base, byte_lut, output, output_idx_base);
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
    fl::u8 *scratch_psram = fl_parlio_alloc(SCRATCH_BYTES, /*psram=*/true);
    fl::u8 *output_sram = fl_parlio_alloc(OUTPUT_BYTES, /*psram=*/false);
    fl::u8 *output_psram = fl_parlio_alloc(OUTPUT_BYTES, /*psram=*/true);

    if (!scratch_sram || !scratch_psram || !output_sram || !output_psram) {
        // Partial alloc failed; bail with zeros so the caller knows.
        heap_caps_free(scratch_sram);
        heap_caps_free(scratch_psram);
        heap_caps_free(output_sram);
        heap_caps_free(output_psram);
        return result;
    }

    // Fill both scratches with representative LED data.
    for (fl::size_t i = 0; i < SCRATCH_BYTES; ++i) {
        const fl::u8 v = static_cast<fl::u8>((i * 31 + 7) & 0xFF);
        scratch_sram[i] = v;
        scratch_psram[i] = v;
    }
    fl::memset(output_sram, 0, OUTPUT_BYTES);
    fl::memset(output_psram, 0, OUTPUT_BYTES);

    volatile fl::u32 sink = 0;

    // Warm caches / icache.
    fl_parlio_measure(scratch_sram, SCRATCH_BYTES, output_sram, OUTPUT_BYTES,
                      byte_lut, 64, &sink);

    // Four placements. Each runs the same iters so totals are comparable.
    result.perpos_ss_us = fl_parlio_measure(
        scratch_sram, SCRATCH_BYTES, output_sram, OUTPUT_BYTES,
        byte_lut, iters_in, &sink);
    result.perpos_sp_us = fl_parlio_measure(
        scratch_sram, SCRATCH_BYTES, output_psram, OUTPUT_BYTES,
        byte_lut, iters_in, &sink);
    result.perpos_ps_us = fl_parlio_measure(
        scratch_psram, SCRATCH_BYTES, output_sram, OUTPUT_BYTES,
        byte_lut, iters_in, &sink);
    result.perpos_pp_us = fl_parlio_measure(
        scratch_psram, SCRATCH_BYTES, output_psram, OUTPUT_BYTES,
        byte_lut, iters_in, &sink);

    // #2548 algorithmic variants — all SRAM/SRAM so the algorithm is isolated.
    result.var_l2_us = fl_parlio_measure_variant<fl_parlio_inner_l2>(
        scratch_sram, output_sram, byte_lut, iters_in, &sink);
    result.var_l1l2_us = fl_parlio_measure_variant<fl_parlio_inner_l1l2>(
        scratch_sram, output_sram, byte_lut, iters_in, &sink);
    result.var_l1l2l3_us = fl_parlio_measure_l1l2l3(
        scratch_sram, output_sram, byte_lut, iters_in, &sink);

    // #2548 L4: lazy alloc the 512-KB LUT from PSRAM, build, time, free.
    fl::u8 *l4_buf = fl_parlio_alloc(FL_L4_LUT_BYTES, /*psram=*/true);
    if (l4_buf != nullptr) {
        Wave8L4LutSpan l4_lut(l4_buf, FL_L4_LUT_BYTES);
        result.l4_lut_bytes = static_cast<fl::u32>(FL_L4_LUT_BYTES);
        fl::memset(l4_buf, 0, FL_L4_LUT_BYTES);
        buildWave8L4Lut(byte_lut, l4_lut);
        Wave8L4LutConstSpan l4_lut_const(l4_buf, FL_L4_LUT_BYTES);
        // Warm L2 with the LUT before timing (one full sweep through symbols).
        {
            fl::u8 dummy_lanes[16] = {0};
            fl::u8 dummy_out[128];
            (void)wave8Transpose_16_l4(dummy_lanes, l4_lut_const, dummy_out);
        }
        result.var_l4_us = fl_parlio_measure_l4(
            scratch_sram, output_sram, l4_lut_const, iters_in, &sink);
        heap_caps_free(l4_buf);
    }

    result.sink = static_cast<fl::u32>(sink);

    heap_caps_free(scratch_sram);
    heap_caps_free(scratch_psram);
    heap_caps_free(output_sram);
    heap_caps_free(output_psram);
    return result;
}

inline void printParlioEncodeResultRom(const ParlioEncodeResult &r) {
    if (r.iters == 0) {
        esp_rom_printf("\nBENCH_PARLIO_START (issue #2526 follow-up)\n");
        esp_rom_printf("BENCH_PARLIO ERROR: heap allocation failed\n");
        esp_rom_printf("BENCH_PARLIO_END\n\n");
        return;
    }

    // Per-byte-position cost (microseconds * 1000 for sub-us precision)
    const fl::u64 iters64 = r.iters;
    const fl::u32 ss_ns_per = static_cast<fl::u32>(static_cast<fl::u64>(r.perpos_ss_us) * 1000 / iters64);
    const fl::u32 sp_ns_per = static_cast<fl::u32>(static_cast<fl::u64>(r.perpos_sp_us) * 1000 / iters64);
    const fl::u32 ps_ns_per = static_cast<fl::u32>(static_cast<fl::u64>(r.perpos_ps_us) * 1000 / iters64);
    const fl::u32 pp_ns_per = static_cast<fl::u32>(static_cast<fl::u64>(r.perpos_pp_us) * 1000 / iters64);

    // Frame-equivalent micros: production frame = 768 byte-positions.
    const fl::u32 ss_frame_us = static_cast<fl::u32>(static_cast<fl::u64>(r.perpos_ss_us) * 768 / iters64);
    const fl::u32 sp_frame_us = static_cast<fl::u32>(static_cast<fl::u64>(r.perpos_sp_us) * 768 / iters64);
    const fl::u32 ps_frame_us = static_cast<fl::u32>(static_cast<fl::u64>(r.perpos_ps_us) * 768 / iters64);
    const fl::u32 pp_frame_us = static_cast<fl::u32>(static_cast<fl::u64>(r.perpos_pp_us) * 768 / iters64);

    // PSRAM-vs-SRAM ratio x100 (>100 means PSRAM is slower)
    fl::u32 ratio_pp_ss_x100 = (r.perpos_ss_us > 0)
        ? static_cast<fl::u32>(static_cast<fl::u64>(r.perpos_pp_us) * 100 / r.perpos_ss_us) : 0;
    fl::u32 ratio_ps_ss_x100 = (r.perpos_ss_us > 0)
        ? static_cast<fl::u32>(static_cast<fl::u64>(r.perpos_ps_us) * 100 / r.perpos_ss_us) : 0;
    fl::u32 ratio_sp_ss_x100 = (r.perpos_ss_us > 0)
        ? static_cast<fl::u32>(static_cast<fl::u64>(r.perpos_sp_us) * 100 / r.perpos_ss_us) : 0;

    // WS2812B 800kHz, 16 lanes in parallel, 256 LEDs * 24 bits * 1.25 us/bit.
    constexpr fl::u32 WS2812B_FRAME_US = 7680;

    esp_rom_printf("\nBENCH_PARLIO_START (16 lanes x 256 LEDs, byte-LUT, #2526 follow-up)\n");
    esp_rom_printf("BENCH_PARLIO iters=%u lanes=%u leds=%u sink=%u\n",
                   r.iters, r.lanes, r.leds_per_lane, r.sink);
    esp_rom_printf("BENCH_PARLIO perpos_us  ss=%u sp=%u ps=%u pp=%u\n",
                   r.perpos_ss_us, r.perpos_sp_us, r.perpos_ps_us, r.perpos_pp_us);
    esp_rom_printf("BENCH_PARLIO perpos_ns_each  ss=%u sp=%u ps=%u pp=%u\n",
                   ss_ns_per, sp_ns_per, ps_ns_per, pp_ns_per);
    esp_rom_printf("BENCH_PARLIO frame_equiv_us  ss=%u sp=%u ps=%u pp=%u  ws2812b_tx=%u\n",
                   ss_frame_us, sp_frame_us, ps_frame_us, pp_frame_us, WS2812B_FRAME_US);
    esp_rom_printf("BENCH_PARLIO ratio_x100  sp_vs_ss=%u ps_vs_ss=%u pp_vs_ss=%u\n",
                   ratio_sp_ss_x100, ratio_ps_ss_x100, ratio_pp_ss_x100);

    // #2548 algorithmic variants
    const fl::u32 l2_frame_us = static_cast<fl::u32>(static_cast<fl::u64>(r.var_l2_us) * 768 / iters64);
    const fl::u32 l1l2_frame_us = static_cast<fl::u32>(static_cast<fl::u64>(r.var_l1l2_us) * 768 / iters64);
    const fl::u32 l1l2l3_frame_us = static_cast<fl::u32>(static_cast<fl::u64>(r.var_l1l2l3_us) * 768 / iters64);
    fl::u32 spd_l2_x100 = (r.var_l2_us > 0)
        ? static_cast<fl::u32>(static_cast<fl::u64>(r.perpos_ss_us) * 100 / r.var_l2_us) : 0;
    fl::u32 spd_l1l2_x100 = (r.var_l1l2_us > 0)
        ? static_cast<fl::u32>(static_cast<fl::u64>(r.perpos_ss_us) * 100 / r.var_l1l2_us) : 0;
    fl::u32 spd_l1l2l3_x100 = (r.var_l1l2l3_us > 0)
        ? static_cast<fl::u32>(static_cast<fl::u64>(r.perpos_ss_us) * 100 / r.var_l1l2l3_us) : 0;
    esp_rom_printf("BENCH_PARLIO variants_us  baseline=%u L2=%u L1+L2=%u L1+L2+L3=%u\n",
                   r.perpos_ss_us, r.var_l2_us, r.var_l1l2_us, r.var_l1l2l3_us);
    esp_rom_printf("BENCH_PARLIO variants_frame_us  baseline=%u L2=%u L1+L2=%u L1+L2+L3=%u  ws2812b_tx=%u\n",
                   ss_frame_us, l2_frame_us, l1l2_frame_us, l1l2l3_frame_us, WS2812B_FRAME_US);
    esp_rom_printf("BENCH_PARLIO variants_speedup_x100  L2=%u L1+L2=%u L1+L2+L3=%u (baseline=100)\n",
                   spd_l2_x100, spd_l1l2_x100, spd_l1l2l3_x100);

    // L4 (lazy PSRAM LUT) — separate line because it's a big-LUT class of variant.
    if (r.var_l4_us > 0) {
        const fl::u32 l4_frame_us =
            static_cast<fl::u32>(static_cast<fl::u64>(r.var_l4_us) * 768 / iters64);
        const fl::u32 spd_l4_x100 =
            static_cast<fl::u32>(static_cast<fl::u64>(r.perpos_ss_us) * 100 / r.var_l4_us);
        esp_rom_printf("BENCH_PARLIO L4_lazy_psram  lut_bytes=%u total_us=%u frame_us=%u speedup_x100=%u (baseline=100)\n",
                       r.l4_lut_bytes, r.var_l4_us, l4_frame_us, spd_l4_x100);
    } else {
        esp_rom_printf("BENCH_PARLIO L4_lazy_psram  alloc FAILED (lut_bytes=%u)\n",
                       static_cast<fl::u32>(FL_L4_LUT_BYTES));
    }

    esp_rom_printf("BENCH_PARLIO_END\n\n");
}

#else // !FL_PARLIO_BENCH_ENABLED

inline ParlioEncodeResult measureParlioEncode(int /*iters*/ = 12000) { return {}; }
inline void printParlioEncodeResultRom(const ParlioEncodeResult & /*r*/) {}

#endif

} // namespace parlio_bench
} // namespace autoresearch
