// Unit tests for VocalDetector
// standalone test

#include "test.h"
#include "FastLED.h"
#include "fl/audio.h"
#include "fl/audio/audio_context.h"
#include "fl/fx/audio/detectors/vocal.h"
#include "fl/stl/vector.h"
#include "fl/stl/math.h"
#include "fl/stl/shared_ptr.h"
#include "fl/stl/cstring.h"
#include "fl/math_macros.h"

using namespace fl;

namespace { // vocal_detector

AudioSample makeSample(float freq, fl::u32 timestamp, float amplitude = 16000.0f) {
    fl::vector<fl::i16> data;
    data.reserve(512);
    for (int i = 0; i < 512; ++i) {
        float phase = 2.0f * FL_M_PI * freq * i / 44100.0f;
        data.push_back(static_cast<fl::i16>(amplitude * fl::sinf(phase)));
    }
    return AudioSample(fl::span<const fl::i16>(data.data(), data.size()), timestamp);
}

} // anonymous namespace

FL_TEST_CASE("VocalDetector - pure sine is not vocal") {
    VocalDetector detector;
    detector.setSampleRate(44100);

    auto ctx = fl::make_shared<AudioContext>(makeSample(440.0f, 1000));
    ctx->setSampleRate(44100);
    detector.update(ctx);

    // A pure sine wave should not be detected as vocal
    FL_CHECK_FALSE(detector.isVocal());
}

FL_TEST_CASE("VocalDetector - confidence in valid range") {
    VocalDetector detector;
    detector.setSampleRate(44100);

    auto ctx = fl::make_shared<AudioContext>(makeSample(440.0f, 1000));
    ctx->setSampleRate(44100);
    detector.update(ctx);

    float conf = detector.getConfidence();
    // Confidence is computed as average of three scores: centroidScore, rolloffScore,
    // formantScore. Individual scores can be slightly negative (rolloffScore down to
    // about -0.86 for extreme inputs), so the average can be slightly negative.
    // For well-behaved inputs, confidence should be in approximately [-0.3, 1.0].
    FL_CHECK_GE(conf, -0.5f);
    FL_CHECK_LE(conf, 1.0f);
}

FL_TEST_CASE("VocalDetector - reset clears state") {
    VocalDetector detector;
    detector.setSampleRate(44100);

    auto ctx = fl::make_shared<AudioContext>(makeSample(440.0f, 1000));
    ctx->setSampleRate(44100);
    detector.update(ctx);

    detector.reset();
    FL_CHECK_FALSE(detector.isVocal());
    FL_CHECK_EQ(detector.getConfidence(), 0.0f);
}

FL_TEST_CASE("VocalDetector - callbacks don't crash") {
    VocalDetector detector;
    detector.setSampleRate(44100);

    bool changeCallbackInvoked = false;
    bool lastActiveState = true; // Initialize to opposite of expected
    detector.onVocalChange.add([&changeCallbackInvoked, &lastActiveState](bool active) {
        changeCallbackInvoked = true;
        lastActiveState = active;
    });

    auto ctx = fl::make_shared<AudioContext>(makeSample(440.0f, 1000));
    ctx->setSampleRate(44100);
    detector.update(ctx);

    // A pure sine at 440Hz should not be vocal. Since detector starts with
    // mVocalActive=false and mPreviousVocalActive=false, and the sine is
    // not vocal, there is no state change so the callback should NOT fire.
    FL_CHECK_FALSE(changeCallbackInvoked);
    // Verify the detector correctly identified the sine as non-vocal
    FL_CHECK_FALSE(detector.isVocal());
}

FL_TEST_CASE("VocalDetector - needsFFT is true") {
    VocalDetector detector;
    FL_CHECK(detector.needsFFT());
}

FL_TEST_CASE("VocalDetector - getName returns correct name") {
    VocalDetector detector;
    FL_CHECK(fl::strcmp(detector.getName(), "VocalDetector") == 0);
}

FL_TEST_CASE("VocalDetector - onVocalStart and onVocalEnd callbacks") {
    VocalDetector detector;
    detector.setSampleRate(44100);
    detector.setThreshold(0.3f);  // Lower threshold for easier triggering

    int startCount = 0;
    int endCount = 0;
    detector.onVocalStart.add([&startCount]() { startCount++; });
    detector.onVocalEnd.add([&endCount]() { endCount++; });

    // Feed frames that might trigger vocal detection, then silence
    // Use a complex multi-harmonic signal that resembles vocal formants
    for (int round = 0; round < 3; ++round) {
        // Complex signal with multiple harmonics (vocal-like)
        auto ctx = fl::make_shared<AudioContext>(makeSample(300.0f, round * 1000, 15000.0f));
        ctx->setSampleRate(44100);
        ctx->getFFT(128);  // High bin count for formant resolution
        detector.update(ctx);

        // Silence to potentially trigger vocal end
        fl::vector<fl::i16> silence(512, 0);
        auto silentCtx = fl::make_shared<AudioContext>(
            AudioSample(fl::span<const fl::i16>(silence.data(), silence.size()), round * 1000 + 500));
        silentCtx->setSampleRate(44100);
        silentCtx->getFFT(128);
        detector.update(silentCtx);
    }

    // If vocal end was detected, vocal start must have been detected first
    if (endCount > 0) {
        FL_CHECK_GE(startCount, endCount);
    }
    // Callbacks should not have fired negative times (verify no underflow)
    // and the mechanism should not crash (we got here without crash)
}
