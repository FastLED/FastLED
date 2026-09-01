#pragma once

// Implementation for fl/codec/mp3_vbr_tag.h. Included by that header;
// every definition is inline, so including it from more than one
// translation unit is safe.

#include "fl/codec/mp3_vbr_tag.h"

namespace fl {
namespace third_party {

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

inline bool Mp3ParseVbrTag(fl::span<const fl::u8> bytes,
                           Mp3VbrTag* out) FL_NO_EXCEPT {
    out->present = false;
    out->encoderDelay = 0;
    out->encoderPadding = 0;
    if (bytes.size() < 40) {
        return false;
    }
    const fl::u8* const frame = bytes.data();
    const fl::size length = bytes.size();
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
