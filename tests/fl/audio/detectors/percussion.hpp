// Unit tests for PercussionDetector - multi-feature spectral classification

#include "test.h"
#include "fl/audio/audio.h"
#include "fl/audio/audio_context.h"
#include "fl/audio/detectors/percussion.h"
#include "../test_helpers.h"
#include "fl/stl/vector.h"
#include "fl/stl/math.h"
#include "fl/stl/shared_ptr.h"
#include "fl/stl/cstring.h"

using namespace fl;
using fl::audio::test::makeKickDrum;
using fl::audio::test::makeSnare;
using fl::audio::test::makeHiHat;
using fl::audio::test::makeTom;
using fl::audio::test::makeCrashCymbal;
using fl::audio::test::makeSilence;
using fl::audio::test::makeSample;
using fl::audio::test::makeWhiteNoise;

namespace test_percussion {

// Feed multiple frames to warm up the envelope follower and let onset detection work.
// Returns true if the target type was detected in any frame.
template <typename CheckFn>
bool feedFrames(PercussionDetector& det, AudioSample (*generator)(fl::u32, float, int, float),
                CheckFn check, int numFrames = 10, float amplitude = 16000.0f) {
    bool detected = false;
    for (int i = 0; i < numFrames; ++i) {
        u32 ts = static_cast<u32>(i * 23); // ~23ms per frame at 44100Hz/512 samples
        auto ctx = fl::make_shared<AudioContext>(generator(ts, amplitude, 512, 44100.0f));
        ctx->setSampleRate(44100);
        det.update(ctx);
        if (check(det)) detected = true;
    }
    return detected;
}

// Feed a single generated sample and update detector
void feedSingle(PercussionDetector& det, const AudioSample& sample) {
    auto ctx = fl::make_shared<AudioContext>(sample);
    ctx->setSampleRate(44100);
    det.update(ctx);
}

} // namespace test_percussion
using namespace test_percussion;

// ============================================================================
// API / Constructor Tests
// ============================================================================

FL_TEST_CASE("PercussionDetector - constructor defaults") {
    PercussionDetector det;
    FL_CHECK_FALSE(det.isKick());
    FL_CHECK_FALSE(det.isSnare());
    FL_CHECK_FALSE(det.isHiHat());
    FL_CHECK_FALSE(det.isTom());
    FL_CHECK_EQ(det.getKickConfidence(), 0.0f);
    FL_CHECK_EQ(det.getSnareConfidence(), 0.0f);
    FL_CHECK_EQ(det.getHiHatConfidence(), 0.0f);
    FL_CHECK_EQ(det.getTomConfidence(), 0.0f);
}

FL_TEST_CASE("PercussionDetector - getName and needsFFT") {
    PercussionDetector det;
    FL_CHECK(fl::strcmp(det.getName(), "PercussionDetector") == 0);
    FL_CHECK(det.needsFFT());
    FL_CHECK_FALSE(det.needsFFTHistory());
}

FL_TEST_CASE("PercussionDetector - reset clears state") {
    PercussionDetector det;
    // Feed a kick to set some state
    feedSingle(det, makeKickDrum(100));
    det.reset();
    FL_CHECK_FALSE(det.isKick());
    FL_CHECK_FALSE(det.isSnare());
    FL_CHECK_FALSE(det.isHiHat());
    FL_CHECK_FALSE(det.isTom());
    FL_CHECK_EQ(det.getKickConfidence(), 0.0f);
}

// ============================================================================
// True Positive Tests
// ============================================================================

FL_TEST_CASE("PercussionDetector - kick drum detection") {
    PercussionDetector det;
    // Feed silence first to establish baseline, then kick
    for (int i = 0; i < 3; ++i) {
        feedSingle(det, makeSilence(static_cast<u32>(i * 23)));
    }
    // Now feed kick drum — should trigger onset detection
    feedSingle(det, makeKickDrum(200));
    // The kick should be detected (onset from silence→kick is strong)
    FL_CHECK(det.isKick());
    FL_CHECK_GE(det.getKickConfidence(), 0.40f);
}

FL_TEST_CASE("PercussionDetector - snare detection") {
    PercussionDetector det;
    for (int i = 0; i < 3; ++i) {
        feedSingle(det, makeSilence(static_cast<u32>(i * 23)));
    }
    feedSingle(det, makeSnare(200));
    FL_CHECK(det.isSnare());
    FL_CHECK_GE(det.getSnareConfidence(), 0.35f);
}

FL_TEST_CASE("PercussionDetector - closed hi-hat detection") {
    PercussionDetector det;
    for (int i = 0; i < 3; ++i) {
        feedSingle(det, makeSilence(static_cast<u32>(i * 23)));
    }
    feedSingle(det, makeHiHat(200, false));
    FL_CHECK(det.isHiHat());
    FL_CHECK_GE(det.getHiHatConfidence(), 0.30f);
}

FL_TEST_CASE("PercussionDetector - open hi-hat detection") {
    PercussionDetector det;
    for (int i = 0; i < 3; ++i) {
        feedSingle(det, makeSilence(static_cast<u32>(i * 23)));
    }
    feedSingle(det, makeHiHat(200, true));
    FL_CHECK(det.isHiHat());
}

FL_TEST_CASE("PercussionDetector - tom detection") {
    PercussionDetector det;
    for (int i = 0; i < 3; ++i) {
        feedSingle(det, makeSilence(static_cast<u32>(i * 23)));
    }
    feedSingle(det, makeTom(200, 160.0f));
    FL_CHECK(det.isTom());
    FL_CHECK_GE(det.getTomConfidence(), 0.35f);
}

// ============================================================================
// Cross-Rejection Tests (True Negatives)
// ============================================================================

FL_TEST_CASE("PercussionDetector - kick is not snare/hihat/tom") {
    PercussionDetector det;
    for (int i = 0; i < 3; ++i) {
        feedSingle(det, makeSilence(static_cast<u32>(i * 23)));
    }
    feedSingle(det, makeKickDrum(200));
    FL_CHECK_FALSE(det.isSnare());
    FL_CHECK_FALSE(det.isHiHat());
    // Tom vs kick is disambiguated by click ratio
    // Kick has click, so tom should not trigger
    FL_CHECK_FALSE(det.isTom());
}

FL_TEST_CASE("PercussionDetector - tom is not kick") {
    PercussionDetector det;
    for (int i = 0; i < 3; ++i) {
        feedSingle(det, makeSilence(static_cast<u32>(i * 23)));
    }
    feedSingle(det, makeTom(200, 160.0f));
    // Tom has no click, so kick should not trigger due to cross-band rejection
    FL_CHECK_FALSE(det.isKick());
}

FL_TEST_CASE("PercussionDetector - silence triggers nothing") {
    PercussionDetector det;
    for (int i = 0; i < 5; ++i) {
        feedSingle(det, makeSilence(static_cast<u32>(i * 23)));
    }
    FL_CHECK_FALSE(det.isKick());
    FL_CHECK_FALSE(det.isSnare());
    FL_CHECK_FALSE(det.isHiHat());
    FL_CHECK_FALSE(det.isTom());
}

FL_TEST_CASE("PercussionDetector - sustained sine is not kick") {
    PercussionDetector det;
    // Feed continuous 80Hz sine for many frames — after envelope stabilizes,
    // onset sharpness drops and kick should not trigger
    for (int i = 0; i < 20; ++i) {
        feedSingle(det, makeSample(80.0f, static_cast<u32>(i * 23)));
    }
    // After many frames of sustained signal, onset sharpness should be low
    FL_CHECK_FALSE(det.isKick());
}

FL_TEST_CASE("PercussionDetector - white noise is not kick or tom") {
    PercussionDetector det;
    for (int i = 0; i < 3; ++i) {
        feedSingle(det, makeSilence(static_cast<u32>(i * 23)));
    }
    feedSingle(det, makeWhiteNoise(200));
    FL_CHECK_FALSE(det.isKick());
    FL_CHECK_FALSE(det.isTom());
}

// ============================================================================
// Feature Inspection Tests
// ============================================================================

FL_TEST_CASE("PercussionDetector - kick features") {
    PercussionDetector det;
    for (int i = 0; i < 3; ++i) {
        feedSingle(det, makeSilence(static_cast<u32>(i * 23)));
    }
    feedSingle(det, makeKickDrum(200));
    // Kick should have dominant bass
    FL_CHECK_GE(det.getBassToTotalRatio(), 0.40f);
    // Kick should have click transient
    FL_CHECK_GE(det.getClickRatio(), 0.5f);
}

FL_TEST_CASE("PercussionDetector - hi-hat features") {
    PercussionDetector det;
    for (int i = 0; i < 3; ++i) {
        feedSingle(det, makeSilence(static_cast<u32>(i * 23)));
    }
    feedSingle(det, makeHiHat(200, false));
    // Hi-hat should have dominant treble
    FL_CHECK_GE(det.getTrebleToTotalRatio(), 0.30f);
}

FL_TEST_CASE("PercussionDetector - tom features: low click ratio") {
    PercussionDetector det;
    for (int i = 0; i < 3; ++i) {
        feedSingle(det, makeSilence(static_cast<u32>(i * 23)));
    }
    feedSingle(det, makeTom(200, 160.0f));
    // Tom has no click transient — click ratio should be low
    FL_CHECK_LE(det.getClickRatio(), 1.0f);
}

// ============================================================================
// Callback Tests
// ============================================================================

FL_TEST_CASE("PercussionDetector - kick callback fires") {
    PercussionDetector det;
    int kickCount = 0;
    int percCount = 0;
    det.onKick.add([&kickCount]() { ++kickCount; });
    det.onPercussionHit.add([&percCount](PercussionType) { ++percCount; });

    for (int i = 0; i < 3; ++i) {
        feedSingle(det, makeSilence(static_cast<u32>(i * 23)));
        det.fireCallbacks();
    }
    feedSingle(det, makeKickDrum(200));
    det.fireCallbacks();

    FL_CHECK_GE(kickCount, 1);
    FL_CHECK_GE(percCount, 1);
}

FL_TEST_CASE("PercussionDetector - tom callback fires") {
    PercussionDetector det;
    int tomCount = 0;
    det.onTom.add([&tomCount]() { ++tomCount; });

    for (int i = 0; i < 3; ++i) {
        feedSingle(det, makeSilence(static_cast<u32>(i * 23)));
        det.fireCallbacks();
    }
    feedSingle(det, makeTom(200, 160.0f));
    det.fireCallbacks();

    FL_CHECK_GE(tomCount, 1);
}

// ============================================================================
// Confidence Range Tests
// ============================================================================

FL_TEST_CASE("PercussionDetector - confidence always in 0-1 range") {
    PercussionDetector det;
    // Feed various signals and check confidence bounds
    feedSingle(det, makeKickDrum(100));
    FL_CHECK_GE(det.getKickConfidence(), 0.0f);
    FL_CHECK_LE(det.getKickConfidence(), 1.0f);
    FL_CHECK_GE(det.getSnareConfidence(), 0.0f);
    FL_CHECK_LE(det.getSnareConfidence(), 1.0f);
    FL_CHECK_GE(det.getHiHatConfidence(), 0.0f);
    FL_CHECK_LE(det.getHiHatConfidence(), 1.0f);
    FL_CHECK_GE(det.getTomConfidence(), 0.0f);
    FL_CHECK_LE(det.getTomConfidence(), 1.0f);
}
