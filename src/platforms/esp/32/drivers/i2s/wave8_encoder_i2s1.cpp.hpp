/// @file wave8_encoder_i2s1.cpp.hpp
/// @brief Implementation of `encodeChannelWave8_i2s1` (FastLED#3526 Phase 2a).
///
/// Structural mirror of PARLIO's wave8 hot loop
/// (`parlio_engine.cpp.hpp:1024-1069`) — deliberately calls the SAME
/// `fl::wave8Transpose_16_bf1` and `fl::wave8Transpose_16x4_bf1_pipe4`
/// entry points so this encoder cannot silently drift from production.
/// If PARLIO's encoding path changes, this encoder inherits the change
/// automatically because it dispatches through the shared transpose
/// kernels rather than duplicating them.

// IWYU pragma: private

#include "platforms/esp/32/drivers/i2s/wave8_encoder_i2s1.h"

#include "fl/stl/bit_cast.h"  // fl::ptr_to_int — alignment check in wave8I2s1ExpandTo32Samples

// Public wave3.h / wave8.h declarations only (transitively via
// wave8_encoder_i2s1.h) — do NOT include `fl/channels/detail/*.hpp`
// here. Those force-inline headers would emit their transpose kernels
// in every translation unit that includes this .cpp.hpp (unity build
// pulls in this file across multiple TUs), doubling the code
// footprint. The public declarations resolve at link time to the
// single out-of-line definitions in `fl/channels/wave3.cpp.hpp` /
// `wave8.cpp.hpp`.

namespace fl {

bool encodeChannelWave8_i2s1(fl::span<const fl::u8> input,
                             fl::size_t bytes_per_lane,
                             fl::size_t num_lanes,
                             const Wave8ByteExpansionLut &lut,
                             fl::span<fl::u8> output) FL_NO_EXCEPT {
    // Bounds checks — bail cleanly if the caller supplied bad shapes.
    if (num_lanes == 0 || num_lanes > 16) {
        return false;
    }
    if (bytes_per_lane == 0) {
        return false;
    }
    if (input.size() < 16 * bytes_per_lane) {
        return false;
    }
    const fl::size_t required_output = wave8I2s1EncodedFrameSize(bytes_per_lane);
    if (output.size() < required_output) {
        return false;
    }

    constexpr fl::size_t kBlockSize = 16 * sizeof(Wave8Byte);

    fl::u8 *out_ptr = output.data();
    fl::size_t out_idx = 0;

    // For each input byte position, gather one byte from each of 16
    // lane slots into the transpose kernel. Inactive lanes past
    // num_lanes get zero-padded so the transpose still runs 16 wide
    // (predictable memory access, no per-lane branch inside the hot
    // path). The 4-byte-batched pipe4 variant fires when there are at
    // least 4 remaining byte positions, matching PARLIO.
    fl::size_t byte_offset = 0;
    while (byte_offset < bytes_per_lane) {
        fl::u8 lanes_a[16];
        for (fl::size_t lane = 0; lane < 16; lane++) {
            lanes_a[lane] = (lane < num_lanes)
                ? input[lane * bytes_per_lane + byte_offset]
                : 0;
        }

        if (byte_offset + 3 < bytes_per_lane
            && out_idx + 4 * kBlockSize <= required_output) {
            fl::u8 lanes_b[16];
            fl::u8 lanes_c[16];
            fl::u8 lanes_d[16];
            for (fl::size_t lane = 0; lane < 16; lane++) {
                const bool active = (lane < num_lanes);
                const fl::u8 *base = input.data() + lane * bytes_per_lane;
                lanes_b[lane] = active ? base[byte_offset + 1] : 0;
                lanes_c[lane] = active ? base[byte_offset + 2] : 0;
                lanes_d[lane] = active ? base[byte_offset + 3] : 0;
            }
            wave8Transpose_16x4_bf1_pipe4(
                reinterpret_cast<const fl::u8(&)[16]>(lanes_a), // ok reinterpret cast - array reference type conversion
                reinterpret_cast<const fl::u8(&)[16]>(lanes_b), // ok reinterpret cast - array reference type conversion
                reinterpret_cast<const fl::u8(&)[16]>(lanes_c), // ok reinterpret cast - array reference type conversion
                reinterpret_cast<const fl::u8(&)[16]>(lanes_d), // ok reinterpret cast - array reference type conversion
                lut,
                *reinterpret_cast<fl::u8(*)[kBlockSize]>(out_ptr + out_idx),                     // ok reinterpret cast - direct write to DMA buffer (wave8 transpose output)
                *reinterpret_cast<fl::u8(*)[kBlockSize]>(out_ptr + out_idx + kBlockSize),        // ok reinterpret cast - direct write to DMA buffer
                *reinterpret_cast<fl::u8(*)[kBlockSize]>(out_ptr + out_idx + 2 * kBlockSize),    // ok reinterpret cast - direct write to DMA buffer
                *reinterpret_cast<fl::u8(*)[kBlockSize]>(out_ptr + out_idx + 3 * kBlockSize));   // ok reinterpret cast - direct write to DMA buffer
            out_idx += 4 * kBlockSize;
            byte_offset += 4;
        } else {
            wave8Transpose_16_bf1(
                reinterpret_cast<const fl::u8(&)[16]>(lanes_a), lut, // ok reinterpret cast - array reference type conversion
                *reinterpret_cast<fl::u8(*)[kBlockSize]>(out_ptr + out_idx));  // ok reinterpret cast - direct write to DMA buffer
            out_idx += kBlockSize;
            byte_offset += 1;
        }
    }

    return true;
}

bool wave8I2s1ExpandTo32Samples(fl::span<const fl::u8> pulses,
                                fl::span<fl::u8> output) FL_NO_EXCEPT {
    const fl::size_t pulse_bytes = pulses.size();
    if (pulse_bytes == 0 || (pulse_bytes & 1u) != 0) {
        return false;
    }
    if (output.size() < pulse_bytes * 2) {
        return false;
    }
    // The DMA/FIFO consumes the buffer as little-endian u32 words —
    // require 4-byte output alignment so the u32 stores below are legal.
    if ((fl::ptr_to_int(output.data()) & 3u) != 0) {
        return false;
    }
    const fl::u8 *src = pulses.data();
    fl::u32 *dst = reinterpret_cast<fl::u32 *>(output.data()); // ok reinterpret cast - 4-byte-aligned DMA buffer written as u32 samples
    const fl::size_t pulse_count = pulse_bytes / 2;
    for (fl::size_t p = 0; p < pulse_count; ++p) {
        const fl::u32 lo = src[2 * p + 0];  // lanes 0-7
        const fl::u32 hi = src[2 * p + 1];  // lanes 8-15
        // Sample bit (n + 8) drives DATA_OUT(n) in tx_bits_mod=32 LCD
        // mode (see header) — place the 16 lanes at bits 8..23.
        dst[p] = (lo << 8) | (hi << 16);
    }
    return true;
}

bool encodeChannelWave3_i2s1(fl::span<const fl::u8> input,
                             fl::size_t bytes_per_lane,
                             fl::size_t num_lanes,
                             const Wave3BitExpansionLut &lut,
                             fl::span<fl::u8> output) FL_NO_EXCEPT {
    if (num_lanes == 0 || num_lanes > 16) {
        return false;
    }
    if (bytes_per_lane == 0) {
        return false;
    }
    if (input.size() < 16 * bytes_per_lane) {
        return false;
    }
    const fl::size_t required_output = wave3I2s1EncodedFrameSize(bytes_per_lane);
    if (output.size() < required_output) {
        return false;
    }

    constexpr fl::size_t kBlockSize = 16 * sizeof(Wave3Byte);

    fl::u8 *out_ptr = output.data();
    fl::size_t out_idx = 0;

    for (fl::size_t byte_offset = 0; byte_offset < bytes_per_lane; ++byte_offset) {
        fl::u8 lanes[16];
        for (fl::size_t lane = 0; lane < 16; lane++) {
            lanes[lane] = (lane < num_lanes)
                ? input[lane * bytes_per_lane + byte_offset]
                : 0;
        }
        wave3Transpose_16(
            reinterpret_cast<const fl::u8(&)[16]>(lanes), lut, // ok reinterpret cast - array reference type conversion
            *reinterpret_cast<fl::u8(*)[kBlockSize]>(out_ptr + out_idx)); // ok reinterpret cast - direct write to DMA buffer (wave3 transpose output)
        out_idx += kBlockSize;
    }

    return true;
}

} // namespace fl
