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
/// so capture works without depending on the testSimd RPC routing (#2541).
/// **Invocation:** RPC-only via `parlioEncodeBenchmark` in AutoResearchRemote.cpp.

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

    result.sink = static_cast<fl::u32>(sink);

    heap_caps_free(scratch_sram);
    heap_caps_free(scratch_psram);
    heap_caps_free(output_sram);
    heap_caps_free(output_psram);
    return result;
}

inline void printParlioEncodeResultRom(const ParlioEncodeResult &r) {
    if (r.iters == 0) {
        esp_rom_printf("\nBENCH_PARLIO_START (issue #2526 follow-up)\n");  // ok esp_rom_printf - boot-time bench output to COM25 (#2541)
        esp_rom_printf("BENCH_PARLIO ERROR: heap allocation failed\n");  // ok esp_rom_printf - boot-time bench output to COM25 (#2541)
        esp_rom_printf("BENCH_PARLIO_END\n\n");  // ok esp_rom_printf - boot-time bench output to COM25 (#2541)
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

    esp_rom_printf("\nBENCH_PARLIO_START (16 lanes x 256 LEDs, byte-LUT, #2526 follow-up)\n");  // ok esp_rom_printf - boot-time bench output to COM25 (#2541)
    esp_rom_printf("BENCH_PARLIO iters=%u lanes=%u leds=%u sink=%u\n",  // ok esp_rom_printf - boot-time bench output to COM25 (#2541)
                   r.iters, r.lanes, r.leds_per_lane, r.sink);
    esp_rom_printf("BENCH_PARLIO perpos_us  ss=%u sp=%u ps=%u pp=%u\n",  // ok esp_rom_printf - boot-time bench output to COM25 (#2541)
                   r.perpos_ss_us, r.perpos_sp_us, r.perpos_ps_us, r.perpos_pp_us);
    esp_rom_printf("BENCH_PARLIO perpos_ns_each  ss=%u sp=%u ps=%u pp=%u\n",  // ok esp_rom_printf - boot-time bench output to COM25 (#2541)
                   ss_ns_per, sp_ns_per, ps_ns_per, pp_ns_per);
    esp_rom_printf("BENCH_PARLIO frame_equiv_us  ss=%u sp=%u ps=%u pp=%u  ws2812b_tx=%u\n",  // ok esp_rom_printf - boot-time bench output to COM25 (#2541)
                   ss_frame_us, sp_frame_us, ps_frame_us, pp_frame_us, WS2812B_FRAME_US);
    esp_rom_printf("BENCH_PARLIO ratio_x100  sp_vs_ss=%u ps_vs_ss=%u pp_vs_ss=%u\n",  // ok esp_rom_printf - boot-time bench output to COM25 (#2541)
                   ratio_sp_ss_x100, ratio_ps_ss_x100, ratio_pp_ss_x100);
    esp_rom_printf("BENCH_PARLIO_END\n\n");  // ok esp_rom_printf - boot-time bench output to COM25 (#2541)
}

#else // !FL_PARLIO_BENCH_ENABLED

inline ParlioEncodeResult measureParlioEncode(int /*iters*/ = 12000) { return {}; }
inline void printParlioEncodeResultRom(const ParlioEncodeResult & /*r*/) {}

#endif

} // namespace parlio_bench
} // namespace autoresearch
