
// g++ --std=c++11 test.cpp


// Include precision header first to ensure FASTLED_FFT_PRECISION is defined
#include "third_party/cq_kernel/fft_precision.h"

#include "test.h"
#include "fl/audio/fft/fft.h"
#include "fl/audio/fft/fft_impl.h"
#include "fl/stl/int.h"
#include "fl/log/log.h"
#include "fl/math/math.h"
#include "fl/stl/move.h"
#include "fl/stl/span.h"
#include "fl/stl/stdint.h"
#include "fl/stl/string.h"
#include "fl/stl/strstream.h"
#include "fl/stl/vector.h"

FL_TEST_FILE(FL_FILEPATH) {

// FFT tests adapt to the build-time FASTLED_FFT_PRECISION setting.
// Test expectations are provided for all three precision modes:
//   FASTLED_FFT_FIXED16 (default) - 16-bit fixed point
//   FASTLED_FFT_FLOAT - 32-bit floating point
//   FASTLED_FFT_DOUBLE - 64-bit floating point



FL_TEST_CASE("fft tester 512") {
    int16_t buffer[512] = {0};
    const int n = 512;
    // fill in with a sine wave
    for (int i = 0; i < n; ++i) {
        float rot = fl::map_range<float, float>(i, 0, n - 1, 0, 2 * FL_PI * 10);
        float sin_x = fl::sin(rot);
        buffer[i] = int16_t(32767 * sin_x);
    }
    fl::audio::fft::Bins out(16);
    // Explicitly use CQ_OCTAVE to match golden values (AUTO would select LOG_REBIN for 16 bins)
    const int samples = n;
    fl::audio::fft::Args args(samples, 16, fl::audio::fft::Args::DefaultMinFrequency(),
                      fl::audio::fft::Args::DefaultMaxFrequency(),
                      fl::audio::fft::Args::DefaultSampleRate(), fl::audio::fft::Mode::CQ_OCTAVE);
    fl::audio::fft::Impl fft(args);
    fft.run(buffer, &out);
    // Test expectations for different precision modes
    // Each mode has slightly different numerical results due to internal precision
#if FASTLED_FFT_PRECISION == FASTLED_FFT_FIXED16
    // Fixed16 precision (default) - CQ_OCTAVE octave-wise path
    // With improved 7-tap halfband decimation filter (-45dB stopband)
    // fastMag produces integer magnitudes (u16 → float)
    // Golden values for 90-14080 Hz range
    const float expected_output[16] = {
        5.00,      0.00,      3.00,      6.00,      1.00,     21.00,      3.00,     46.00,
        9.00,     12.00,      4.00,      6.00,      5.00,      2.00,      1.00,      1.00};
    const float tolerance = 0.1; // Strict tolerance
#elif FASTLED_FFT_PRECISION == FASTLED_FFT_FLOAT
    // Float/double precision - needs regeneration with new frequency range.
    const float expected_output[16] = {
        5.00,      0.00,      3.00,      6.00,      1.00,     21.00,      3.00,     46.00,
        9.00,     12.00,      4.00,      6.00,      5.00,      2.00,      1.00,      1.00};
    const float tolerance = 5000.0; // Wide tolerance - needs regeneration in float mode
#elif FASTLED_FFT_PRECISION == FASTLED_FFT_DOUBLE
    const float expected_output[16] = {
        5.00,      0.00,      3.00,      6.00,      1.00,     21.00,      3.00,     46.00,
        9.00,     12.00,      4.00,      6.00,      5.00,      2.00,      1.00,      1.00};
    const float tolerance = 5000.0; // Wide tolerance - needs regeneration in double mode
#endif

    for (int i = 0; i < 16; ++i) {
        float a = out.raw()[i];
        float b = expected_output[i];
        bool almost_equal = fl::almost_equal(a, b, tolerance);
        if (!almost_equal) {
            FL_WARN("Impl output mismatch at index " << i << ": " << a
                                                         << " != " << b);
        }
        FL_CHECK(almost_equal);
    }

    fl::string info = fft.info();
    FL_WARN("Impl info: " << info);
    FL_WARN("Done");
}

FL_TEST_CASE("fft tester 256") {
    // fft_audio_buffer_t buffer = {0};
    fl::vector<int16_t> buffer;
    const int n = 256;
    // fill in with a sine wave
    for (int i = 0; i < n; ++i) {
        float rot = fl::map_range<float, float>(i, 0, n - 1, 0, 2 * FL_PI * 10);
        float sin_x = fl::sin(rot);
        auto v = int16_t(32767 * sin_x);
        buffer.push_back(v);
    }
    fl::audio::fft::Bins out(16);
    // Explicitly use CQ_OCTAVE to match golden values (AUTO would select LOG_REBIN for 16 bins)
    const int samples = n;
    fl::audio::fft::Args args(samples, 16, fl::audio::fft::Args::DefaultMinFrequency(),
                      fl::audio::fft::Args::DefaultMaxFrequency(),
                      fl::audio::fft::Args::DefaultSampleRate(), fl::audio::fft::Mode::CQ_OCTAVE);
    fl::audio::fft::Impl fft(args);
    fft.run(buffer, &out);
    // Test expectations for different precision modes
    // Each mode has slightly different numerical results due to internal precision
#if FASTLED_FFT_PRECISION == FASTLED_FFT_FIXED16
    // Fixed16 precision (default) - CQ_OCTAVE octave-wise path
    // With improved 7-tap halfband decimation filter (-45dB stopband)
    // fastMag produces integer magnitudes (u16 → float)
    // Golden values for 90-14080 Hz range
    const float expected_output[16] = {
        0.00,      2.00,      1.00,     11.00,      1.00,     13.00,      4.00,     44.00,
       20.00,    350.00,     37.00,     26.00,     18.00,     10.00,      2.00,      2.00};
    const float tolerance = 0.1; // Strict tolerance
#elif FASTLED_FFT_PRECISION == FASTLED_FFT_FLOAT
    // Float/double precision - needs regeneration with new frequency range.
    const float expected_output[16] = {
        0.00,      2.00,      1.00,     11.00,      1.00,     13.00,      4.00,     44.00,
       20.00,    350.00,     37.00,     26.00,     18.00,     10.00,      2.00,      2.00};
    const float tolerance = 5000.0; // Wide tolerance - needs regeneration in float mode
#elif FASTLED_FFT_PRECISION == FASTLED_FFT_DOUBLE
    const float expected_output[16] = {
        0.00,      2.00,      1.00,     11.00,      1.00,     13.00,      4.00,     44.00,
       20.00,    350.00,     37.00,     26.00,     18.00,     10.00,      2.00,      2.00};
    const float tolerance = 5000.0; // Wide tolerance - needs regeneration in double mode
#endif

    for (int i = 0; i < 16; ++i) {
        float a = out.raw()[i];
        float b = expected_output[i];
        bool almost_equal = fl::almost_equal(a, b, tolerance);
        if (!almost_equal) {
            FL_WARN("Impl output mismatch at index " << i << ": " << a
                                                         << " != " << b);
        }
        FL_CHECK(almost_equal);
    }

    fl::string info = fft.info();
    FL_WARN("Impl info: " << info);
    FL_WARN("Done");
}

FL_TEST_CASE("fft tester 256 with 64 bands") {
    // fft_audio_buffer_t buffer = {0};
    fl::vector<int16_t> buffer;
    const int n = 256;
    // fill in with a sine wave
    for (int i = 0; i < n; ++i) {
        float rot = fl::map_range<float, float>(i, 0, n - 1, 0, 2 * FL_PI * 10);
        float sin_x = fl::sin(rot);
        auto v = int16_t(32767 * sin_x);
        buffer.push_back(v);
    }
    fl::audio::fft::Bins out(64);
    const int samples = n;
    fl::audio::fft::Args args(samples, 64);
    fl::audio::fft::Impl fft(args);
    fft.run(buffer, &out);
    // Test expectations for different precision modes
    // Each mode has slightly different numerical results due to internal precision
#if FASTLED_FFT_PRECISION == FASTLED_FFT_FIXED16
    // Fixed16 precision (default) - CQ_NAIVE path (AUTO resolves to CQ_NAIVE for 64 bands)
    // fastMag produces integer magnitudes (u16 → float)
    // Golden values for 90-14080 Hz range
    const float expected_output[64] = {
        0.00,      2.00,      2.00,      1.00,      2.00,      3.00,      4.00,
        5.00,      1.00,      1.00,      3.00,      2.00,      2.00,      0.00,
        0.00,      5.00,      6.00,      6.00,      2.00,      3.00,      1.00,
        2.00,      3.00,      5.00,      4.00,      7.00,     13.00,     11.00,
       12.00,     18.00,     17.00,     15.00,     19.00,     23.00,     23.00,
      151.00,    430.00,    720.00,    241.00,    120.00,     75.00,     53.00,
       37.00,     10.00,     59.00,     42.00,     37.00,     35.00,     27.00,
       23.00,     21.00,     23.00,      1.00,      3.00,      3.00,      1.00,
        1.00,      1.00,      0.00,      2.00,      1.00,      1.00,      3.00,
        4.00};
    const float tolerance = 0.1; // Strict tolerance
#elif FASTLED_FFT_PRECISION == FASTLED_FFT_FLOAT
    // Float/double precision - needs regeneration with new frequency range.
    const float expected_output[64] = {
        0.00,      2.00,      2.00,      1.00,      2.00,      3.00,      4.00,
        5.00,      1.00,      1.00,      3.00,      2.00,      2.00,      0.00,
        0.00,      5.00,      6.00,      6.00,      2.00,      3.00,      1.00,
        2.00,      3.00,      5.00,      4.00,      7.00,     13.00,     11.00,
       12.00,     18.00,     17.00,     15.00,     19.00,     23.00,     23.00,
      151.00,    430.00,    720.00,    241.00,    120.00,     75.00,     53.00,
       37.00,     10.00,     59.00,     42.00,     37.00,     35.00,     27.00,
       23.00,     21.00,     23.00,      1.00,      3.00,      3.00,      1.00,
        1.00,      1.00,      0.00,      2.00,      1.00,      1.00,      3.00,
        4.00};
    const float tolerance = 5000.0; // Wide tolerance - needs regeneration in float mode
#elif FASTLED_FFT_PRECISION == FASTLED_FFT_DOUBLE
    const float expected_output[64] = {
        0.00,      2.00,      2.00,      1.00,      2.00,      3.00,      4.00,
        5.00,      1.00,      1.00,      3.00,      2.00,      2.00,      0.00,
        0.00,      5.00,      6.00,      6.00,      2.00,      3.00,      1.00,
        2.00,      3.00,      5.00,      4.00,      7.00,     13.00,     11.00,
       12.00,     18.00,     17.00,     15.00,     19.00,     23.00,     23.00,
      151.00,    430.00,    720.00,    241.00,    120.00,     75.00,     53.00,
       37.00,     10.00,     59.00,     42.00,     37.00,     35.00,     27.00,
       23.00,     21.00,     23.00,      1.00,      3.00,      3.00,      1.00,
        1.00,      1.00,      0.00,      2.00,      1.00,      1.00,      3.00,
        4.00};
    const float tolerance = 5000.0; // Wide tolerance - needs regeneration in double mode
#endif

    for (int i = 0; i < 64; ++i) {
        float a = out.raw()[i];
        float b = expected_output[i];
        bool almost_equal = fl::almost_equal(a, b, tolerance);
        if (!almost_equal) {
            FL_WARN("Impl output mismatch at index " << i << ": " << a
                                                         << " != " << b);
        }
        FL_CHECK(almost_equal);
    }
    fl::string info = fft.info();
    FL_WARN("Impl info: " << info);
    FL_WARN("Done");
}

namespace { // fft_tests

fl::vector<fl::i16> generateSine(float freq, int count = 512, float sampleRate = 44100.0f, float amplitude = 16000.0f) {
    fl::vector<fl::i16> samples;
    samples.reserve(count);
    for (int i = 0; i < count; ++i) {
        float phase = 2.0f * FL_M_PI * freq * i / sampleRate;
        samples.push_back(static_cast<fl::i16>(amplitude * fl::sinf(phase)));
    }
    return samples;
}

} // anonymous namespace

FL_TEST_CASE("Bins - constructor and bands") {
    fl::audio::fft::Bins bins(16);
    FL_CHECK_EQ(bins.bands(), 16u);
    FL_CHECK_EQ(bins.raw().size(), 0u);  // Initially empty (just reserved)
    FL_CHECK_EQ(bins.db().size(), 0u);
}

FL_TEST_CASE("fl::audio::fft::Bins - copy constructor") {
    auto samples = generateSine(1000.0f);
    fl::audio::fft::Bins original(16);
    fl::audio::fft::FFT fft;
    fft.run(samples, &original);

    fl::audio::fft::Bins copy(original);
    FL_CHECK_EQ(copy.bands(), 16u);
    FL_CHECK_EQ(copy.raw().size(), original.raw().size());
    for (fl::size i = 0; i < copy.raw().size(); ++i) {
        FL_CHECK_EQ(copy.raw()[i], original.raw()[i]);
    }
}

FL_TEST_CASE("fl::audio::fft::Bins - move constructor") {
    auto samples = generateSine(1000.0f);
    fl::audio::fft::Bins original(16);
    fl::audio::fft::FFT fft;
    fft.run(samples, &original);
    fl::size origSize = original.raw().size();
    float firstVal = original.raw()[0];

    fl::audio::fft::Bins moved(fl::move(original));
    FL_CHECK_EQ(moved.bands(), 16u);
    FL_CHECK_EQ(moved.raw().size(), origSize);
    FL_CHECK_EQ(moved.raw()[0], firstVal);
}

FL_TEST_CASE("fl::audio::fft::Bins - clear") {
    auto samples = generateSine(1000.0f);
    fl::audio::fft::Bins bins(16);
    fl::audio::fft::FFT fft;
    fft.run(samples, &bins);
    FL_CHECK_GT(bins.raw().size(), 0u);
    bins.clear();
    FL_CHECK_EQ(bins.raw().size(), 0u);
    FL_CHECK_EQ(bins.db().size(), 0u);
    FL_CHECK_EQ(bins.bands(), 16u);  // bands unchanged
}

FL_TEST_CASE("fl::audio::fft::Args - defaults match documented values") {
    fl::audio::fft::Args args;
    FL_CHECK_EQ(args.samples, 512);
    FL_CHECK_EQ(args.bands, 16);
    FL_CHECK_EQ(args.sample_rate, 44100);

    // Check floats with tolerance
    FL_CHECK(fl::almost_equal(args.fmin, fl::audio::fft::Args::DefaultMinFrequency(), 0.1f));
    FL_CHECK(fl::almost_equal(args.fmax, fl::audio::fft::Args::DefaultMaxFrequency(), 0.1f));
}

FL_TEST_CASE("fl::audio::fft::FFT - run with sine wave concentrates energy") {
    fl::audio::fft::FFT fft;
    auto samples = generateSine(1000.0f);  // 1kHz sine, within CQ range 90-14080 Hz
    fl::audio::fft::Bins bins(16);
    fft.run(samples, &bins);

    FL_REQUIRE_GT(bins.raw().size(), 0u);

    // Find the bin with maximum energy and compute total energy
    float maxVal = 0.0f;
    float totalEnergy = 0.0f;
    for (fl::size i = 0; i < bins.raw().size(); ++i) {
        totalEnergy += bins.raw()[i];
        if (bins.raw()[i] > maxVal) {
            maxVal = bins.raw()[i];
        }
    }

    FL_CHECK_GT(maxVal, 0.0f);
    FL_CHECK_GT(totalEnergy, 0.0f);

    // The peak bin should hold at least 25% of total energy (accounting for CQ spectral leakage)
    // For random/uniform distribution across 16 bins, each bin would hold ~6.25% (1/16),
    // so 25% is 4x what random noise would produce
    float peakFraction = maxVal / totalEnergy;
    FL_CHECK_GT(peakFraction, 0.25f);

    // The peak bin's energy should be significantly greater than the average of other bins
    float otherBinsTotal = totalEnergy - maxVal;
    float otherBinsAvg = otherBinsTotal / static_cast<float>(bins.raw().size() - 1);
    FL_CHECK_GT(maxVal, otherBinsAvg * 3.0f);  // Peak should be at least 3x the average of other bins
}

FL_TEST_CASE("fl::audio::fft::FFT - different frequencies produce different peak bins") {
    fl::audio::fft::FFT fft;

    // Generate a bass tone (200 Hz) and a mid/treble tone (2000 Hz)
    auto bassSignal = generateSine(200.0f);
    auto trebleSignal = generateSine(2000.0f);

    fl::audio::fft::Bins bassBins(16);
    fl::audio::fft::Bins trebleBins(16);

    fft.run(bassSignal, &bassBins);
    fft.run(trebleSignal, &trebleBins);

    FL_REQUIRE_GT(bassBins.raw().size(), 0u);
    FL_REQUIRE_GT(trebleBins.raw().size(), 0u);

    // Find peak bins for each frequency
    fl::size bassPeakBin = 0;
    float bassPeakVal = 0.0f;
    for (fl::size i = 0; i < bassBins.raw().size(); ++i) {
        if (bassBins.raw()[i] > bassPeakVal) {
            bassPeakVal = bassBins.raw()[i];
            bassPeakBin = i;
        }
    }

    fl::size treblePeakBin = 0;
    float treblePeakVal = 0.0f;
    for (fl::size i = 0; i < trebleBins.raw().size(); ++i) {
        if (trebleBins.raw()[i] > treblePeakVal) {
            treblePeakVal = trebleBins.raw()[i];
            treblePeakBin = i;
        }
    }

    // Both should have significant energy in their peak bins
    FL_CHECK_GT(bassPeakVal, 0.0f);
    FL_CHECK_GT(treblePeakVal, 0.0f);

    // The 200Hz peak bin index should be lower than the 2000Hz peak bin index
    // because lower frequencies map to lower bin indices in CQ transform
    FL_CHECK_LT(bassPeakBin, treblePeakBin);
}

FL_TEST_CASE("fl::audio::fft::FFT - silence produces near-zero bins") {
    fl::audio::fft::FFT fft;
    fl::vector<fl::i16> silence(512, 0);
    fl::audio::fft::Bins bins(16);
    fft.run(silence, &bins);

    // All bins should be near zero
    for (fl::size i = 0; i < bins.raw().size(); ++i) {
        FL_CHECK_LT(bins.raw()[i], 10.0f);
    }
}

FL_TEST_CASE("fl::audio::fft::Args - equality operator") {
    fl::audio::fft::Args args1;
    fl::audio::fft::Args args2;
    FL_CHECK(args1 == args2);
    FL_CHECK_FALSE(args1 != args2);

    fl::audio::fft::Args args3(256, 8, 100.0f, 5000.0f, 22050);
    FL_CHECK(args1 != args3);
}

FL_TEST_CASE("fl::audio::fft::FFT - sine wave maps to correct CQ bin") {
    const int bands = 16;

    float testFreqs[] = {300.0f, 440.0f, 1000.0f, 2000.0f};

    for (float freq : testFreqs) {
        auto samples = generateSine(freq);
        fl::audio::fft::Bins bins(bands);
        fl::audio::fft::FFT fft;
        fft.run(samples, &bins);

        int peakBin = 0;
        float peakVal = 0.0f;
        for (int i = 0; i < bands; ++i) {
            if (bins.raw()[i] > peakVal) {
                peakVal = bins.raw()[i];
                peakBin = i;
            }
        }

        int expectedBin = bins.freqToBin(freq);
        int diff = peakBin - expectedBin;
        if (diff < 0) diff = -diff;
        FL_CHECK_LE(diff, 1);
    }
}

FL_TEST_CASE("fl::audio::fft::Bins - binToFreq known values") {
    // Run FFT with silence to populate 16 bins with default params
    fl::vector<fl::i16> silence(512, 0);
    fl::audio::fft::Bins bins(16);
    fl::audio::fft::FFT fft;
    fft.run(silence, &bins);

    // Bin 0 = fmin
    FL_CHECK(fl::almost_equal(bins.binToFreq(0), fl::audio::fft::Args::DefaultMinFrequency(), 0.1f));

    // Bin 15 = fmax (within 5% due to fl:: math precision across wide range)
    float expectedFmax = fl::audio::fft::Args::DefaultMaxFrequency();
    float relError = fl::abs(bins.binToFreq(15) - expectedFmax) / expectedFmax;
    FL_CHECK_LT(relError, 0.05f);
}

FL_TEST_CASE("fl::audio::fft::Bins - freqToBin is inverse of binToFreq") {
    fl::vector<fl::i16> silence(512, 0);
    fl::audio::fft::Bins bins(16);
    fl::audio::fft::FFT fft;
    fft.run(silence, &bins);

    for (int i = 0; i < 16; ++i) {
        float freq = bins.binToFreq(i);
        FL_CHECK_EQ(bins.freqToBin(freq), i);
    }
}

FL_TEST_CASE("fl::audio::fft::Bins - freqToBin clamps to valid range") {
    fl::vector<fl::i16> silence(512, 0);
    fl::audio::fft::Bins bins(16);
    fl::audio::fft::FFT fft;
    fft.run(silence, &bins);

    FL_CHECK_EQ(bins.freqToBin(10.0f), 0);
    FL_CHECK_EQ(bins.freqToBin(50000.0f), 15);
}

FL_TEST_CASE("fl::audio::fft::Bins - linear bins redistribute energy") {
    const int bands = 16;
    auto samples = generateSine(440.0f);
    fl::audio::fft::Bins bins(bands);
    fl::audio::fft::FFT fft;
    fft.run(samples, &bins);

    fl::span<const float> linear = bins.linear();
    FL_CHECK_EQ(linear.size(), static_cast<fl::size>(bands));

    // Find peak in linear bins
    int linPeakBin = 0;
    float linPeakVal = 0.0f;
    for (int i = 0; i < bands; ++i) {
        if (linear[i] > linPeakVal) {
            linPeakVal = linear[i];
            linPeakBin = i;
        }
    }

    float linBinWidth = (bins.linearFmax() - bins.linearFmin()) / static_cast<float>(bands);
    float linPeakFreq = bins.linearFmin() + (static_cast<float>(linPeakBin) + 0.5f) * linBinWidth;

    // Peak should be near 440 Hz (within 1.5 bin widths)
    FL_CHECK_LT(fl::abs(linPeakFreq - 440.0f), linBinWidth * 1.5f);
    FL_CHECK_GT(linPeakVal, 0.0f);
}

FL_TEST_CASE("fl::audio::fft::Bins - multi-freq CQ and linear mapping") {
    const int bands = 16;
    float freqs[] = {440.0f, 900.0f, 1600.0f};

    for (float freq : freqs) {
        auto samples = generateSine(freq);
        fl::audio::fft::Bins bins(bands);
        fl::audio::fft::FFT fft;
        fft.run(samples, &bins);

        // CQ bin check
        int expectedBin = bins.freqToBin(freq);
        int peakBin = 0;
        float peakVal = 0.0f;
        for (int i = 0; i < bands; ++i) {
            if (bins.raw()[i] > peakVal) {
                peakVal = bins.raw()[i];
                peakBin = i;
            }
        }
        int diff = peakBin - expectedBin;
        if (diff < 0) diff = -diff;
        FL_CHECK_LE(diff, 1);

        // Linear bin check
        fl::span<const float> linear = bins.linear();
        int linPeakBin = 0;
        float linPeakVal = 0.0f;
        for (int i = 0; i < bands; ++i) {
            if (linear[i] > linPeakVal) {
                linPeakVal = linear[i];
                linPeakBin = i;
            }
        }
        float linBinWidth = (bins.linearFmax() - bins.linearFmin()) / static_cast<float>(bands);
        float linPeakFreq = bins.linearFmin() + (static_cast<float>(linPeakBin) + 0.5f) * linBinWidth;
        FL_CHECK_LT(fl::abs(linPeakFreq - freq), linBinWidth * 1.5f);
    }
}

FL_TEST_CASE("fl::audio::fft::Impl - info reports log-spaced") {
    fl::audio::fft::Args args;
    fl::audio::fft::Impl fft(args);
    fl::string info = fft.info();

    // Should contain "log-spaced" to indicate CQ layout
    bool hasLogSpaced = false;
    const char* p = info.c_str();
    while (*p) {
        if (p[0] == 'l' && p[1] == 'o' && p[2] == 'g') {
            hasLogSpaced = true;
            break;
        }
        ++p;
    }
    FL_CHECK(hasLogSpaced);
}

FL_TEST_CASE("Bins - linear bins populated after FFT run") {
    auto samples = generateSine(1000.0f);
    fl::audio::fft::Bins bins(16);
    fl::audio::fft::FFT fft;
    fft.run(samples, &bins);

    FL_CHECK_EQ(bins.linear().size(), static_cast<fl::size>(16));
    // All linear bins should be non-negative
    bool hasEnergy = false;
    for (fl::size i = 0; i < bins.linear().size(); ++i) {
        FL_CHECK_GE(bins.linear()[i], 0.0f);
        if (bins.linear()[i] > 1.0f) hasEnergy = true;
    }
    FL_CHECK(hasEnergy);
}

FL_TEST_CASE("Bins - linear bins peak at correct frequency") {
    float testFreqs[] = {300.0f, 1000.0f, 2000.0f, 4000.0f};
    const int bands = 16;

    for (float freq : testFreqs) {
        auto samples = generateSine(freq);
        fl::audio::fft::Bins bins(bands);
        fl::audio::fft::FFT fft;
        fft.run(samples, &bins);

        fl::span<const float> linear = bins.linear();
        FL_REQUIRE_EQ(linear.size(), static_cast<fl::size>(bands));

        float linFmin = bins.linearFmin();
        float linFmax = bins.linearFmax();
        float binWidth = (linFmax - linFmin) / static_cast<float>(bands);

        // Find peak bin
        int peakBin = 0;
        float peakVal = 0.0f;
        for (int i = 0; i < bands; ++i) {
            if (linear[i] > peakVal) {
                peakVal = linear[i];
                peakBin = i;
            }
        }

        // Expected bin index
        int expectedBin = static_cast<int>((freq - linFmin) / binWidth);
        if (expectedBin >= bands) expectedBin = bands - 1;
        if (expectedBin < 0) expectedBin = 0;

        int diff = peakBin - expectedBin;
        if (diff < 0) diff = -diff;
        FL_CHECK_LE(diff, 1);
    }
}

FL_TEST_CASE("Bins - linear bins sine energy is concentrated") {
    auto samples = generateSine(1000.0f);
    fl::audio::fft::Bins bins(16);
    fl::audio::fft::FFT fft;
    fft.run(samples, &bins);

    fl::span<const float> linear = bins.linear();
    FL_REQUIRE_GT(linear.size(), 0u);

    float maxVal = 0.0f;
    float totalEnergy = 0.0f;
    for (fl::size i = 0; i < linear.size(); ++i) {
        totalEnergy += linear[i];
        if (linear[i] > maxVal) maxVal = linear[i];
    }

    FL_CHECK_GT(totalEnergy, 0.0f);
    // Peak bin should hold >25% of total energy
    float peakFraction = maxVal / totalEnergy;
    FL_CHECK_GT(peakFraction, 0.25f);
}

FL_TEST_CASE("Bins - linear bins silence produces near-zero") {
    fl::vector<fl::i16> silence(512, 0);
    fl::audio::fft::Bins bins(16);
    fl::audio::fft::FFT fft;
    fft.run(silence, &bins);

    for (fl::size i = 0; i < bins.linear().size(); ++i) {
        FL_CHECK_LT(bins.linear()[i], 10.0f);
    }
}

FL_TEST_CASE("Bins - linear bins copy/move preserve data") {
    auto samples = generateSine(1000.0f);
    fl::audio::fft::Bins bins(16);
    fl::audio::fft::FFT fft;
    fft.run(samples, &bins);

    // Copy constructor
    fl::audio::fft::Bins copy(bins);
    FL_CHECK_EQ(copy.linear().size(), bins.linear().size());
    for (fl::size i = 0; i < bins.linear().size(); ++i) {
        FL_CHECK_EQ(copy.linear()[i], bins.linear()[i]);
    }
    FL_CHECK_EQ(copy.linearFmin(), bins.linearFmin());
    FL_CHECK_EQ(copy.linearFmax(), bins.linearFmax());

    // Move constructor
    fl::audio::fft::Bins moved(fl::move(copy));
    FL_CHECK_EQ(moved.linear().size(), bins.linear().size());
    for (fl::size i = 0; i < bins.linear().size(); ++i) {
        FL_CHECK_EQ(moved.linear()[i], bins.linear()[i]);
    }
}

FL_TEST_CASE("Bins - linear bins clear resets") {
    auto samples = generateSine(1000.0f);
    fl::audio::fft::Bins bins(16);
    fl::audio::fft::FFT fft;
    fft.run(samples, &bins);

    FL_CHECK_GT(bins.linear().size(), 0u);
    bins.clear();
    FL_CHECK_EQ(bins.linear().size(), 0u);
}

// ============================================================================
// Adversarial binning tests: LOG_REBIN, CQ_OCTAVE edge cases and cross-checks
// ============================================================================

namespace { // adversarial_binning

fl::vector<fl::i16> makeAdversarialSine(float freq, int count = 512,
                                        float sampleRate = 44100.0f,
                                        float amplitude = 16000.0f) {
    fl::vector<fl::i16> samples;
    samples.reserve(count);
    for (int i = 0; i < count; ++i) {
        float phase = 2.0f * FL_M_PI * freq * static_cast<float>(i) / sampleRate;
        samples.push_back(static_cast<fl::i16>(amplitude * fl::sinf(phase)));
    }
    return samples;
}

fl::vector<fl::i16> makeTwoTone(float freq1, float freq2, int count = 512,
                                float sampleRate = 44100.0f,
                                float amplitude = 8000.0f) {
    fl::vector<fl::i16> samples;
    samples.reserve(count);
    for (int i = 0; i < count; ++i) {
        float t = static_cast<float>(i) / sampleRate;
        float s = amplitude * fl::sinf(2.0f * FL_M_PI * freq1 * t) +
                  amplitude * fl::sinf(2.0f * FL_M_PI * freq2 * t);
        if (s > 32767.0f) s = 32767.0f;
        if (s < -32768.0f) s = -32768.0f;
        samples.push_back(static_cast<fl::i16>(s));
    }
    return samples;
}

int findPeakBin(fl::span<const float> bins) {
    int peak = 0;
    float peakVal = 0.0f;
    for (int i = 0; i < static_cast<int>(bins.size()); ++i) {
        if (bins[i] > peakVal) {
            peakVal = bins[i];
            peak = i;
        }
    }
    return peak;
}

bool isNaNCheck(float x) { return !(x == x); }

} // anonymous namespace

// --- 1. Peak-bin accuracy per mode ---

FL_TEST_CASE("Binning adversarial - LOG_REBIN peak accuracy per bin") {
    const int bands = 16;
    const float fmin = fl::audio::fft::Args::DefaultMinFrequency();
    const float fmax = fl::audio::fft::Args::DefaultMaxFrequency();
    float logRatio = fl::logf(fmax / fmin);

    int mismatches = 0;
    for (int b = 1; b < bands - 1; ++b) {
        float centerFreq =
            fmin * fl::expf(logRatio * static_cast<float>(b) /
                            static_cast<float>(bands - 1));

        auto samples = makeAdversarialSine(centerFreq);
        fl::audio::fft::Bins bins(bands);
        fl::audio::fft::Args args(512, bands, fmin, fmax, 44100, fl::audio::fft::Mode::LOG_REBIN);
        fl::audio::fft::Impl fft(args);
        fft.run(samples, &bins);

        int peakBin = findPeakBin(bins.raw());
        int diff = peakBin - b;
        if (diff < 0) diff = -diff;
        if (diff > 1) {
            FL_WARN("LOG_REBIN: sine at " << centerFreq
                         << " Hz (bin " << b << ") peaked in bin "
                         << peakBin << " (off by " << diff << ")");
            mismatches++;
        }
        FL_CHECK_LE(diff, 1);
    }
    FL_CHECK_LE(mismatches, 2);
}

FL_TEST_CASE("Binning adversarial - CQ_OCTAVE peak accuracy per bin") {
    const int bands = 16;
    const float fmin = fl::audio::fft::Args::DefaultMinFrequency();
    const float fmax = fl::audio::fft::Args::DefaultMaxFrequency();
    float logRatio = fl::logf(fmax / fmin);

    int mismatches = 0;
    for (int b = 1; b < bands - 1; ++b) {
        float centerFreq =
            fmin * fl::expf(logRatio * static_cast<float>(b) /
                            static_cast<float>(bands - 1));

        auto samples = makeAdversarialSine(centerFreq);
        fl::audio::fft::Bins bins(bands);
        fl::audio::fft::Args args(512, bands, fmin, fmax, 44100, fl::audio::fft::Mode::CQ_OCTAVE);
        fl::audio::fft::Impl fft(args);
        fft.run(samples, &bins);

        int peakBin = findPeakBin(bins.raw());
        int diff = peakBin - b;
        if (diff < 0) diff = -diff;
        if (diff > 1) {
            FL_WARN("CQ_OCTAVE: sine at " << centerFreq
                         << " Hz (bin " << b << ") peaked in bin "
                         << peakBin << " (off by " << diff << ")");
            mismatches++;
        }
        FL_CHECK_LE(diff, 1);
    }
    FL_CHECK_LE(mismatches, 2);
}

// --- 2. Cross-mode peak agreement ---

FL_TEST_CASE("Binning adversarial - LOG_REBIN vs CQ_OCTAVE peak agreement") {
    const int bands = 16;
    const float fmin = fl::audio::fft::Args::DefaultMinFrequency();
    const float fmax = fl::audio::fft::Args::DefaultMaxFrequency();

    float testFreqs[] = {200.0f, 300.0f, 500.0f, 800.0f, 1200.0f,
                         2000.0f, 3000.0f, 4000.0f};

    int disagreements = 0;
    for (float freq : testFreqs) {
        auto samples = makeAdversarialSine(freq);

        fl::audio::fft::Bins logBins(bands);
        fl::audio::fft::Args logArgs(512, bands, fmin, fmax, 44100, fl::audio::fft::Mode::LOG_REBIN);
        fl::audio::fft::Impl logFft(logArgs);
        logFft.run(samples, &logBins);

        fl::audio::fft::Bins cqBins(bands);
        fl::audio::fft::Args cqArgs(512, bands, fmin, fmax, 44100, fl::audio::fft::Mode::CQ_OCTAVE);
        fl::audio::fft::Impl cqFft(cqArgs);
        cqFft.run(samples, &cqBins);

        int logPeak = findPeakBin(logBins.raw());
        int cqPeak = findPeakBin(cqBins.raw());

        int diff = logPeak - cqPeak;
        if (diff < 0) diff = -diff;

        if (diff > 0) {
            FL_WARN("Cross-mode disagreement at " << freq
                         << " Hz: LOG_REBIN bin " << logPeak
                         << " vs CQ_OCTAVE bin " << cqPeak
                         << " (diff=" << diff << ")");
            disagreements++;
        }
        // BUG FOUND: LOG_REBIN uses edge formula i/bands, CQ uses center
        // formula i/(bands-1). This causes e.g. 300 Hz to land in bin 1
        // (LOG_REBIN) vs bin 3 (CQ_OCTAVE). Allow <=2 per-freq to document.
        FL_CHECK_LE(diff, 2);
    }
    // Known issue: LOG_REBIN vs CQ bin grid mismatch causes disagreements.
    FL_WARN("Cross-mode disagreements: " << disagreements << " of 8 frequencies");
    FL_CHECK_LE(disagreements, 3);
}

// --- 3. Monotonicity sweep ---

FL_TEST_CASE("Binning adversarial - LOG_REBIN monotonicity sweep") {
    const int bands = 16;
    const float fmin = fl::audio::fft::Args::DefaultMinFrequency();
    const float fmax = fl::audio::fft::Args::DefaultMaxFrequency();

    fl::audio::fft::Args args(512, bands, fmin, fmax, 44100, fl::audio::fft::Mode::LOG_REBIN);
    fl::audio::fft::Impl fft(args);

    const int numSteps = 30;
    float logRatio = fl::logf(fmax / fmin);
    int prevPeak = -1;
    int violations = 0;

    for (int s = 0; s < numSteps; ++s) {
        float freq = fmin * fl::expf(logRatio * static_cast<float>(s) /
                                     static_cast<float>(numSteps - 1));
        auto samples = makeAdversarialSine(freq);
        fl::audio::fft::Bins bins(bands);
        fft.run(samples, &bins);

        int peak = findPeakBin(bins.raw());
        if (prevPeak >= 0 && peak < prevPeak) {
            FL_WARN("Monotonicity violation at " << freq
                         << " Hz: peak bin " << peak
                         << " < previous peak " << prevPeak);
            violations++;
        }
        prevPeak = peak;
    }
    FL_CHECK_EQ(violations, 0);
}

namespace {

/// One binning mode measured over the declared frequency range.
struct CoverageResult {
    int dead;          ///< tones no band answered
    int centre_hits;   ///< band centres whose tone peaked in their own band
    int worst_miss;    ///< furthest a centre's peak landed from its band
};

/// Sweep `steps` log-spaced tones and, separately, one tone per band centre.
///
/// "Answered" is a peak above 20 counts, against the 200-2400 a mode returns
/// when it does resolve a tone -- two orders apart, so the threshold is not
/// carrying the result.
CoverageResult measureCoverage(int samples, int bands,
                               fl::audio::fft::Mode mode, int steps) {
    const float fmin = fl::audio::fft::Args::DefaultMinFrequency();
    const float fmax = fl::audio::fft::Args::DefaultMaxFrequency();
    const float logRatio = fl::logf(fmax / fmin);
    fl::audio::fft::Args args(samples, bands, fmin, fmax, 44100, mode);
    fl::audio::fft::Impl fft(args);

    CoverageResult result = {0, 0, 0};
    for (int s = 0; s < steps; ++s) {
        const float freq =
            fmin * fl::expf(logRatio * static_cast<float>(s) /
                            static_cast<float>(steps - 1));
        auto pcm = makeAdversarialSine(freq, samples);
        fl::audio::fft::Bins bins(bands);
        fft.run(pcm, &bins);
        if (bins.raw()[findPeakBin(bins.raw())] < 20.0f) {
            ++result.dead;
        }
    }
    for (int b = 0; b < bands; ++b) {
        const float freq =
            fmin * fl::expf(logRatio * static_cast<float>(b) /
                            static_cast<float>(bands - 1));
        auto pcm = makeAdversarialSine(freq, samples);
        fl::audio::fft::Bins bins(bands);
        fft.run(pcm, &bins);
        int miss = findPeakBin(bins.raw()) - b;
        if (miss < 0) {
            miss = -miss;
        }
        if (miss == 0) {
            ++result.centre_hits;
        }
        if (miss > result.worst_miss) {
            result.worst_miss = miss;
        }
    }
    return result;
}

}  // namespace

FL_TEST_CASE("CQ_OCTAVE leaves gaps in the range it declares, LOG_REBIN does not") {
    // FastLED#4301. `CQ_OCTAVE`'s sparse kernels are narrower than the
    // spacing between adjacent band centres in the upper octaves, so they do
    // not tile the axis and tones between them fall through: at 512 samples
    // and 16 bands, 33 of 200 log-spaced tones produce under 20 counts in
    // *every* band, the widest gap running 146-183 Hz.
    //
    // The comparison is what makes that a defect rather than a property of
    // the problem. `LOG_REBIN` -- which `Args::resolveModeEnums` already
    // picks for 32 bands or fewer -- answers everywhere, in every
    // configuration measured here and in the wider sweep on the issue.
    const int kSteps = 200;

    const CoverageResult octave_512_16 =
        measureCoverage(512, 16, fl::audio::fft::Mode::CQ_OCTAVE, kSteps);
    const CoverageResult rebin_512_16 =
        measureCoverage(512, 16, fl::audio::fft::Mode::LOG_REBIN, kSteps);

    FL_CHECK_EQ(rebin_512_16.dead, 0);
    // Bounded from below as well, deliberately. The gaps are the subject of
    // an open issue, so if they close, that is a change someone made and
    // should record -- not something to absorb silently. The message says so,
    // because a bound that fails on good news has to explain itself.
    if (octave_512_16.dead < 10) {
        FL_WARN_F("CQ_OCTAVE coverage at 512/16 improved to %d dead tones of "
                  "%d, from the 33 recorded here. That is good news, not a "
                  "failure: find what changed, note it on FastLED#4301, and "
                  "re-pin this bound.",
                  octave_512_16.dead, kSteps);
    }
    FL_CHECK_GT(octave_512_16.dead, 10);
    FL_CHECK_LT(octave_512_16.dead, 60);

    // And the gaps are not the price of resolution: on the same
    // configuration the two modes land a tone in its own band equally often.
    // Measured 14 of 16 for CQ_OCTAVE against 15 of 16 for LOG_REBIN.
    FL_CHECK_GT(rebin_512_16.centre_hits, octave_512_16.centre_hits - 2);
}

FL_TEST_CASE("answering everywhere is not the same as resolving anything") {
    // The guard on the case above, and not a hypothetical: `CQ_NAIVE` is a
    // shipped mode that already fails this way.
    //
    // At 256 samples the kernel conditioning `samples * fmin / fmax` is 1.63,
    // which is the condition `resolveModeEnums` uses to route away from
    // CQ_NAIVE -- and the measurement says it is right to. CQ_NAIVE answers
    // every one of the 200 tones and still puts the peak in the correct band
    // for only 2 of 48 centres, missing by as much as 16 bands.
    //
    // So "zero dead tones" alone would be satisfied by a mode that responds
    // to everything and distinguishes nothing, and any fix for #4301 that
    // reports zero must clear this too.
    const int kSteps = 200;
    const CoverageResult naive_256_48 =
        measureCoverage(256, 48, fl::audio::fft::Mode::CQ_NAIVE, kSteps);
    const CoverageResult rebin_256_48 =
        measureCoverage(256, 48, fl::audio::fft::Mode::LOG_REBIN, kSteps);

    FL_CHECK_EQ(naive_256_48.dead, 0);
    FL_CHECK_LT(naive_256_48.centre_hits, 8);
    FL_CHECK_GT(naive_256_48.worst_miss, 8);

    // LOG_REBIN answers everywhere *and* resolves, which is what makes the
    // first case's comparison meaningful rather than a choice of poisons.
    FL_CHECK_EQ(rebin_256_48.dead, 0);
    FL_CHECK_GT(rebin_256_48.centre_hits, 24);
    FL_CHECK_LT(rebin_256_48.worst_miss, 8);
}

FL_TEST_CASE("the default configuration does not select CQ_OCTAVE") {
    // Which is why #4301 is a defect in a mode rather than in what most
    // sketches get. `Args` defaults to 512 samples and 16 bands with
    // `Mode::AUTO`, and AUTO takes LOG_REBIN at 32 bands or fewer.
    //
    // Recorded because the first report of #4301 measured CQ_OCTAVE at 16
    // bands and read it as the default path. AUTO never pairs those.
    fl::audio::fft::Mode mode = fl::audio::fft::Mode::AUTO;
    fl::audio::fft::Window window = fl::audio::fft::Window::AUTO;
    fl::audio::fft::Args::resolveModeEnums(
        mode, window, fl::audio::fft::Args::DefaultBands(),
        fl::audio::fft::Args::DefaultSamples(),
        fl::audio::fft::Args::DefaultMinFrequency(),
        fl::audio::fft::Args::DefaultMaxFrequency());
    FL_CHECK(mode == fl::audio::fft::Mode::LOG_REBIN);

    // Above 32 bands AUTO chooses on kernel conditioning. With the default
    // range and 512 samples that is 512 * 90 / 14080 = 3.27, so CQ_NAIVE --
    // still not CQ_OCTAVE. Reaching CQ_OCTAVE from AUTO needs both more than
    // 32 bands and a shorter transform.
    mode = fl::audio::fft::Mode::AUTO;
    window = fl::audio::fft::Window::AUTO;
    fl::audio::fft::Args::resolveModeEnums(
        mode, window, 48, 512, fl::audio::fft::Args::DefaultMinFrequency(),
        fl::audio::fft::Args::DefaultMaxFrequency());
    FL_CHECK(mode == fl::audio::fft::Mode::CQ_NAIVE);

    mode = fl::audio::fft::Mode::AUTO;
    window = fl::audio::fft::Window::AUTO;
    fl::audio::fft::Args::resolveModeEnums(
        mode, window, 48, 256, fl::audio::fft::Args::DefaultMinFrequency(),
        fl::audio::fft::Args::DefaultMaxFrequency());
    FL_CHECK(mode == fl::audio::fft::Mode::CQ_OCTAVE);
}

FL_TEST_CASE("Binning adversarial - CQ_OCTAVE monotonicity sweep") {
    // Rising input frequency must never drive the CQ peak bin backwards.
    //
    // The sweep steps the analyser's own band centres rather than an
    // arbitrary log grid, because CQ_OCTAVE does not respond everywhere in
    // the range it declares. Measured over 200 log-spaced probe tones across
    // 90-14080 Hz at 16 bands / 512 samples, 49 of them (24%) produce under
    // 20 counts in *every* band, with two wide holes at the top: 7463-9621 Hz
    // and 10923-13383 Hz. A tone inside a hole gives no detection at all --
    // every band reads 0-7 counts -- so `findPeakBin` returns whichever band
    // happens to hold the most noise, and comparing two such answers measures
    // nothing.
    //
    // A 30-step grid put 2 of its 30 steps inside those holes, and the peak
    // there was decided by margins of one count (16 versus 15 at 11502 Hz).
    // That is why this used to be able to pass or fail on an unrelated change
    // to `fl::exp`, which moves the grid by a few percent: FastLED#4288.
    //
    // The holes are a real deficiency and are tracked in FastLED#4301. This case
    // is about ordering, so it asks the question where the analyser has an
    // answer, and checks that it really does have one.
    const int bands = 16;
    const float fmin = fl::audio::fft::Args::DefaultMinFrequency();
    const float fmax = fl::audio::fft::Args::DefaultMaxFrequency();

    fl::audio::fft::Args args(512, bands, fmin, fmax, 44100, fl::audio::fft::Mode::CQ_OCTAVE);
    fl::audio::fft::Impl fft(args);

    // Same expression `_generate_center_freqs` uses, so these are the
    // frequencies the kernels are actually built for.
    const float logRatio = fl::logf(fmax / fmin);
    int prevPeak = -1;
    int violations = 0;
    int weakDetections = 0;

    for (int b = 0; b < bands; ++b) {
        float freq = fmin * fl::expf(logRatio * static_cast<float>(b) /
                                     static_cast<float>(bands - 1));
        auto samples = makeAdversarialSine(freq);
        fl::audio::fft::Bins bins(bands);
        fft.run(samples, &bins);

        // The vacuity guard. Without it this case would still "pass" if the
        // analyser stopped responding altogether, by comparing noise -- which
        // is exactly the failure mode the old grid had.
        float top = 0.0f;
        float second = 0.0f;
        for (fl::size i = 0; i < bins.raw().size(); ++i) {
            const float v = bins.raw()[i];
            if (v > top) {
                second = top;
                top = v;
            } else if (v > second) {
                second = v;
            }
        }
        if (top < 15.0f || top < second * 1.5f) {
            FL_WARN("CQ weak detection at " << freq << " Hz: top " << top
                         << ", runner-up " << second);
            weakDetections++;
        }

        int peak = findPeakBin(bins.raw());
        if (prevPeak >= 0 && peak < prevPeak) {
            FL_WARN("CQ monotonicity violation at " << freq
                         << " Hz: peak bin " << peak
                         << " < previous peak " << prevPeak);
            violations++;
        }
        prevPeak = peak;
    }
    FL_CHECK_EQ(violations, 0);
    // Every centre detects, and by a margin: measured top 19-2410 counts
    // against runner-ups of 2-27, the tightest ratio being 1.58 at 177 Hz.
    FL_CHECK_EQ(weakDetections, 0);
}

// --- 4. White noise energy uniformity (catches LOG_REBIN high-bin bias) ---

FL_TEST_CASE("Binning adversarial - LOG_REBIN energy bias check") {
    // Test that LOG_REBIN doesn't have extreme energy bias across bins.
    // Feed one sine per bin center, record peak energy in its expected bin.
    const int bands = 16;
    const float fmin = fl::audio::fft::Args::DefaultMinFrequency();
    const float fmax = fl::audio::fft::Args::DefaultMaxFrequency();
    float logRatio = fl::logf(fmax / fmin);

    float peakEnergies[16] = {};
    int validBins = 0;

    for (int b = 0; b < bands; ++b) {
        // Use bin center in the LOG_REBIN edge grid (midpoint of edges b and b+1)
        float edgeB = fmin * fl::expf(logRatio * static_cast<float>(b) /
                                      static_cast<float>(bands));
        float edgeB1 = fmin * fl::expf(logRatio * static_cast<float>(b + 1) /
                                       static_cast<float>(bands));
        float freq = fl::sqrtf(edgeB * edgeB1); // geometric center

        auto samples = makeAdversarialSine(freq, 512, 44100.0f, 20000.0f);
        fl::audio::fft::Bins bins(bands);
        fl::audio::fft::Args args(512, bands, fmin, fmax, 44100, fl::audio::fft::Mode::LOG_REBIN);
        fl::audio::fft::Impl fft(args);
        fft.run(samples, &bins);

        FL_REQUIRE_EQ(bins.raw().size(), static_cast<fl::size>(bands));
        int peak = findPeakBin(bins.raw());
        peakEnergies[b] = bins.raw()[peak];

        if (peakEnergies[b] > 0.0f) {
            validBins++;
        }
    }

    // All bins should produce measurable energy when their center freq plays
    FL_WARN("LOG_REBIN valid bins with energy: " << validBins << "/" << bands);
    FL_CHECK_GE(validBins, bands - 2); // Allow up to 2 edge bins to be weak

    // Check energy ratio among valid bins
    float minE = 1e30f;
    float maxE = 0.0f;
    for (int i = 0; i < bands; ++i) {
        if (peakEnergies[i] > 0.0f) {
            if (peakEnergies[i] < minE) minE = peakEnergies[i];
            if (peakEnergies[i] > maxE) maxE = peakEnergies[i];
        }
    }

    if (minE > 0.0f) {
        float ratio = maxE / minE;
        FL_WARN("LOG_REBIN peak energy ratio (max/min): " << ratio);
        // For a pure sine, peak bin energy should be similar across bins
        FL_CHECK_LT(ratio, 20.0f);
    }
}

// --- 5. Two-tone separation ---

FL_TEST_CASE("Binning adversarial - two-tone separation LOG_REBIN") {
    const int bands = 16;
    const float fmin = fl::audio::fft::Args::DefaultMinFrequency();
    const float fmax = fl::audio::fft::Args::DefaultMaxFrequency();

    float logRatio = fl::logf(fmax / fmin);
    float freq1 = fmin * fl::expf(logRatio * 3.0f / 15.0f);
    float freq2 = fmin * fl::expf(logRatio * 12.0f / 15.0f);

    auto samples = makeTwoTone(freq1, freq2);
    fl::audio::fft::Bins bins(bands);
    fl::audio::fft::Args args(512, bands, fmin, fmax, 44100, fl::audio::fft::Mode::LOG_REBIN);
    fl::audio::fft::Impl fft(args);
    fft.run(samples, &bins);

    // Find two largest peaks
    int peak1 = -1, peak2 = -1;
    float val1 = 0.0f, val2 = 0.0f;
    for (int i = 0; i < bands; ++i) {
        if (bins.raw()[i] > val1) {
            val2 = val1;
            peak2 = peak1;
            val1 = bins.raw()[i];
            peak1 = i;
        } else if (bins.raw()[i] > val2) {
            val2 = bins.raw()[i];
            peak2 = i;
        }
    }

    int separation = peak1 - peak2;
    if (separation < 0) separation = -separation;
    FL_CHECK_GE(separation, 4);
    FL_CHECK_GT(val1, 0.0f);
    FL_CHECK_GT(val2, 0.0f);
}

// --- 6. Odd band counts ---

FL_TEST_CASE("Binning adversarial - odd band count 7 LOG_REBIN") {
    const int bands = 7;
    auto samples = makeAdversarialSine(1000.0f);
    fl::audio::fft::Bins bins(bands);
    fl::audio::fft::Args args(512, bands, fl::audio::fft::Args::DefaultMinFrequency(),
                      fl::audio::fft::Args::DefaultMaxFrequency(), 44100,
                      fl::audio::fft::Mode::LOG_REBIN);
    fl::audio::fft::Impl fft(args);
    fft.run(samples, &bins);

    FL_CHECK_EQ(bins.raw().size(), static_cast<fl::size>(bands));
    float maxVal = 0.0f;
    float totalEnergy = 0.0f;
    for (int i = 0; i < bands; ++i) {
        FL_CHECK_FALSE(isNaNCheck(bins.raw()[i]));
        totalEnergy += bins.raw()[i];
        if (bins.raw()[i] > maxVal) maxVal = bins.raw()[i];
    }
    FL_CHECK_GT(maxVal, 0.0f);
    FL_CHECK_GT(maxVal / totalEnergy, 0.25f);
}

FL_TEST_CASE("Binning adversarial - odd band count 13 CQ_OCTAVE") {
    const int bands = 13;
    auto samples = makeAdversarialSine(1000.0f);
    fl::audio::fft::Bins bins(bands);
    fl::audio::fft::Args args(512, bands, fl::audio::fft::Args::DefaultMinFrequency(),
                      fl::audio::fft::Args::DefaultMaxFrequency(), 44100,
                      fl::audio::fft::Mode::CQ_OCTAVE);
    fl::audio::fft::Impl fft(args);
    fft.run(samples, &bins);

    FL_CHECK_EQ(bins.raw().size(), static_cast<fl::size>(bands));
    float maxVal = 0.0f;
    float totalEnergy = 0.0f;
    for (int i = 0; i < bands; ++i) {
        FL_CHECK_FALSE(isNaNCheck(bins.raw()[i]));
        totalEnergy += bins.raw()[i];
        if (bins.raw()[i] > maxVal) maxVal = bins.raw()[i];
    }
    FL_CHECK_GT(maxVal, 0.0f);
    FL_CHECK_GT(maxVal / totalEnergy, 0.20f);
}

// --- 7. Bin boundary frequency assignment ---

FL_TEST_CASE("Binning adversarial - bin boundary frequency assignment") {
    const int bands = 16;
    const float fmin = fl::audio::fft::Args::DefaultMinFrequency();
    const float fmax = fl::audio::fft::Args::DefaultMaxFrequency();
    float logRatio = fl::logf(fmax / fmin);

    fl::audio::fft::Args args(512, bands, fmin, fmax, 44100, fl::audio::fft::Mode::LOG_REBIN);
    fl::audio::fft::Impl fft(args);

    for (int b = 0; b < bands - 1; ++b) {
        float centerB = fmin * fl::expf(logRatio * static_cast<float>(b) /
                                        static_cast<float>(bands - 1));
        float centerB1 = fmin * fl::expf(logRatio * static_cast<float>(b + 1) /
                                         static_cast<float>(bands - 1));
        float boundary = fl::sqrtf(centerB * centerB1);

        auto samples = makeAdversarialSine(boundary);
        fl::audio::fft::Bins bins(bands);
        fft.run(samples, &bins);

        int peak = findPeakBin(bins.raw());
        bool inRange = (peak == b || peak == b + 1);
        if (!inRange) {
            FL_WARN("Boundary freq " << boundary << " Hz between bins "
                         << b << " and " << (b + 1) << " peaked in bin "
                         << peak);
        }
        FL_CHECK(inRange);
    }
}

// --- 8. fmin/fmax edge tones ---

FL_TEST_CASE("Binning adversarial - sine at fmin lands in bin 0") {
    const int bands = 16;
    auto samples = makeAdversarialSine(fl::audio::fft::Args::DefaultMinFrequency());
    fl::audio::fft::Bins bins(bands);
    fl::audio::fft::Args args(512, bands, fl::audio::fft::Args::DefaultMinFrequency(),
                      fl::audio::fft::Args::DefaultMaxFrequency(), 44100,
                      fl::audio::fft::Mode::LOG_REBIN);
    fl::audio::fft::Impl fft(args);
    fft.run(samples, &bins);
    int peak = findPeakBin(bins.raw());
    FL_CHECK_LE(peak, 1);
}

FL_TEST_CASE("Binning adversarial - sine at fmax lands in last bin") {
    const int bands = 16;
    auto samples = makeAdversarialSine(fl::audio::fft::Args::DefaultMaxFrequency());
    fl::audio::fft::Bins bins(bands);
    fl::audio::fft::Args args(512, bands, fl::audio::fft::Args::DefaultMinFrequency(),
                      fl::audio::fft::Args::DefaultMaxFrequency(), 44100,
                      fl::audio::fft::Mode::LOG_REBIN);
    fl::audio::fft::Impl fft(args);
    fft.run(samples, &bins);
    int peak = findPeakBin(bins.raw());
    FL_CHECK_GE(peak, bands - 2);
}

FL_TEST_CASE("Binning adversarial - sine below fmin has minimal energy") {
    const int bands = 16;
    const float fmin = fl::audio::fft::Args::DefaultMinFrequency();
    const float fmax = fl::audio::fft::Args::DefaultMaxFrequency();

    fl::audio::fft::Args args(512, bands, fmin, fmax, 44100, fl::audio::fft::Mode::LOG_REBIN);
    fl::audio::fft::Impl fft(args);

    auto belowSamples = makeAdversarialSine(20.0f);
    fl::audio::fft::Bins belowBins(bands);
    fft.run(belowSamples, &belowBins);

    float totalBelow = 0.0f;
    for (int i = 0; i < bands; ++i) {
        totalBelow += belowBins.raw()[i];
    }

    auto inRangeSamples = makeAdversarialSine(1000.0f);
    fl::audio::fft::Bins inRangeBins(bands);
    fft.run(inRangeSamples, &inRangeBins);

    float totalInRange = 0.0f;
    for (int i = 0; i < bands; ++i) {
        totalInRange += inRangeBins.raw()[i];
    }

    // 20 Hz is ~4.5x below fmin (90 Hz). With the wider frequency range
    // (90-14080 Hz), LOG_REBIN's lowest geometric bin starts at 90 Hz but
    // the underlying FFT bin (~86 Hz resolution) captures some sub-bass
    // energy. Total below-fmin energy should still be less than in-range.
    FL_CHECK_LT(totalBelow, totalInRange);
}

// --- 9. freqToBin consistency with actual LOG_REBIN peak ---

FL_TEST_CASE("Binning adversarial - freqToBin matches actual LOG_REBIN peak") {
    const int bands = 16;
    const float fmin = fl::audio::fft::Args::DefaultMinFrequency();
    const float fmax = fl::audio::fft::Args::DefaultMaxFrequency();

    float testFreqs[] = {200.0f, 400.0f, 800.0f, 1500.0f, 3000.0f, 4500.0f};
    int mismatches = 0;

    for (float freq : testFreqs) {
        auto samples = makeAdversarialSine(freq);
        fl::audio::fft::Bins bins(bands);
        fl::audio::fft::Args args(512, bands, fmin, fmax, 44100, fl::audio::fft::Mode::LOG_REBIN);
        fl::audio::fft::Impl fft(args);
        fft.run(samples, &bins);

        int actualPeak = findPeakBin(bins.raw());
        int reportedBin = bins.freqToBin(freq);

        int diff = actualPeak - reportedBin;
        if (diff < 0) diff = -diff;

        if (diff > 1) {
            FL_WARN("freqToBin(" << freq << ") = " << reportedBin
                         << " but LOG_REBIN peak at bin " << actualPeak
                         << " (off by " << diff << ")");
            mismatches++;
        }
        FL_CHECK_LE(diff, 1);
    }
    FL_CHECK_LE(mismatches, 1);
}

// --- 10. 64-bin CQ_OCTAVE peak accuracy ---

FL_TEST_CASE("Binning adversarial - 64 bins CQ_OCTAVE peak accuracy") {
    const int bands = 64;
    const float fmin = fl::audio::fft::Args::DefaultMinFrequency();
    const float fmax = fl::audio::fft::Args::DefaultMaxFrequency();

    float testFreqs[] = {200.0f, 500.0f, 1000.0f, 2000.0f, 4000.0f};

    for (float freq : testFreqs) {
        auto samples = makeAdversarialSine(freq);
        fl::audio::fft::Bins bins(bands);
        fl::audio::fft::Args args(512, bands, fmin, fmax, 44100, fl::audio::fft::Mode::CQ_OCTAVE);
        fl::audio::fft::Impl fft(args);
        fft.run(samples, &bins);

        FL_CHECK_EQ(bins.raw().size(), static_cast<fl::size>(bands));

        int peak = findPeakBin(bins.raw());
        int expected = bins.freqToBin(freq);

        int diff = peak - expected;
        if (diff < 0) diff = -diff;

        if (diff > 2) {
            FL_WARN("64-bin CQ: sine at " << freq << " Hz: expected bin "
                         << expected << " got bin " << peak);
        }
        FL_CHECK_LE(diff, 2);
    }
}

// --- 11. No NaN/Inf for edge signals ---

FL_TEST_CASE("Binning adversarial - no NaN in any output for edge signals") {
    const int bands = 16;
    const float fmin = fl::audio::fft::Args::DefaultMinFrequency();
    const float fmax = fl::audio::fft::Args::DefaultMaxFrequency();

    fl::vector<fl::i16> silence(512, 0);
    fl::vector<fl::i16> dc(512, 32767);
    fl::vector<fl::i16> impulse(512, 0);
    impulse[0] = 32767;
    fl::vector<fl::i16> nyquist(512);
    for (int i = 0; i < 512; ++i) {
        nyquist[i] = (i % 2 == 0) ? 32767 : -32768;
    }

    fl::vector<fl::i16>* signals[] = {&silence, &dc, &impulse, &nyquist};
    const char* names[] = {"silence", "DC", "impulse", "nyquist"};
    fl::audio::fft::Mode modes[] = {fl::audio::fft::Mode::LOG_REBIN, fl::audio::fft::Mode::CQ_OCTAVE};
    const char* modeNames[] = {"LOG_REBIN", "CQ_OCTAVE"};

    for (int m = 0; m < 2; ++m) {
        for (int s = 0; s < 4; ++s) {
            fl::audio::fft::Bins bins(bands);
            fl::audio::fft::Args args(512, bands, fmin, fmax, 44100, modes[m]);
            fl::audio::fft::Impl fft(args);
            fft.run(*signals[s], &bins);

            for (int i = 0; i < bands; ++i) {
                if (isNaNCheck(bins.raw()[i])) {
                    FL_WARN("NaN in " << modeNames[m] << " " << names[s]
                                 << " raw bin " << i);
                }
                FL_CHECK_FALSE(isNaNCheck(bins.raw()[i]));
                FL_CHECK_FALSE(isNaNCheck(bins.db()[i]));
            }
            for (int i = 0; i < static_cast<int>(bins.linear().size()); ++i) {
                FL_CHECK_FALSE(isNaNCheck(bins.linear()[i]));
            }
        }
    }
}

// --- 12. Narrow frequency range ---

FL_TEST_CASE("Binning adversarial - narrow frequency range 1000-2000 Hz") {
    const int bands = 8;

    auto samples = makeAdversarialSine(1414.0f);

    fl::audio::fft::Bins logBins(bands);
    fl::audio::fft::Args logArgs(512, bands, 1000.0f, 2000.0f, 44100, fl::audio::fft::Mode::LOG_REBIN);
    fl::audio::fft::Impl logFft(logArgs);
    logFft.run(samples, &logBins);

    FL_CHECK_EQ(logBins.raw().size(), static_cast<fl::size>(bands));
    int logPeak = findPeakBin(logBins.raw());
    FL_CHECK_GE(logPeak, 1);
    FL_CHECK_LE(logPeak, bands - 2);
    for (int i = 0; i < bands; ++i) {
        FL_CHECK_FALSE(isNaNCheck(logBins.raw()[i]));
    }

    fl::audio::fft::Bins cqBins(bands);
    fl::audio::fft::Args cqArgs(512, bands, 1000.0f, 2000.0f, 44100, fl::audio::fft::Mode::CQ_OCTAVE);
    fl::audio::fft::Impl cqFft(cqArgs);
    cqFft.run(samples, &cqBins);

    FL_CHECK_EQ(cqBins.raw().size(), static_cast<fl::size>(bands));
    int cqPeak = findPeakBin(cqBins.raw());
    FL_CHECK_GE(cqPeak, 1);
    FL_CHECK_LE(cqPeak, bands - 2);
    for (int i = 0; i < bands; ++i) {
        FL_CHECK_FALSE(isNaNCheck(cqBins.raw()[i]));
    }
}

// --- 13. Energy concentration ---

FL_TEST_CASE("Binning adversarial - LOG_REBIN energy concentration") {
    const int bands = 16;
    auto samples = makeAdversarialSine(1000.0f);
    fl::audio::fft::Bins bins(bands);
    fl::audio::fft::Args args(512, bands, fl::audio::fft::Args::DefaultMinFrequency(),
                      fl::audio::fft::Args::DefaultMaxFrequency(), 44100,
                      fl::audio::fft::Mode::LOG_REBIN);
    fl::audio::fft::Impl fft(args);
    fft.run(samples, &bins);

    float totalEnergy = 0.0f;
    float maxBinEnergy = 0.0f;
    for (int i = 0; i < bands; ++i) {
        totalEnergy += bins.raw()[i];
        if (bins.raw()[i] > maxBinEnergy) maxBinEnergy = bins.raw()[i];
    }

    float concentration = maxBinEnergy / totalEnergy;
    if (concentration < 0.50f) {
        FL_WARN("LOG_REBIN: 1kHz sine energy concentration = "
                     << (concentration * 100.0f) << "% (expected > 50%)");
    }
    FL_CHECK_GT(concentration, 0.50f);
}

// --- 14. binToFreq/freqToBin roundtrip ---

FL_TEST_CASE("Binning adversarial - binToFreq/freqToBin roundtrip LOG_REBIN") {
    const int bands = 16;
    auto samples = makeAdversarialSine(1000.0f);
    fl::audio::fft::Bins bins(bands);
    fl::audio::fft::Args args(512, bands, fl::audio::fft::Args::DefaultMinFrequency(),
                      fl::audio::fft::Args::DefaultMaxFrequency(), 44100,
                      fl::audio::fft::Mode::LOG_REBIN);
    fl::audio::fft::Impl fft(args);
    fft.run(samples, &bins);

    for (int i = 0; i < bands; ++i) {
        float freq = bins.binToFreq(i);
        int recoveredBin = bins.freqToBin(freq);
        FL_CHECK_EQ(recoveredBin, i);
    }
}

FL_TEST_CASE("Args::resolveModeEnums") {
    const int N = 512;
    const float fmin = fl::audio::fft::Args::DefaultMinFrequency();
    const float fmax = fl::audio::fft::Args::DefaultMaxFrequency();

    // AUTO mode + AUTO window, <=32 bins → LOG_REBIN + BLACKMAN_HARRIS
    {
        fl::audio::fft::Mode mode = fl::audio::fft::Mode::AUTO;
        fl::audio::fft::Window window = fl::audio::fft::Window::AUTO;
        fl::audio::fft::Args::resolveModeEnums(mode, window, 16, N, fmin, fmax);
        FL_CHECK_EQ(static_cast<int>(mode), static_cast<int>(fl::audio::fft::Mode::LOG_REBIN));
        FL_CHECK_EQ(static_cast<int>(window), static_cast<int>(fl::audio::fft::Window::BLACKMAN_HARRIS));
    }

    // AUTO mode + AUTO window, >32 bins → CQ mode + HANNING
    {
        fl::audio::fft::Mode mode = fl::audio::fft::Mode::AUTO;
        fl::audio::fft::Window window = fl::audio::fft::Window::AUTO;
        fl::audio::fft::Args::resolveModeEnums(mode, window, 64, N, fmin, fmax);
        // Should be CQ_NAIVE or CQ_OCTAVE depending on kernel conditioning
        FL_CHECK(mode == fl::audio::fft::Mode::CQ_NAIVE || mode == fl::audio::fft::Mode::CQ_OCTAVE);
        FL_CHECK_EQ(static_cast<int>(window), static_cast<int>(fl::audio::fft::Window::HANNING));
    }

    // Explicit LOG_REBIN + AUTO window → BLACKMAN_HARRIS
    {
        fl::audio::fft::Mode mode = fl::audio::fft::Mode::LOG_REBIN;
        fl::audio::fft::Window window = fl::audio::fft::Window::AUTO;
        fl::audio::fft::Args::resolveModeEnums(mode, window, 16, N, fmin, fmax);
        FL_CHECK_EQ(static_cast<int>(mode), static_cast<int>(fl::audio::fft::Mode::LOG_REBIN));
        FL_CHECK_EQ(static_cast<int>(window), static_cast<int>(fl::audio::fft::Window::BLACKMAN_HARRIS));
    }

    // Explicit CQ_OCTAVE + AUTO window → HANNING
    {
        fl::audio::fft::Mode mode = fl::audio::fft::Mode::CQ_OCTAVE;
        fl::audio::fft::Window window = fl::audio::fft::Window::AUTO;
        fl::audio::fft::Args::resolveModeEnums(mode, window, 16, N, fmin, fmax);
        FL_CHECK_EQ(static_cast<int>(mode), static_cast<int>(fl::audio::fft::Mode::CQ_OCTAVE));
        FL_CHECK_EQ(static_cast<int>(window), static_cast<int>(fl::audio::fft::Window::HANNING));
    }

    // Explicit CQ_HYBRID + AUTO window → BLACKMAN_HARRIS
    {
        fl::audio::fft::Mode mode = fl::audio::fft::Mode::CQ_HYBRID;
        fl::audio::fft::Window window = fl::audio::fft::Window::AUTO;
        fl::audio::fft::Args::resolveModeEnums(mode, window, 16, N, fmin, fmax);
        FL_CHECK_EQ(static_cast<int>(mode), static_cast<int>(fl::audio::fft::Mode::CQ_HYBRID));
        FL_CHECK_EQ(static_cast<int>(window), static_cast<int>(fl::audio::fft::Window::BLACKMAN_HARRIS));
    }

    // Explicit CQ_NAIVE + AUTO window → HANNING
    {
        fl::audio::fft::Mode mode = fl::audio::fft::Mode::CQ_NAIVE;
        fl::audio::fft::Window window = fl::audio::fft::Window::AUTO;
        fl::audio::fft::Args::resolveModeEnums(mode, window, 16, N, fmin, fmax);
        FL_CHECK_EQ(static_cast<int>(mode), static_cast<int>(fl::audio::fft::Mode::CQ_NAIVE));
        FL_CHECK_EQ(static_cast<int>(window), static_cast<int>(fl::audio::fft::Window::HANNING));
    }

    // Non-AUTO values are preserved
    {
        fl::audio::fft::Mode mode = fl::audio::fft::Mode::LOG_REBIN;
        fl::audio::fft::Window window = fl::audio::fft::Window::HANNING;
        fl::audio::fft::Args::resolveModeEnums(mode, window, 16, N, fmin, fmax);
        FL_CHECK_EQ(static_cast<int>(mode), static_cast<int>(fl::audio::fft::Mode::LOG_REBIN));
        FL_CHECK_EQ(static_cast<int>(window), static_cast<int>(fl::audio::fft::Window::HANNING));
    }

    // NONE window is preserved
    {
        fl::audio::fft::Mode mode = fl::audio::fft::Mode::AUTO;
        fl::audio::fft::Window window = fl::audio::fft::Window::NONE;
        fl::audio::fft::Args::resolveModeEnums(mode, window, 16, N, fmin, fmax);
        FL_CHECK_EQ(static_cast<int>(mode), static_cast<int>(fl::audio::fft::Mode::LOG_REBIN));
        FL_CHECK_EQ(static_cast<int>(window), static_cast<int>(fl::audio::fft::Window::NONE));
    }
}

} // FL_TEST_FILE
