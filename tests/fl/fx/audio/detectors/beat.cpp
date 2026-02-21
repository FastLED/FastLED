// Unit tests for BeatDetector

#include "test.h"
#include "fl/audio.h"
#include "fl/audio/audio_context.h"
#include "fl/fft.h"
#include "fl/fx/audio/detectors/beat.h"
#include "../../../audio/test_helpers.h"
#include "fl/stl/vector.h"
#include "fl/stl/math.h"
#include "fl/stl/shared_ptr.h"
#include "fl/math_macros.h"

using namespace fl;
using fl::audio::test::makeSample;
using fl::audio::test::makeSilence;

namespace {

static AudioSample makeSample_BeatDetector(float freq, fl::u32 timestamp, float amplitude = 16000.0f) {
    return makeSample(freq, timestamp, amplitude);
}

static AudioSample makeSilence_BeatDetector(fl::u32 timestamp) {
    return makeSilence(timestamp);
}

} // anonymous namespace

FL_TEST_CASE("BeatDetector - silence produces no beats") {
    BeatDetector detector;
    int beatCount = 0;
    detector.onBeat.add([&beatCount]() { beatCount++; });

    for (int i = 0; i < 20; ++i) {
        auto ctx = fl::make_shared<AudioContext>(makeSilence_BeatDetector(i * 23));
        ctx->setSampleRate(44100);
        detector.update(ctx);
        detector.fireCallbacks();
    }

    FL_CHECK_EQ(beatCount, 0);
    FL_CHECK_FALSE(detector.isBeat());
}

FL_TEST_CASE("BeatDetector - strong bass onset after silence triggers beat") {
    BeatDetector detector;
    detector.setThreshold(0.1f);  // Lower threshold for testing
    int beatCount = 0;
    detector.onBeat.add([&beatCount]() { beatCount++; });

    auto ctx = fl::make_shared<AudioContext>(makeSilence_BeatDetector(0));
    ctx->setSampleRate(44100);

    // Feed silence to establish baseline
    for (int i = 0; i < 20; ++i) {
        ctx->setSample(makeSilence_BeatDetector(i * 23));
        detector.update(ctx);
        detector.fireCallbacks();
    }

    // Now inject a strong bass signal (200 Hz, within CQ range)
    ctx->setSample(makeSample_BeatDetector(200.0f, 500, 20000.0f));
    detector.update(ctx);
    detector.fireCallbacks();

    // Should have detected at least one beat (strong bass onset)
    bool gotBeat = (beatCount >= 1) || detector.isBeat();
    FL_CHECK_TRUE(gotBeat);
}

FL_TEST_CASE("BeatDetector - pure treble should not trigger beat") {
    BeatDetector detector;
    int beatCount = 0;
    detector.onBeat.add([&beatCount]() { beatCount++; });

    auto ctx = fl::make_shared<AudioContext>(makeSilence_BeatDetector(0));
    ctx->setSampleRate(44100);

    // Establish baseline with silence
    for (int i = 0; i < 20; ++i) {
        ctx->setSample(makeSilence_BeatDetector(i * 23));
        detector.update(ctx);
        detector.fireCallbacks();
    }

    // Inject pure treble (4kHz)
    ctx->setSample(makeSample_BeatDetector(4000.0f, 500, 20000.0f));
    detector.update(ctx);
    detector.fireCallbacks();

    // Treble transient should NOT trigger a beat
    FL_CHECK_EQ(beatCount, 0);
}

FL_TEST_CASE("BeatDetector - getPhase returns valid range") {
    BeatDetector detector;
    auto ctx = fl::make_shared<AudioContext>(makeSilence_BeatDetector(0));
    ctx->setSampleRate(44100);
    detector.update(ctx);

    float phase = detector.getPhase();
    FL_CHECK_GE(phase, 0.0f);
    FL_CHECK_LE(phase, 1.0f);
}

FL_TEST_CASE("BeatDetector - getConfidence returns valid range") {
    BeatDetector detector;
    auto ctx = fl::make_shared<AudioContext>(makeSilence_BeatDetector(0));
    ctx->setSampleRate(44100);
    detector.update(ctx);

    float conf = detector.getConfidence();
    FL_CHECK_GE(conf, 0.0f);
    FL_CHECK_LE(conf, 1.0f);
}

FL_TEST_CASE("BeatDetector - reset clears state") {
    BeatDetector detector;
    auto ctx = fl::make_shared<AudioContext>(makeSilence_BeatDetector(0));
    ctx->setSampleRate(44100);

    // Process some frames
    for (int i = 0; i < 10; ++i) {
        ctx->setSample(makeSample_BeatDetector(200.0f, i * 23));
        detector.update(ctx);
    }

    detector.reset();

    FL_CHECK_FALSE(detector.isBeat());
    // After reset, BPM returns to default 120.0f
    FL_CHECK_EQ(detector.getBPM(), 120.0f);
    FL_CHECK_EQ(detector.getConfidence(), 0.0f);
}

FL_TEST_CASE("BeatDetector - needsFFT and needsFFTHistory") {
    BeatDetector detector;
    FL_CHECK_TRUE(detector.needsFFT());
    FL_CHECK_TRUE(detector.needsFFTHistory());
}

FL_TEST_CASE("BeatDetector - periodic bass onsets converge BPM") {
    BeatDetector detector;
    detector.setThreshold(0.1f);

    int beatCount = 0;
    detector.onBeat.add([&beatCount]() { beatCount++; });

    // At 43 fps, each frame is ~23ms. 22 frames * 23ms = 506ms per beat ≈ 118.6 BPM
    const int framesPerBeat = 22;
    const int totalBeats = 12;
    const u32 frameIntervalMs = 23;  // ~43 fps

    fl::vector<fl::i16> silenceData(512, 0);
    AudioSample silence(fl::span<const fl::i16>(silenceData.data(), silenceData.size()), 0);
    auto ctx = fl::make_shared<AudioContext>(silence);
    ctx->setSampleRate(44100);
    ctx->getFFT(16);
    ctx->getFFTHistory(4);

    u32 timestamp = 0;
    for (int beat = 0; beat < totalBeats; ++beat) {
        for (int frame = 0; frame < framesPerBeat; ++frame) {
            timestamp += frameIntervalMs;

            if (frame == 0 && beat > 0) {
                fl::vector<fl::i16> bassData;
                bassData.reserve(512);
                for (int s = 0; s < 512; ++s) {
                    float phase = 2.0f * FL_M_PI * 200.0f * s / 44100.0f;
                    bassData.push_back(static_cast<fl::i16>(20000.0f * fl::sinf(phase)));
                }
                ctx->setSample(AudioSample(fl::span<const fl::i16>(bassData.data(), bassData.size()), timestamp));
            } else {
                ctx->setSample(AudioSample(fl::span<const fl::i16>(silenceData.data(), silenceData.size()), timestamp));
            }

            ctx->getFFT(16);
            detector.update(ctx);
            detector.fireCallbacks();
        }
    }

    // Should detect some beats
    FL_CHECK_GT(beatCount, 5);  // At least half the onsets should trigger

    // BPM should converge near 120 (22 frames * 23ms = 506ms ≈ 118.6 BPM)
    float bpm = detector.getBPM();
    FL_CHECK_GT(bpm, 90.0f);
    FL_CHECK_LT(bpm, 150.0f);

    // Confidence should increase
    FL_CHECK_GT(detector.getConfidence(), 0.0f);
}

FL_TEST_CASE("BeatDetector - phase increases monotonically between beats") {
    BeatDetector detector;
    detector.setThreshold(0.1f);

    fl::vector<fl::i16> silenceData(512, 0);
    AudioSample silence(fl::span<const fl::i16>(silenceData.data(), silenceData.size()), 0);
    auto ctx = fl::make_shared<AudioContext>(silence);
    ctx->setSampleRate(44100);
    ctx->getFFT(16);
    ctx->getFFTHistory(4);

    // Establish tempo with periodic bass bursts
    u32 timestamp = 0;
    for (int beat = 0; beat < 8; ++beat) {
        for (int frame = 0; frame < 22; ++frame) {
            timestamp += 23;
            if (frame == 0 && beat > 0) {
                fl::vector<fl::i16> bassData;
                bassData.reserve(512);
                for (int s = 0; s < 512; ++s) {
                    float phase = 2.0f * FL_M_PI * 200.0f * s / 44100.0f;
                    bassData.push_back(static_cast<fl::i16>(20000.0f * fl::sinf(phase)));
                }
                ctx->setSample(AudioSample(fl::span<const fl::i16>(bassData.data(), bassData.size()), timestamp));
            } else {
                ctx->setSample(AudioSample(fl::span<const fl::i16>(silenceData.data(), silenceData.size()), timestamp));
            }
            ctx->getFFT(16);
            detector.update(ctx);
            detector.fireCallbacks();
        }
    }

    // After establishing tempo, track phase over several frames
    float prevPhase = -1.0f;
    int monotoneCount = 0;
    int totalChecks = 0;

    for (int frame = 0; frame < 20; ++frame) {
        timestamp += 23;
        ctx->setSample(AudioSample(fl::span<const fl::i16>(silenceData.data(), silenceData.size()), timestamp));
        ctx->getFFT(16);
        detector.update(ctx);
        detector.fireCallbacks();

        float phase = detector.getPhase();
        FL_CHECK_GE(phase, 0.0f);
        FL_CHECK_LE(phase, 1.0f);

        if (prevPhase >= 0.0f && !detector.isBeat()) {
            totalChecks++;
            if (phase >= prevPhase) {
                monotoneCount++;
            }
        }
        prevPhase = phase;
    }

    // Phase should be mostly monotonically increasing between beats
    if (totalChecks > 0) {
        float monotoneRatio = static_cast<float>(monotoneCount) / static_cast<float>(totalChecks);
        FL_CHECK_GT(monotoneRatio, 0.5f);  // At least half should be monotonic
    }
}
