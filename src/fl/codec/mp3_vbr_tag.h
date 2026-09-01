#pragma once

#include "fl/stl/stdint.h"
#include "fl/stl/noexcept.h"

namespace fl {
namespace third_party {

/// Metadata carried by an MP3's first frame when an encoder wrote a Xing,
/// Info, or VBRI header there.
///
/// Such a frame is *not* audio. It occupies a complete, valid MPEG frame, so a
/// decoder that only scans for sync words will happily decode it and emit a
/// frame of noise before the music starts. LAME additionally records how many
/// samples of encoder priming precede the real signal, and how many samples of
/// padding follow it; ignoring those leaves the priming audible at the start.
///
/// Measured on the `l3-nonstandard-sin1k0db_lame_vbrtag` conformance vector,
/// the total to drop is 2257 samples per channel, which decomposes exactly:
///
///     1152   the tag frame itself, emitted as audio
///      576   LAME's recorded encoder delay for that file
///      529   MP3D_DECODER_DELAY, the format's own synthesis delay
///     ----
///     2257
///
/// Skipping them moves that vector from 2.85 dB to 108.18 dB against its
/// reference (FastLED#4129). The 529 is not in the file: it is a property of
/// the MPEG synthesis filterbank, and every gapless-capable player adds it to
/// whatever the encoder recorded.
/// The synthesis filterbank's own latency, in samples per channel. Constant for
/// MPEG Layer III and not recorded in the stream, so it has to be added to the
/// encoder's reported delay to get the true offset of the first real sample.
static const fl::u32 MP3D_DECODER_DELAY = 529;

struct Mp3VbrTag {
    /// The frame is metadata and must not be emitted as audio.
    bool present;
    /// Samples per channel of encoder priming to drop from the start.
    fl::u32 encoderDelay;
    /// Samples per channel of padding to drop from the end.
    fl::u32 encoderPadding;
};

namespace detail {

inline fl::u32 mp3VbrRead32(const fl::u8* p) FL_NO_EXCEPT {
    return (static_cast<fl::u32>(p[0]) << 24) |
           (static_cast<fl::u32>(p[1]) << 16) |
           (static_cast<fl::u32>(p[2]) << 8) | static_cast<fl::u32>(p[3]);
}

inline bool mp3VbrMagic(const fl::u8* p, const char* magic) FL_NO_EXCEPT {
    return p[0] == static_cast<fl::u8>(magic[0]) &&
           p[1] == static_cast<fl::u8>(magic[1]) &&
           p[2] == static_cast<fl::u8>(magic[2]) &&
           p[3] == static_cast<fl::u8>(magic[3]);
}

} // namespace detail

/// Inspect one complete MPEG audio frame for a VBR metadata header.
///
/// Header-only and dependency-free on purpose: the streaming decoder and the
/// CI conformance harness both need it, and the harness builds against `-I src`
/// with no FastLED library to link.
inline bool Mp3ParseVbrTag(const fl::u8* frame, fl::size length,
                           Mp3VbrTag* out) FL_NO_EXCEPT {
    out->present = false;
    out->encoderDelay = 0;
    out->encoderPadding = 0;
    if (!frame || length < 40) {
        return false;
    }
    if (frame[0] != 0xFF || (frame[1] & 0xE0) != 0xE0) {
        return false;
    }

    const int version_bits = (frame[1] >> 3) & 0x03;
    const int layer_bits = (frame[1] >> 1) & 0x03;
    if (layer_bits != 0x01) {
        return false; // Layer III only; Xing/VBRI are not defined elsewhere.
    }
    const bool mpeg1 = version_bits == 0x03;
    const bool mono = ((frame[3] >> 6) & 0x03) == 0x03;

    // Side information sits between the header and the tag, and its size is
    // fixed by version and channel count.
    const fl::size side_info = mpeg1 ? (mono ? 17u : 32u) : (mono ? 9u : 17u);

    // VBRI (Fraunhofer) always sits at a fixed offset and carries no delay we
    // can trust to the sample, so it is reported as metadata without one.
    if (length >= 40 && detail::mp3VbrMagic(frame + 36, "VBRI")) {
        out->present = true;
        return true;
    }

    const fl::size xing = 4 + side_info;
    if (length < xing + 8) {
        return false;
    }
    if (!detail::mp3VbrMagic(frame + xing, "Xing") &&
        !detail::mp3VbrMagic(frame + xing, "Info")) {
        return false;
    }
    out->present = true;

    const fl::u32 flags = detail::mp3VbrRead32(frame + xing + 4);
    fl::size cursor = xing + 8;
    if (flags & 0x0001) { cursor += 4; }   // frame count
    if (flags & 0x0002) { cursor += 4; }   // byte count
    if (flags & 0x0004) { cursor += 100; } // seek table
    if (flags & 0x0008) { cursor += 4; }   // quality

    // The encoder tag that follows is optional. Its delay and padding live 21
    // bytes in, as 12 bits each packed across three bytes.
    if (cursor + 24 > length) {
        return true; // a tag frame, but with no usable delay information
    }
    const bool lame = detail::mp3VbrMagic(frame + cursor, "LAME") ||
                      detail::mp3VbrMagic(frame + cursor, "Lavc") ||
                      detail::mp3VbrMagic(frame + cursor, "Lavf");
    if (!lame) {
        return true;
    }
    const fl::u8* gap = frame + cursor + 21;
    out->encoderDelay =
        (static_cast<fl::u32>(gap[0]) << 4) | (static_cast<fl::u32>(gap[1]) >> 4);
    out->encoderPadding =
        ((static_cast<fl::u32>(gap[1]) & 0x0F) << 8) | static_cast<fl::u32>(gap[2]);
    return true;
}

} // namespace third_party
} // namespace fl
