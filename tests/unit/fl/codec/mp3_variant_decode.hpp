#pragma once

/// @file mp3_variant_decode.hpp
/// @brief Decode a whole bitstream through one minimp3 variant.
///
/// Shared by the fixed-vs-float gates in `mp3_fixed_point.hpp` (unit tier)
/// and the SIMD-vs-scalar timing in
/// `tests/integration/misc/mp3_simd_perf.cpp` (integration tier).

#include "test.h"
#include "fl/fs/fs.h"
#include "fl/stl/vector.h"
#include "tests/unit/fl/codec/minimp3_variants.hpp" // ok cpp include

#ifdef FASTLED_TESTING

namespace {

fl::vector<fl::u8> readCorpusBytes(const char* path) {
    fl::FileSystem fs;
    FL_REQUIRE(fs.beginSd(0));
    fl::ifstream file = fs.openRead(path);
    FL_REQUIRE(file.is_open());
    fl::vector<fl::u8> bytes;
    bytes.resize(file.size());
    FL_REQUIRE_EQ(file.read(bytes.data(), bytes.size()), bytes.size());
    return bytes;
}

struct VariantDecodeResult {
    fl::vector<fl::i16> mPcm;
    int mFrames = 0;
    int mHz = 0;
    int mChannels = 0;
};

/// Drive one variant across a whole bitstream, mirroring how
/// `Mp3Minimp3Decoder` walks the stream in production.
template <typename Variant>
VariantDecodeResult decodeWithVariant(const fl::vector<fl::u8>& bytes) {
    typename Variant::decoder_type decoder;
    typename Variant::scratch_type scratch;
    Variant::init(&decoder);

    VariantDecodeResult result;
    fl::vector<typename Variant::sample_type> pcm;
    pcm.resize(MINIMP3_MAX_SAMPLES_PER_FRAME);

    const fl::u8* cursor = bytes.data();
    int remaining = static_cast<int>(bytes.size());
    while (remaining > 0) {
        typename Variant::info_type info;
        const int samples = Variant::decodeFrame(&decoder, &scratch, cursor,
                                                 remaining, pcm.data(), &info);
        if (info.frame_bytes <= 0) {
            break;
        }
        cursor += info.frame_bytes;
        remaining -= info.frame_bytes;
        if (samples <= 0) {
            continue;
        }
        if (result.mHz == 0) {
            result.mHz = info.hz;
            result.mChannels = info.channels;
        }
        ++result.mFrames;
        const int values = samples * info.channels;
        for (int i = 0; i < values; ++i) {
            result.mPcm.push_back(static_cast<fl::i16>(pcm[i]));
        }
    }
    return result;
}

} // anonymous namespace

#endif // FASTLED_TESTING
