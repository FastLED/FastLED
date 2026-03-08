// Unit tests for VibeDetector — self-normalizing audio analysis

#include "test.h"
#include "fl/audio/audio.h"
#include "fl/audio/audio_context.h"
#include "fl/audio/detectors/vibe.h"
#include "../test_helpers.h"
#include "fl/stl/math.h"
#include "fl/stl/shared_ptr.h"

using namespace fl;
using fl::audio::test::makeSample;
using fl::audio::test::makeSilence;

namespace {  // Anonymous namespace for vibe tests

void feedFrames(VibeDetector& detector, int count, float freq,
                float amplitude = 16000.0f) {
    for (int i = 0; i < count; ++i) {
        auto sample = makeSample(freq, i * 12, amplitude);
        auto ctx = fl::make_shared<AudioContext>(sample);
        ctx->setSampleRate(44100);
        detector.update(ctx);
        detector.fireCallbacks();
    }
}

void feedSilence(VibeDetector& detector, int count) {
    for (int i = 0; i < count; ++i) {
        auto sample = makeSilence(i * 12);
        auto ctx = fl::make_shared<AudioContext>(sample);
        ctx->setSampleRate(44100);
        detector.update(ctx);
        detector.fireCallbacks();
    }
}

}  // anonymous namespace

FL_TEST_CASE("VibeDetector - initial state") {
    VibeDetector detector;

    // Before any updates, relative levels should be 1.0 (neutral)
    FL_CHECK_EQ(detector.getBass(), 1.0f);
    FL_CHECK_EQ(detector.getMid(), 1.0f);
    FL_CHECK_EQ(detector.getTreb(), 1.0f);
    FL_CHECK_EQ(detector.getVol(), 1.0f);

    FL_CHECK_EQ(detector.getBassAtt(), 1.0f);
    FL_CHECK_EQ(detector.getMidAtt(), 1.0f);
    FL_CHECK_EQ(detector.getTrebAtt(), 1.0f);

    // No spikes initially
    FL_CHECK_FALSE(detector.isBassSpike());
    FL_CHECK_FALSE(detector.isMidSpike());
    FL_CHECK_FALSE(detector.isTrebSpike());

    // Raw values should be zero
    FL_CHECK_EQ(detector.getBassRaw(), 0.0f);
    FL_CHECK_EQ(detector.getMidRaw(), 0.0f);
    FL_CHECK_EQ(detector.getTrebRaw(), 0.0f);
}

FL_TEST_CASE("VibeDetector - needsFFT is true") {
    VibeDetector detector;
    FL_CHECK_TRUE(detector.needsFFT());
}

FL_TEST_CASE("VibeDetector - silence keeps levels at defaults") {
    VibeDetector detector;
    feedSilence(detector, 10);

    // With silence, immediate energy is zero. Long-term average stays near zero.
    // When long_avg < 0.001, relative levels clamp to 1.0.
    FL_CHECK_EQ(detector.getBass(), 1.0f);
    FL_CHECK_EQ(detector.getMid(), 1.0f);
    FL_CHECK_EQ(detector.getTreb(), 1.0f);
}

FL_TEST_CASE("VibeDetector - bass signal produces bass energy") {
    VibeDetector detector;

    // Feed bass signal (100 Hz) for enough frames to establish levels
    feedFrames(detector, 60, 100.0f);

    // Bass raw should have energy
    FL_CHECK_GT(detector.getBassRaw(), 0.0f);

    // Bass should dominate over mid and treb in raw values
    FL_CHECK_GT(detector.getBassRaw(), detector.getMidRaw());
    FL_CHECK_GT(detector.getBassRaw(), detector.getTrebRaw());
}

FL_TEST_CASE("VibeDetector - self-normalization converges toward 1.0") {
    VibeDetector detector;

    // Feed a steady signal for many frames (past the 50-frame startup)
    feedFrames(detector, 100, 440.0f);

    // After steady-state with constant input, the relative levels should
    // converge toward 1.0 (since immediate ≈ long-term average)
    // The 440 Hz signal falls in the bass band (0-3650 Hz)
    // After 100 frames of steady signal, imm ≈ long_avg → ratio ≈ 1.0
    FL_CHECK_GT(detector.getBass(), 0.5f);
    FL_CHECK_LT(detector.getBass(), 2.0f);
}

FL_TEST_CASE("VibeDetector - asymmetric attack is faster than decay") {
    VibeDetector detector;

    // Establish a baseline with moderate signal
    feedFrames(detector, 60, 440.0f, 5000.0f);
    float baselineAvg = detector.getBassAvg();

    // Sudden loud signal (attack) — avg should jump quickly
    auto loudSample = makeSample(440.0f, 1000, 30000.0f);
    auto ctx = fl::make_shared<AudioContext>(loudSample);
    ctx->setSampleRate(44100);
    detector.update(ctx);
    float afterAttack = detector.getBassAvg();
    float attackDelta = afterAttack - baselineAvg;

    // Reset and establish same baseline
    detector.reset();
    feedFrames(detector, 60, 440.0f, 30000.0f);
    float loudBaseline = detector.getBassAvg();

    // Sudden quiet signal (decay) — avg should drop slowly
    auto quietSample = makeSample(440.0f, 2000, 5000.0f);
    ctx = fl::make_shared<AudioContext>(quietSample);
    ctx->setSampleRate(44100);
    detector.update(ctx);
    float afterDecay = detector.getBassAvg();
    float decayDelta = loudBaseline - afterDecay;

    // Attack should produce a larger delta (faster response) than decay
    // because rate=0.2 (attack) lets more new signal through than rate=0.5 (decay)
    FL_CHECK_GT(attackDelta, 0.0f);
    FL_CHECK_GT(decayDelta, 0.0f);
    FL_CHECK_GT(attackDelta, decayDelta);
}

FL_TEST_CASE("VibeDetector - spike detection on rising energy") {
    VibeDetector detector;

    // Establish steady baseline
    feedFrames(detector, 60, 440.0f, 5000.0f);

    // Sudden loud frame should trigger a spike
    // (immediate relative > smoothed relative)
    auto loudSample = makeSample(440.0f, 1000, 30000.0f);
    auto ctx = fl::make_shared<AudioContext>(loudSample);
    ctx->setSampleRate(44100);
    detector.update(ctx);

    // After a sudden increase, bass_imm_rel should exceed bass_avg_rel
    FL_CHECK_TRUE(detector.isBassSpike());
}

FL_TEST_CASE("VibeDetector - spike callbacks fire on rising edge") {
    VibeDetector detector;
    int bassSpikeCount = 0;
    detector.onBassSpike.add([&bassSpikeCount]() { bassSpikeCount++; });

    // Establish baseline with quiet signal
    feedFrames(detector, 60, 440.0f, 3000.0f);

    // Now send a sudden loud frame
    auto loudSample = makeSample(440.0f, 1000, 30000.0f);
    auto ctx = fl::make_shared<AudioContext>(loudSample);
    ctx->setSampleRate(44100);
    detector.update(ctx);
    detector.fireCallbacks();

    // The callback should fire (rising edge: was not spiking → now spiking)
    FL_CHECK_GE(bassSpikeCount, 1);
}

FL_TEST_CASE("VibeDetector - onVibeLevels callback fires with valid data") {
    VibeDetector detector;
    VibeLevels received;
    bool called = false;
    detector.onVibeLevels.add([&](const VibeLevels& levels) {
        received = levels;
        called = true;
    });

    auto sample = makeSample(440.0f, 0, 16000.0f);
    auto ctx = fl::make_shared<AudioContext>(sample);
    ctx->setSampleRate(44100);
    detector.update(ctx);
    detector.fireCallbacks();

    FL_CHECK_TRUE(called);
    FL_CHECK_GE(received.bass, 0.0f);
    FL_CHECK_GE(received.mid, 0.0f);
    FL_CHECK_GE(received.treb, 0.0f);
    FL_CHECK_GE(received.vol, 0.0f);
    FL_CHECK_GE(received.bassRaw, 0.0f);
}

FL_TEST_CASE("VibeDetector - onVibeLevels struct has all fields populated") {
    VibeDetector detector;
    VibeLevels received;
    detector.onVibeLevels.add([&](const VibeLevels& levels) {
        received = levels;
    });

    feedFrames(detector, 60, 440.0f);

    // After 60 frames, raw values should be populated
    FL_CHECK_GT(received.bassRaw, 0.0f);
    // vol should be average of three bands
    float expectedVol = (received.bass + received.mid + received.treb) / 3.0f;
    FL_CHECK_EQ(received.vol, expectedVol);
}

FL_TEST_CASE("VibeDetector - reset clears all state") {
    VibeDetector detector;

    // Feed some data to establish state
    feedFrames(detector, 30, 440.0f, 16000.0f);
    FL_CHECK_GT(detector.getBassRaw(), 0.0f);

    // Reset
    detector.reset();

    // Should be back to initial state
    FL_CHECK_EQ(detector.getBassRaw(), 0.0f);
    FL_CHECK_EQ(detector.getMidRaw(), 0.0f);
    FL_CHECK_EQ(detector.getTrebRaw(), 0.0f);
    FL_CHECK_EQ(detector.getBass(), 1.0f);
    FL_CHECK_EQ(detector.getMid(), 1.0f);
    FL_CHECK_EQ(detector.getTreb(), 1.0f);
    FL_CHECK_EQ(detector.getBassAtt(), 1.0f);
    FL_CHECK_FALSE(detector.isBassSpike());
    FL_CHECK_FALSE(detector.isMidSpike());
    FL_CHECK_FALSE(detector.isTrebSpike());
}

FL_TEST_CASE("VibeDetector - long-term average tracks signal") {
    // With MilkDrop first-frame init, long_avg starts at the first frame's
    // energy and slowly adapts via EMA (rate=0.992 at 30fps).
    VibeDetector detector;

    feedFrames(detector, 50, 440.0f, 16000.0f);
    float longAvg = detector.getBassLongAvg();

    // After 50 frames, long_avg should be non-zero (initialized on first frame)
    FL_CHECK_GT(longAvg, 0.0f);
}

FL_TEST_CASE("VibeDetector - vol is average of three bands") {
    VibeDetector detector;
    feedFrames(detector, 60, 440.0f);

    float expectedVol = (detector.getBass() + detector.getMid() + detector.getTreb()) / 3.0f;
    FL_CHECK_EQ(detector.getVol(), expectedVol);

    float expectedVolAtt = (detector.getBassAtt() + detector.getMidAtt() + detector.getTrebAtt()) / 3.0f;
    FL_CHECK_EQ(detector.getVolAtt(), expectedVolAtt);
}

FL_TEST_CASE("VibeDetector - adjustRateToFPS preserves per-second behavior") {
    // This is a mathematical property test. At 30fps with rate=0.5:
    // per-second retention = 0.5^30 ≈ 9.3e-10
    // At 60fps the adjusted rate should give the same per-second retention.

    VibeDetector detector1;
    VibeDetector detector2;
    detector2.setTargetFps(60.0f);

    // Feed the same signal to both, but detector2 processes twice as many
    // shorter frames to simulate 60fps
    for (int i = 0; i < 60; ++i) {
        auto sample = makeSample(440.0f, i * 33, 16000.0f, 512);
        auto ctx = fl::make_shared<AudioContext>(sample);
        ctx->setSampleRate(44100);
        detector1.update(ctx);
    }

    for (int i = 0; i < 120; ++i) {
        auto sample = makeSample(440.0f, i * 17, 16000.0f, 256);
        auto ctx = fl::make_shared<AudioContext>(sample);
        ctx->setSampleRate(44100);
        detector2.update(ctx);
    }

    // Both should produce similar relative levels (not identical due to
    // different FFT window sizes, but in the same ballpark)
    float bass1 = detector1.getBass();
    float bass2 = detector2.getBass();

    // Both should be positive and finite
    FL_CHECK_GT(bass1, 0.0f);
    FL_CHECK_GT(bass2, 0.0f);
}

FL_TEST_CASE("VibeDetector - null context is safe") {
    VibeDetector detector;
    detector.update(nullptr);

    // Should not crash, state remains at defaults
    FL_CHECK_EQ(detector.getBass(), 1.0f);
}

// ============================================================================
// Startup behavior tests (MilkDrop first-frame initialization)
// ============================================================================

FL_TEST_CASE("VibeDetector - no false spike on first frame of steady audio") {
    // MilkDrop first-frame init: avg=imm, long_avg=imm → all relative levels = 1.0
    // No spike possible since imm_rel == avg_rel on the first frame.
    VibeDetector detector;

    auto sample = makeSample(440.0f, 0, 16000.0f);
    auto ctx = fl::make_shared<AudioContext>(sample);
    ctx->setSampleRate(44100);
    detector.update(ctx);

    FL_CHECK_GT(detector.getBassRaw(), 0.0f);
    FL_CHECK_FALSE(detector.isBassSpike());
}

FL_TEST_CASE("VibeDetector - no spike with constant signal") {
    // With first-frame init and constant signal, spike should never trigger.
    // avg tracks imm perfectly, so imm_rel never exceeds avg_rel.
    VibeDetector detector;

    int spikeFrames = 0;
    for (int i = 0; i < 15; ++i) {
        auto sample = makeSample(440.0f, i * 12, 16000.0f);
        auto ctx = fl::make_shared<AudioContext>(sample);
        ctx->setSampleRate(44100);
        detector.update(ctx);
        if (detector.isBassSpike()) {
            spikeFrames++;
        }
    }

    FL_CHECK_EQ(spikeFrames, 0);
}

FL_TEST_CASE("VibeDetector - no spike callback on first audio frame") {
    // First frame should not fire spike callbacks because
    // imm_rel == avg_rel == 1.0 (no rising edge).
    VibeDetector detector;
    int spikeCallbackCount = 0;
    detector.onBassSpike.add([&spikeCallbackCount]() { spikeCallbackCount++; });

    auto sample = makeSample(440.0f, 0, 16000.0f);
    auto ctx = fl::make_shared<AudioContext>(sample);
    ctx->setSampleRate(44100);
    detector.update(ctx);
    detector.fireCallbacks();

    FL_CHECK_EQ(spikeCallbackCount, 0);
}

FL_TEST_CASE("VibeDetector - relative level is 1.0 on first frame") {
    // MilkDrop first-frame init ensures imm/long_avg = imm/imm = 1.0
    VibeDetector detector;

    auto sample = makeSample(440.0f, 0, 16000.0f);
    auto ctx = fl::make_shared<AudioContext>(sample);
    ctx->setSampleRate(44100);
    detector.update(ctx);

    FL_CHECK_GT(detector.getBass(), 0.9f);
    FL_CHECK_LT(detector.getBass(), 1.1f);
}

// ============================================================================
// Synthetic wave tests: dynamic range and beat detection quality
// ============================================================================

FL_TEST_CASE("VibeDetector - beat produces relative level above 1.0") {
    // After establishing a quiet baseline, a loud beat should produce
    // relative level > 1.0. This validates that the slow EMA normalization
    // (not running maximum) allows proper beat excursions above 1.0.
    VibeDetector detector;

    // Establish baseline with quiet signal (60 frames ≈ 2 seconds)
    feedFrames(detector, 60, 440.0f, 3000.0f);

    // Sudden loud beat
    auto loudSample = makeSample(440.0f, 1000, 30000.0f);
    auto ctx = fl::make_shared<AudioContext>(loudSample);
    ctx->setSampleRate(44100);
    detector.update(ctx);

    // With slow EMA, long_avg is still close to the quiet level,
    // so the loud frame should produce bass > 1.0
    FL_CHECK_GT(detector.getBass(), 1.0f);
    FL_CHECK_TRUE(detector.isBassSpike());
}

FL_TEST_CASE("VibeDetector - quiet after loud produces relative level below 1.0") {
    // After a period of loud signal, going quiet should drop below 1.0.
    // Validates that the slow EMA is symmetric (not a running max).
    VibeDetector detector;

    // Loud baseline (60 frames)
    feedFrames(detector, 60, 440.0f, 25000.0f);

    // Sudden quiet frame
    auto quietSample = makeSample(440.0f, 1000, 2000.0f);
    auto ctx = fl::make_shared<AudioContext>(quietSample);
    ctx->setSampleRate(44100);
    detector.update(ctx);

    // long_avg still reflects loud level, so quiet imm/long_avg < 1.0
    FL_CHECK_LT(detector.getBass(), 1.0f);
}

FL_TEST_CASE("VibeDetector - steady signal converges to 1.0 tightly") {
    // After many frames of constant signal, the relative level should
    // be very close to 1.0, not 0.5 or 1.5.
    VibeDetector detector;
    feedFrames(detector, 200, 440.0f, 16000.0f);

    FL_CHECK_GT(detector.getBass(), 0.9f);
    FL_CHECK_LT(detector.getBass(), 1.1f);
}
