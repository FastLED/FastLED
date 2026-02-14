// Unit tests for EnergyAnalyzer
#include "../../../audio/test_helpers.hpp"

#include "test.h"
#include "FastLED.h"
#include "fl/audio.h"
#include "fl/audio/audio_context.h"
#include "fl/fx/audio/detectors/energy_analyzer.h"
#include "fl/stl/vector.h"
#include "fl/stl/math.h"
#include "fl/stl/shared_ptr.h"
#include "fl/math_macros.h"

using namespace fl;
using namespace fl::test;

FL_TEST_CASE("EnergyAnalyzer - silence gives zero RMS") {
    EnergyAnalyzer analyzer;
    auto ctx = fl::make_shared<AudioContext>(makeSilence(0));
    analyzer.update(ctx);
    FL_CHECK_EQ(analyzer.getRMS(), 0.0f);
    FL_CHECK_EQ(analyzer.getPeak(), 0.0f);
}

FL_TEST_CASE("EnergyAnalyzer - known amplitude gives predictable RMS") {
    EnergyAnalyzer analyzer;
    auto ctx = fl::make_shared<AudioContext>(makeSample(static_cast<i16>(10000), 100));
    analyzer.update(ctx);
    float rms = analyzer.getRMS();
    // Constant amplitude RMS = amplitude ≈ 10000
    FL_CHECK_GT(rms, 9000.0f);
    FL_CHECK_LT(rms, 11000.0f);
}

FL_TEST_CASE("EnergyAnalyzer - peak tracking") {
    EnergyAnalyzer analyzer;

    // Feed quiet signal
    auto ctx1 = fl::make_shared<AudioContext>(makeSample(static_cast<i16>(1000), 100));
    analyzer.update(ctx1);
    float quietPeak = analyzer.getPeak();

    // Feed louder signal
    auto ctx2 = fl::make_shared<AudioContext>(makeSample(static_cast<i16>(15000), 200));
    analyzer.update(ctx2);
    float loudPeak = analyzer.getPeak();

    FL_CHECK_GT(loudPeak, quietPeak);
}

FL_TEST_CASE("EnergyAnalyzer - average energy tracking") {
    EnergyAnalyzer analyzer;

    for (int i = 0; i < 10; ++i) {
        auto ctx = fl::make_shared<AudioContext>(makeSample(static_cast<i16>(5000), i * 100));
        analyzer.update(ctx);
    }

    float avg = analyzer.getAverageEnergy();
    // Constant amplitude 5000 -> RMS ≈ 5000
    // With 10 identical samples, the average should converge to the RMS value
    FL_CHECK_GT(avg, 4000.0f);
    FL_CHECK_LT(avg, 6000.0f);
}

FL_TEST_CASE("EnergyAnalyzer - min/max energy tracking") {
    EnergyAnalyzer analyzer;

    // Feed varying amplitudes
    auto ctx1 = fl::make_shared<AudioContext>(makeSample(static_cast<i16>(2000), 100));
    analyzer.update(ctx1);
    auto ctx2 = fl::make_shared<AudioContext>(makeSample(static_cast<i16>(15000), 200));
    analyzer.update(ctx2);
    auto ctx3 = fl::make_shared<AudioContext>(makeSample(static_cast<i16>(5000), 300));
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
        auto ctx = fl::make_shared<AudioContext>(makeSample(static_cast<i16>(amplitude), i * 100));
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

    auto ctx = fl::make_shared<AudioContext>(makeSample(static_cast<i16>(10000), 100));
    analyzer.update(ctx);

    FL_CHECK_GT(lastRMS, 0.0f);
    FL_CHECK_GT(lastPeak, 0.0f);
}

FL_TEST_CASE("EnergyAnalyzer - reset clears state") {
    EnergyAnalyzer analyzer;

    auto ctx = fl::make_shared<AudioContext>(makeSample(static_cast<i16>(10000), 100));
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
        auto ctx = fl::make_shared<AudioContext>(makeSample(static_cast<i16>(amplitude), i * 100));
        analyzer.update(ctx);
    }

    // The normalized energy callback should have fired
    FL_CHECK_GE(lastNormalized, 0.0f);
    FL_CHECK_LE(lastNormalized, 1.0f);
}

FL_TEST_CASE("EnergyAnalyzer - peak decay over time") {
    EnergyAnalyzer analyzer;
    analyzer.setPeakDecay(0.9f);  // Faster decay for testing

    // Create a loud sample to establish peak
    auto ctx = fl::make_shared<AudioContext>(makeSample(static_cast<i16>(15000), 0));
    analyzer.update(ctx);
    float initialPeak = analyzer.getPeak();
    FL_CHECK_GT(initialPeak, 0.0f);

    // Feed silence for many frames - peak should decay
    for (int i = 1; i <= 50; ++i) {
        auto silentCtx = fl::make_shared<AudioContext>(makeSilence(i * 100));
        analyzer.update(silentCtx);
    }

    float finalPeak = analyzer.getPeak();
    // Peak should have decayed significantly
    FL_CHECK_LT(finalPeak, initialPeak);
}
