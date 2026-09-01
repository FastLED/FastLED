#pragma once

#include "fl/stl/stdint.h"
#include "fl/stl/noexcept.h"
#include "fl/stl/span.h"

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

/// Inspect one complete MPEG audio frame for a VBR metadata header.
///
/// `frame` is a single frame's bytes; a short or non-Layer-III span simply
/// reports no tag. Returns true when the frame is metadata rather than audio.
///
/// The definitions live in `mp3_vbr_tag.cpp.hpp`, included below: this parser
/// is needed by the streaming decoder, the CI conformance harness, and the
/// unit tests, and the harness builds against `-I src` with no FastLED library
/// to link -- so it has to stay usable from a plain include, with no separate
/// translation unit to compile.
bool Mp3ParseVbrTag(fl::span<const fl::u8> frame, Mp3VbrTag* out) FL_NO_EXCEPT;

} // namespace third_party
} // namespace fl

#include "fl/codec/mp3_vbr_tag.cpp.hpp"
