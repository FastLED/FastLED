// Unit tests for EnergyAnalyzer

#include "test.h"
#include "fl/audio.h"
#include "fl/audio/audio_context.h"
#include "fl/fx/audio/detectors/energy_analyzer.h"
#include "../../../audio/test_helpers.h"
#include "fl/stl/vector.h"
#include "fl/stl/math.h"
#include "fl/stl/shared_ptr.h"
#include "fl/math_macros.h"

using namespace fl;
using fl::audio::test::makeSample;
using fl::audio::test::makeSilence;

namespace {

static AudioSample makeSample_EnergyAnalyzer(float amplitude, fl::u32 timestamp) {
    return makeSample(440.0f, timestamp, amplitude);
}

static AudioSample makeSilence_EnergyAnalyzer(fl::u32 timestamp) {
    return makeSilence(timestamp);
}

} // anonymous namespace

FL_TEST_CASE("EnergyAnalyzer - silence gives zero RMS") {
    EnergyAnalyzer analyzer;
    auto ctx = fl::make_shared<AudioContext>(makeSilence_EnergyAnalyzer(0));
    analyzer.update(ctx);
    analyzer.fireCallbacks();
    FL_CHECK_EQ(analyzer.getRMS(), 0.0f);
    FL_CHECK_EQ(analyzer.getPeak(), 0.0f);
}

FL_TEST_CASE("EnergyAnalyzer - known amplitude gives predictable RMS") {
    EnergyAnalyzer analyzer;
    auto ctx = fl::make_shared<AudioContext>(makeSample_EnergyAnalyzer(10000.0f, 100));
    analyzer.update(ctx);
    analyzer.fireCallbacks();
    float rms = analyzer.getRMS();
    // Sine wave RMS = amplitude / sqrt(2) ≈ 0.707 * amplitude ≈ 7071
    FL_CHECK_GT(rms, 6500.0f);
    FL_CHECK_LT(rms, 7500.0f);
}

FL_TEST_CASE("EnergyAnalyzer - peak tracking") {
    EnergyAnalyzer analyzer;

    // Feed quiet signal
    auto ctx1 = fl::make_shared<AudioContext>(makeSample_EnergyAnalyzer(1000.0f, 100));
    analyzer.update(ctx1);
    analyzer.fireCallbacks();
    float quietPeak = analyzer.getPeak();

    // Feed louder signal
    auto ctx2 = fl::make_shared<AudioContext>(makeSample_EnergyAnalyzer(15000.0f, 200));
    analyzer.update(ctx2);
    analyzer.fireCallbacks();
    float loudPeak = analyzer.getPeak();

    FL_CHECK_GT(loudPeak, quietPeak);
}

FL_TEST_CASE("EnergyAnalyzer - average energy tracking") {
    EnergyAnalyzer analyzer;

    for (int i = 0; i < 10; ++i) {
        auto ctx = fl::make_shared<AudioContext>(makeSample_EnergyAnalyzer(5000.0f, i * 100));
        analyzer.update(ctx);
    }

    float avg = analyzer.getAverageEnergy();
    // Sine wave RMS = amplitude / sqrt(2) ≈ 0.707 * 5000 ≈ 3536
    // With 10 identical samples, the average should converge to the RMS value
    FL_CHECK_GT(avg, 3000.0f);
    FL_CHECK_LT(avg, 4000.0f);
}

FL_TEST_CASE("EnergyAnalyzer - min/max energy tracking") {
    EnergyAnalyzer analyzer;

    // Feed varying amplitudes
    auto ctx1 = fl::make_shared<AudioContext>(makeSample_EnergyAnalyzer(2000.0f, 100));
    analyzer.update(ctx1);
    auto ctx2 = fl::make_shared<AudioContext>(makeSample_EnergyAnalyzer(15000.0f, 200));
    analyzer.update(ctx2);
    auto ctx3 = fl::make_shared<AudioContext>(makeSample_EnergyAnalyzer(5000.0f, 300));
    analyzer.update(ctx3);

    float minE = analyzer.getMinEnergy();
    float maxE = analyzer.getMaxEnergy();

    FL_CHECK_GT(maxE, minE);
}

FL_TEST_CASE("EnergyAnalyzer - normalized RMS in 0-1 range") {
    EnergyAnalyzer analyzer;

    // Feed several samples to establish range
    for (int i = 0; i < 20; ++i) {
        float amplitude = 1000.0f + static_cast<float>(i) * 500.0f;
        auto ctx = fl::make_shared<AudioContext>(makeSample_EnergyAnalyzer(amplitude, i * 100));
        analyzer.update(ctx);
    }

    float normalized = analyzer.getNormalizedRMS();
    FL_CHECK_GE(normalized, 0.0f);
    FL_CHECK_LE(normalized, 1.0f);
}

FL_TEST_CASE("EnergyAnalyzer - callbacks fire") {
    EnergyAnalyzer analyzer;
    float lastRMS = -1.0f;
    float lastPeak = -1.0f;
    analyzer.onEnergy.add([&lastRMS](float rms) { lastRMS = rms; });
    analyzer.onPeak.add([&lastPeak](float peak) { lastPeak = peak; });

    auto ctx = fl::make_shared<AudioContext>(makeSample_EnergyAnalyzer(10000.0f, 100));
    analyzer.update(ctx);
    analyzer.fireCallbacks();

    FL_CHECK_GT(lastRMS, 0.0f);
    FL_CHECK_GT(lastPeak, 0.0f);
}

FL_TEST_CASE("EnergyAnalyzer - reset clears state") {
    EnergyAnalyzer analyzer;

    auto ctx = fl::make_shared<AudioContext>(makeSample_EnergyAnalyzer(10000.0f, 100));
    analyzer.update(ctx);
    FL_CHECK_GT(analyzer.getRMS(), 0.0f);

    analyzer.reset();
    FL_CHECK_EQ(analyzer.getRMS(), 0.0f);
    FL_CHECK_EQ(analyzer.getPeak(), 0.0f);
    FL_CHECK_EQ(analyzer.getAverageEnergy(), 0.0f);
}

FL_TEST_CASE("EnergyAnalyzer - needsFFT is false") {
    EnergyAnalyzer analyzer;
    FL_CHECK_FALSE(analyzer.needsFFT());
}

FL_TEST_CASE("EnergyAnalyzer - onNormalizedEnergy callback fires") {
    EnergyAnalyzer analyzer;
    float lastNormalized = -1.0f;
    analyzer.onNormalizedEnergy.add([&lastNormalized](float val) { lastNormalized = val; });

    // Feed several samples to establish range
    for (int i = 0; i < 20; ++i) {
        float amplitude = 1000.0f + static_cast<float>(i) * 500.0f;
        auto ctx = fl::make_shared<AudioContext>(makeSample_EnergyAnalyzer(amplitude, i * 100));
        analyzer.update(ctx);
        analyzer.fireCallbacks();
    }

    // The normalized energy callback should have fired
    FL_CHECK_GE(lastNormalized, 0.0f);
    FL_CHECK_LE(lastNormalized, 1.0f);
}

FL_TEST_CASE("EnergyAnalyzer - peak decay over time") {
    EnergyAnalyzer analyzer;
    analyzer.setPeakDecay(0.9f);  // Faster decay for testing

    // Create a loud sample to establish peak
    auto ctx = fl::make_shared<AudioContext>(makeSample_EnergyAnalyzer(15000.0f, 0));
    analyzer.update(ctx);
    float initialPeak = analyzer.getPeak();
    FL_CHECK_GT(initialPeak, 0.0f);

    // Feed silence for many frames - peak should decay
    for (int i = 1; i <= 50; ++i) {
        auto silentCtx = fl::make_shared<AudioContext>(makeSilence_EnergyAnalyzer(i * 100));
        analyzer.update(silentCtx);
    }

    float finalPeak = analyzer.getPeak();
    // Peak should have decayed significantly
    FL_CHECK_LT(finalPeak, initialPeak);
}
