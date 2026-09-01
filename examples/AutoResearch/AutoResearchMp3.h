/// @file AutoResearchMp3.h
/// @brief On-device verification of the fixed-point MP3 decoder.
///
/// FastLED ships minimp3 in fixed-point mode by default -- the choice is made
/// in the header, not by a build flag -- so every embedded target runs the
/// integer pipeline. That pipeline is where 32-bit-specific defects live, and
/// host testing on x86-64 is structurally blind to a whole class of them: an
/// int64 add costs one instruction there and two plus carry on a 32-bit
/// machine, signed overflow that the optimiser is entitled to assume away
/// behaves differently under different register pressure, and alignment
/// padding in the scratch arena differs outright.
///
/// So this checks the decode is *bit-exact* against the host, not merely
/// plausible: an FNV-1a over the emitted PCM, compared to the value the same
/// mp3dec_decode_frame_r produced on the host. A checksum rather than a PSNR
/// because there is no tolerance to allow -- the fixed-point decoder is
/// deterministic, and any difference at all is a defect worth seeing.

#pragma once

#include <FastLED.h>

#include "fl/stl/int.h"
#include "fl/stl/cstring.h"
#include "fl/stl/unique_ptr.h"

#include "AutoResearchMp3Fixture.h"

#include "fl/codec/mp3.h"
#include "third_party/minimp3/minimp3.h"

namespace autoresearch {
namespace mp3_check {

struct Result {
    bool success;
    fl::u32 streams_run;
    fl::u32 streams_failed;
    const char* first_failure;
    fl::u32 expected_fnv1a;
    fl::u32 actual_fnv1a;
    fl::u32 expected_samples;
    fl::u32 actual_samples;
    fl::u32 decode_micros;
    fl::u32 scratch_bytes;
    bool shared_decoder_matched;
    /* Populated on every run, pass or fail. A green result that reports only
       zeros is not evidence that anything was decoded -- it just means nothing
       tripped the failure path. These say what the device actually produced. */
    fl::u32 samples_decoded;
    fl::u32 frames_decoded;
    fl::u32 combined_fnv1a;
    /* Wall-clock for the first pass only, so it has the same scope as the
       counters above it. Timing both passes while counting one reads as if the
       decode were twice as slow as it is. */
    fl::u32 verify_micros;
    /* How much audio those samples represent, from the fixture's own sample
       rate and channel count. Reporting it alongside decode_micros is what
       makes the pair interpretable: the ratio is the real-time margin, and on a
       160 MHz part it is not large enough to leave implicit. */
    fl::u32 audio_micros;
};

inline fl::u32 fnv1a(fl::u32 hash, const fl::i16* samples, fl::u32 count) {
    for (fl::u32 i = 0; i < count; ++i) {
        const fl::u16 v = (fl::u16)samples[i];
        hash = (hash ^ (fl::u8)(v & 0xFF)) * 16777619u;
        hash = (hash ^ (fl::u8)(v >> 8)) * 16777619u;
    }
    return hash;
}

/// Fold one stream's checksum into the run-wide one, so a single reported
/// value covers every stream in order.
inline fl::u32 fnv1a_mix(fl::u32 hash, fl::u32 value) {
    for (int byte = 0; byte < 4; ++byte) {
        hash = (hash ^ (fl::u8)((value >> (byte * 8)) & 0xFF)) * 16777619u;
    }
    return hash;
}

struct Decoded {
    fl::u32 fnv1a;
    fl::u32 samples;
    fl::u32 frames;
    /* The fixture records each stream's sample rate, channel count and layer.
       Carrying them back is what makes them assertions rather than comments: a
       decoder that emitted the right sample values at the wrong rate, or fell
       back to mono, would otherwise match on the checksum path and pass. */
    fl::u32 hz;
    fl::u32 channels;
    fl::u32 layer;
};

/// Decode one stream. `dec` is supplied by the caller so the same decoder can
/// be driven across two streams of different layers, which is what exercises
/// the Layer I/II + Layer III scratch union on real hardware.
inline Decoded decodeStream(fl::third_party::mp3dec_t* dec,
                            fl::third_party::mp3dec_scratch_t* scratch,
                            fl::i16* pcm, const mp3_fixture::Stream& stream) {
    Decoded out;
    out.fnv1a = 2166136261u;
    out.samples = 0;
    out.frames = 0;
    out.hz = 0;
    out.channels = 0;
    out.layer = 0;

    fl::u32 offset = 0;
    while (offset + 4 < stream.size) {
        fl::third_party::mp3dec_frame_info_t info;
        fl::memset(&info, 0, sizeof(info));
        const int samples = fl::third_party::mp3dec_decode_frame_r(
            dec, scratch, stream.data + offset, (int)(stream.size - offset),
            pcm, &info);
        if (info.frame_bytes <= 0) {
            break;
        }
        offset += (fl::u32)info.frame_bytes;
        if (samples > 0) {
            const fl::u32 count = (fl::u32)samples * (fl::u32)info.channels;
            out.fnv1a = fnv1a(out.fnv1a, pcm, count);
            out.samples += count;
            ++out.frames;
            out.hz = (fl::u32)info.hz;
            out.channels = (fl::u32)info.channels;
            out.layer = (fl::u32)info.layer;
        }
    }
    return out;
}

inline void recordFailure(Result& r, const char* name, fl::u32 expected_hash,
                          fl::u32 actual_hash, fl::u32 expected_samples,
                          fl::u32 actual_samples) {
    ++r.streams_failed;
    if (r.first_failure == nullptr) {
        r.first_failure = name;
        r.expected_fnv1a = expected_hash;
        r.actual_fnv1a = actual_hash;
        r.expected_samples = expected_samples;
        r.actual_samples = actual_samples;
    }
}

inline Result run() {
    Result r;
    fl::memset(&r, 0, sizeof(r));
    r.success = true;
    r.first_failure = nullptr;
    r.scratch_bytes = (fl::u32)sizeof(fl::third_party::mp3dec_scratch_t);
    r.shared_decoder_matched = true;
    r.combined_fnv1a = 2166136261u;

    // The decoder state and scratch arena are far too large for the stack on a
    // microcontroller -- roughly 11 KB and 8 KB -- so they go on the heap, once.
    fl::unique_ptr<fl::third_party::mp3dec_t> dec =
        fl::make_unique<fl::third_party::mp3dec_t>();
    fl::unique_ptr<fl::third_party::mp3dec_scratch_t> scratch =
        fl::make_unique<fl::third_party::mp3dec_scratch_t>();
    // make_unique<T[]> yields unique_ptr<T[]>, released with delete[].
    fl::unique_ptr<fl::i16[]> pcm = fl::make_unique<fl::i16[]>(2304);
    if (!dec || !scratch || !pcm) {
        r.success = false;
        r.first_failure = "allocation";
        return r;
    }

    const fl::u32 started = (fl::u32)micros();
    fl::u32 audio_us = 0;

    // Pass 1: a fresh decoder per stream, which is how the host fixture was
    // produced. This is the bit-exactness check.
    for (fl::u32 i = 0; i < mp3_fixture::kStreamCount; ++i) {
        const mp3_fixture::Stream& s = mp3_fixture::kStreams[i];
        fl::third_party::mp3dec_init(dec.get());
        const Decoded got = decodeStream(dec.get(), scratch.get(), pcm.get(), s);
        ++r.streams_run;
        r.samples_decoded += got.samples;
        r.frames_decoded += got.frames;
        r.combined_fnv1a = fnv1a_mix(r.combined_fnv1a, got.fnv1a);
        if (s.hz > 0u && s.channels > 0u) {
            const fl::u32 per_channel = got.samples / s.channels;
            audio_us += (fl::u32)(((fl::u64)per_channel * 1000000ull) / s.hz);
        }
        if (got.fnv1a != s.fnv1a || got.samples != s.samples ||
            got.frames != s.frames || got.hz != s.hz ||
            got.channels != s.channels || got.layer != s.layer) {
            recordFailure(r, s.name, s.fnv1a, got.fnv1a, s.samples,
                          got.samples);
        }
    }

    r.decode_micros = (fl::u32)micros() - started;
    r.audio_micros = audio_us;
    const fl::u32 verify_started = (fl::u32)micros();

    // Pass 2: one decoder driven across every stream in turn, without
    // re-initialising between them. Layer I/II scale info and Layer III's
    // main-data window share arena storage (FastLED#4116) on the grounds that a
    // frame is one layer or the other and never both. If that is wrong -- or if
    // it is right on x86-64 and wrong here, where the struct is packed
    // differently -- the second stream decodes differently than it did alone.
    fl::third_party::mp3dec_init(dec.get());
    for (fl::u32 i = 0; i < mp3_fixture::kStreamCount; ++i) {
        const mp3_fixture::Stream& s = mp3_fixture::kStreams[i];
        const Decoded got = decodeStream(dec.get(), scratch.get(), pcm.get(), s);
        if (got.fnv1a != s.fnv1a || got.samples != s.samples ||
            got.hz != s.hz || got.channels != s.channels ||
            got.layer != s.layer) {
            r.shared_decoder_matched = false;
            if (r.first_failure == nullptr) {
                r.first_failure = s.name;
                r.expected_fnv1a = s.fnv1a;
                r.actual_fnv1a = got.fnv1a;
                r.expected_samples = s.samples;
                r.actual_samples = got.samples;
            }
        }
    }

    r.verify_micros = (fl::u32)micros() - verify_started;
    r.success = r.streams_failed == 0u && r.shared_decoder_matched &&
                r.streams_run == mp3_fixture::kStreamCount;
    return r;
}

} // namespace mp3_check
} // namespace autoresearch
