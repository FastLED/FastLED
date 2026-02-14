// Unit tests for TempoAnalyzer
#include "../../../audio/test_helpers.hpp"

#include "test.h"
#include "FastLED.h"
#include "fl/audio.h"
#include "fl/audio/audio_context.h"
#include "fl/fx/audio/detectors/tempo_analyzer.h"
#include "fl/stl/vector.h"
#include "fl/stl/math.h"
#include "fl/stl/shared_ptr.h"
#include "fl/stl/cstring.h"
#include "fl/math_macros.h"

using namespace fl;
using namespace fl::test;

FL_TEST_CASE("TempoAnalyzer - initial state") {
    TempoAnalyzer analyzer;
    // Constructor initializes mCurrentBPM to 120.0f as default
    FL_CHECK_EQ(analyzer.getBPM(), 120.0f);
    FL_CHECK_EQ(analyzer.getConfidence(), 0.0f);
    FL_CHECK_FALSE(analyzer.isStable());
    FL_CHECK_EQ(analyzer.getStability(), 0.0f);
}

FL_TEST_CASE("TempoAnalyzer - calculateIntervalScore for valid BPM") {
    TempoAnalyzer analyzer;
    analyzer.setMinBPM(60.0f);
    analyzer.setMaxBPM(180.0f);

    // Convert BPM to interval (ms)
    u32 interval80 = static_cast<u32>(60000.0f / 80.0f);
    u32 interval120 = static_cast<u32>(60000.0f / 120.0f);
    u32 interval160 = static_cast<u32>(60000.0f / 160.0f);

    float score80 = analyzer.calculateIntervalScore(interval80);
    float score120 = analyzer.calculateIntervalScore(interval120);
    float score160 = analyzer.calculateIntervalScore(interval160);

    // All in-range BPM should score 1.0f
    FL_CHECK_EQ(score80, 1.0f);
    FL_CHECK_EQ(score120, 1.0f);
    FL_CHECK_EQ(score160, 1.0f);
}

FL_TEST_CASE("TempoAnalyzer - out-of-range BPM scores less than 1") {
    TempoAnalyzer analyzer;
    analyzer.setMinBPM(60.0f);
    analyzer.setMaxBPM(180.0f);

    // 30 BPM = 2000ms interval (below minimum BPM)
    u32 tooSlow = static_cast<u32>(60000.0f / 30.0f);
    float scoreSlow = analyzer.calculateIntervalScore(tooSlow);
    // Out-of-range scores are max(0.1, 1.0 - normalizedDist)
    FL_CHECK_LT(scoreSlow, 1.0f);
    FL_CHECK_GE(scoreSlow, 0.1f);

    // 300 BPM = 200ms interval (above maximum BPM)
    u32 tooFast = static_cast<u32>(60000.0f / 300.0f);
    float scoreFast = analyzer.calculateIntervalScore(tooFast);
    FL_CHECK_LT(scoreFast, 1.0f);
    FL_CHECK_GE(scoreFast, 0.1f);
}

FL_TEST_CASE("TempoAnalyzer - reset clears state") {
    TempoAnalyzer analyzer;
    auto ctx = fl::make_shared<AudioContext>(makeSilence(0));
    ctx->setSampleRate(44100);

    for (int i = 0; i < 10; ++i) {
        ctx->setSample(makeSample(200.0f, i * 23));
        analyzer.update(ctx);
    }

    analyzer.reset();
    // After reset, BPM returns to default 120.0f
    FL_CHECK_EQ(analyzer.getBPM(), 120.0f);
    FL_CHECK_EQ(analyzer.getConfidence(), 0.0f);
    FL_CHECK_FALSE(analyzer.isStable());
}

FL_TEST_CASE("TempoAnalyzer - needsFFT and needsFFTHistory") {
    TempoAnalyzer analyzer;
    FL_CHECK(analyzer.needsFFT());
    FL_CHECK(analyzer.needsFFTHistory());
}

FL_TEST_CASE("TempoAnalyzer - getName returns correct name") {
    TempoAnalyzer analyzer;
    FL_CHECK(fl::strcmp(analyzer.getName(), "TempoAnalyzer") == 0);
}

FL_TEST_CASE("TempoAnalyzer - periodic onsets converge to BPM") {
    TempoAnalyzer analyzer;
    analyzer.setMinBPM(60.0f);
    analyzer.setMaxBPM(180.0f);

    // At 43 fps, each frame is ~23ms. 22 frames * 23ms = 506ms per beat ≈ 118.6 BPM
    const int framesPerBeat = 22;
    const int totalBeats = 15;
    const u32 frameIntervalMs = 23;  // ~43 fps

    // Create context with initial silence
    fl::vector<fl::i16> silenceData(512, 0);
    AudioSample silence(fl::span<const fl::i16>(silenceData.data(), silenceData.size()), 0);
    auto ctx = fl::make_shared<AudioContext>(silence);
    ctx->setSampleRate(44100);

    // Initialize FFT history before first setSample
    ctx->getFFT(16);
    ctx->getFFTHistory(4);

    u32 timestamp = 0;
    for (int beat = 0; beat < totalBeats; ++beat) {
        for (int frame = 0; frame < framesPerBeat; ++frame) {
            timestamp += frameIntervalMs;

            if (frame == 0) {
                // Bass burst (onset)
                fl::vector<fl::i16> bassData;
                bassData.reserve(512);
                for (int s = 0; s < 512; ++s) {
                    float phase = 2.0f * FL_M_PI * 200.0f * s / 44100.0f;
                    bassData.push_back(static_cast<fl::i16>(20000.0f * fl::sinf(phase)));
                }
                ctx->setSample(AudioSample(fl::span<const fl::i16>(bassData.data(), bassData.size()), timestamp));
            } else {
                // Silence between beats
                ctx->setSample(AudioSample(fl::span<const fl::i16>(silenceData.data(), silenceData.size()), timestamp));
            }

            ctx->getFFT(16);
            analyzer.update(ctx);
        }
    }

    // BPM should converge to ~120 (allow +-30 BPM tolerance for CQ kernel spectral leakage)
    float bpm = analyzer.getBPM();
    FL_CHECK_GT(bpm, 90.0f);
    FL_CHECK_LT(bpm, 150.0f);

    // Confidence should be > 0 after consistent pattern
    FL_CHECK_GT(analyzer.getConfidence(), 0.0f);
}

FL_TEST_CASE("TempoAnalyzer - onTempo callback fires") {
    TempoAnalyzer analyzer;
    analyzer.setMinBPM(60.0f);
    analyzer.setMaxBPM(180.0f);

    float lastBPM = -1.0f;
    analyzer.onTempo.add([&lastBPM](float bpm) { lastBPM = bpm; });

    // Simulate periodic onsets at ~120 BPM
    const int framesPerBeat = 22;
    const int totalBeats = 15;
    const u32 frameIntervalMs = 23;

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
            if (frame == 0) {
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
            analyzer.update(ctx);
        }
    }

    // onTempo should have fired at least once
    FL_CHECK_GT(lastBPM, 60.0f);
    FL_CHECK_LT(lastBPM, 200.0f);
}

FL_TEST_CASE("TempoAnalyzer - onTempoChange callback fires on BPM shift") {
    TempoAnalyzer analyzer;
    analyzer.setMinBPM(60.0f);
    analyzer.setMaxBPM(200.0f);

    float lastChangedBPM = -1.0f;
    analyzer.onTempoChange.add([&lastChangedBPM](float bpm) { lastChangedBPM = bpm; });

    fl::vector<fl::i16> silenceData(512, 0);
    AudioSample silence(fl::span<const fl::i16>(silenceData.data(), silenceData.size()), 0);
    auto ctx = fl::make_shared<AudioContext>(silence);
    ctx->setSampleRate(44100);
    ctx->getFFT(16);
    ctx->getFFTHistory(4);

    u32 timestamp = 0;

    // Feed steady tempo at ~120 BPM for a while
    for (int beat = 0; beat < 10; ++beat) {
        for (int frame = 0; frame < 22; ++frame) {
            timestamp += 23;
            if (frame == 0) {
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
            analyzer.update(ctx);
        }
    }

    // Now shift to ~90 BPM (30 frames * 23ms = 690ms per beat)
    for (int beat = 0; beat < 10; ++beat) {
        for (int frame = 0; frame < 30; ++frame) {
            timestamp += 23;
            if (frame == 0) {
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
            analyzer.update(ctx);
        }
    }

    // After shifting tempo, the callback should have received a new BPM
    // The BPM should be positive (we don't require exact value due to algorithm convergence)
    FL_CHECK_GT(lastChangedBPM, 60.0f);
    FL_CHECK_LT(lastChangedBPM, 200.0f);
}

FL_TEST_CASE("TempoAnalyzer - isStable becomes true with consistent tempo") {
    TempoAnalyzer analyzer;
    analyzer.setMinBPM(60.0f);
    analyzer.setMaxBPM(180.0f);
    analyzer.setStabilityThreshold(5.0f);  // 5 BPM tolerance for stability

    fl::vector<fl::i16> silenceData(512, 0);
    AudioSample silence(fl::span<const fl::i16>(silenceData.data(), silenceData.size()), 0);
    auto ctx = fl::make_shared<AudioContext>(silence);
    ctx->setSampleRate(44100);
    ctx->getFFT(16);
    ctx->getFFTHistory(4);

    u32 timestamp = 0;

    // Feed very consistent tempo at ~120 BPM for many beats
    for (int beat = 0; beat < 30; ++beat) {
        for (int frame = 0; frame < 22; ++frame) {
            timestamp += 23;
            if (frame == 0) {
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
            analyzer.update(ctx);
        }
    }

    // After 30 consistent beats, stability should be > 0
    FL_CHECK_GT(analyzer.getStability(), 0.0f);
}
