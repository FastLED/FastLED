// Unit tests for audio::detector::EnergyAnalyzer

#include "test.h"
#include "fl/audio/audio.h"
#include "fl/audio/audio_context.h"
#include "fl/audio/detector/energy_analyzer.h"
#include "tests/fl/audio/test_helpers.h"
#include "fl/stl/vector.h"
#include "fl/math/math.h"
#include "fl/stl/shared_ptr.h"
#include "fl/math/math.h"

using namespace fl;
using fl::audio::test::makeSample;
using fl::audio::test::makeSilence;

namespace test_energy_analyzer {

static audio::Sample makeSample_EnergyAnalyzer(float amplitude, fl::u32 timestamp) {
    return makeSample(440.0f, timestamp, amplitude);
}

static audio::Sample makeSilence_EnergyAnalyzer(fl::u32 timestamp) {
    return makeSilence(timestamp);
}

} // namespace test_energy_analyzer
using namespace test_energy_analyzer;

FL_TEST_CASE("audio::detector::EnergyAnalyzer - silence gives zero RMS") {
    audio::detector::EnergyAnalyzer analyzer;
    auto ctx = fl::make_shared<audio::Context>(makeSilence_EnergyAnalyzer(0));
    analyzer.update(ctx);
    analyzer.fireCallbacks();
    FL_CHECK_EQ(analyzer.getRMS(), 0.0f);
    FL_CHECK_EQ(analyzer.getPeak(), 0.0f);
}

FL_TEST_CASE("audio::detector::EnergyAnalyzer - known amplitude gives predictable RMS") {
    audio::detector::EnergyAnalyzer analyzer;
    auto ctx = fl::make_shared<audio::Context>(makeSample_EnergyAnalyzer(10000.0f, 100));
    analyzer.update(ctx);
    analyzer.fireCallbacks();
    float rms = analyzer.getRMS();
    // Sine wave RMS = amplitude / sqrt(2) ≈ 0.707 * amplitude ≈ 7071
    FL_CHECK_GT(rms, 6500.0f);
    FL_CHECK_LT(rms, 7500.0f);
}

FL_TEST_CASE("audio::detector::EnergyAnalyzer - peak tracking") {
    audio::detector::EnergyAnalyzer analyzer;

    // Feed quiet signal
    auto ctx1 = fl::make_shared<audio::Context>(makeSample_EnergyAnalyzer(1000.0f, 100));
    analyzer.update(ctx1);
    analyzer.fireCallbacks();
    float quietPeak = analyzer.getPeak();

    // Feed louder signal
    auto ctx2 = fl::make_shared<audio::Context>(makeSample_EnergyAnalyzer(15000.0f, 200));
    analyzer.update(ctx2);
    analyzer.fireCallbacks();
    float loudPeak = analyzer.getPeak();

    FL_CHECK_GT(loudPeak, quietPeak);
}

FL_TEST_CASE("audio::detector::EnergyAnalyzer - average energy tracking") {
    audio::detector::EnergyAnalyzer analyzer;

    for (int i = 0; i < 10; ++i) {
        auto ctx = fl::make_shared<audio::Context>(makeSample_EnergyAnalyzer(5000.0f, i * 100));
        analyzer.update(ctx);
    }

    float avg = analyzer.getAverageEnergy();
    // Sine wave RMS = amplitude / sqrt(2) ≈ 0.707 * 5000 ≈ 3536
    // With 10 identical samples, the average should converge to the RMS value
    FL_CHECK_GT(avg, 3000.0f);
    FL_CHECK_LT(avg, 4000.0f);
}

FL_TEST_CASE("audio::detector::EnergyAnalyzer - min/max energy tracking") {
    audio::detector::EnergyAnalyzer analyzer;

    // Feed varying amplitudes
    auto ctx1 = fl::make_shared<audio::Context>(makeSample_EnergyAnalyzer(2000.0f, 100));
    analyzer.update(ctx1);
    auto ctx2 = fl::make_shared<audio::Context>(makeSample_EnergyAnalyzer(15000.0f, 200));
    analyzer.update(ctx2);
    auto ctx3 = fl::make_shared<audio::Context>(makeSample_EnergyAnalyzer(5000.0f, 300));
    analyzer.update(ctx3);

    float minE = analyzer.getMinEnergy();
    float maxE = analyzer.getMaxEnergy();

    FL_CHECK_GT(maxE, minE);
}

FL_TEST_CASE("audio::detector::EnergyAnalyzer - normalized RMS in 0-1 range") {
    audio::detector::EnergyAnalyzer analyzer;

    // Feed several samples to establish range
    for (int i = 0; i < 20; ++i) {
        float amplitude = 1000.0f + static_cast<float>(i) * 500.0f;
        auto ctx = fl::make_shared<audio::Context>(makeSample_EnergyAnalyzer(amplitude, i * 100));
        analyzer.update(ctx);
    }

    float normalized = analyzer.getNormalizedRMS();
    FL_CHECK_GE(normalized, 0.0f);
    FL_CHECK_LE(normalized, 1.0f);
}

FL_TEST_CASE("audio::detector::EnergyAnalyzer - callbacks fire") {
    audio::detector::EnergyAnalyzer analyzer;
    float lastRMS = -1.0f;
    float lastPeak = -1.0f;
    analyzer.onEnergy.add([&lastRMS](float rms) { lastRMS = rms; });
    analyzer.onPeak.add([&lastPeak](float peak) { lastPeak = peak; });

    auto ctx = fl::make_shared<audio::Context>(makeSample_EnergyAnalyzer(10000.0f, 100));
    analyzer.update(ctx);
    analyzer.fireCallbacks();

    FL_CHECK_GT(lastRMS, 0.0f);
    FL_CHECK_GT(lastPeak, 0.0f);
}

FL_TEST_CASE("audio::detector::EnergyAnalyzer - reset clears state") {
    audio::detector::EnergyAnalyzer analyzer;

    auto ctx = fl::make_shared<audio::Context>(makeSample_EnergyAnalyzer(10000.0f, 100));
    analyzer.update(ctx);
    FL_CHECK_GT(analyzer.getRMS(), 0.0f);

    analyzer.reset();
    FL_CHECK_EQ(analyzer.getRMS(), 0.0f);
    FL_CHECK_EQ(analyzer.getPeak(), 0.0f);
    FL_CHECK_EQ(analyzer.getAverageEnergy(), 0.0f);
}

FL_TEST_CASE("audio::detector::EnergyAnalyzer - needsFFT is false") {
    audio::detector::EnergyAnalyzer analyzer;
    FL_CHECK_FALSE(analyzer.needsFFT());
}

FL_TEST_CASE("audio::detector::EnergyAnalyzer - onNormalizedEnergy callback fires") {
    audio::detector::EnergyAnalyzer analyzer;
    float lastNormalized = -1.0f;
    analyzer.onNormalizedEnergy.add([&lastNormalized](float val) { lastNormalized = val; });

    // Feed several samples to establish range
    for (int i = 0; i < 20; ++i) {
        float amplitude = 1000.0f + static_cast<float>(i) * 500.0f;
        auto ctx = fl::make_shared<audio::Context>(makeSample_EnergyAnalyzer(amplitude, i * 100));
        analyzer.update(ctx);
        analyzer.fireCallbacks();
    }

    // The normalized energy callback should have fired
    FL_CHECK_GE(lastNormalized, 0.0f);
    FL_CHECK_LE(lastNormalized, 1.0f);
}

FL_TEST_CASE("audio::detector::EnergyAnalyzer - peak decay over time") {
    audio::detector::EnergyAnalyzer analyzer;
    analyzer.setPeakDecay(0.9f);  // Faster decay for testing

    // Create a loud sample to establish peak
    auto ctx = fl::make_shared<audio::Context>(makeSample_EnergyAnalyzer(15000.0f, 0));
    analyzer.update(ctx);
    float initialPeak = analyzer.getPeak();
    FL_CHECK_GT(initialPeak, 0.0f);

    // Feed silence for many frames - peak should decay
    for (int i = 1; i <= 50; ++i) {
        auto silentCtx = fl::make_shared<audio::Context>(makeSilence_EnergyAnalyzer(i * 100));
        analyzer.update(silentCtx);
    }

    float finalPeak = analyzer.getPeak();
    // Peak should have decayed significantly
    FL_CHECK_LT(finalPeak, initialPeak);
}
