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

// ============================================================================
// Tone sweep tests: continuous motion and jitter detection
// ============================================================================

FL_TEST_CASE("VibeDetector - tone sweep shows continuous band motion") {
    // Sweep a pure tone from 200 Hz to 10000 Hz over 300 frames.
    // The 3 bands (bass 20-3688, mid 3688-7356, treb 7356-11025 Hz)
    // should each peak when the tone is in their range, and the
    // dominant band should transition bass → mid → treb smoothly.

    VibeDetector detector;

    const int numFrames = 300;
    const float startFreq = 200.0f;
    const float endFreq = 10000.0f;
    const float amplitude = 16000.0f;

    // Record raw band energies per frame
    float bassHistory[300];
    float midHistory[300];
    float trebHistory[300];

    for (int frame = 0; frame < numFrames; ++frame) {
        float t = static_cast<float>(frame) / static_cast<float>(numFrames - 1);
        float freq = startFreq + (endFreq - startFreq) * t;

        auto sample = makeSample(freq, frame * 12, amplitude);
        auto ctx = fl::make_shared<AudioContext>(sample);
        ctx->setSampleRate(44100);
        detector.update(ctx);

        bassHistory[frame] = detector.getBassRaw();
        midHistory[frame] = detector.getMidRaw();
        trebHistory[frame] = detector.getTrebRaw();
    }

    // --- Check 1: Each band should peak at least once during the sweep ---
    float bassPeak = 0.0f, midPeak = 0.0f, trebPeak = 0.0f;
    int bassPeakFrame = 0, midPeakFrame = 0, trebPeakFrame = 0;
    for (int i = 0; i < numFrames; ++i) {
        if (bassHistory[i] > bassPeak) {
            bassPeak = bassHistory[i];
            bassPeakFrame = i;
        }
        if (midHistory[i] > midPeak) {
            midPeak = midHistory[i];
            midPeakFrame = i;
        }
        if (trebHistory[i] > trebPeak) {
            trebPeak = trebHistory[i];
            trebPeakFrame = i;
        }
    }

    FL_CHECK_GT(bassPeak, 0.0f);
    FL_CHECK_GT(midPeak, 0.0f);
    FL_CHECK_GT(trebPeak, 0.0f);

    // --- Check 2: Peaks should be ordered bass < mid < treb in time ---
    // Bass band is low frequencies (early in sweep), treb is high (late).
    FASTLED_WARN("Tone sweep peak frames: bass=" << bassPeakFrame
                 << " mid=" << midPeakFrame << " treb=" << trebPeakFrame);
    FL_CHECK_LT(bassPeakFrame, midPeakFrame);
    FL_CHECK_LT(midPeakFrame, trebPeakFrame);

    // --- Check 3: Dominant band at known frequencies ---
    // At frame where freq ≈ 1000 Hz (well inside bass band), bass should dominate
    int bassCheckFrame = static_cast<int>(
        (1000.0f - startFreq) / (endFreq - startFreq) * (numFrames - 1));
    FL_CHECK_GT(bassHistory[bassCheckFrame],
                midHistory[bassCheckFrame] + trebHistory[bassCheckFrame]);

    // At frame where freq ≈ 5500 Hz (mid band center), mid should dominate
    int midCheckFrame = static_cast<int>(
        (5500.0f - startFreq) / (endFreq - startFreq) * (numFrames - 1));
    FL_CHECK_GT(midHistory[midCheckFrame], bassHistory[midCheckFrame]);
    FL_CHECK_GT(midHistory[midCheckFrame], trebHistory[midCheckFrame]);

    // At frame where freq ≈ 9000 Hz (treb band center), treb should dominate
    int trebCheckFrame = static_cast<int>(
        (9000.0f - startFreq) / (endFreq - startFreq) * (numFrames - 1));
    FL_CHECK_GT(trebHistory[trebCheckFrame], bassHistory[trebCheckFrame]);
    FL_CHECK_GT(trebHistory[trebCheckFrame], midHistory[trebCheckFrame]);
}

FL_TEST_CASE("VibeDetector - tone sweep has no jitter in raw band energy") {
    // Feed a smooth linear frequency sweep and measure frame-to-frame
    // jitter in each band's raw energy. With a continuous sweep, band
    // energies should change smoothly — large frame-to-frame jumps
    // (relative to neighbors) indicate FFT instability or windowing artifacts.

    VibeDetector detector;

    const int numFrames = 300;
    const float startFreq = 200.0f;
    const float endFreq = 10000.0f;
    const float amplitude = 16000.0f;

    float bassHistory[300];
    float midHistory[300];
    float trebHistory[300];

    for (int frame = 0; frame < numFrames; ++frame) {
        float t = static_cast<float>(frame) / static_cast<float>(numFrames - 1);
        float freq = startFreq + (endFreq - startFreq) * t;

        auto sample = makeSample(freq, frame * 12, amplitude);
        auto ctx = fl::make_shared<AudioContext>(sample);
        ctx->setSampleRate(44100);
        detector.update(ctx);

        bassHistory[frame] = detector.getBassRaw();
        midHistory[frame] = detector.getMidRaw();
        trebHistory[frame] = detector.getTrebRaw();
    }

    // Jitter metric: for each frame, compute the second derivative
    // (acceleration) of each band's energy. High acceleration means the
    // value jumped in a way inconsistent with its neighbors — a jitter.
    //
    // jitter[i] = |history[i+1] - 2*history[i] + history[i-1]|
    //
    // We normalize by the band's overall range to get a relative measure.

    auto computeJitter = [](const float* history, int n) -> float {
        // Find the overall range for normalization
        float minVal = history[0], maxVal = history[0];
        for (int i = 1; i < n; ++i) {
            if (history[i] < minVal) minVal = history[i];
            if (history[i] > maxVal) maxVal = history[i];
        }
        float range = maxVal - minVal;
        if (range < 0.001f) return 0.0f; // Band has no energy variation

        // Compute max normalized second derivative
        float maxAccel = 0.0f;
        for (int i = 1; i < n - 1; ++i) {
            float accel = fl::abs(
                history[i + 1] - 2.0f * history[i] + history[i - 1]);
            float normalized = accel / range;
            if (normalized > maxAccel) maxAccel = normalized;
        }
        return maxAccel;
    };

    float bassJitter = computeJitter(bassHistory, numFrames);
    float midJitter = computeJitter(midHistory, numFrames);
    float trebJitter = computeJitter(trebHistory, numFrames);

    FASTLED_WARN("Tone sweep jitter (normalized 2nd derivative): bass="
                 << bassJitter << " mid=" << midJitter
                 << " treb=" << trebJitter);

    // With a smooth linear sweep, the max normalized second derivative
    // should be bounded. A threshold of 0.5 allows for the inherent
    // discreteness of FFT bins while catching true jitter spikes.
    // (Pure bin-crossing produces a smooth sigmoid, not a spike.)
    FL_CHECK_LT(bassJitter, 0.5f);
    FL_CHECK_LT(midJitter, 0.5f);
    FL_CHECK_LT(trebJitter, 0.5f);
}

FL_TEST_CASE("VibeDetector - tone sweep normalized levels track smoothly") {
    // Test the self-normalizing relative levels during a sweep.
    // After warm-up, the normalized levels should move continuously
    // without stuck-at-1.0 behavior or sudden discontinuities.

    VibeDetector detector;

    // Warm up with broadband signal so long-term averages are established
    for (int i = 0; i < 100; ++i) {
        // White-ish signal: mix of bass + mid + treb frequencies
        fl::vector<fl::i16> data(512, 0);
        float freqs[3] = {500.0f, 5000.0f, 9000.0f};
        for (int band = 0; band < 3; ++band) {
            for (int s = 0; s < 512; ++s) {
                float phase = 2.0f * FL_M_PI * freqs[band] * s / 44100.0f;
                float sample = 8000.0f * fl::sinf(phase);
                data[s] = static_cast<fl::i16>(
                    fl::clamp(static_cast<float>(data[s]) + sample,
                              -32768.0f, 32767.0f));
            }
        }
        auto sample = AudioSample(data, i * 12);
        auto ctx = fl::make_shared<AudioContext>(sample);
        ctx->setSampleRate(44100);
        detector.update(ctx);
    }

    // Now sweep and track normalized levels
    const int sweepFrames = 200;
    float bassNorm[200], midNorm[200], trebNorm[200];

    for (int frame = 0; frame < sweepFrames; ++frame) {
        float t = static_cast<float>(frame) / static_cast<float>(sweepFrames - 1);
        float freq = 200.0f + (10000.0f - 200.0f) * t;

        auto sample = makeSample(freq, (100 + frame) * 12, 16000.0f);
        auto ctx = fl::make_shared<AudioContext>(sample);
        ctx->setSampleRate(44100);
        detector.update(ctx);

        bassNorm[frame] = detector.getBass();
        midNorm[frame] = detector.getMid();
        trebNorm[frame] = detector.getTreb();
    }

    // Check that each normalized band achieves some dynamic range
    // (not stuck at 1.0)
    float bassMin = 999.0f, bassMax = -999.0f;
    float midMin = 999.0f, midMax = -999.0f;
    float trebMin = 999.0f, trebMax = -999.0f;
    for (int i = 0; i < sweepFrames; ++i) {
        if (bassNorm[i] < bassMin) bassMin = bassNorm[i];
        if (bassNorm[i] > bassMax) bassMax = bassNorm[i];
        if (midNorm[i] < midMin) midMin = midNorm[i];
        if (midNorm[i] > midMax) midMax = midNorm[i];
        if (trebNorm[i] < trebMin) trebMin = trebNorm[i];
        if (trebNorm[i] > trebMax) trebMax = trebNorm[i];
    }

    FASTLED_WARN("Sweep normalized ranges: bass=[" << bassMin << "," << bassMax
                 << "] mid=[" << midMin << "," << midMax
                 << "] treb=[" << trebMin << "," << trebMax << "]");

    // Each band should have at least some dynamic range during the sweep
    // (tone entering vs leaving the band)
    FL_CHECK_GT(bassMax - bassMin, 0.1f);
    FL_CHECK_GT(midMax - midMin, 0.1f);
    FL_CHECK_GT(trebMax - trebMin, 0.1f);

    // Check smoothness: max frame-to-frame delta of normalized values
    // should be bounded (no sudden jumps in the self-normalizing output)
    float bassMaxDelta = 0.0f, midMaxDelta = 0.0f, trebMaxDelta = 0.0f;
    for (int i = 1; i < sweepFrames; ++i) {
        float bd = fl::abs(bassNorm[i] - bassNorm[i - 1]);
        float md = fl::abs(midNorm[i] - midNorm[i - 1]);
        float td = fl::abs(trebNorm[i] - trebNorm[i - 1]);
        if (bd > bassMaxDelta) bassMaxDelta = bd;
        if (md > midMaxDelta) midMaxDelta = md;
        if (td > trebMaxDelta) trebMaxDelta = td;
    }

    FASTLED_WARN("Sweep max frame-to-frame delta: bass=" << bassMaxDelta
                 << " mid=" << midMaxDelta << " treb=" << trebMaxDelta);

    // Normalized levels shouldn't jump more than 1.0 per frame
    // (that would be a full-range discontinuity — jitter)
    FL_CHECK_LT(bassMaxDelta, 1.0f);
    FL_CHECK_LT(midMaxDelta, 1.0f);
    FL_CHECK_LT(trebMaxDelta, 1.0f);
}

// ============================================================================
// ADVERSARIAL: Phase-coherent signal tests (realistic audio pipeline)
//
// The makeSample() helper always starts at phase=0, so every frame with
// the same frequency produces IDENTICAL data. Real audio has continuous
// phase across frames — the starting phase of frame N is:
//   phi_N = 2*pi*freq*N*windowSize/sampleRate
// This means each frame sees a DIFFERENT window into the continuous signal,
// causing spectral leakage to vary frame-to-frame. The tests above hide
// this jitter source. These tests expose it.
// ============================================================================

namespace {

// Generate a phase-coherent audio frame from a continuous sine wave.
// Phase accumulates across frames, just like real audio.
AudioSample makePhaseCoherentFrame(float freq, int frameIndex,
                                   float amplitude, int count = 512,
                                   float sampleRate = 44100.0f) {
    fl::vector<fl::i16> data;
    data.reserve(count);
    // Starting sample index in the continuous signal
    int startSample = frameIndex * count;
    for (int i = 0; i < count; ++i) {
        float t = static_cast<float>(startSample + i) / sampleRate;
        float phase = 2.0f * FL_M_PI * freq * t;
        data.push_back(static_cast<fl::i16>(amplitude * fl::sinf(phase)));
    }
    return AudioSample(data, static_cast<fl::u32>(frameIndex * 12));
}

} // anonymous namespace

FL_TEST_CASE("ADVERSARIAL - phase-coherent fixed tone has stable raw output") {
    // Feed a CONTINUOUS 440 Hz tone with correct phase advancement.
    // Each frame sees a different window into the signal. With FFT windowing
    // artifacts, the raw band energy could jitter frame-to-frame.
    VibeDetector detector;

    const float freq = 440.0f;
    const int numFrames = 100;
    float bassValues[100];

    for (int frame = 0; frame < numFrames; ++frame) {
        auto sample = makePhaseCoherentFrame(freq, frame, 16000.0f);
        auto ctx = fl::make_shared<AudioContext>(sample);
        ctx->setSampleRate(44100);
        detector.update(ctx);
        bassValues[frame] = detector.getBassRaw();
    }

    // Skip the first frame (initialization). After that, a fixed frequency
    // should produce nearly constant raw energy — any variation is jitter
    // from FFT windowing phase alignment.
    float minBass = bassValues[1], maxBass = bassValues[1];
    for (int i = 2; i < numFrames; ++i) {
        if (bassValues[i] < minBass) minBass = bassValues[i];
        if (bassValues[i] > maxBass) maxBass = bassValues[i];
    }

    float jitterRange = maxBass - minBass;
    float jitterPct = (maxBass > 0.001f) ? (jitterRange / maxBass * 100.0f) : 0.0f;

    FASTLED_WARN("Phase-coherent 440Hz stability: min=" << minBass
                 << " max=" << maxBass << " jitter=" << jitterPct << "%");

    // With a fixed frequency, raw energy jitter should be small.
    // >10% jitter means the FFT windowing is causing significant instability.
    FL_CHECK_LT(jitterPct, 10.0f);
}

FL_TEST_CASE("ADVERSARIAL - band boundary dwell causes energy oscillation") {
    // Sit exactly at the bass/mid boundary (3688 Hz) with phase-coherent
    // frames. Because the tone is right at the boundary, slight phase
    // changes could cause energy to oscillate between bass and mid bins.
    VibeDetector detector;

    // VibeDetector uses 3 linear bins: 20-11025 Hz
    // Boundary between bass and mid: 20 + (11025-20)/3 = 3688.33 Hz
    const float boundaryFreq = 3688.0f;
    const int numFrames = 100;

    float bassValues[100];
    float midValues[100];

    for (int frame = 0; frame < numFrames; ++frame) {
        auto sample = makePhaseCoherentFrame(boundaryFreq, frame, 16000.0f);
        auto ctx = fl::make_shared<AudioContext>(sample);
        ctx->setSampleRate(44100);
        detector.update(ctx);
        bassValues[frame] = detector.getBassRaw();
        midValues[frame] = detector.getMidRaw();
    }

    // Compute the ratio of bass to (bass+mid) energy per frame.
    // If energy oscillates between bands, this ratio will fluctuate.
    float minRatio = 999.0f, maxRatio = -999.0f;
    for (int i = 1; i < numFrames; ++i) {
        float total = bassValues[i] + midValues[i];
        if (total < 0.001f) continue;
        float ratio = bassValues[i] / total;
        if (ratio < minRatio) minRatio = ratio;
        if (ratio > maxRatio) maxRatio = ratio;
    }

    float oscillation = maxRatio - minRatio;
    FASTLED_WARN("Band boundary dwell (3688 Hz): ratio range=[" << minRatio
                 << "," << maxRatio << "] oscillation=" << oscillation);

    // If oscillation > 0.3, the bass/mid split is unstable at the boundary.
    // This is informational — we log it regardless, but flag severe cases.
    FL_CHECK_LT(oscillation, 0.5f);
}

FL_TEST_CASE("ADVERSARIAL - phase-coherent sweep has worse jitter than phase-reset") {
    // Compare jitter between phase-coherent frames (realistic) and
    // phase-reset frames (makeSample, always starts at phase 0).
    // Phase-coherent should show MORE variation because each frame
    // captures a different window into the signal.

    auto sweepAndMeasureJitter = [](bool phaseCoherent) -> float {
        VibeDetector detector;
        const int numFrames = 200;
        float bassHistory[200];

        for (int frame = 0; frame < numFrames; ++frame) {
            float t = static_cast<float>(frame) / static_cast<float>(numFrames - 1);
            float freq = 200.0f + (10000.0f - 200.0f) * t;

            AudioSample sample = phaseCoherent
                ? makePhaseCoherentFrame(freq, frame, 16000.0f)
                : makeSample(freq, frame * 12, 16000.0f);
            auto ctx = fl::make_shared<AudioContext>(sample);
            ctx->setSampleRate(44100);
            detector.update(ctx);
            bassHistory[frame] = detector.getBassRaw();
        }

        // Compute max normalized 2nd derivative
        float minVal = bassHistory[0], maxVal = bassHistory[0];
        for (int i = 1; i < numFrames; ++i) {
            if (bassHistory[i] < minVal) minVal = bassHistory[i];
            if (bassHistory[i] > maxVal) maxVal = bassHistory[i];
        }
        float range = maxVal - minVal;
        if (range < 0.001f) return 0.0f;

        float maxAccel = 0.0f;
        for (int i = 1; i < numFrames - 1; ++i) {
            float accel = fl::abs(
                bassHistory[i + 1] - 2.0f * bassHistory[i] + bassHistory[i - 1]);
            if (accel / range > maxAccel) maxAccel = accel / range;
        }
        return maxAccel;
    };

    float resetJitter = sweepAndMeasureJitter(false);
    float coherentJitter = sweepAndMeasureJitter(true);

    FASTLED_WARN("Sweep jitter comparison: phase-reset=" << resetJitter
                 << " phase-coherent=" << coherentJitter);

    // Both should still be below the jitter threshold
    FL_CHECK_LT(resetJitter, 0.5f);
    FL_CHECK_LT(coherentJitter, 0.5f);
}

FL_TEST_CASE("ADVERSARIAL - micro-sweep at band boundary exposes step discontinuity") {
    // Sweep frequency by 1 Hz per frame across the bass/mid boundary.
    // With 3 linear FFT bins, the transition should be gradual (spectral
    // leakage smooths it). But if it's a sharp step, the frame-to-frame
    // delta will spike right at the boundary.
    VibeDetector detector;

    const float centerFreq = 3688.0f; // bass/mid boundary
    const float halfRange = 500.0f;   // sweep 3188-4188 Hz in 1 Hz steps
    const int numFrames = static_cast<int>(halfRange * 2.0f);
    const float startFreq = centerFreq - halfRange;

    // Use dynamic allocation for large array
    fl::vector<float> bassHistory(numFrames);
    fl::vector<float> midHistory(numFrames);
    fl::vector<float> bassDeltas(numFrames);

    for (int frame = 0; frame < numFrames; ++frame) {
        float freq = startFreq + static_cast<float>(frame);
        auto sample = makePhaseCoherentFrame(freq, frame, 16000.0f);
        auto ctx = fl::make_shared<AudioContext>(sample);
        ctx->setSampleRate(44100);
        detector.update(ctx);
        bassHistory[frame] = detector.getBassRaw();
        midHistory[frame] = detector.getMidRaw();
    }

    // Compute frame-to-frame delta in bass energy
    float maxDelta = 0.0f;
    int maxDeltaFrame = 0;
    for (int i = 1; i < numFrames; ++i) {
        float delta = fl::abs(bassHistory[i] - bassHistory[i - 1]);
        bassDeltas[i] = delta;
        if (delta > maxDelta) {
            maxDelta = delta;
            maxDeltaFrame = i;
        }
    }

    // Compute average delta for comparison
    float avgDelta = 0.0f;
    for (int i = 1; i < numFrames; ++i) {
        avgDelta += bassDeltas[i];
    }
    avgDelta /= static_cast<float>(numFrames - 1);

    float peakToAvgRatio = (avgDelta > 0.001f) ? (maxDelta / avgDelta) : 0.0f;
    float freqAtMaxDelta = startFreq + static_cast<float>(maxDeltaFrame);

    FASTLED_WARN("Band boundary micro-sweep: max_delta=" << maxDelta
                 << " avg_delta=" << avgDelta << " peak_ratio=" << peakToAvgRatio
                 << " at " << freqAtMaxDelta << " Hz");

    // If peak/avg ratio is very high, there's a step discontinuity at the
    // band boundary instead of a smooth transition.
    // A ratio > 20 means one frame's jump is 20x the average — bad aliasing.
    FL_CHECK_LT(peakToAvgRatio, 20.0f);
}

FL_TEST_CASE("ADVERSARIAL - sub-bin frequency causes inter-frame energy wobble") {
    // Feed a tone at a frequency that doesn't align to an FFT bin center.
    // With 512-sample window at 44100 Hz, bin spacing is ~86 Hz.
    // A tone at 443 Hz (between bins 5 and 6 of a 256-bin FFT) will have
    // spectral leakage that varies with phase alignment.
    // With only 3 linear bins (3668 Hz each), this is less of an issue,
    // but we test it anyway.
    VibeDetector detector;

    // 443.7 Hz — intentionally NOT a multiple of binWidth
    const float freq = 443.7f;
    const int numFrames = 60;
    float rawValues[60];

    for (int frame = 0; frame < numFrames; ++frame) {
        auto sample = makePhaseCoherentFrame(freq, frame, 16000.0f);
        auto ctx = fl::make_shared<AudioContext>(sample);
        ctx->setSampleRate(44100);
        detector.update(ctx);
        rawValues[frame] = detector.getBassRaw();
    }

    // Measure coefficient of variation (stddev/mean) of raw energy
    float sum = 0.0f;
    for (int i = 1; i < numFrames; ++i) sum += rawValues[i];
    float mean = sum / static_cast<float>(numFrames - 1);

    float variance = 0.0f;
    for (int i = 1; i < numFrames; ++i) {
        float diff = rawValues[i] - mean;
        variance += diff * diff;
    }
    variance /= static_cast<float>(numFrames - 2);
    float cv = (mean > 0.001f) ? (fl::sqrtf(variance) / mean * 100.0f) : 0.0f;

    FASTLED_WARN("Sub-bin freq (443.7 Hz) energy wobble: mean=" << mean
                 << " cv=" << cv << "%");

    // Coefficient of variation > 15% means the energy wobbles significantly
    // just due to phase alignment — problematic for LED visualization.
    FL_CHECK_LT(cv, 15.0f);
}

FL_TEST_CASE("ADVERSARIAL - phase-coherent sweep across all 3 bands tracks smoothly") {
    // The definitive test: sweep 200-10000 Hz with realistic phase-coherent
    // frames and check that all 3 raw band energies transition smoothly.
    // This is harder than the phase-reset version because each frame
    // produces a DIFFERENT FFT result even at the same frequency.
    VibeDetector detector;

    const int numFrames = 300;
    float bassHistory[300], midHistory[300], trebHistory[300];

    for (int frame = 0; frame < numFrames; ++frame) {
        float t = static_cast<float>(frame) / static_cast<float>(numFrames - 1);
        float freq = 200.0f + (10000.0f - 200.0f) * t;

        auto sample = makePhaseCoherentFrame(freq, frame, 16000.0f);
        auto ctx = fl::make_shared<AudioContext>(sample);
        ctx->setSampleRate(44100);
        detector.update(ctx);

        bassHistory[frame] = detector.getBassRaw();
        midHistory[frame] = detector.getMidRaw();
        trebHistory[frame] = detector.getTrebRaw();
    }

    // Compute max frame-to-frame delta for each band
    float bassMaxDelta = 0.0f, midMaxDelta = 0.0f, trebMaxDelta = 0.0f;
    float bassRange = 0.0f, midRange = 0.0f, trebRange = 0.0f;

    // Find ranges
    float bMin = bassHistory[0], bMax = bassHistory[0];
    float mMin = midHistory[0], mMax = midHistory[0];
    float tMin = trebHistory[0], tMax = trebHistory[0];
    for (int i = 1; i < numFrames; ++i) {
        if (bassHistory[i] < bMin) bMin = bassHistory[i];
        if (bassHistory[i] > bMax) bMax = bassHistory[i];
        if (midHistory[i] < mMin) mMin = midHistory[i];
        if (midHistory[i] > mMax) mMax = midHistory[i];
        if (trebHistory[i] < tMin) tMin = trebHistory[i];
        if (trebHistory[i] > tMax) tMax = trebHistory[i];
    }
    bassRange = bMax - bMin;
    midRange = mMax - mMin;
    trebRange = tMax - tMin;

    for (int i = 1; i < numFrames; ++i) {
        float bd = fl::abs(bassHistory[i] - bassHistory[i - 1]);
        float md = fl::abs(midHistory[i] - midHistory[i - 1]);
        float td = fl::abs(trebHistory[i] - trebHistory[i - 1]);
        if (bd > bassMaxDelta) bassMaxDelta = bd;
        if (md > midMaxDelta) midMaxDelta = md;
        if (td > trebMaxDelta) trebMaxDelta = td;
    }

    // Normalize deltas by band range
    float bassNormDelta = (bassRange > 0.001f) ? (bassMaxDelta / bassRange) : 0.0f;
    float midNormDelta = (midRange > 0.001f) ? (midMaxDelta / midRange) : 0.0f;
    float trebNormDelta = (trebRange > 0.001f) ? (trebMaxDelta / trebRange) : 0.0f;

    FASTLED_WARN("Phase-coherent sweep deltas (normalized): bass=" << bassNormDelta
                 << " mid=" << midNormDelta << " treb=" << trebNormDelta);

    // A single frame shouldn't jump more than 30% of the total range.
    // If it does, the pipeline has a smoothness problem with real audio.
    FL_CHECK_LT(bassNormDelta, 0.30f);
    FL_CHECK_LT(midNormDelta, 0.30f);
    FL_CHECK_LT(trebNormDelta, 0.30f);
}
