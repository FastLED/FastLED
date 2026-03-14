// Unit tests for BeatDetector

#include "test.h"
#include "fl/audio/audio.h"
#include "fl/audio/audio_context.h"
#include "fl/audio/fft/fft.h"
#include "fl/audio/detectors/beat.h"
#include "../test_helpers.h"
#include "fl/stl/vector.h"
#include "fl/stl/math.h"
#include "fl/stl/shared_ptr.h"
#include "fl/stl/math.h"

using namespace fl;
using fl::audio::test::makeSample;
using fl::audio::test::makeSilence;

namespace test_beat {

struct AmplitudeLevel {
    const char* label;
    float amplitude;
};

static const AmplitudeLevel kBeatAmplitudes[] = {
    {"very_quiet", 500.0f},   // ~-36 dBFS
    {"quiet",      2000.0f},  // ~-24 dBFS
    {"normal",     10000.0f}, // ~-10 dBFS
    {"loud",       20000.0f}, // ~-4  dBFS
    {"max",        32000.0f}, // ~0   dBFS
};

static AudioSample makeSample_BeatDetector(float freq, fl::u32 timestamp, float amplitude = 16000.0f) {
    return makeSample(freq, timestamp, amplitude);
}

static AudioSample makeSilence_BeatDetector(fl::u32 timestamp) {
    return makeSilence(timestamp);
}

} // namespace test_beat
using namespace test_beat;

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
    AudioSample silence(silenceData, 0);
    auto ctx = fl::make_shared<AudioContext>(silence);
    ctx->setSampleRate(44100);
    ctx->getFFT(16);
    ctx->setFFTHistoryDepth(4);

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
                ctx->setSample(AudioSample(bassData, timestamp));
            } else {
                ctx->setSample(AudioSample(silenceData, timestamp));
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
    AudioSample silence(silenceData, 0);
    auto ctx = fl::make_shared<AudioContext>(silence);
    ctx->setSampleRate(44100);
    ctx->getFFT(16);
    ctx->setFFTHistoryDepth(4);

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
                ctx->setSample(AudioSample(bassData, timestamp));
            } else {
                ctx->setSample(AudioSample(silenceData, timestamp));
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
        ctx->setSample(AudioSample(silenceData, timestamp));
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

FL_TEST_CASE("BeatDetector - amplitude sweep: onset detection across dB levels") {
    for (const auto& level : kBeatAmplitudes) {
        BeatDetector detector;
        detector.setThreshold(0.1f);
        int beatCount = 0;
        detector.onBeat.add([&beatCount]() { beatCount++; });

        fl::vector<fl::i16> silenceData(512, 0);
        AudioSample silence(silenceData, 0);
        auto ctx = fl::make_shared<AudioContext>(silence);
        ctx->setSampleRate(44100);

        // Feed 20 frames of silence to establish baseline
        u32 timestamp = 0;
        for (int i = 0; i < 20; ++i) {
            timestamp += 23;
            ctx->setSample(AudioSample(silenceData, timestamp));
            ctx->getFFT(16, 30.0f);
            detector.update(ctx);
            detector.fireCallbacks();
        }

        // Inject a strong bass onset at this amplitude level
        timestamp += 23;
        fl::vector<fl::i16> bassData;
        bassData.reserve(512);
        for (int s = 0; s < 512; ++s) {
            float phase = 2.0f * FL_M_PI * 200.0f * s / 44100.0f;
            bassData.push_back(static_cast<fl::i16>(level.amplitude * fl::sinf(phase)));
        }
        ctx->setSample(AudioSample(bassData, timestamp));
        ctx->getFFT(16, 30.0f);
        detector.update(ctx);
        detector.fireCallbacks();

        bool gotBeat = (beatCount >= 1) || detector.isBeat();

        // Hard assert at normal/loud/max — these must detect the onset
        if (level.amplitude >= 10000.0f) {
            FL_CHECK_TRUE(gotBeat);
        } else {
            // very_quiet/quiet: log result. MIN_FLUX_THRESHOLD=50 blocks low amplitudes.
            FL_MESSAGE("BeatDetector amplitude sweep [" << level.label
                       << " amp=" << level.amplitude
                       << "]: beat=" << (gotBeat ? "yes" : "no")
                       << " (known limitation at low volumes)");
        }
    }
}

FL_TEST_CASE("BeatDetector - amplitude sweep: periodic beats converge BPM") {
    // Only test normal/loud/max — below detection floor BPM tracking is meaningless
    for (int idx = 2; idx < 5; ++idx) {
        const auto& level = kBeatAmplitudes[idx];
        BeatDetector detector;
        detector.setThreshold(0.1f);

        int beatCount = 0;
        detector.onBeat.add([&beatCount]() { beatCount++; });

        const int framesPerBeat = 22;
        const int totalBeats = 12;
        const u32 frameIntervalMs = 23;

        fl::vector<fl::i16> silenceData(512, 0);
        AudioSample silence(silenceData, 0);
        auto ctx = fl::make_shared<AudioContext>(silence);
        ctx->setSampleRate(44100);
        ctx->getFFT(16, 30.0f);
        ctx->setFFTHistoryDepth(4);

        u32 timestamp = 0;
        for (int beat = 0; beat < totalBeats; ++beat) {
            for (int frame = 0; frame < framesPerBeat; ++frame) {
                timestamp += frameIntervalMs;

                if (frame == 0 && beat > 0) {
                    fl::vector<fl::i16> bassData;
                    bassData.reserve(512);
                    for (int s = 0; s < 512; ++s) {
                        float phase = 2.0f * FL_M_PI * 200.0f * s / 44100.0f;
                        bassData.push_back(static_cast<fl::i16>(level.amplitude * fl::sinf(phase)));
                    }
                    ctx->setSample(AudioSample(bassData, timestamp));
                } else {
                    ctx->setSample(AudioSample(silenceData, timestamp));
                }

                ctx->getFFT(16, 30.0f);
                detector.update(ctx);
                detector.fireCallbacks();
            }
        }

        FL_CHECK_GT(beatCount, 5);

        float bpm = detector.getBPM();
        FL_CHECK_GT(bpm, 90.0f);
        FL_CHECK_LT(bpm, 150.0f);

        FL_CHECK_GT(detector.getConfidence(), 0.0f);
    }
}

// =============================================================================
// Synthetic Wave Tests - diagnose onBeat detection bugs
// =============================================================================

namespace test_beat_synthetic {

// Generate a bass pulse (200 Hz sine) AudioSample
static AudioSample makeBassSync(u32 timestamp, float amplitude = 20000.0f) {
    return makeSample(200.0f, timestamp, amplitude);
}

// Run a periodic beat pattern and return results
struct BeatTestResult {
    int beatCount;
    fl::vector<u32> beatTimestamps;
    float finalBPM;
    float finalConfidence;
};

static BeatTestResult runPeriodicBeatTest(
    int totalBeats, int framesPerBeat, float amplitude = 20000.0f,
    float threshold = 1.3f, float sensitivity = 1.0f)
{
    BeatDetector detector;
    detector.setThreshold(threshold);
    detector.setSensitivity(sensitivity);

    BeatTestResult result;
    result.beatCount = 0;

    detector.onBeat.add([&result]() { result.beatCount++; });

    const u32 frameMs = 23; // ~43 fps
    fl::vector<fl::i16> silenceData(512, 0);
    auto ctx = fl::make_shared<AudioContext>(AudioSample(silenceData, 0));
    ctx->setSampleRate(44100);
    ctx->setFFTHistoryDepth(4);

    // Warmup: 43 frames of silence to fill MovingAverage window
    u32 timestamp = 0;
    for (int i = 0; i < 43; ++i) {
        timestamp += frameMs;
        ctx->setSample(AudioSample(silenceData, timestamp));
        detector.update(ctx);
        detector.fireCallbacks();
    }

    // Run periodic pattern
    for (int beat = 0; beat < totalBeats; ++beat) {
        for (int frame = 0; frame < framesPerBeat; ++frame) {
            timestamp += frameMs;

            if (frame == 0) {
                ctx->setSample(makeBassSync(timestamp, amplitude));
            } else {
                ctx->setSample(AudioSample(silenceData, timestamp));
            }

            detector.update(ctx);

            if (detector.isBeat()) {
                result.beatTimestamps.push_back(timestamp);
            }

            detector.fireCallbacks();
        }
    }

    result.finalBPM = detector.getBPM();
    result.finalConfidence = detector.getConfidence();
    return result;
}

} // namespace test_beat_synthetic
using test_beat_synthetic::makeBassSync;

// Test: Clean 120 BPM periodic beats - detection rate
// At 43 fps, 22 frames per beat = 506ms ~= 118.6 BPM
FL_TEST_CASE("BeatSynthetic - 120 BPM detection rate") {
    auto result = test_beat_synthetic::runPeriodicBeatTest(
        /*totalBeats=*/16,
        /*framesPerBeat=*/22,
        /*amplitude=*/20000.0f
    );

    float detectionRate = static_cast<float>(result.beatCount) / 16.0f;
    FL_MESSAGE("120 BPM: detected " << result.beatCount << "/16 beats ("
               << static_cast<int>(detectionRate * 100) << "%)");
    FL_MESSAGE("120 BPM: final BPM=" << result.finalBPM
               << " confidence=" << result.finalConfidence);

    // Clean loud periodic beats: expect >75% detection rate
    FL_CHECK_GT(result.beatCount, 12);
}

// Test: Beats fire on the CORRECT frames (bass pulse frames, not delayed)
FL_TEST_CASE("BeatSynthetic - beats fire on bass pulse frames") {
    BeatDetector detector;
    detector.setThreshold(1.3f);

    const u32 frameMs = 23;
    const int framesPerBeat = 22;
    fl::vector<fl::i16> silenceData(512, 0);
    auto ctx = fl::make_shared<AudioContext>(AudioSample(silenceData, 0));
    ctx->setSampleRate(44100);
    ctx->setFFTHistoryDepth(4);

    u32 timestamp = 0;
    for (int i = 0; i < 43; ++i) {
        timestamp += frameMs;
        ctx->setSample(AudioSample(silenceData, timestamp));
        detector.update(ctx);
        detector.fireCallbacks();
    }

    fl::vector<int> beatFrameIndices;
    int frameIndex = 0;

    for (int beat = 0; beat < 12; ++beat) {
        for (int frame = 0; frame < framesPerBeat; ++frame) {
            timestamp += frameMs;

            if (frame == 0) {
                ctx->setSample(makeBassSync(timestamp, 20000.0f));
            } else {
                ctx->setSample(AudioSample(silenceData, timestamp));
            }

            detector.update(ctx);
            if (detector.isBeat()) {
                beatFrameIndices.push_back(frameIndex);
            }
            detector.fireCallbacks();
            frameIndex++;
        }
    }

    int onPulseFrame = 0;
    int offPulseFrame = 0;
    for (fl::size i = 0; i < beatFrameIndices.size(); ++i) {
        int idx = beatFrameIndices[i];
        if (idx % framesPerBeat == 0) {
            onPulseFrame++;
        } else {
            offPulseFrame++;
        }
    }

    FL_MESSAGE("Beats on pulse frames: " << onPulseFrame
               << ", off pulse frames: " << offPulseFrame);

    if (onPulseFrame + offPulseFrame > 0) {
        float accuracy = static_cast<float>(onPulseFrame) /
                         static_cast<float>(onPulseFrame + offPulseFrame);
        FL_CHECK_GT(accuracy, 0.8f);
    }
}

// Test: Adaptive threshold poisoning
// updateAdaptiveThreshold() is called BEFORE detectBeat(), so the current
// frame's high spectral flux inflates the running average, raising the
// threshold just when we need it lowest.
FL_TEST_CASE("BeatSynthetic - onset after silence always triggers beat") {
    BeatDetector detector;
    detector.setThreshold(1.3f);

    const u32 frameMs = 23;
    fl::vector<fl::i16> silenceData(512, 0);
    auto ctx = fl::make_shared<AudioContext>(AudioSample(silenceData, 0));
    ctx->setSampleRate(44100);
    ctx->setFFTHistoryDepth(4);

    // Fill the 43-sample MovingAverage window with silence
    u32 timestamp = 0;
    for (int i = 0; i < 43; ++i) {
        timestamp += frameMs;
        ctx->setSample(AudioSample(silenceData, timestamp));
        detector.update(ctx);
        detector.fireCallbacks();
    }

    // Inject a strong bass pulse after silence.
    // If the threshold is poisoned by including the current frame,
    // this beat might be missed.
    timestamp += 300;
    ctx->setSample(makeBassSync(timestamp, 20000.0f));
    detector.update(ctx);
    detector.fireCallbacks();

    FL_CHECK_TRUE(detector.isBeat());
}

// Test: Alternating loud/soft beats
// After a loud beat inflates the adaptive threshold, the following
// soft beat may be missed.
FL_TEST_CASE("BeatSynthetic - alternating loud/soft beats detected") {
    BeatDetector detector;
    detector.setThreshold(1.3f);

    const u32 frameMs = 23;
    const int framesPerBeat = 22;
    fl::vector<fl::i16> silenceData(512, 0);
    auto ctx = fl::make_shared<AudioContext>(AudioSample(silenceData, 0));
    ctx->setSampleRate(44100);
    ctx->setFFTHistoryDepth(4);

    u32 timestamp = 0;
    for (int i = 0; i < 43; ++i) {
        timestamp += frameMs;
        ctx->setSample(AudioSample(silenceData, timestamp));
        detector.update(ctx);
        detector.fireCallbacks();
    }

    int loudBeats = 0, softBeats = 0;
    int loudDetected = 0, softDetected = 0;

    for (int beat = 0; beat < 16; ++beat) {
        bool isLoud = (beat % 2 == 0);
        float amp = isLoud ? 20000.0f : 8000.0f;

        for (int frame = 0; frame < framesPerBeat; ++frame) {
            timestamp += frameMs;

            if (frame == 0) {
                ctx->setSample(makeBassSync(timestamp, amp));
            } else {
                ctx->setSample(AudioSample(silenceData, timestamp));
            }

            detector.update(ctx);

            if (frame == 0 && detector.isBeat()) {
                if (isLoud) loudDetected++;
                else softDetected++;
            }

            detector.fireCallbacks();
        }

        if (isLoud) loudBeats++;
        else softBeats++;
    }

    FL_MESSAGE("Loud beats: " << loudDetected << "/" << loudBeats
               << ", Soft beats: " << softDetected << "/" << softBeats);

    FL_CHECK_GT(loudDetected, loudBeats / 2);
    FL_CHECK_GT(softDetected, softBeats / 4);
}

// Test: BPM convergence at 100 BPM
FL_TEST_CASE("BeatSynthetic - BPM convergence at 100 BPM") {
    auto result = test_beat_synthetic::runPeriodicBeatTest(
        /*totalBeats=*/20,
        /*framesPerBeat=*/26, // 26*23ms=598ms ~100.3 BPM
        /*amplitude=*/20000.0f
    );

    float expectedBPM = 60000.0f / (26.0f * 23.0f);
    FL_MESSAGE("100 BPM test: detected BPM=" << result.finalBPM
               << " (expected ~" << expectedBPM << ")");
    FL_MESSAGE("100 BPM test: " << result.beatCount << "/20 beats detected");

    FL_CHECK_GT(result.beatCount, 10);
    FL_CHECK_GT(result.finalBPM, 80.0f);
    FL_CHECK_LT(result.finalBPM, 130.0f);
}

// Test: BPM convergence at 140 BPM
FL_TEST_CASE("BeatSynthetic - BPM convergence at 140 BPM") {
    auto result = test_beat_synthetic::runPeriodicBeatTest(
        /*totalBeats=*/20,
        /*framesPerBeat=*/19, // 19*23ms=437ms ~137.3 BPM
        /*amplitude=*/20000.0f
    );

    float expectedBPM = 60000.0f / (19.0f * 23.0f);
    FL_MESSAGE("140 BPM test: detected BPM=" << result.finalBPM
               << " (expected ~" << expectedBPM << ")");
    FL_MESSAGE("140 BPM test: " << result.beatCount << "/20 beats detected");

    FL_CHECK_GT(result.beatCount, 10);
    FL_CHECK_GT(result.finalBPM, 110.0f);
    FL_CHECK_LT(result.finalBPM, 170.0f);
}

// Test: Near max BPM boundary (250ms cooldown)
FL_TEST_CASE("BeatSynthetic - near max BPM boundary") {
    auto result = test_beat_synthetic::runPeriodicBeatTest(
        /*totalBeats=*/20,
        /*framesPerBeat=*/11, // 11*23ms=253ms ~237 BPM
        /*amplitude=*/20000.0f
    );

    float expectedBPM = 60000.0f / (11.0f * 23.0f);
    FL_MESSAGE("Max BPM test: detected BPM=" << result.finalBPM
               << " (expected ~" << expectedBPM << ")");
    FL_MESSAGE("Max BPM test: " << result.beatCount << "/20 beats detected");

    FL_CHECK_GT(result.beatCount, 5);
}

// Test: Sustained tone should not produce beats after initial onset
FL_TEST_CASE("BeatSynthetic - sustained tone no repeated beats") {
    BeatDetector detector;

    const u32 frameMs = 23;
    fl::vector<fl::i16> silenceData(512, 0);
    auto ctx = fl::make_shared<AudioContext>(AudioSample(silenceData, 0));
    ctx->setSampleRate(44100);
    ctx->setFFTHistoryDepth(4);

    int beatCount = 0;
    detector.onBeat.add([&beatCount]() { beatCount++; });

    u32 timestamp = 0;
    for (int i = 0; i < 100; ++i) {
        timestamp += frameMs;
        ctx->setSample(makeBassSync(timestamp, 20000.0f));
        detector.update(ctx);
        detector.fireCallbacks();
    }

    FL_MESSAGE("Sustained tone beats: " << beatCount << " (expected <=3)");
    FL_CHECK_LE(beatCount, 3);
}

// Test: Inter-beat interval consistency
FL_TEST_CASE("BeatSynthetic - inter-beat intervals are consistent") {
    BeatDetector detector;
    detector.setThreshold(1.3f);

    const u32 frameMs = 23;
    const int framesPerBeat = 22;
    fl::vector<fl::i16> silenceData(512, 0);
    auto ctx = fl::make_shared<AudioContext>(AudioSample(silenceData, 0));
    ctx->setSampleRate(44100);
    ctx->setFFTHistoryDepth(4);

    fl::vector<u32> beatTimestamps;

    u32 timestamp = 0;
    for (int i = 0; i < 43; ++i) {
        timestamp += frameMs;
        ctx->setSample(AudioSample(silenceData, timestamp));
        detector.update(ctx);
        detector.fireCallbacks();
    }

    for (int beat = 0; beat < 20; ++beat) {
        for (int frame = 0; frame < framesPerBeat; ++frame) {
            timestamp += frameMs;

            if (frame == 0) {
                ctx->setSample(makeBassSync(timestamp, 20000.0f));
            } else {
                ctx->setSample(AudioSample(silenceData, timestamp));
            }

            detector.update(ctx);
            if (detector.isBeat()) {
                beatTimestamps.push_back(timestamp);
            }
            detector.fireCallbacks();
        }
    }

    FL_MESSAGE("IBI test: detected " << beatTimestamps.size() << " beats");

    if (beatTimestamps.size() >= 4) {
        u32 expectedInterval = framesPerBeat * frameMs;
        int goodIntervals = 0;
        int totalIntervals = static_cast<int>(beatTimestamps.size()) - 1;

        for (int i = 0; i < totalIntervals; ++i) {
            u32 interval = beatTimestamps[i + 1] - beatTimestamps[i];
            float ratio = static_cast<float>(interval) /
                          static_cast<float>(expectedInterval);
            if (ratio > 0.8f && ratio < 1.2f) {
                goodIntervals++;
            }
        }

        float consistencyRate = static_cast<float>(goodIntervals) /
                                static_cast<float>(totalIntervals);
        FL_MESSAGE("IBI consistency: " << goodIntervals << "/"
                   << totalIntervals << " within 20% ("
                   << static_cast<int>(consistencyRate * 100) << "%)");

        FL_CHECK_GT(consistencyRate, 0.6f);
    }
}

// =============================================================================
// Real-world scenario tests: expose practical onBeat bugs
// =============================================================================

// Test: Dynamic range adaptation - loud→quiet transition
// After a section of loud beats, the adaptive threshold is elevated.
// When music gets quieter, beats should still be detected.
FL_TEST_CASE("BeatSynthetic - loud to quiet transition detects quiet beats") {
    BeatDetector detector;
    detector.setThreshold(1.3f);

    const u32 frameMs = 23;
    const int framesPerBeat = 22;
    fl::vector<fl::i16> silenceData(512, 0);
    auto ctx = fl::make_shared<AudioContext>(AudioSample(silenceData, 0));
    ctx->setSampleRate(44100);
    ctx->setFFTHistoryDepth(4);

    // Warmup
    u32 timestamp = 0;
    for (int i = 0; i < 43; ++i) {
        timestamp += frameMs;
        ctx->setSample(AudioSample(silenceData, timestamp));
        detector.update(ctx);
        detector.fireCallbacks();
    }

    // Phase 1: 8 LOUD beats (amplitude 25000) to inflate adaptive threshold
    int loudDetected = 0;
    for (int beat = 0; beat < 8; ++beat) {
        for (int frame = 0; frame < framesPerBeat; ++frame) {
            timestamp += frameMs;
            if (frame == 0) {
                ctx->setSample(makeBassSync(timestamp, 25000.0f));
            } else {
                ctx->setSample(AudioSample(silenceData, timestamp));
            }
            detector.update(ctx);
            if (frame == 0 && detector.isBeat()) loudDetected++;
            detector.fireCallbacks();
        }
    }

    // Phase 2: 8 QUIET beats (amplitude 5000) - threshold should adapt down
    int quietDetected = 0;
    for (int beat = 0; beat < 8; ++beat) {
        for (int frame = 0; frame < framesPerBeat; ++frame) {
            timestamp += frameMs;
            if (frame == 0) {
                ctx->setSample(makeBassSync(timestamp, 5000.0f));
            } else {
                ctx->setSample(AudioSample(silenceData, timestamp));
            }
            detector.update(ctx);
            if (frame == 0 && detector.isBeat()) quietDetected++;
            detector.fireCallbacks();
        }
    }

    FL_MESSAGE("Loud→Quiet: loud=" << loudDetected << "/8, quiet="
               << quietDetected << "/8");

    FL_CHECK_GT(loudDetected, 5);
    // BUG EXPOSURE: After loud beats inflate the adaptive threshold,
    // the MovingAverage (window=43) takes ~43 frames to decay.
    // At 22 frames/beat, it takes ~2 beats for the threshold to adapt.
    // The first 2 quiet beats may be missed, but the remaining 6 should fire.
    FL_CHECK_GT(quietDetected, 4);
}

// Test: Background noise + beats
// Simulates music with continuous mid-frequency content and periodic bass kicks
FL_TEST_CASE("BeatSynthetic - beats detected over background noise") {
    BeatDetector detector;
    detector.setThreshold(1.3f);

    const u32 frameMs = 23;
    const int framesPerBeat = 22;
    auto ctx = fl::make_shared<AudioContext>(AudioSample());
    ctx->setSampleRate(44100);
    ctx->setFFTHistoryDepth(4);

    // Helper: generate bass + background sample
    auto makeBackgroundWithBass = [](u32 ts, bool addBass) -> AudioSample {
        fl::vector<fl::i16> data;
        data.reserve(512);
        for (int s = 0; s < 512; ++s) {
            float t = static_cast<float>(s) / 44100.0f;
            // Continuous background: 440Hz pad + 1200Hz pad (both moderate)
            float bg = 3000.0f * fl::sinf(2.0f * FL_M_PI * 440.0f * t)
                     + 2000.0f * fl::sinf(2.0f * FL_M_PI * 1200.0f * t);
            float bass = 0.0f;
            if (addBass) {
                // Strong kick: 80Hz + 160Hz
                bass = 15000.0f * fl::sinf(2.0f * FL_M_PI * 80.0f * t)
                     + 8000.0f * fl::sinf(2.0f * FL_M_PI * 160.0f * t);
            }
            float sample = bg + bass;
            if (sample > 32767.0f) sample = 32767.0f;
            if (sample < -32768.0f) sample = -32768.0f;
            data.push_back(static_cast<fl::i16>(sample));
        }
        return AudioSample(data, ts);
    };

    // Warmup with background only
    u32 timestamp = 0;
    for (int i = 0; i < 43; ++i) {
        timestamp += frameMs;
        ctx->setSample(makeBackgroundWithBass(timestamp, false));
        detector.update(ctx);
        detector.fireCallbacks();
    }

    // 16 periodic beats over background
    int beatCount = 0;
    for (int beat = 0; beat < 16; ++beat) {
        for (int frame = 0; frame < framesPerBeat; ++frame) {
            timestamp += frameMs;
            bool isBeatFrame = (frame == 0);
            ctx->setSample(makeBackgroundWithBass(timestamp, isBeatFrame));
            detector.update(ctx);
            if (isBeatFrame && detector.isBeat()) beatCount++;
            detector.fireCallbacks();
        }
    }

    FL_MESSAGE("Beats over background: " << beatCount << "/16 detected");

    // With background noise, the adaptive threshold is higher but bass onsets
    // should still be detected because CQ bins separate bass from mid
    FL_CHECK_GT(beatCount, 8);
}

// Test: Kick-snare alternation (real drum pattern)
// Kick (low freq, should trigger beat) alternating with snare (mid/high freq,
// should NOT trigger beat). Tests frequency selectivity.
FL_TEST_CASE("BeatSynthetic - kick triggers beat, snare does not") {
    BeatDetector detector;
    detector.setThreshold(1.3f);

    const u32 frameMs = 23;
    const int framesPerBeat = 22; // ~120 BPM, 8th notes = 11 frames
    fl::vector<fl::i16> silenceData(512, 0);
    auto ctx = fl::make_shared<AudioContext>(AudioSample(silenceData, 0));
    ctx->setSampleRate(44100);
    ctx->setFFTHistoryDepth(4);

    // Helper: generate high-frequency snare-like signal
    auto makeSnareSignal = [](u32 ts) -> AudioSample {
        fl::vector<fl::i16> data;
        data.reserve(512);
        for (int s = 0; s < 512; ++s) {
            float t = static_cast<float>(s) / 44100.0f;
            // Snare: mostly high freq content (2kHz + 4kHz + 6kHz)
            float sample = 15000.0f * fl::sinf(2.0f * FL_M_PI * 2000.0f * t)
                         + 10000.0f * fl::sinf(2.0f * FL_M_PI * 4000.0f * t)
                         + 5000.0f * fl::sinf(2.0f * FL_M_PI * 6000.0f * t);
            if (sample > 32767.0f) sample = 32767.0f;
            if (sample < -32768.0f) sample = -32768.0f;
            data.push_back(static_cast<fl::i16>(sample));
        }
        return AudioSample(data, ts);
    };

    // Warmup
    u32 timestamp = 0;
    for (int i = 0; i < 43; ++i) {
        timestamp += frameMs;
        ctx->setSample(AudioSample(silenceData, timestamp));
        detector.update(ctx);
        detector.fireCallbacks();
    }

    // Kick-snare pattern: kick on even beats, snare on odd beats
    int kickBeats = 0, snareBeats = 0;
    int kickDetected = 0, snareDetected = 0;

    for (int beat = 0; beat < 16; ++beat) {
        bool isKick = (beat % 2 == 0);

        for (int frame = 0; frame < framesPerBeat; ++frame) {
            timestamp += frameMs;

            if (frame == 0) {
                if (isKick) {
                    ctx->setSample(makeBassSync(timestamp, 20000.0f));
                } else {
                    ctx->setSample(makeSnareSignal(timestamp));
                }
            } else {
                ctx->setSample(AudioSample(silenceData, timestamp));
            }

            detector.update(ctx);

            if (frame == 0 && detector.isBeat()) {
                if (isKick) kickDetected++;
                else snareDetected++;
            }

            detector.fireCallbacks();
        }

        if (isKick) kickBeats++;
        else snareBeats++;
    }

    FL_MESSAGE("Kick-snare: kick=" << kickDetected << "/" << kickBeats
               << " snare=" << snareDetected << "/" << snareBeats);

    // Kicks should be detected as beats
    FL_CHECK_GT(kickDetected, kickBeats / 2);
    // Snares (treble-only) should NOT trigger beats
    FL_CHECK_LE(snareDetected, 2);
}

// Test: Decrescendo (gradually quieter beats)
// Tests that the adaptive threshold tracks downward properly
FL_TEST_CASE("BeatSynthetic - decrescendo detects most beats") {
    BeatDetector detector;
    detector.setThreshold(1.3f);

    const u32 frameMs = 23;
    const int framesPerBeat = 22;
    fl::vector<fl::i16> silenceData(512, 0);
    auto ctx = fl::make_shared<AudioContext>(AudioSample(silenceData, 0));
    ctx->setSampleRate(44100);
    ctx->setFFTHistoryDepth(4);

    u32 timestamp = 0;
    for (int i = 0; i < 43; ++i) {
        timestamp += frameMs;
        ctx->setSample(AudioSample(silenceData, timestamp));
        detector.update(ctx);
        detector.fireCallbacks();
    }

    // 16 beats with gradually decreasing amplitude: 25000 → 3000
    int totalDetected = 0;
    int firstHalfDetected = 0;
    int secondHalfDetected = 0;

    for (int beat = 0; beat < 16; ++beat) {
        // Linear amplitude ramp from 25000 to 3000
        float amp = 25000.0f - (22000.0f * static_cast<float>(beat) / 15.0f);

        for (int frame = 0; frame < framesPerBeat; ++frame) {
            timestamp += frameMs;

            if (frame == 0) {
                ctx->setSample(makeBassSync(timestamp, amp));
            } else {
                ctx->setSample(AudioSample(silenceData, timestamp));
            }

            detector.update(ctx);

            if (frame == 0 && detector.isBeat()) {
                totalDetected++;
                if (beat < 8) firstHalfDetected++;
                else secondHalfDetected++;
            }

            detector.fireCallbacks();
        }
    }

    FL_MESSAGE("Decrescendo: total=" << totalDetected << "/16"
               << " first_half=" << firstHalfDetected << "/8"
               << " second_half=" << secondHalfDetected << "/8");

    // The first half (loud) should all be detected
    FL_CHECK_GT(firstHalfDetected, 6);
    // BUG EXPOSURE: The adaptive threshold from the loud beginning may
    // prevent quieter beats from being detected. At least some should work
    // as the moving average decays.
    FL_CHECK_GT(secondHalfDetected, 3);
}

// Test: Full AudioProcessor pipeline with signal conditioning
// The AudioProcessor wraps BeatDetector but adds signal conditioning
// (DC removal, spike filter, noise gate) which may affect beat signals.
FL_TEST_CASE("BeatSynthetic - AudioProcessor pipeline detects periodic beats") {
    AudioProcessor processor;
    // Signal conditioning is enabled by default

    int beatCount = 0;
    float lastBPM = 0;
    processor.onBeat([&beatCount]() { beatCount++; });
    processor.onTempoChange([&lastBPM](float bpm, float) { lastBPM = bpm; });

    const u32 frameMs = 23;
    const int framesPerBeat = 22;
    fl::vector<fl::i16> silenceData(512, 0);

    // Warmup with silence
    u32 timestamp = 0;
    for (int i = 0; i < 43; ++i) {
        timestamp += frameMs;
        processor.update(AudioSample(silenceData, timestamp));
    }

    // 16 periodic bass beats
    for (int beat = 0; beat < 16; ++beat) {
        for (int frame = 0; frame < framesPerBeat; ++frame) {
            timestamp += frameMs;
            if (frame == 0) {
                processor.update(makeBassSync(timestamp, 20000.0f));
            } else {
                processor.update(AudioSample(silenceData, timestamp));
            }
        }
    }

    FL_MESSAGE("AudioProcessor pipeline: " << beatCount << "/16 beats, BPM=" << lastBPM);

    // Through the full pipeline, most beats should still be detected
    FL_CHECK_GT(beatCount, 10);
}

// Test: Low amplitude beats through full pipeline
// Signal conditioning (noise gate) may suppress quiet signals entirely
FL_TEST_CASE("BeatSynthetic - low amplitude beats through pipeline") {
    AudioProcessor processor;

    int beatCount = 0;
    processor.onBeat([&beatCount]() { beatCount++; });

    const u32 frameMs = 23;
    const int framesPerBeat = 22;
    fl::vector<fl::i16> silenceData(512, 0);

    u32 timestamp = 0;
    for (int i = 0; i < 43; ++i) {
        timestamp += frameMs;
        processor.update(AudioSample(silenceData, timestamp));
    }

    // Low amplitude beats (3000 = ~-20 dBFS)
    for (int beat = 0; beat < 16; ++beat) {
        for (int frame = 0; frame < framesPerBeat; ++frame) {
            timestamp += frameMs;
            if (frame == 0) {
                processor.update(makeBassSync(timestamp, 3000.0f));
            } else {
                processor.update(AudioSample(silenceData, timestamp));
            }
        }
    }

    FL_MESSAGE("Low amplitude pipeline: " << beatCount << "/16 beats");

    // Even at low amplitude, clean onsets should trigger some beats
    FL_CHECK_GT(beatCount, 4);
}

// Test: Noisy bass signal with spectral variation between frames
// Real microphone audio has noise that creates baseline spectral flux
FL_TEST_CASE("BeatSynthetic - beats detected in noisy signal") {
    BeatDetector detector;
    detector.setThreshold(1.3f);

    const u32 frameMs = 23;
    const int framesPerBeat = 22;
    auto ctx = fl::make_shared<AudioContext>(AudioSample());
    ctx->setSampleRate(44100);
    ctx->setFFTHistoryDepth(4);

    // Helper: generate noisy sample (deterministic noise seeded by frame index)
    auto makeNoisySample = [](u32 ts, int seed, bool addBeat) -> AudioSample {
        fl::fl_random rng(static_cast<fl::u16>(seed * 1000 + 42));
        fl::vector<fl::i16> data;
        data.reserve(512);
        for (int s = 0; s < 512; ++s) {
            // Background noise at ~2000 amplitude (moderate noise floor)
            float noise = 2000.0f * ((static_cast<float>(rng.random16()) / 32767.5f) - 1.0f);
            float bass = 0.0f;
            if (addBeat) {
                float t = static_cast<float>(s) / 44100.0f;
                bass = 18000.0f * fl::sinf(2.0f * FL_M_PI * 100.0f * t);
            }
            float sample = noise + bass;
            if (sample > 32767.0f) sample = 32767.0f;
            if (sample < -32768.0f) sample = -32768.0f;
            data.push_back(static_cast<fl::i16>(sample));
        }
        return AudioSample(data, ts);
    };

    // Warmup with noise only
    u32 timestamp = 0;
    int frameSeed = 0;
    for (int i = 0; i < 43; ++i) {
        timestamp += frameMs;
        ctx->setSample(makeNoisySample(timestamp, frameSeed++, false));
        detector.update(ctx);
        detector.fireCallbacks();
    }

    // 16 periodic beats embedded in noise
    int beatCount = 0;
    for (int beat = 0; beat < 16; ++beat) {
        for (int frame = 0; frame < framesPerBeat; ++frame) {
            timestamp += frameMs;
            bool isBeatFrame = (frame == 0);
            ctx->setSample(makeNoisySample(timestamp, frameSeed++, isBeatFrame));
            detector.update(ctx);
            if (isBeatFrame && detector.isBeat()) beatCount++;
            detector.fireCallbacks();
        }
    }

    FL_MESSAGE("Noisy signal: " << beatCount << "/16 beats detected");

    // Noise creates spectral flux baseline, making beat detection harder.
    // The adaptive threshold adapts to the noise, but the beat signal
    // must still exceed threshold*noise_mean. With 18000 amplitude bass
    // vs 2000 noise, the SNR is ~19dB which should be sufficient.
    FL_CHECK_GT(beatCount, 8);
}

// Test: Sub-bass beats (40-60 Hz) - CQ with 512 samples at 44100 Hz
// Very low frequencies need long windows. With only ~11.6ms of audio,
// a 50Hz tone has less than one full cycle.
FL_TEST_CASE("BeatSynthetic - sub-bass 50Hz beats detected") {
    BeatDetector detector;
    detector.setThreshold(1.3f);

    const u32 frameMs = 23;
    const int framesPerBeat = 22;
    fl::vector<fl::i16> silenceData(512, 0);
    auto ctx = fl::make_shared<AudioContext>(AudioSample(silenceData, 0));
    ctx->setSampleRate(44100);
    ctx->setFFTHistoryDepth(4);

    u32 timestamp = 0;
    for (int i = 0; i < 43; ++i) {
        timestamp += frameMs;
        ctx->setSample(AudioSample(silenceData, timestamp));
        detector.update(ctx);
        detector.fireCallbacks();
    }

    // Use 50Hz bass (deep sub-bass kick drum)
    int beatCount = 0;
    for (int beat = 0; beat < 16; ++beat) {
        for (int frame = 0; frame < framesPerBeat; ++frame) {
            timestamp += frameMs;
            if (frame == 0) {
                ctx->setSample(makeSample(50.0f, timestamp, 20000.0f));
            } else {
                ctx->setSample(AudioSample(silenceData, timestamp));
            }
            detector.update(ctx);
            if (frame == 0 && detector.isBeat()) beatCount++;
            detector.fireCallbacks();
        }
    }

    FL_MESSAGE("Sub-bass 50Hz: " << beatCount << "/16 beats detected");

    // 50Hz with 512 samples at 44100Hz = only ~0.58 cycles in the window.
    // CQ might struggle to resolve this, but fmin=30Hz should include it.
    FL_CHECK_GT(beatCount, 8);
}

// Test: Realistic kick drum with exponential decay envelope
// Real kick drums have ~5ms attack and ~80ms decay. The bass energy
// persists across multiple frames, spreading the spectral flux onset.
FL_TEST_CASE("BeatSynthetic - realistic kick envelope detected") {
    BeatDetector detector;
    detector.setThreshold(1.3f);

    const u32 frameMs = 23;
    const int framesPerBeat = 22;
    auto ctx = fl::make_shared<AudioContext>(AudioSample());
    ctx->setSampleRate(44100);
    ctx->setFFTHistoryDepth(4);

    // Track continuous time for envelope calculation
    u32 timestamp = 0;
    fl::vector<u32> kickOnsetTimes;

    auto makeKickFrame = [&](u32 ts) -> AudioSample {
        fl::vector<fl::i16> data;
        data.reserve(512);
        for (int s = 0; s < 512; ++s) {
            float t_sample = static_cast<float>(s) / 44100.0f;
            float total = 0.0f;

            // Sum contribution from all active kicks (exponential decay)
            for (fl::size k = 0; k < kickOnsetTimes.size(); ++k) {
                float t_since_onset = static_cast<float>(ts - kickOnsetTimes[k]) / 1000.0f
                                    + t_sample;
                if (t_since_onset < 0.0f || t_since_onset > 0.2f) continue;

                // Exponential decay: 80ms time constant
                float envelope = fl::expf(-t_since_onset / 0.080f);
                // Kick body: 80Hz + 160Hz
                total += 20000.0f * envelope * (
                    0.7f * fl::sinf(2.0f * FL_M_PI * 80.0f * t_since_onset) +
                    0.3f * fl::sinf(2.0f * FL_M_PI * 160.0f * t_since_onset)
                );
            }
            if (total > 32767.0f) total = 32767.0f;
            if (total < -32768.0f) total = -32768.0f;
            data.push_back(static_cast<fl::i16>(total));
        }
        return AudioSample(data, ts);
    };

    // Warmup
    for (int i = 0; i < 43; ++i) {
        timestamp += frameMs;
        fl::vector<fl::i16> silence(512, 0);
        ctx->setSample(AudioSample(silence, timestamp));
        detector.update(ctx);
        detector.fireCallbacks();
    }

    // 16 periodic kicks
    int beatCount = 0;
    for (int beat = 0; beat < 16; ++beat) {
        for (int frame = 0; frame < framesPerBeat; ++frame) {
            timestamp += frameMs;

            if (frame == 0) {
                kickOnsetTimes.push_back(timestamp);
            }

            ctx->setSample(makeKickFrame(timestamp));
            detector.update(ctx);

            // Check within first 3 frames of beat period (onset may be delayed
            // because the envelope spreads across frames)
            if (frame < 3 && detector.isBeat()) {
                beatCount++;
                // Only count once per beat period
                // Skip remaining frames for this beat check
                for (++frame; frame < framesPerBeat; ++frame) {
                    timestamp += frameMs;
                    ctx->setSample(makeKickFrame(timestamp));
                    detector.update(ctx);
                    detector.fireCallbacks();
                }
                break;
            }
            detector.fireCallbacks();
        }
    }

    FL_MESSAGE("Realistic kick: " << beatCount << "/16 detected");

    // Kick drums with decay envelopes spread energy across frames.
    // The spectral flux onset should still be detectable on the first frame.
    FL_CHECK_GT(beatCount, 10);
}

// Test: Low SNR - beat only 2x above background level
// In dense mixes, kick drums may only be slightly above the background
FL_TEST_CASE("BeatSynthetic - low SNR beats (2x above background)") {
    BeatDetector detector;
    detector.setThreshold(1.3f);

    const u32 frameMs = 23;
    const int framesPerBeat = 22;
    auto ctx = fl::make_shared<AudioContext>(AudioSample());
    ctx->setSampleRate(44100);
    ctx->setFFTHistoryDepth(4);

    // Background: sustained bass pad at moderate level
    auto makeLowSNR = [](u32 ts, int seed, bool addKick) -> AudioSample {
        fl::fl_random rng(static_cast<fl::u16>(seed));
        fl::vector<fl::i16> data;
        data.reserve(512);
        for (int s = 0; s < 512; ++s) {
            float t = static_cast<float>(s) / 44100.0f;
            // Sustained bass pad: 80Hz at 5000 amplitude (always present)
            float bg_bass = 5000.0f * fl::sinf(2.0f * FL_M_PI * 80.0f * t);
            // Mid content
            float bg_mid = 3000.0f * fl::sinf(2.0f * FL_M_PI * 600.0f * t);
            // Random phase variation in background to create flux
            float bg_noise = 1000.0f * ((static_cast<float>(rng.random16()) / 32767.5f) - 1.0f);

            float kick = 0.0f;
            if (addKick) {
                // Kick adds only 2x the sustained bass level
                kick = 10000.0f * fl::sinf(2.0f * FL_M_PI * 80.0f * t);
            }
            float sample = bg_bass + bg_mid + bg_noise + kick;
            if (sample > 32767.0f) sample = 32767.0f;
            if (sample < -32768.0f) sample = -32768.0f;
            data.push_back(static_cast<fl::i16>(sample));
        }
        return AudioSample(data, ts);
    };

    // Warmup with background
    u32 timestamp = 0;
    int seed = 0;
    for (int i = 0; i < 43; ++i) {
        timestamp += frameMs;
        ctx->setSample(makeLowSNR(timestamp, seed++, false));
        detector.update(ctx);
        detector.fireCallbacks();
    }

    // 16 beats at low SNR
    int beatCount = 0;
    int falseBeatCount = 0;
    for (int beat = 0; beat < 16; ++beat) {
        for (int frame = 0; frame < framesPerBeat; ++frame) {
            timestamp += frameMs;
            bool isBeatFrame = (frame == 0);
            ctx->setSample(makeLowSNR(timestamp, seed++, isBeatFrame));
            detector.update(ctx);
            if (detector.isBeat()) {
                if (isBeatFrame) beatCount++;
                else falseBeatCount++;
            }
            detector.fireCallbacks();
        }
    }

    FL_MESSAGE("Low SNR: " << beatCount << "/16 detected, "
               << falseBeatCount << " false positives");

    // With only 2x SNR and a sustained bass background creating baseline
    // spectral flux, the detector may struggle. This exposes the threshold
    // sensitivity issue.
    FL_CHECK_GT(beatCount, 6);
}

// Test: Irregular timing (jittered beat intervals)
// Real performers have +-20ms timing variation
FL_TEST_CASE("BeatSynthetic - irregular timing still detects beats") {
    BeatDetector detector;
    detector.setThreshold(1.3f);

    const u32 frameMs = 23;
    fl::vector<fl::i16> silenceData(512, 0);
    auto ctx = fl::make_shared<AudioContext>(AudioSample(silenceData, 0));
    ctx->setSampleRate(44100);
    ctx->setFFTHistoryDepth(4);

    u32 timestamp = 0;
    for (int i = 0; i < 43; ++i) {
        timestamp += frameMs;
        ctx->setSample(AudioSample(silenceData, timestamp));
        detector.update(ctx);
        detector.fireCallbacks();
    }

    // Base interval = 500ms (120 BPM) with +-50ms jitter
    // Jitter pattern: +30, -20, +50, -40, +10, -30, +40, -10, ...
    const int jitterMs[] = {30, -20, 50, -40, 10, -30, 40, -10,
                            25, -15, 45, -35, 5, -25, 35, -5};
    const u32 baseInterval = 500;

    int beatCount = 0;
    u32 nextBeatTime = timestamp + baseInterval;

    for (int beat = 0; beat < 16; ++beat) {
        u32 jitteredBeatTime = nextBeatTime + jitterMs[beat];

        // Run frames until we pass the beat time
        while (timestamp < jitteredBeatTime + frameMs) {
            timestamp += frameMs;
            bool isBeatFrame = (timestamp >= jitteredBeatTime &&
                               timestamp < jitteredBeatTime + frameMs);
            if (isBeatFrame) {
                ctx->setSample(makeBassSync(timestamp, 20000.0f));
            } else {
                ctx->setSample(AudioSample(silenceData, timestamp));
            }
            detector.update(ctx);
            if (isBeatFrame && detector.isBeat()) beatCount++;
            detector.fireCallbacks();
        }

        nextBeatTime += baseInterval;
    }

    FL_MESSAGE("Irregular timing: " << beatCount << "/16 detected");

    // Even with +-50ms jitter (10% of beat interval), onsets should be detected
    FL_CHECK_GT(beatCount, 10);
}

// =============================================================================
// Bass frequency coverage tests: verify bassBins covers kick drum range
// =============================================================================
// With 16 CQ bins from 30-4698 Hz and bassBins = numBins/2 = 8,
// bins 0-7 cover ~30-226 Hz, which includes the full kick drum range.

FL_TEST_CASE("BeatSynthetic - 60Hz bass vs 120Hz bass detection rate") {
    // All common kick drum frequencies should be detected well.
    // 60Hz (bin 2), 120Hz (bin 4), 200Hz (bin 6) all fall within bassBins 0-7.

    auto test_freq = [](float freq) -> int {
        BeatDetector detector;
        detector.setThreshold(1.3f);

        const u32 frameMs = 23;
        const int framesPerBeat = 22;
        fl::vector<fl::i16> silenceData(512, 0);
        auto ctx = fl::make_shared<AudioContext>(AudioSample(silenceData, 0));
        ctx->setSampleRate(44100);
        ctx->setFFTHistoryDepth(4);

        u32 timestamp = 0;
        for (int i = 0; i < 43; ++i) {
            timestamp += frameMs;
            ctx->setSample(AudioSample(silenceData, timestamp));
            detector.update(ctx);
            detector.fireCallbacks();
        }

        int beats = 0;
        for (int beat = 0; beat < 16; ++beat) {
            for (int frame = 0; frame < framesPerBeat; ++frame) {
                timestamp += frameMs;
                if (frame == 0) {
                    ctx->setSample(makeSample(freq, timestamp, 20000.0f));
                } else {
                    ctx->setSample(AudioSample(silenceData, timestamp));
                }
                detector.update(ctx);
                if (frame == 0 && detector.isBeat()) beats++;
                detector.fireCallbacks();
            }
        }
        return beats;
    };

    int beats_60Hz = test_freq(60.0f);
    int beats_120Hz = test_freq(120.0f);
    int beats_200Hz = test_freq(200.0f);

    FL_MESSAGE("Frequency sweep: 60Hz=" << beats_60Hz
               << "/16, 120Hz=" << beats_120Hz
               << "/16, 200Hz=" << beats_200Hz << "/16");

    // All common kick drum frequencies should be detected well
    FL_CHECK_GT(beats_60Hz, 12);   // In bassBins range
    FL_CHECK_GT(beats_120Hz, 12);  // BUG: might fail if bassBins too narrow
    FL_CHECK_GT(beats_200Hz, 12);  // BUG: definitely outside bassBins range
}

// Test: Verify CQ bin frequency boundaries
// This test directly inspects which CQ bins a 120Hz kick lands in
FL_TEST_CASE("BeatSynthetic - CQ bin coverage for kick drum frequencies") {
    // Check what CQ bins cover the typical kick drum range (60-200 Hz)
    fl::vector<fl::i16> silence(512, 0);
    FFTBins bins(16);
    FFT fft;
    fft.run(silence, &bins, FFT_Args(512, 16, 30.0f, 4698.3f, 44100));

    int bin_60Hz = bins.freqToBin(60.0f);
    int bin_100Hz = bins.freqToBin(100.0f);
    int bin_120Hz = bins.freqToBin(120.0f);
    int bin_200Hz = bins.freqToBin(200.0f);

    // bassBins = numBins/2 = 8, so bins 0-7 are used (~30-226 Hz)
    FL_MESSAGE("CQ bin mapping: 60Hz→bin" << bin_60Hz
               << " 100Hz→bin" << bin_100Hz
               << " 120Hz→bin" << bin_120Hz
               << " 200Hz→bin" << bin_200Hz
               << " (bassBins uses 0-7)");

    // All common kick drum frequencies (60-200 Hz) should fall within bassBins
    FL_CHECK_LE(bin_60Hz, 7);
    FL_CHECK_LE(bin_100Hz, 7);
    FL_CHECK_LE(bin_120Hz, 7);
    FL_CHECK_LE(bin_200Hz, 7);
}
