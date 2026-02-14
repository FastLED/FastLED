// Unit tests for FFT and FFTBins
// standalone test

#include "test.h"
#include "FastLED.h"
#include "fl/fft.h"
#include "fl/stl/vector.h"
#include "fl/stl/math.h"
#include "fl/stl/span.h"
#include "fl/math_macros.h"

using namespace fl;

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

FL_TEST_CASE("FFTBins - constructor and size") {
    FFTBins bins(16);
    FL_CHECK_EQ(bins.size(), 16u);
    FL_CHECK_EQ(bins.bins_raw.size(), 0u);  // Initially empty (just reserved)
    FL_CHECK_EQ(bins.bins_db.size(), 0u);
}

FL_TEST_CASE("FFTBins - copy constructor") {
    FFTBins original(16);
    original.bins_raw.push_back(1.0f);
    original.bins_raw.push_back(2.0f);
    original.bins_db.push_back(10.0f);
    original.bins_db.push_back(20.0f);

    FFTBins copy(original);
    FL_CHECK_EQ(copy.size(), 16u);
    FL_CHECK_EQ(copy.bins_raw.size(), 2u);
    FL_CHECK_EQ(copy.bins_raw[0], 1.0f);
    FL_CHECK_EQ(copy.bins_raw[1], 2.0f);
}

FL_TEST_CASE("FFTBins - move constructor") {
    FFTBins original(16);
    original.bins_raw.push_back(42.0f);

    FFTBins moved(fl::move(original));
    FL_CHECK_EQ(moved.size(), 16u);
    FL_CHECK_EQ(moved.bins_raw.size(), 1u);
    FL_CHECK_EQ(moved.bins_raw[0], 42.0f);
}

FL_TEST_CASE("FFTBins - clear") {
    FFTBins bins(16);
    bins.bins_raw.push_back(1.0f);
    bins.bins_db.push_back(10.0f);
    bins.clear();
    FL_CHECK_EQ(bins.bins_raw.size(), 0u);
    FL_CHECK_EQ(bins.bins_db.size(), 0u);
    FL_CHECK_EQ(bins.size(), 16u);  // mSize unchanged
}

FL_TEST_CASE("FFT_Args - defaults match documented values") {
    FFT_Args args;
    FL_CHECK_EQ(args.samples, 512);
    FL_CHECK_EQ(args.bands, 16);
    FL_CHECK_EQ(args.sample_rate, 44100);

    // Check floats with tolerance
    FL_CHECK_GT(args.fmin, 174.0f);
    FL_CHECK_LT(args.fmin, 175.0f);
    FL_CHECK_GT(args.fmax, 4698.0f);
    FL_CHECK_LT(args.fmax, 4699.0f);
}

FL_TEST_CASE("FFT - run with sine wave concentrates energy") {
    FFT fft;
    auto samples = generateSine(1000.0f);  // 1kHz sine, within CQ range 174.6-4698.3 Hz
    FFTBins bins(16);
    fft.run(fl::span<const fl::i16>(samples.data(), samples.size()), &bins);

    FL_REQUIRE_GT(bins.bins_raw.size(), 0u);

    // Find the bin with maximum energy and compute total energy
    float maxVal = 0.0f;
    float totalEnergy = 0.0f;
    for (fl::size i = 0; i < bins.bins_raw.size(); ++i) {
        totalEnergy += bins.bins_raw[i];
        if (bins.bins_raw[i] > maxVal) {
            maxVal = bins.bins_raw[i];
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
    float otherBinsAvg = otherBinsTotal / static_cast<float>(bins.bins_raw.size() - 1);
    FL_CHECK_GT(maxVal, otherBinsAvg * 3.0f);  // Peak should be at least 3x the average of other bins
}

FL_TEST_CASE("FFT - different frequencies produce different peak bins") {
    FFT fft;

    // Generate a bass tone (200 Hz) and a mid/treble tone (2000 Hz)
    auto bassSignal = generateSine(200.0f);
    auto trebleSignal = generateSine(2000.0f);

    FFTBins bassBins(16);
    FFTBins trebleBins(16);

    fft.run(fl::span<const fl::i16>(bassSignal.data(), bassSignal.size()), &bassBins);
    fft.run(fl::span<const fl::i16>(trebleSignal.data(), trebleSignal.size()), &trebleBins);

    FL_REQUIRE_GT(bassBins.bins_raw.size(), 0u);
    FL_REQUIRE_GT(trebleBins.bins_raw.size(), 0u);

    // Find peak bins for each frequency
    fl::size bassPeakBin = 0;
    float bassPeakVal = 0.0f;
    for (fl::size i = 0; i < bassBins.bins_raw.size(); ++i) {
        if (bassBins.bins_raw[i] > bassPeakVal) {
            bassPeakVal = bassBins.bins_raw[i];
            bassPeakBin = i;
        }
    }

    fl::size treblePeakBin = 0;
    float treblePeakVal = 0.0f;
    for (fl::size i = 0; i < trebleBins.bins_raw.size(); ++i) {
        if (trebleBins.bins_raw[i] > treblePeakVal) {
            treblePeakVal = trebleBins.bins_raw[i];
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

FL_TEST_CASE("FFT - silence produces near-zero bins") {
    FFT fft;
    fl::vector<fl::i16> silence(512, 0);
    FFTBins bins(16);
    fft.run(fl::span<const fl::i16>(silence.data(), silence.size()), &bins);

    // All bins should be near zero
    for (fl::size i = 0; i < bins.bins_raw.size(); ++i) {
        FL_CHECK_LT(bins.bins_raw[i], 10.0f);
    }
}

FL_TEST_CASE("FFT_Args - equality operator") {
    FFT_Args args1;
    FFT_Args args2;
    FL_CHECK(args1 == args2);
    FL_CHECK_FALSE(args1 != args2);

    FFT_Args args3(256, 8, 100.0f, 5000.0f, 22050);
    FL_CHECK(args1 != args3);
}
