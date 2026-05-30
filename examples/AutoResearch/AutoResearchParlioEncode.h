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
// Per-stage cycle-counter instrumentation (#2548 Phase 0). Routes to
// esp_cpu_get_cycle_count() on IDF v5+ — single inline call, ~few cycle overhead.
#include "platforms/esp/32/core/clock_cycles.h"  // ok platform headers — needed for __clock_cycles() (P4 RV32 cycle counter, #2548)
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

    // #2548 L4 NIBBLE prototype — same shape as L4 but nibble-indexed (16
    // entries/lane instead of 256), split by hi/lo nibble halves. 32 KB total —
    // fits trivially in P4's 128 KB L2 cache, unlike byte-L4.
    fl::u32 var_l4n_us;
    fl::u32 l4n_lut_bytes;     // 32768 expected

    // #2548 L7 prototype — pre-shifted spread LUT, 512 B, L1-resident.
    // kSpreadPreShifted[lane_pos][nibble] = kSpreadNibble[nibble] << lane_pos.
    // Eliminates 256 runtime shifts per byte-position from the OR-tree's
    // critical path without growing memory traffic.
    fl::u32 var_l7_us;
    fl::u32 l7_lut_bytes;      // 512 expected

    // #2548 Phase 3 — Register-cached fused v2: caches 16 × u32 PAIRS of
    // expansion bytes (16 GPRs live at a time across two outer loop halves)
    // instead of the L1 variant's 16 × Wave8Byte* pointers. Fixes L1's
    // register-pressure regression. Attacks the 37% expand stage.
    fl::u32 var_fused_v2_us;

    // Post-L2 follow-up — pipe2: process TWO consecutive byte positions per
    // outer-loop iteration with software pipelining. Two independent OR-trees
    // live in the same symbol-major inner loop so the compiler can interleave
    // ALU ops from position N with load completions from position N+1 (and
    // vice versa). P4 is single-issue in-order, but the Phase 0 measurement
    // showed transpose taking ~5.6 cyc/op vs the 1-cyc/op theoretical max →
    // ~80 % of cycles are bubbles (load-use stalls + dependency chains).
    // Filling those bubbles with independent work is the one un-attacked angle.
    fl::u32 var_pipe2_us;

    fl::u32 sink;
};

// #2548 Phase 0: per-stage cycle-counter breakdown. Drives which of L7 / fused-v2
// / L5 to prioritize. Times each stage of the production hot loop separately:
//   gather:    16 strided u8 reads from scratch -> lanes[16]
//   expand:    16 x wave8_expand_byte (byte_lut -> Wave8Byte intermediate)
//   transpose: 8 x spread_transpose16_symbol (the spread-LUT)
//   store:     128-B memcpy to DMA output buffer
struct ParlioStageBreakdown {
    fl::u32 iters;
    fl::u64 gather_cyc;
    fl::u64 expand_cyc;
    fl::u64 transpose_cyc;
    fl::u64 store_cyc;
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

// --- #2548 L4 NIBBLE (32 KB, fits in L2 cache) ---
// Splits the byte dimension by nibble — the natural wave8 expansion is already
// nibble-based (hi nibble of input → symbols 0..3, lo nibble → symbols 4..7).
//
// Layout (32 KB total = 2 × 16 × 16 × 64):
//   first  16 KB: lut_hi[lane][nibble][i] — 64 bytes covering symbols 0..3
//   second 16 KB: lut_lo[lane][nibble][i] — 64 bytes covering symbols 4..7
//
// Per byte_position the working set is 16 × 16 × 64 × 2 = 32 KB — fits L2
// trivially (vs the 512 KB byte-L4 which thrashed L2 → 7.7× regression).
constexpr fl::size FL_L4N_LUT_BYTES = 2u * 16u * 16u * 64u; // 32768
using Wave8L4NibbleSpan = fl::span<fl::u8, FL_L4N_LUT_BYTES>;
using Wave8L4NibbleConstSpan = fl::span<const fl::u8, FL_L4N_LUT_BYTES>;

inline void buildWave8L4NibbleLut(const fl::Wave8ByteExpansionLut &byte_lut,
                                  Wave8L4NibbleSpan dst) {
    fl::u8 *base = dst.data();
    constexpr fl::size HI_BYTES = 16u * 16u * 64u; // 16 KB hi half
    fl::u8 *hi = base;
    fl::u8 *lo = base + HI_BYTES;

    // Hi-nibble entries: fake byte = nibble << 4 (hi nibble = nibble, lo = 0).
    // wave8 expansion of this byte produces symbols 0..3 (from kNibbleLut[hi])
    // and symbols 4..7 = 0 — so only symbols 0..3 contribute.
    for (int lane = 0; lane < 16; ++lane) {
        for (int n = 0; n < 16; ++n) {
            const fl::u8 fake_byte = static_cast<fl::u8>(n << 4);
            for (int s = 0; s < 4; ++s) {
                fl::u8 dummy[16] = {0};
                dummy[lane] = byte_lut.lut[fake_byte].symbols[s].data;
                fl::u8 contrib[16];
                fl::detail::spread_transpose16_symbol(dummy, contrib);
                fl::u8 *entry =
                    hi + (static_cast<fl::size>(lane) * 16 + n) * 64 + s * 16;
                for (int i = 0; i < 16; ++i) entry[i] = contrib[i];
            }
        }
    }

    // Lo-nibble entries: fake byte = nibble (hi = 0, lo nibble = nibble).
    // Only symbols 4..7 contribute.
    for (int lane = 0; lane < 16; ++lane) {
        for (int n = 0; n < 16; ++n) {
            const fl::u8 fake_byte = static_cast<fl::u8>(n);
            for (int s = 4; s < 8; ++s) {
                fl::u8 dummy[16] = {0};
                dummy[lane] = byte_lut.lut[fake_byte].symbols[s].data;
                fl::u8 contrib[16];
                fl::detail::spread_transpose16_symbol(dummy, contrib);
                fl::u8 *entry =
                    lo + (static_cast<fl::size>(lane) * 16 + n) * 64 + (s - 4) * 16;
                for (int i = 0; i < 16; ++i) entry[i] = contrib[i];
            }
        }
    }
}

FASTLED_FORCE_INLINE FL_IRAM FL_OPTIMIZE_FUNCTION
void wave8Transpose_16_l4_nibble(const fl::u8 lanes[16],
                                 Wave8L4NibbleConstSpan lut,
                                 fl::u8 *output /*128 bytes*/) {
    const fl::u8 *base = lut.data();
    const fl::u8 *hi_base = base;
    const fl::u8 *lo_base = base + 16 * 16 * 64;

    // OR straight into the output (zeroed first). Output is split: bytes 0..63
    // = symbols 0..3 (hi nibble), bytes 64..127 = symbols 4..7 (lo nibble).
    fl::u32 *out_hi = fl::bit_cast_ptr<fl::u32>(output);
    fl::u32 *out_lo = fl::bit_cast_ptr<fl::u32>(output + 64);
    for (int i = 0; i < 16; ++i) {
        out_hi[i] = 0;
        out_lo[i] = 0;
    }
    for (int lane = 0; lane < 16; ++lane) {
        const fl::u8 b = lanes[lane];
        const fl::u32 *uhi = fl::bit_cast_ptr<const fl::u32>(
            hi_base + (static_cast<fl::size>(lane) * 16 + (b >> 4)) * 64);
        const fl::u32 *ulo = fl::bit_cast_ptr<const fl::u32>(
            lo_base + (static_cast<fl::size>(lane) * 16 + (b & 0xF)) * 64);
        for (int i = 0; i < 16; ++i) {
            out_hi[i] |= uhi[i];
            out_lo[i] |= ulo[i];
        }
    }
}

inline fl::u32 fl_parlio_measure_l4n(const fl::u8 *scratch,
                                     fl::u8 *output,
                                     Wave8L4NibbleConstSpan l4n_lut,
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
        wave8Transpose_16_l4_nibble(lanes, l4n_lut, output + output_idx);
        *sink ^= static_cast<fl::u32>(
            output[output_idx] ^ output[output_idx + 127]);
    }
    return micros() - t0;
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

// --- #2548 L7: pre-shifted spread LUT, 512 B, L1-resident ---
//
// kSpreadPreShifted[lane_pos][nibble] = kSpreadNibble[nibble] << lane_pos.
// Indexed by (lane_pos ∈ 0..7, nibble ∈ 0..15). Replaces the runtime shifts in
// spread_transpose16_symbol's OR-tree without growing memory traffic — same
// 32 loads per call, just from a pre-shifted table.
//
// Mirror values of kSpreadNibble for reference:
//   {0x00000000, 0x01000000, 0x00010000, 0x01010000,
//    0x00000100, 0x01000100, 0x00010100, 0x01010100,
//    0x00000001, 0x01000001, 0x00010001, 0x01010001,
//    0x00000101, 0x01000101, 0x00010101, 0x01010101};
// 512 B total, well under P4 L1d (32 KB) — no eviction during the inner loop.

FL_ALIGNAS(16) fl::u32 g_kSpreadPreShifted[8][16];

inline void buildSpreadPreShifted() {
    static const fl::u32 kBase[16] = {
        0x00000000u, 0x01000000u, 0x00010000u, 0x01010000u,
        0x00000100u, 0x01000100u, 0x00010100u, 0x01010100u,
        0x00000001u, 0x01000001u, 0x00010001u, 0x01010001u,
        0x00000101u, 0x01000101u, 0x00010101u, 0x01010101u,
    };
    for (int lane = 0; lane < 8; ++lane) {
        for (int n = 0; n < 16; ++n) {
            g_kSpreadPreShifted[lane][n] = kBase[n] << lane;
        }
    }
}

// L7-style spread_transpose16_symbol: same shape as the production one but
// the inner expressions are 8 loads + 7 ORs each (no shifts on the critical
// path). The store-narrowing block at the bottom is byte-identical.
FASTLED_FORCE_INLINE FL_IRAM FL_OPTIMIZE_FUNCTION
void spread_transpose16_symbol_preshifted(const fl::u8 l[16], fl::u8 out[16]) {
    const fl::u32 aLo =
        g_kSpreadPreShifted[0][l[0] >> 4] | g_kSpreadPreShifted[1][l[1] >> 4] |
        g_kSpreadPreShifted[2][l[2] >> 4] | g_kSpreadPreShifted[3][l[3] >> 4] |
        g_kSpreadPreShifted[4][l[4] >> 4] | g_kSpreadPreShifted[5][l[5] >> 4] |
        g_kSpreadPreShifted[6][l[6] >> 4] | g_kSpreadPreShifted[7][l[7] >> 4];
    const fl::u32 bLo =
        g_kSpreadPreShifted[0][l[0] & 0xF] | g_kSpreadPreShifted[1][l[1] & 0xF] |
        g_kSpreadPreShifted[2][l[2] & 0xF] | g_kSpreadPreShifted[3][l[3] & 0xF] |
        g_kSpreadPreShifted[4][l[4] & 0xF] | g_kSpreadPreShifted[5][l[5] & 0xF] |
        g_kSpreadPreShifted[6][l[6] & 0xF] | g_kSpreadPreShifted[7][l[7] & 0xF];
    const fl::u32 aHi =
        g_kSpreadPreShifted[0][l[ 8] >> 4] | g_kSpreadPreShifted[1][l[ 9] >> 4] |
        g_kSpreadPreShifted[2][l[10] >> 4] | g_kSpreadPreShifted[3][l[11] >> 4] |
        g_kSpreadPreShifted[4][l[12] >> 4] | g_kSpreadPreShifted[5][l[13] >> 4] |
        g_kSpreadPreShifted[6][l[14] >> 4] | g_kSpreadPreShifted[7][l[15] >> 4];
    const fl::u32 bHi =
        g_kSpreadPreShifted[0][l[ 8] & 0xF] | g_kSpreadPreShifted[1][l[ 9] & 0xF] |
        g_kSpreadPreShifted[2][l[10] & 0xF] | g_kSpreadPreShifted[3][l[11] & 0xF] |
        g_kSpreadPreShifted[4][l[12] & 0xF] | g_kSpreadPreShifted[5][l[13] & 0xF] |
        g_kSpreadPreShifted[6][l[14] & 0xF] | g_kSpreadPreShifted[7][l[15] & 0xF];

    // Identical store-narrowing to the production spread_transpose16_symbol.
    out[ 0] = static_cast<fl::u8>(aLo);        out[ 1] = static_cast<fl::u8>(aHi);
    out[ 2] = static_cast<fl::u8>(aLo >>  8);  out[ 3] = static_cast<fl::u8>(aHi >>  8);
    out[ 4] = static_cast<fl::u8>(aLo >> 16);  out[ 5] = static_cast<fl::u8>(aHi >> 16);
    out[ 6] = static_cast<fl::u8>(aLo >> 24);  out[ 7] = static_cast<fl::u8>(aHi >> 24);
    out[ 8] = static_cast<fl::u8>(bLo);        out[ 9] = static_cast<fl::u8>(bHi);
    out[10] = static_cast<fl::u8>(bLo >>  8);  out[11] = static_cast<fl::u8>(bHi >>  8);
    out[12] = static_cast<fl::u8>(bLo >> 16);  out[13] = static_cast<fl::u8>(bHi >> 16);
    out[14] = static_cast<fl::u8>(bLo >> 24);  out[15] = static_cast<fl::u8>(bHi >> 24);
}

// L7 transpose: same shape as wave8Transpose_16 byte-LUT path but the inner
// spread_transpose calls use the pre-shifted table.
FASTLED_FORCE_INLINE FL_IRAM FL_OPTIMIZE_FUNCTION
void wave8Transpose_16_l7(const fl::u8 lanes[16],
                          const fl::Wave8ByteExpansionLut &lut,
                          fl::u8 *output) {
    fl::Wave8Byte laneWaveformSymbols[16];
    for (int lane = 0; lane < 16; ++lane) {
        fl::detail::wave8_expand_byte(lanes[lane], lut, &laneWaveformSymbols[lane]);
    }
    for (int s = 0; s < 8; ++s) {
        fl::u8 l[16];
        for (int lane = 0; lane < 16; ++lane) {
            l[lane] = laneWaveformSymbols[lane].symbols[s].data;
        }
        spread_transpose16_symbol_preshifted(l, output + s * 16);
    }
}

FASTLED_FORCE_INLINE FL_IRAM
fl::u32 fl_parlio_inner_l7(const fl::u8 *scratch,
                           fl::size_t lane_stride,
                           fl::size_t byte_offset,
                           const fl::Wave8ByteExpansionLut &byte_lut,
                           fl::u8 *output,
                           fl::size_t output_idx) {
    fl::u8 lanes[16];
    for (fl::size_t lane = 0; lane < 16; lane++) {
        lanes[lane] = scratch[lane * lane_stride + byte_offset];
    }
    wave8Transpose_16_l7(lanes, byte_lut, output + output_idx);
    return static_cast<fl::u32>(output[output_idx] ^ output[output_idx + 127]);
}

// --- #2548 Phase 3: Register-cached fused v2 ---
//
// Two-pass register-cached fusion that fixes L1's 16-pointer-array spill
// regression. Cache the per-lane Wave8Byte expansion as TWO u32 PAIRS (one
// holding symbols 0..3, one holding symbols 4..7). The symbol loop is split
// in half so only 16 u32 are live at any one time — fits in RV32's 32 GPRs
// without spilling. Eliminates the 128-B `laneWaveformSymbols` intermediate
// stack array entirely.
//
// Uses the production spread_transpose16_symbol (NOT the L7 preshifted one
// — that was a loss).
FASTLED_FORCE_INLINE FL_IRAM FL_OPTIMIZE_FUNCTION
void wave8Transpose_16_fused_v2(const fl::u8 lanes[16],
                                const fl::Wave8ByteExpansionLut &lut,
                                fl::u8 *output) {
    // Phase A: cache expansion bytes as 32 u32 (split into two halves so only
    // 16 are live across each symbol sub-loop).
    fl::u32 e_lo[16];
    fl::u32 e_hi[16];
    for (int lane = 0; lane < 16; ++lane) {
        const fl::u32 *p = fl::bit_cast_ptr<const fl::u32>(&lut.lut[lanes[lane]]);
        e_lo[lane] = p[0];
        e_hi[lane] = p[1];
    }

    // Phase B1: symbols 0..3 from e_lo. Only e_lo is needed → 16 u32 live.
    for (int s = 0; s < 4; ++s) {
        const int shift = s * 8;
        fl::u8 l[16];
        for (int lane = 0; lane < 16; ++lane) {
            l[lane] = static_cast<fl::u8>(e_lo[lane] >> shift);
        }
        fl::detail::spread_transpose16_symbol(l, output + s * 16);
    }

    // Phase B2: symbols 4..7 from e_hi. Only e_hi is needed → 16 u32 live.
    for (int s = 4; s < 8; ++s) {
        const int shift = (s - 4) * 8;
        fl::u8 l[16];
        for (int lane = 0; lane < 16; ++lane) {
            l[lane] = static_cast<fl::u8>(e_hi[lane] >> shift);
        }
        fl::detail::spread_transpose16_symbol(l, output + s * 16);
    }
}

FASTLED_FORCE_INLINE FL_IRAM
fl::u32 fl_parlio_inner_fused_v2(const fl::u8 *scratch,
                                 fl::size_t lane_stride,
                                 fl::size_t byte_offset,
                                 const fl::Wave8ByteExpansionLut &byte_lut,
                                 fl::u8 *output,
                                 fl::size_t output_idx) {
    fl::u8 lanes[16];
    for (fl::size_t lane = 0; lane < 16; lane++) {
        lanes[lane] = scratch[lane * lane_stride + byte_offset];
    }
    wave8Transpose_16_fused_v2(lanes, byte_lut, output + output_idx);
    return static_cast<fl::u32>(output[output_idx] ^ output[output_idx + 127]);
}

// --- Post-L2 pipe2: process 2 byte positions in lockstep ---
//
// Strategy: gather + expand both positions into stack Wave8Byte arrays first,
// then run a SINGLE symbol-major loop that transposes both positions back-to-
// back per symbol. Because spread_transpose16_symbol is FORCE_INLINE, the
// compiler sees both OR-trees in the same basic block and can interleave the
// independent table loads / shifts / ORs from position A with the same ops
// from position B. On in-order RV32 with 1-cyc shifts + 2-3 cyc load-use
// latency, this should hide A's load-use stalls under B's ALU work.
//
// Register-pressure budget: 2 × 128 B Wave8Byte arrays live on the stack
// (256 B), but at any given symbol the per-symbol working set is just
// la[16] + lb[16] = 32 bytes (8 GPRs worth) + the four 32-bit accumulators
// per call × 2 calls = 8 GPRs. Stays well inside the 32-GPR budget.
FASTLED_FORCE_INLINE FL_IRAM FL_OPTIMIZE_FUNCTION
void wave8Transpose_16x2_pipe2(const fl::u8 lanes_a[16],
                               const fl::u8 lanes_b[16],
                               const fl::Wave8ByteExpansionLut &lut,
                               fl::u8 *output_a,
                               fl::u8 *output_b) {
    fl::Wave8Byte e_a[16];
    fl::Wave8Byte e_b[16];
    // Phase A1: expand position A.
    for (int lane = 0; lane < 16; ++lane) {
        e_a[lane] = lut.lut[lanes_a[lane]];
    }
    // Phase A2: expand position B. Two independent expansion streams — compiler
    // is free to interleave A1 and A2 reads if it helps issue scheduling.
    for (int lane = 0; lane < 16; ++lane) {
        e_b[lane] = lut.lut[lanes_b[lane]];
    }
    // Phase B: symbol-major. The two transpose calls per symbol share no data
    // dependencies, so the compiler should interleave their OR-trees once they
    // are inlined.
    for (int s = 0; s < 8; ++s) {
        fl::u8 la[16];
        fl::u8 lb[16];
        for (int lane = 0; lane < 16; ++lane) {
            la[lane] = e_a[lane].symbols[s].data;
            lb[lane] = e_b[lane].symbols[s].data;
        }
        fl::detail::spread_transpose16_symbol(la, output_a + s * 16);
        fl::detail::spread_transpose16_symbol(lb, output_b + s * 16);
    }
}

FASTLED_FORCE_INLINE FL_IRAM
fl::u32 fl_parlio_inner_pipe2(const fl::u8 *scratch,
                              fl::size_t lane_stride,
                              fl::size_t byte_offset_a,
                              fl::size_t byte_offset_b,
                              const fl::Wave8ByteExpansionLut &byte_lut,
                              fl::u8 *output,
                              fl::size_t output_idx_a,
                              fl::size_t output_idx_b) {
    fl::u8 lanes_a[16];
    fl::u8 lanes_b[16];
    for (fl::size_t lane = 0; lane < 16; lane++) {
        lanes_a[lane] = scratch[lane * lane_stride + byte_offset_a];
        lanes_b[lane] = scratch[lane * lane_stride + byte_offset_b];
    }
    wave8Transpose_16x2_pipe2(lanes_a, lanes_b, byte_lut,
                              output + output_idx_a, output + output_idx_b);
    return static_cast<fl::u32>(
        output[output_idx_a] ^ output[output_idx_a + 127] ^
        output[output_idx_b] ^ output[output_idx_b + 127]);
}

// Pipe2 measure: each iter does 2 byte-positions. iters_byte_positions is
// the total byte-positions to cover, so the outer loop runs iters/2 times.
inline fl::u32 fl_parlio_measure_pipe2(const fl::u8 *scratch,
                                        fl::u8 *output,
                                        const fl::Wave8ByteExpansionLut &byte_lut,
                                        int iters_byte_positions,
                                        volatile fl::u32 *sink) {
    constexpr fl::size_t LANES = 16;
    constexpr fl::size_t BYTES_PER_LANE = 768;
    const fl::size_t lane_stride = BYTES_PER_LANE;
    const int outer_iters = iters_byte_positions / 2;

    fl::u32 t0 = micros();
    for (int it = 0; it < outer_iters; ++it) {
        const fl::size_t base = (static_cast<fl::size_t>(it) * 2) % BYTES_PER_LANE;
        const fl::size_t byte_offset_a = base;
        const fl::size_t byte_offset_b = (base + 1) % BYTES_PER_LANE;
        const fl::size_t output_idx_a =
            byte_offset_a * LANES * sizeof(fl::Wave8Byte);
        const fl::size_t output_idx_b =
            byte_offset_b * LANES * sizeof(fl::Wave8Byte);
        *sink ^= fl_parlio_inner_pipe2(
            scratch, lane_stride, byte_offset_a, byte_offset_b, byte_lut,
            output, output_idx_a, output_idx_b);
    }
    return micros() - t0;
}

inline fl::u8 *fl_parlio_alloc(fl::size_t size, bool psram) {
    return reinterpret_cast<fl::u8 *>(heap_caps_malloc(
        size,
        psram ? (MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT)
              : (MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT)));
}

// --- #2548 Phase 0: per-stage cycle-counter instrumentation ---
//
// Mirrors the production hot path but inserts `__clock_cycles()` reads between
// each stage, with `asm volatile("" ::: "memory")` barriers that prevent the
// compiler from hoisting/reordering across the timer boundary. Stage totals are
// accumulated across all iterations (single counter avoids per-iter overhead
// drift). The barriers cost ~0 instructions but keep the stages clearly
// separated in the resulting timing.
inline void fl_parlio_measure_stages(const fl::u8 *scratch,
                                     fl::u8 *output,
                                     const fl::Wave8ByteExpansionLut &byte_lut,
                                     int iters_byte_positions,
                                     ParlioStageBreakdown *sb,
                                     volatile fl::u32 *sink) {
    constexpr fl::size LANES = 16;
    constexpr fl::size BYTES_PER_LANE = 768;
    const fl::size lane_stride = BYTES_PER_LANE;

    fl::u64 gather_total = 0;
    fl::u64 expand_total = 0;
    fl::u64 transpose_total = 0;
    fl::u64 store_total = 0;

    for (int it = 0; it < iters_byte_positions; ++it) {
        const fl::size byte_offset =
            static_cast<fl::size>(it) % BYTES_PER_LANE;
        const fl::size output_idx =
            byte_offset * LANES * sizeof(fl::Wave8Byte);

        fl::u8 lanes[16];
        fl::Wave8Byte expanded[16];
        fl::u8 transposed[16 * sizeof(fl::Wave8Byte)];

        // STAGE 1: gather (16 strided u8 reads)
        const fl::u32 t0 = __clock_cycles();
        for (fl::size lane = 0; lane < 16; lane++) {
            lanes[lane] = scratch[lane * lane_stride + byte_offset];
        }
        asm volatile("" : "+m"(lanes) : : "memory");
        const fl::u32 t1 = __clock_cycles();

        // STAGE 2: expand (16 x wave8_expand_byte, byte-LUT path)
        for (int lane = 0; lane < 16; ++lane) {
            fl::detail::wave8_expand_byte(lanes[lane], byte_lut, &expanded[lane]);
        }
        asm volatile("" : "+m"(expanded) : : "memory");
        const fl::u32 t2 = __clock_cycles();

        // STAGE 3: spread-transpose (8 x spread_transpose16_symbol)
        fl::detail::wave8_transpose_16(expanded, transposed);
        asm volatile("" : "+m"(transposed) : : "memory");
        const fl::u32 t3 = __clock_cycles();

        // STAGE 4: store 128 B to output buffer
        fl::memcpy(output + output_idx, transposed, sizeof(transposed));
        asm volatile("" : "+m"(*(output + output_idx)) : : "memory");
        const fl::u32 t4 = __clock_cycles();

        gather_total    += static_cast<fl::u64>(t1 - t0);
        expand_total    += static_cast<fl::u64>(t2 - t1);
        transpose_total += static_cast<fl::u64>(t3 - t2);
        store_total     += static_cast<fl::u64>(t4 - t3);

        *sink ^= output[output_idx] ^ output[output_idx + 127];
    }

    sb->iters = static_cast<fl::u32>(iters_byte_positions);
    sb->gather_cyc = gather_total;
    sb->expand_cyc = expand_total;
    sb->transpose_cyc = transpose_total;
    sb->store_cyc = store_total;
    sb->sink = static_cast<fl::u32>(*sink);
}

} // anonymous namespace

// Run only the per-stage breakdown (smaller iter count by default to keep boot
// time reasonable when both this and measureParlioEncode fire at startup).
inline ParlioStageBreakdown
measureParlioStageBreakdown(int iters_in = 8000) {
    ParlioStageBreakdown result{};
    if (iters_in < 1) iters_in = 1;
    if (iters_in > 200000) iters_in = 200000;

    constexpr fl::size LANES = 16;
    constexpr fl::size BYTES_PER_LANE = 768;
    constexpr fl::size SCRATCH_BYTES = LANES * BYTES_PER_LANE;
    constexpr fl::size OUTPUT_BYTES = BYTES_PER_LANE * LANES * sizeof(fl::Wave8Byte);

    fl::ChipsetTiming timing;
    timing.T1 = 400;
    timing.T2 = 450;
    timing.T3 = 400;
    fl::Wave8BitExpansionLut nib_lut = fl::buildWave8ExpansionLUT(timing);
    fl::Wave8ByteExpansionLut byte_lut = fl::buildWave8ByteExpansionLUT(nib_lut);

    fl::u8 *scratch = fl_parlio_alloc(SCRATCH_BYTES, /*psram=*/false);
    fl::u8 *output = fl_parlio_alloc(OUTPUT_BYTES, /*psram=*/false);
    if (!scratch || !output) {
        heap_caps_free(scratch);
        heap_caps_free(output);
        return result;
    }

    for (fl::size i = 0; i < SCRATCH_BYTES; ++i) {
        scratch[i] = static_cast<fl::u8>((i * 31 + 7) & 0xFF);
    }
    fl::memset(output, 0, OUTPUT_BYTES);

    volatile fl::u32 sink = 0;

    // Warm caches / icache before timing.
    {
        ParlioStageBreakdown warm{};
        fl_parlio_measure_stages(scratch, output, byte_lut, 64, &warm, &sink);
    }

    fl_parlio_measure_stages(scratch, output, byte_lut, iters_in, &result, &sink);

    heap_caps_free(scratch);
    heap_caps_free(output);
    return result;
}

inline void
printParlioStageBreakdownRom(const ParlioStageBreakdown &sb) {
    if (sb.iters == 0) {
        esp_rom_printf("\nBENCH_PARLIO_STAGES ERROR: alloc failed\n\n");
        return;
    }
    constexpr fl::u32 FL_P4_MHZ = 360u;
    const fl::u64 iters64 = sb.iters;
    const fl::u64 total = sb.gather_cyc + sb.expand_cyc + sb.transpose_cyc + sb.store_cyc;

    const fl::u32 g_per = static_cast<fl::u32>(sb.gather_cyc    / iters64);
    const fl::u32 e_per = static_cast<fl::u32>(sb.expand_cyc    / iters64);
    const fl::u32 t_per = static_cast<fl::u32>(sb.transpose_cyc / iters64);
    const fl::u32 s_per = static_cast<fl::u32>(sb.store_cyc     / iters64);
    const fl::u32 sum_per = g_per + e_per + t_per + s_per;

    const fl::u32 g_ns = (g_per * 1000u) / FL_P4_MHZ;
    const fl::u32 e_ns = (e_per * 1000u) / FL_P4_MHZ;
    const fl::u32 t_ns = (t_per * 1000u) / FL_P4_MHZ;
    const fl::u32 s_ns = (s_per * 1000u) / FL_P4_MHZ;
    const fl::u32 sum_ns = g_ns + e_ns + t_ns + s_ns;

    const fl::u32 g_pct = total > 0 ? static_cast<fl::u32>(sb.gather_cyc    * 10000u / total) : 0;
    const fl::u32 e_pct = total > 0 ? static_cast<fl::u32>(sb.expand_cyc    * 10000u / total) : 0;
    const fl::u32 t_pct = total > 0 ? static_cast<fl::u32>(sb.transpose_cyc * 10000u / total) : 0;
    const fl::u32 s_pct = total > 0 ? static_cast<fl::u32>(sb.store_cyc     * 10000u / total) : 0;

    esp_rom_printf("\nBENCH_PARLIO_STAGES_START (issue #2548 Phase 0)\n");
    esp_rom_printf("BENCH_PARLIO_STAGES iters=%u cpu_mhz=%u sink=%u\n",
                   sb.iters, FL_P4_MHZ, sb.sink);
    esp_rom_printf("BENCH_PARLIO_STAGES cyc/iter  gather=%u expand=%u transpose=%u store=%u total=%u\n",
                   g_per, e_per, t_per, s_per, sum_per);
    esp_rom_printf("BENCH_PARLIO_STAGES ns/iter   gather=%u expand=%u transpose=%u store=%u total=%u\n",
                   g_ns, e_ns, t_ns, s_ns, sum_ns);
    esp_rom_printf("BENCH_PARLIO_STAGES pct_x100  gather=%u expand=%u transpose=%u store=%u (total=10000)\n",
                   g_pct, e_pct, t_pct, s_pct);
    esp_rom_printf("BENCH_PARLIO_STAGES_END\n\n");
}

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

    // #2548 Phase 2 — L7 pre-shifted spread LUT (512 B, L1-resident).
    buildSpreadPreShifted();
    result.l7_lut_bytes = static_cast<fl::u32>(sizeof(g_kSpreadPreShifted));
    // Warm icache on the new kernel.
    for (int i = 0; i < 64; ++i) {
        sink ^= fl_parlio_inner_l7(scratch_sram, BYTES_PER_LANE, i % BYTES_PER_LANE,
                                   byte_lut, output_sram,
                                   (i % BYTES_PER_LANE) * LANES * sizeof(fl::Wave8Byte));
    }
    result.var_l7_us = fl_parlio_measure_variant<fl_parlio_inner_l7>(
        scratch_sram, output_sram, byte_lut, iters_in, &sink);

    // #2548 Phase 3 — Register-cached fused v2 (fixes L1's spill regression).
    for (int i = 0; i < 64; ++i) {
        sink ^= fl_parlio_inner_fused_v2(scratch_sram, BYTES_PER_LANE,
                                         i % BYTES_PER_LANE, byte_lut, output_sram,
                                         (i % BYTES_PER_LANE) * LANES * sizeof(fl::Wave8Byte));
    }
    result.var_fused_v2_us = fl_parlio_measure_variant<fl_parlio_inner_fused_v2>(
        scratch_sram, output_sram, byte_lut, iters_in, &sink);

    // Post-L2 pipe2 (2-position software pipelining).
    for (int i = 0; i < 64; i += 2) {
        sink ^= fl_parlio_inner_pipe2(
            scratch_sram, BYTES_PER_LANE, i % BYTES_PER_LANE,
            (i + 1) % BYTES_PER_LANE, byte_lut, output_sram,
            (i % BYTES_PER_LANE) * LANES * sizeof(fl::Wave8Byte),
            ((i + 1) % BYTES_PER_LANE) * LANES * sizeof(fl::Wave8Byte));
    }
    result.var_pipe2_us = fl_parlio_measure_pipe2(
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

    // #2548 L4-NIBBLE: 32-KB LUT (fits L2). Could be SRAM, but use PSRAM here
    // to keep the bench's SRAM footprint stable + show that L2 hides PSRAM
    // when working set ≤ 128 KB.
    fl::u8 *l4n_buf = fl_parlio_alloc(FL_L4N_LUT_BYTES, /*psram=*/true);
    if (l4n_buf != nullptr) {
        Wave8L4NibbleSpan l4n_lut(l4n_buf, FL_L4N_LUT_BYTES);
        result.l4n_lut_bytes = static_cast<fl::u32>(FL_L4N_LUT_BYTES);
        fl::memset(l4n_buf, 0, FL_L4N_LUT_BYTES);
        buildWave8L4NibbleLut(byte_lut, l4n_lut);
        Wave8L4NibbleConstSpan l4n_lut_const(l4n_buf, FL_L4N_LUT_BYTES);
        {
            fl::u8 dummy_lanes[16] = {0};
            fl::u8 dummy_out[128];
            (void)wave8Transpose_16_l4_nibble(dummy_lanes, l4n_lut_const, dummy_out);
        }
        result.var_l4n_us = fl_parlio_measure_l4n(
            scratch_sram, output_sram, l4n_lut_const, iters_in, &sink);
        heap_caps_free(l4n_buf);
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

    // L7 (512 B pre-shifted spread LUT, L1-resident) — Phase 2 of #2548.
    if (r.var_l7_us > 0) {
        const fl::u32 l7_frame_us =
            static_cast<fl::u32>(static_cast<fl::u64>(r.var_l7_us) * 768 / iters64);
        const fl::u32 spd_l7_x100 =
            static_cast<fl::u32>(static_cast<fl::u64>(r.perpos_ss_us) * 100 / r.var_l7_us);
        esp_rom_printf("BENCH_PARLIO L7_preshift_l1 lut_bytes=%u total_us=%u frame_us=%u speedup_x100=%u (baseline=100)  ws2812b_tx=%u\n",
                       r.l7_lut_bytes, r.var_l7_us, l7_frame_us, spd_l7_x100,
                       WS2812B_FRAME_US);
    }

    // Fused v2 (register-cached, 32 u32 pairs) — Phase 3 of #2548.
    if (r.var_fused_v2_us > 0) {
        const fl::u32 fv2_frame_us =
            static_cast<fl::u32>(static_cast<fl::u64>(r.var_fused_v2_us) * 768 / iters64);
        const fl::u32 spd_fv2_x100 =
            static_cast<fl::u32>(static_cast<fl::u64>(r.perpos_ss_us) * 100 / r.var_fused_v2_us);
        esp_rom_printf("BENCH_PARLIO fused_v2_reg   total_us=%u frame_us=%u speedup_x100=%u (baseline=100)  ws2812b_tx=%u\n",
                       r.var_fused_v2_us, fv2_frame_us, spd_fv2_x100,
                       WS2812B_FRAME_US);
    }

    // Pipe2 (2-position software pipelining) — post-L2 ILP exploration.
    if (r.var_pipe2_us > 0) {
        const fl::u32 p2_frame_us =
            static_cast<fl::u32>(static_cast<fl::u64>(r.var_pipe2_us) * 768 / iters64);
        const fl::u32 spd_p2_x100 =
            static_cast<fl::u32>(static_cast<fl::u64>(r.perpos_ss_us) * 100 / r.var_pipe2_us);
        esp_rom_printf("BENCH_PARLIO pipe2_2pos     total_us=%u frame_us=%u speedup_x100=%u (baseline=100)  ws2812b_tx=%u\n",
                       r.var_pipe2_us, p2_frame_us, spd_p2_x100,
                       WS2812B_FRAME_US);
    }

    // L4-NIBBLE (32 KB, fits L2)
    if (r.var_l4n_us > 0) {
        const fl::u32 l4n_frame_us =
            static_cast<fl::u32>(static_cast<fl::u64>(r.var_l4n_us) * 768 / iters64);
        const fl::u32 spd_l4n_x100 =
            static_cast<fl::u32>(static_cast<fl::u64>(r.perpos_ss_us) * 100 / r.var_l4n_us);
        esp_rom_printf("BENCH_PARLIO L4_nibble      lut_bytes=%u total_us=%u frame_us=%u speedup_x100=%u (baseline=100)  ws2812b_tx=%u\n",
                       r.l4n_lut_bytes, r.var_l4n_us, l4n_frame_us, spd_l4n_x100,
                       WS2812B_FRAME_US);
    } else {
        esp_rom_printf("BENCH_PARLIO L4_nibble      alloc FAILED (lut_bytes=%u)\n",
                       static_cast<fl::u32>(FL_L4N_LUT_BYTES));
    }

    esp_rom_printf("BENCH_PARLIO_END\n\n");
}

#else // !FL_PARLIO_BENCH_ENABLED

inline ParlioEncodeResult measureParlioEncode(int /*iters*/ = 12000) { return {}; }
inline void printParlioEncodeResultRom(const ParlioEncodeResult & /*r*/) {}
inline ParlioStageBreakdown measureParlioStageBreakdown(int /*iters*/ = 8000) { return {}; }
inline void printParlioStageBreakdownRom(const ParlioStageBreakdown & /*sb*/) {}

#endif

} // namespace parlio_bench
} // namespace autoresearch
