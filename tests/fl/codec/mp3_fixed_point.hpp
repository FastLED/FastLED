#pragma once

/// @file mp3_fixed_point.hpp
/// @brief Phase 3 (#4054) fixed-vs-float acceptance gates for minimp3.
///
/// These decode the same corpus twice — once through the float pipeline, once
/// through the fixed-point pipeline — and hold the fixed-point output to a
/// bounded distance from the float reference. The float decoder is the
/// reference rather than the ISO vectors because the question this phase has
/// to answer is narrow: did converting a stage to integer arithmetic change
/// what the decoder produces? Conformance against ISO is already covered by
/// the Phase 0 golden tests in `mp3.hpp`.

#include "test.h"
#include "fl/fs/fs.h"
#include "fl/math/math.h"
#include "fl/stl/span.h"
#include "fl/stl/vector.h"
#include "tests/fl/codec/minimp3_variants.hpp" // ok cpp include
#ifdef FASTLED_TESTING
#include "platforms/stub/fs_stub.hpp" // ok platform headers
#endif

#ifdef FASTLED_TESTING

namespace {

/// Every Layer III stream in the Phase 0 corpus. `ILL2_layer1.bit` is Layer I
/// and takes a different path through the decoder, so it is listed too — the
/// fixed-point conversion must not quietly break it.
const char* const kFixedPointCorpus[] = {
    "codec/minimp3/l3-hecommon.bit",
    "codec/minimp3/l3-he_free.bit",
    "codec/minimp3/l3-lame-vbrtag.bit",
    "codec/minimp3/M2L3_bitrate_16_all.bit",
    "codec/minimp3/ILL2_layer1.bit",
};

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

struct PcmComparison {
    double mPsnrDb = 0.0;
    int mMaxAbsDiff = 0;
    fl::size mDiffering = 0;
    fl::size mCount = 0;
    bool mIdentical = false;
};

/// Full-scale-referenced PSNR, the figure the Phase 3 gate is written in.
/// Reported against a 32768 reference so the number does not move with signal
/// level; the separate max-LSB bound is what catches a localized blowup that a
/// whole-file average would hide.
PcmComparison comparePcm(const fl::vector<fl::i16>& reference,
                         const fl::vector<fl::i16>& candidate) {
    PcmComparison out;
    out.mCount = reference.size() < candidate.size() ? reference.size()
                                                     : candidate.size();
    double energy = 0.0;
    for (fl::size i = 0; i < out.mCount; ++i) {
        const int diff = static_cast<int>(reference[i]) -
                         static_cast<int>(candidate[i]);
        const int magnitude = diff < 0 ? -diff : diff;
        if (magnitude > out.mMaxAbsDiff) {
            out.mMaxAbsDiff = magnitude;
        }
        if (magnitude != 0) {
            ++out.mDiffering;
        }
        energy += static_cast<double>(diff) * static_cast<double>(diff);
    }
    out.mIdentical = (out.mDiffering == 0);
    if (out.mIdentical || out.mCount == 0) {
        out.mPsnrDb = 1000.0; // treated as "perfect" by the gates
        return out;
    }
    const double mse = energy / static_cast<double>(out.mCount);
    out.mPsnrDb = 20.0 * fl::log10(32768.0 / fl::sqrt(mse));
    return out;
}

} // anonymous namespace

// ---------------------------------------------------------------------------
// Enabling gate. Everything below is only meaningful once this passes: a build
// that merely accepted -DMINIMP3_FIXED_POINT and kept decoding in float would
// otherwise sail through every fixed-vs-float comparison with a perfect score.
// ---------------------------------------------------------------------------
FL_TEST_CASE("minimp3 fixed-point variant decodes with integer arithmetic") {
    FL_CHECK(fl::Minimp3FixedVariant::isFixedPoint());
    FL_CHECK_FALSE(fl::Minimp3FloatVariant::isFixedPoint());
    FL_CHECK_FALSE(fl::Minimp3FloatVariant::dspIsInteger());

    // RED until the integer kernels land (#4054).
    FL_CHECK(fl::Minimp3FixedVariant::dspIsInteger());
}

FL_TEST_CASE("minimp3 float and fixed variants coexist in one binary") {
    fl::setTestFileSystemRoot("tests/data");
    const fl::vector<fl::u8> bytes =
        readCorpusBytes("codec/minimp3/l3-hecommon.bit");

    const VariantDecodeResult flt =
        decodeWithVariant<fl::Minimp3FloatVariant>(bytes);
    const VariantDecodeResult fixed =
        decodeWithVariant<fl::Minimp3FixedVariant>(bytes);

    FL_CHECK_GT(flt.mFrames, 0);
    FL_CHECK_EQ(fixed.mFrames, flt.mFrames);
    FL_CHECK_EQ(fixed.mHz, flt.mHz);
    FL_CHECK_EQ(fixed.mChannels, flt.mChannels);
    FL_CHECK_EQ(fixed.mPcm.size(), flt.mPcm.size());
}

FL_TEST_CASE("minimp3 fixed-point tracks float across the golden corpus") {
    fl::setTestFileSystemRoot("tests/data");

    for (const char* path : kFixedPointCorpus) {
        const fl::vector<fl::u8> bytes = readCorpusBytes(path);
        const VariantDecodeResult flt =
            decodeWithVariant<fl::Minimp3FloatVariant>(bytes);
        const VariantDecodeResult fixed =
            decodeWithVariant<fl::Minimp3FixedVariant>(bytes);

        FL_CHECK_GT(flt.mFrames, 0);
        // Frame acceptance is a bit-exact property: the bitstream parser is
        // shared, so a divergence here is a parsing bug, not a rounding one.
        FL_CHECK_EQ(fixed.mFrames, flt.mFrames);
        FL_REQUIRE_EQ(fixed.mPcm.size(), flt.mPcm.size());

        const PcmComparison cmp = comparePcm(flt.mPcm, fixed.mPcm);
        printf("[fixed-vs-float] %-40s psnr=%.2f dB max=%d differing=%zu/%zu\n",
               path, cmp.mPsnrDb, cmp.mMaxAbsDiff, cmp.mDiffering, cmp.mCount);

        FL_CHECK_GE(cmp.mPsnrDb, 90.0);
        FL_CHECK_LE(cmp.mMaxAbsDiff, 8);
    }
}

#endif // FASTLED_TESTING
