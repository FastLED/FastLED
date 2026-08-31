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

/// Range measurement additionally sweeps the musical fixtures. The `.bit`
/// conformance vectors are synthetic and narrow; real encoded music is what
/// actually exercises the loud end the Q format has to hold.
const char* const kRangeCorpus[] = {
    "codec/minimp3/l3-hecommon.bit",
    "codec/minimp3/l3-he_free.bit",
    "codec/minimp3/l3-lame-vbrtag.bit",
    "codec/minimp3/M2L3_bitrate_16_all.bit",
    "codec/minimp3/ILL2_layer1.bit",
    "codec/mary_had_a_little_lamb.mp3",
    "codec/jazzy_percussion.mp3",
    "codec/edm_beat.mp3",
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

namespace {

/// Records the largest magnitude each pipeline stage has to represent. The
/// fixed-point Q formats are chosen from these numbers, so the measurement is
/// kept as a test rather than done once by hand: if a corpus addition ever
/// pushes a stage past the headroom a Q format assumes, that has to fail
/// loudly instead of silently saturating.
class Mp3StageRangeSink : public fl::Mp3StageSink {
  public:
    void onStage(int stage, int, const float* buf, int count) FL_NO_EXCEPT override {
        if (stage < 0 || stage >= MINIMP3_STAGE_COUNT) {
            return;
        }
        for (int i = 0; i < count; ++i) {
            const float magnitude = buf[i] < 0 ? -buf[i] : buf[i];
            if (magnitude > mMaxAbs[stage]) {
                mMaxAbs[stage] = magnitude;
            }
        }
        mSamples[stage] += static_cast<fl::size>(count);
    }

    void onStage(int, int, const fl::i32*, int) FL_NO_EXCEPT override {}

    float maxAbs(int stage) const { return mMaxAbs[stage]; }
    fl::size samples(int stage) const { return mSamples[stage]; }

  private:
    float mMaxAbs[MINIMP3_STAGE_COUNT] = {};
    fl::size mSamples[MINIMP3_STAGE_COUNT] = {};
};

} // anonymous namespace

FL_TEST_CASE("minimp3 stage dynamic range stays inside the fixed-point headroom") {
    fl::setTestFileSystemRoot("tests/data");

    Mp3StageRangeSink sink;
    fl::SetMp3StageSink(&sink);
    for (const char* path : kRangeCorpus) {
        const fl::vector<fl::u8> bytes = readCorpusBytes(path);
        decodeWithVariant<fl::Minimp3FloatVariant>(bytes);
    }
    fl::ClearMp3StageSink();

    static const char* const kStageNames[MINIMP3_STAGE_COUNT] = {
        "huffman", "stereo", "antialias", "imdct", "dct2"};
    for (int stage = 0; stage < MINIMP3_STAGE_COUNT; ++stage) {
        printf("[stage-range] %-10s max=%.6g over %zu samples\n",
               kStageNames[stage], static_cast<double>(sink.maxAbs(stage)),
               sink.samples(stage));
        FL_CHECK_GT(sink.samples(stage), 0);
    }
}

// ---------------------------------------------------------------------------
// Helper-level gates.
//
// The two mantissa/exponent helpers are the only genuinely new arithmetic in
// the conversion -- everything else is the float graph with integer operators.
// They are cheap enough to test exhaustively over every input the decoder can
// reach, which is worth doing: a defect in either would otherwise surface much
// later as an unexplained PSNR dip on one corpus file, with the whole pipeline
// as the suspect list.
// ---------------------------------------------------------------------------
FL_TEST_CASE("minimp3 fixed-point x**(4/3) matches the float path for every input") {
    // 8191 + 15 is the largest magnitude an escape-coded Huffman value can
    // carry (15 plus the widest linbits field).
    const int kMaxQuantized = 8191 + 15;
    double worst_relative = 0.0;
    int worst_x = -1;

    for (int x = 0; x <= kMaxQuantized; ++x) {
        int exp = 0;
        const fl::i32 mant = fl::minimp3_fixed_probe::mp3d_pow43(x, &exp);
        const double fixed_value =
            static_cast<double>(mant) * fl::pow(2.0, static_cast<double>(exp - 30));
        const double float_value =
            static_cast<double>(fl::minimp3_float_probe::L3_pow_43(x));

        if (x == 0) {
            FL_CHECK_EQ(mant, 0);
            continue;
        }
        const double relative =
            fl::fabs(fixed_value - float_value) / fl::fabs(float_value);
        if (relative > worst_relative) {
            worst_relative = relative;
            worst_x = x;
        }
    }
    printf("[pow43] worst relative error %.3e at x=%d\n", worst_relative,
           worst_x);
    // Measured worst case is 1.53e-7 (at x=7495). Part of that is the
    // reference's own error, not the conversion's: L3_pow_43 returns a float,
    // so it carries ~6e-8 of its own before the comparison starts. The bound
    // is set a little over 3x the measured figure -- loose enough not to be a
    // rounding tripwire, tight enough that a mistranslated interpolation
    // (which would land orders of magnitude out, not a few ulp) fails.
    FL_CHECK_LT(worst_relative, 5e-7);
}

FL_TEST_CASE("minimp3 fixed-point scalefactor gains match 2**(q/4) exactly") {
    // `quarters` spans every value L3_decode_scalefactors can produce:
    // global_gain is 8-bit and iscf can reach 255 with a shift of 2.
    double worst_relative = 0.0;
    int worst_q = 0;
    for (int quarters = -800; quarters <= 64; ++quarters) {
        fl::i32 mant = 0;
        int exp = 0;
        fl::minimp3_fixed_probe::mp3d_gain_from_quarters(quarters, &mant, &exp);

        // The pair must reconstruct 2**(quarters/4) exactly in the sense that
        // the mantissa is one of the four quarter-powers and the exponent
        // carries the rest -- checked here against the closed form.
        const double reconstructed =
            static_cast<double>(mant) * fl::pow(2.0, static_cast<double>(exp - 30));
        const double expected = fl::pow(2.0, quarters / 4.0);
        const double relative =
            fl::fabs(reconstructed - expected) / expected;
        if (relative > worst_relative) {
            worst_relative = relative;
            worst_q = quarters;
        }
        FL_REQUIRE_GE(mant, (fl::i32)1 << 30);
    }
    printf("[gain] worst relative error %.3e at quarters=%d\n", worst_relative,
           worst_q);
    // Only the Q30 rounding of 2**(r/4) contributes; the exponent is exact.
    FL_CHECK_LT(worst_relative, 1e-9);
}

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

namespace {

/// Captures every stage buffer of a whole decode, converted to double so the
/// two builds' sample types can be compared directly. Q26 is exact in double,
/// so the conversion introduces no error of its own.
class Mp3StageCaptureSink : public fl::Mp3StageSink {
  public:
    void onStage(int stage, int, const float* buf, int count) FL_NO_EXCEPT override {
        if (stage < 0 || stage >= MINIMP3_STAGE_COUNT) {
            return;
        }
        for (int i = 0; i < count; ++i) {
            mValues[stage].push_back(static_cast<double>(buf[i]));
        }
    }

    void onStage(int stage, int, const fl::i32* buf, int count) FL_NO_EXCEPT override {
        if (stage < 0 || stage >= MINIMP3_STAGE_COUNT) {
            return;
        }
        for (int i = 0; i < count; ++i) {
            mValues[stage].push_back(static_cast<double>(buf[i]) /
                                     static_cast<double>(1 << MINIMP3_FRAC_BITS));
        }
    }

    const fl::vector<double>& values(int stage) const { return mValues[stage]; }

  private:
    fl::vector<double> mValues[MINIMP3_STAGE_COUNT];
};

} // anonymous namespace

FL_TEST_CASE("minimp3 fixed-point matches float stage by stage") {
    fl::setTestFileSystemRoot("tests/data");
    const fl::vector<fl::u8> bytes =
        readCorpusBytes("codec/minimp3/l3-hecommon.bit");

    Mp3StageCaptureSink flt;
    fl::SetMp3StageSink(&flt);
    decodeWithVariant<fl::Minimp3FloatVariant>(bytes);
    fl::ClearMp3StageSink();

    Mp3StageCaptureSink fixed;
    fl::SetMp3StageSink(&fixed);
    decodeWithVariant<fl::Minimp3FixedVariant>(bytes);
    fl::ClearMp3StageSink();

    static const char* const kStageNames[MINIMP3_STAGE_COUNT] = {
        "huffman", "stereo", "antialias", "imdct", "dct2"};

    for (int stage = 0; stage < MINIMP3_STAGE_COUNT; ++stage) {
        const fl::vector<double>& a = flt.values(stage);
        const fl::vector<double>& b = fixed.values(stage);
        // Length equality is a structural property: the two builds must visit
        // the same stages the same number of times. A mismatch is a control
        // flow divergence, not a numeric one.
        FL_REQUIRE_EQ(b.size(), a.size());
        FL_REQUIRE_GT(a.size(), 0u);

        double worst = 0.0;
        for (fl::size i = 0; i < a.size(); ++i) {
            const double diff = fl::fabs(a[i] - b[i]);
            if (diff > worst) {
                worst = diff;
            }
        }
        printf("[stage-diff] %-10s worst abs diff %.3e over %zu values\n",
               kStageNames[stage], worst, a.size());
        // Stage values live around 0.5, so this is an absolute bound at
        // roughly 1e-4 of full scale. It localises a regression to one kernel:
        // whichever stage fails first is the one that broke.
        FL_CHECK_LT(worst, 1e-4);
    }
}

namespace {

/// Deterministic xorshift, so a failure is reproducible from the seed alone.
class FuzzRandom {
  public:
    explicit FuzzRandom(fl::u32 seed) : mState(seed ? seed : 1u) {}
    fl::u32 next() {
        mState ^= mState << 13;
        mState ^= mState >> 17;
        mState ^= mState << 5;
        return mState;
    }
    fl::u32 below(fl::u32 bound) { return bound ? next() % bound : 0u; }

  private:
    fl::u32 mState;
};

} // anonymous namespace

FL_TEST_CASE("minimp3 fixed-point and float agree on mutated bitstreams") {
    fl::setTestFileSystemRoot("tests/data");
    const fl::vector<fl::u8> original =
        readCorpusBytes("codec/minimp3/l3-hecommon.bit");
    FL_REQUIRE_GT(original.size(), 64u);

    FuzzRandom rng(0x5eed1234u);
    int worst_diff = 0;
    fl::size total_frames = 0;
    fl::size total_samples = 0;
    fl::size wide_samples = 0;

    // Corpus mutation rather than random noise: random bytes almost never
    // reach the DSP, whereas a mutated real stream keeps passing the header
    // checks and drives Huffman, stereo and the IMDCT with values a valid
    // encoder would never emit. That is the input class the +/-1.0 dequant
    // clamp and the saturating butterflies exist for, so it is the one worth
    // running under the sanitizers.
    for (int iteration = 0; iteration < 96; ++iteration) {
        fl::vector<fl::u8> mutated = original;
        const fl::u32 mutations = 1u + rng.below(24u);
        for (fl::u32 m = 0; m < mutations; ++m) {
            const fl::u32 index = rng.below(static_cast<fl::u32>(mutated.size()));
            mutated[index] = static_cast<fl::u8>(rng.next() & 0xffu);
        }

        const VariantDecodeResult flt =
            decodeWithVariant<fl::Minimp3FloatVariant>(mutated);
        const VariantDecodeResult fixed =
            decodeWithVariant<fl::Minimp3FixedVariant>(mutated);

        // Frame acceptance and bit consumption come from the shared bitstream
        // layer, so they must agree exactly no matter how corrupt the input.
        FL_REQUIRE_EQ(fixed.mFrames, flt.mFrames);
        FL_REQUIRE_EQ(fixed.mPcm.size(), flt.mPcm.size());
        total_frames += static_cast<fl::size>(flt.mFrames);

        const PcmComparison cmp = comparePcm(flt.mPcm, fixed.mPcm);
        if (cmp.mMaxAbsDiff > worst_diff) {
            worst_diff = cmp.mMaxAbsDiff;
        }
        for (fl::size i = 0; i < cmp.mCount; ++i) {
            const int diff = static_cast<int>(flt.mPcm[i]) -
                             static_cast<int>(fixed.mPcm[i]);
            if (diff > 8 || diff < -8) {
                ++wide_samples;
            }
        }
        total_samples += cmp.mCount;
    }

    const double wide_fraction =
        total_samples ? static_cast<double>(wide_samples) /
                            static_cast<double>(total_samples)
                      : 0.0;
    printf("[fuzz] %zu frames, %zu samples, worst diff %d LSB, "
           "%.4f%% beyond 8 LSB\n",
           total_frames, total_samples, worst_diff, wide_fraction * 100.0);

    FL_CHECK_GT(total_frames, 0u);
    // The gate deliberately is not a max-difference bound. On a corrupted
    // stream the float decoder's own output is meaningless -- dequantisation
    // produces astronomically large values and the polyphase result clips --
    // so "the two decoders agree to N LSB" is not a property worth asserting
    // there, and asserting it would only be satisfiable by a number so large
    // it proves nothing. Two things are worth asserting, and they are checked
    // above and here: the builds never diverge structurally (identical frame
    // acceptance and sample counts, which come from the shared bitstream
    // layer), and the divergence stays rare rather than becoming the norm.
    //
    // Measured: 0.105% of samples beyond 8 LSB. The bound is set at 2%, which
    // catches a clamp or saturation defect turning divergence into the common
    // case while tolerating the handful of frames where a mutated stream
    // genuinely drives the two arithmetics apart.
    //
    // The real payload of this test is running under ASan/UBSan
    // (`bash test fl_codec --debug`): signed overflow in the butterflies is
    // precisely what the +/-1.0 dequant clamp and the saturating adds exist to
    // prevent, and this is the input class that would expose their absence.
    FL_CHECK_LT(wide_fraction, 0.02);
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
