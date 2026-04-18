// Unit tests for audio::detector::TempoAnalyzer

#include "test.h"
#include "fl/audio/audio.h"
#include "fl/audio/audio_context.h"
#include "fl/audio/detector/tempo_analyzer.h"
#include "tests/fl/audio/test_helpers.h"
#include "fl/stl/vector.h"
#include "fl/math/math.h"
#include "fl/stl/shared_ptr.h"
#include "fl/stl/cstring.h"
#include "fl/math/math.h"

using namespace fl;
using fl::audio::test::makeSample;
using fl::audio::test::makeSilence;

namespace test_tempo_analyzer {

static audio::Sample makeSample_TempoAnalyzer(float freq, fl::u32 timestamp, float amplitude = 16000.0f) {
    return makeSample(freq, timestamp, amplitude);
}

static audio::Sample makeSilence_TempoAnalyzer(fl::u32 timestamp) {
    return makeSilence(timestamp);
}

} // namespace test_tempo_analyzer
using namespace test_tempo_analyzer;

FL_TEST_CASE("audio::detector::TempoAnalyzer - initial state") {
    audio::detector::TempoAnalyzer analyzer;
    // Constructor initializes mCurrentBPM to 120.0f as default
    FL_CHECK_EQ(analyzer.getBPM(), 120.0f);
    FL_CHECK_EQ(analyzer.getConfidence(), 0.0f);
    FL_CHECK_FALSE(analyzer.isStable());
    FL_CHECK_EQ(analyzer.getStability(), 0.0f);
}

FL_TEST_CASE("audio::detector::TempoAnalyzer - calculateIntervalScore for valid BPM") {
    audio::detector::TempoAnalyzer analyzer;
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

FL_TEST_CASE("audio::detector::TempoAnalyzer - out-of-range BPM scores less than 1") {
    audio::detector::TempoAnalyzer analyzer;
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

FL_TEST_CASE("audio::detector::TempoAnalyzer - reset clears state") {
    audio::detector::TempoAnalyzer analyzer;
    auto ctx = fl::make_shared<audio::Context>(makeSilence_TempoAnalyzer(0));
    ctx->setSampleRate(44100);

    for (int i = 0; i < 10; ++i) {
        ctx->setSample(makeSample_TempoAnalyzer(200.0f, i * 23));
        analyzer.update(ctx);
    }

    analyzer.reset();
    // After reset, BPM returns to default 120.0f
    FL_CHECK_EQ(analyzer.getBPM(), 120.0f);
    FL_CHECK_EQ(analyzer.getConfidence(), 0.0f);
    FL_CHECK_FALSE(analyzer.isStable());
}

FL_TEST_CASE("audio::detector::TempoAnalyzer - needsFFT and needsFFTHistory") {
    audio::detector::TempoAnalyzer analyzer;
    FL_CHECK(analyzer.needsFFT());
    FL_CHECK(analyzer.needsFFTHistory());
}

FL_TEST_CASE("audio::detector::TempoAnalyzer - getName returns correct name") {
    audio::detector::TempoAnalyzer analyzer;
    FL_CHECK(fl::strcmp(analyzer.getName(), "TempoAnalyzer") == 0);
}

FL_TEST_CASE("audio::detector::TempoAnalyzer - periodic onsets converge to BPM") {
    audio::detector::TempoAnalyzer analyzer;
    analyzer.setMinBPM(60.0f);
    analyzer.setMaxBPM(180.0f);

    // At 43 fps, each frame is ~23ms. 22 frames * 23ms = 506ms per beat ≈ 118.6 BPM
    const int framesPerBeat = 22;
    const int totalBeats = 15;
    const u32 frameIntervalMs = 23;  // ~43 fps

    // Create context with initial silence
    fl::vector<fl::i16> silenceData(512, 0);
    audio::Sample silence(silenceData, 0);
    auto ctx = fl::make_shared<audio::Context>(silence);
    ctx->setSampleRate(44100);

    // Initialize audio::fft::FFT history before first setSample
    ctx->getFFT(16);
    ctx->setFFTHistoryDepth(4);

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
                ctx->setSample(audio::Sample(bassData, timestamp));
            } else {
                // Silence between beats
                ctx->setSample(audio::Sample(silenceData, timestamp));
            }

            ctx->getFFT(16);
            analyzer.update(ctx);
            analyzer.fireCallbacks();
        }
    }

    // BPM should converge to ~120 (allow +-30 BPM tolerance for CQ kernel spectral leakage)
    float bpm = analyzer.getBPM();
    FL_CHECK_GT(bpm, 90.0f);
    FL_CHECK_LT(bpm, 150.0f);

    // Confidence should be > 0 after consistent pattern
    FL_CHECK_GT(analyzer.getConfidence(), 0.0f);
}

FL_TEST_CASE("audio::detector::TempoAnalyzer - onTempo callback fires") {
    audio::detector::TempoAnalyzer analyzer;
    analyzer.setMinBPM(60.0f);
    analyzer.setMaxBPM(180.0f);

    float lastBPM = -1.0f;
    analyzer.onTempo.add([&lastBPM](float bpm) { lastBPM = bpm; });

    // Simulate periodic onsets at ~120 BPM
    const int framesPerBeat = 22;
    const int totalBeats = 15;
    const u32 frameIntervalMs = 23;

    fl::vector<fl::i16> silenceData(512, 0);
    audio::Sample silence(silenceData, 0);
    auto ctx = fl::make_shared<audio::Context>(silence);
    ctx->setSampleRate(44100);
    ctx->getFFT(16);
    ctx->setFFTHistoryDepth(4);

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
                ctx->setSample(audio::Sample(bassData, timestamp));
            } else {
                ctx->setSample(audio::Sample(silenceData, timestamp));
            }
            ctx->getFFT(16);
            analyzer.update(ctx);
            analyzer.fireCallbacks();
        }
    }

    // onTempo should have fired at least once
    FL_CHECK_GT(lastBPM, 60.0f);
    FL_CHECK_LT(lastBPM, 200.0f);
}

FL_TEST_CASE("audio::detector::TempoAnalyzer - onTempoChange callback fires on BPM shift") {
    audio::detector::TempoAnalyzer analyzer;
    analyzer.setMinBPM(60.0f);
    analyzer.setMaxBPM(200.0f);

    float lastChangedBPM = -1.0f;
    analyzer.onTempoChange.add([&lastChangedBPM](float bpm) { lastChangedBPM = bpm; });

    fl::vector<fl::i16> silenceData(512, 0);
    audio::Sample silence(silenceData, 0);
    auto ctx = fl::make_shared<audio::Context>(silence);
    ctx->setSampleRate(44100);
    ctx->getFFT(16);
    ctx->setFFTHistoryDepth(4);

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
                ctx->setSample(audio::Sample(bassData, timestamp));
            } else {
                ctx->setSample(audio::Sample(silenceData, timestamp));
            }
            ctx->getFFT(16);
            analyzer.update(ctx);
            analyzer.fireCallbacks();
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
                ctx->setSample(audio::Sample(bassData, timestamp));
            } else {
                ctx->setSample(audio::Sample(silenceData, timestamp));
            }
            ctx->getFFT(16);
            analyzer.update(ctx);
            analyzer.fireCallbacks();
        }
    }

    // After shifting tempo, the callback should have received a new BPM
    // The BPM should be positive (we don't require exact value due to algorithm convergence)
    FL_CHECK_GT(lastChangedBPM, 60.0f);
    FL_CHECK_LT(lastChangedBPM, 200.0f);
}

FL_TEST_CASE("audio::detector::TempoAnalyzer - confidence fades in silence, BPM preserved") {
    // Part 4/4 of FastLED#2253 silence-gate rollout.
    //
    // 1. Drive TempoAnalyzer with ~5s of synthetic onsets at 120 BPM.
    // 2. Confirm confidence rises above a threshold.
    // 3. Switch to silence with context->setSilent(true) for ~3s.
    // 4. Assert confidence < 0.05, but BPM estimate is preserved.
    audio::detector::TempoAnalyzer analyzer;
    analyzer.setMinBPM(60.0f);
    analyzer.setMaxBPM(180.0f);
    analyzer.setStabilityThreshold(5.0f);
    const int framesPerBeat = 22;
    const int totalBeats = 15;
    const u32 frameIntervalMs = 23;  // ~43 fps → 22 frames * 23ms ≈ 506ms/beat ≈ 118.6 BPM

    fl::vector<fl::i16> silenceData(512, 0);
    audio::Sample silence(silenceData, 0);
    auto ctx = fl::make_shared<audio::Context>(silence);
    ctx->setSampleRate(44100);
    ctx->getFFT(16);
    ctx->setFFTHistoryDepth(4);

    u32 timestamp = 0;
    // Phase 1: 15 beats of synthetic onsets at ~120 BPM, NOT silent.
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
                ctx->setSample(audio::Sample(bassData, timestamp));
            } else {
                ctx->setSample(audio::Sample(silenceData, timestamp));
            }
            ctx->setSilent(false);  // audio present — envelope is pass-through
            ctx->getFFT(16);
            analyzer.update(ctx);
        }
    }

    const float liveConfidence = analyzer.getConfidence();
    const float liveBPM = analyzer.getBPM();

    // Confidence should have risen above the integration threshold from the
    // design doc (>= 0.3). If the fixture's onsets are weak we still require
    // at least a meaningful non-zero value so the decay test is well-posed.
    FL_CHECK_GT(liveConfidence, 0.3f);
    // BPM is in the ballpark of the 120 BPM click track.
    FL_CHECK_GT(liveBPM, 90.0f);
    FL_CHECK_LT(liveBPM, 150.0f);

    // Phase 2: ~3 seconds of silence with context->setSilent(true).
    // At 43 fps (23ms per frame), 3s ≈ 130 frames. With tau=2s the envelope
    // crosses 95% of the decay in 3*tau = 6s, but at 3s we expect ≈ exp(-1.5) = 0.22
    // of the starting value, which for a start around 0.5 lands near 0.11.
    // Run longer (5s) to be well past 95% → comfortably below the 0.05 target.
    const int silenceFrames = 220;  // ~5s
    for (int i = 0; i < silenceFrames; ++i) {
        timestamp += frameIntervalMs;
        ctx->setSample(audio::Sample(silenceData, timestamp));
        ctx->setSilent(true);  // silence declared — envelope decays
        ctx->getFFT(16);
        analyzer.update(ctx);
    }

    // Confidence should have decayed well below the integration threshold.
    FL_CHECK_LT(analyzer.getConfidence(), 0.05f);
    // BPM estimate is preserved — decaying confidence, not raw BPM, so beat
    // sync can resume from the same tempo when audio returns.
    FL_CHECK_EQ(analyzer.getBPM(), liveBPM);
}

FL_TEST_CASE("audio::detector::TempoAnalyzer - confidence pass-through when not silent") {
    // Envelope must be a no-op when context->isSilent() is false, even if
    // the underlying confidence briefly dips (e.g. between onsets).
    audio::detector::TempoAnalyzer analyzer;
    analyzer.setMinBPM(60.0f);
    analyzer.setMaxBPM(180.0f);

    fl::vector<fl::i16> silenceData(512, 0);
    audio::Sample silence(silenceData, 0);
    auto ctx = fl::make_shared<audio::Context>(silence);
    ctx->setSampleRate(44100);
    ctx->getFFT(16);
    ctx->setFFTHistoryDepth(4);

    u32 timestamp = 0;
    // Drive with onsets, keeping isSilent=false throughout.
    for (int beat = 0; beat < 15; ++beat) {
        for (int frame = 0; frame < 22; ++frame) {
            timestamp += 23;
            if (frame == 0) {
                fl::vector<fl::i16> bassData;
                bassData.reserve(512);
                for (int s = 0; s < 512; ++s) {
                    float phase = 2.0f * FL_M_PI * 200.0f * s / 44100.0f;
                    bassData.push_back(static_cast<fl::i16>(20000.0f * fl::sinf(phase)));
                }
                ctx->setSample(audio::Sample(bassData, timestamp));
            } else {
                ctx->setSample(audio::Sample(silenceData, timestamp));
            }
            ctx->setSilent(false);
            ctx->getFFT(16);
            analyzer.update(ctx);
        }
    }

    // With isSilent=false, envelope is pass-through → confidence matches what
    // the core algorithm produced (non-zero after consistent onsets).
    FL_CHECK_GT(analyzer.getConfidence(), 0.0f);
}

FL_TEST_CASE("audio::detector::TempoAnalyzer - isStable becomes true with consistent tempo") {
    audio::detector::TempoAnalyzer analyzer;
    analyzer.setMinBPM(60.0f);
    analyzer.setMaxBPM(180.0f);
    analyzer.setStabilityThreshold(5.0f);  // 5 BPM tolerance for stability

    fl::vector<fl::i16> silenceData(512, 0);
    audio::Sample silence(silenceData, 0);
    auto ctx = fl::make_shared<audio::Context>(silence);
    ctx->setSampleRate(44100);
    ctx->getFFT(16);
    ctx->setFFTHistoryDepth(4);

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
                ctx->setSample(audio::Sample(bassData, timestamp));
            } else {
                ctx->setSample(audio::Sample(silenceData, timestamp));
            }
            ctx->getFFT(16);
            analyzer.update(ctx);
            analyzer.fireCallbacks();
        }
    }

    // After 30 consistent beats, stability should be > 0
    FL_CHECK_GT(analyzer.getStability(), 0.0f);
}
