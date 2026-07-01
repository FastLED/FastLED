/// @file wave8_encoder_i2s1.h
/// @brief Wave8 encoder for the classic-ESP32 I2S1 parallel-out driver.
///
/// **FastLED#3526 Phase 2a.** New I2S driver architecture: general clockless
/// driver in the same shape as PARLIO. Reuses the shared
/// `fl::Wave8BitExpansionLut` / `fl::Wave8ByteExpansionLut` LUTs and the
/// existing `fl::wave8Transpose_16*` transpose kernels (`src/fl/channels/wave8.h`).
/// No delegation to the Yves `i2s_esp32dev.cpp.hpp` bit-pattern driver.
///
/// ## Encoding model
///
/// Input: byte-major LED data laid out lane-strided (lane 0's bytes contiguous,
/// then lane 1's bytes, etc). For a 4-lane × 60-LED WS2812 frame that's
/// `4 × 60 × 3 = 720` bytes.
///
/// Output: DMA-ready pulse-major bytes. For each input byte position, the
/// encoder produces 8 wave8-expanded output words × 16 lane positions per
/// word = 128 output bytes per input byte-position. Every byte position
/// produces its own 128-byte block.
///
/// Total output size = `input_bytes_per_lane * 128` bytes (independent of
/// active lane count — inactive lanes are zero-padded).
///
/// ## Wave8 pulse timing
///
/// At 8 MHz pixel clock, each wave8 output pulse is 125 ns wide. Eight
/// pulses per input bit → 1 µs per bit → 800 kHz WS2812 rate. Wave3 (three
/// pulses per bit at 2.4 MHz) is an alternative for smaller memory
/// footprint but only works for chipsets whose T1/T2/T3 fits the three-slot
/// constraint — deferred to Phase 2a follow-up.
///
/// ## Dispatch
///
/// - `encodeChannelWave8_i2s1(...)` — main entry: transposes 16 lanes per
///   byte position, using the fast BF1 (bitfield-1) direct-encode path for
///   any Wave8-eligible chipset.
///
/// Bit-identical to PARLIO's wave8 hot loop
/// (`parlio_engine.cpp.hpp:1024-1069`) — reuse the same `wave8Transpose_16_bf1`
/// / `wave8Transpose_16x4_bf1_pipe4` calls so the encoder can't silently
/// drift from production (avoids the FastLED#3524 mirror-cheat pattern).

#pragma once

// IWYU pragma: private

#include "fl/channels/wave8.h"
#include "fl/chipsets/led_timing.h"
#include "fl/stl/int.h"
#include "fl/stl/noexcept.h"
#include "fl/stl/span.h"

namespace fl {

/// @brief Byte-count per input byte-position in the encoder output.
///
/// Each byte position expands to 16 lane positions × 8 wave8 pulses ×
/// 1 byte per pulse = 128 output bytes. Fixed regardless of the actual
/// active lane count (inactive lanes are zero-padded in the input so the
/// transpose still runs 16 wide).
constexpr fl::size_t kWave8I2s1BytesPerBytePosition = 16 * sizeof(Wave8Byte);

/// @brief Encoded output size for a full frame.
///
/// @param bytes_per_lane  Encoded byte count per LED strip. For a
///                        WS2812 strip of N LEDs that's `3 * N` (GRB
///                        bytes per LED).
/// @return Total DMA-output buffer size the caller must allocate.
constexpr fl::size_t wave8I2s1EncodedFrameSize(fl::size_t bytes_per_lane) FL_NO_EXCEPT {
    return bytes_per_lane * kWave8I2s1BytesPerBytePosition;
}

/// @brief Encode one frame across `num_lanes` LED strips into a wave8
///        pulse-major byte stream ready for I2S1 DMA output.
///
/// @param input   Lane-strided input pixel bytes. Layout: lane 0's
///                bytes are `input[0..bytes_per_lane)`, lane 1's bytes
///                are `input[bytes_per_lane..2*bytes_per_lane)`, etc.
///                Must have exactly `16 * bytes_per_lane` bytes; unused
///                lanes past `num_lanes` are ignored but must be
///                zero-padded by the caller.
/// @param bytes_per_lane  Encoded bytes per LED strip.
/// @param num_lanes       Active lane count, in range `[1, 16]`.
/// @param lut     Byte-indexed expansion LUT for the current chipset —
///                build once via `buildWave8ByteExpansionLUT(...)` and
///                keep in internal SRAM. Passing an incorrect LUT here
///                will produce silently-wrong output; the LUT is not
///                cross-checked against any timing metadata.
/// @param output  Caller-owned DMA-capable output buffer.
///                Size must be at least
///                `wave8I2s1EncodedFrameSize(bytes_per_lane)` bytes.
/// @return true on success. false if inputs are out of range OR the
///         output span is too small. On false, the output span is not
///         written.
///
/// **Not for ISR use.** The transpose kernel is IRAM-safe but the
/// input-bounds validation isn't; call from the engine's `show()` path,
/// not the DMA-refill callback.
bool encodeChannelWave8_i2s1(fl::span<const fl::u8> input,
                             fl::size_t bytes_per_lane,
                             fl::size_t num_lanes,
                             const Wave8ByteExpansionLut &lut,
                             fl::span<fl::u8> output) FL_NO_EXCEPT;

} // namespace fl
