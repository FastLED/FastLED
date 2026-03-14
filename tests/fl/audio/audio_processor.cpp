// Unit tests for AudioProcessor

#include "test.h"
#include "test_helpers.h"
#include "fl/audio/audio.h"
#include "fl/audio/audio_processor.h"
#include "fl/stl/vector.h"
#include "fl/stl/math.h"
#include "fl/stl/math.h"

FL_TEST_FILE(FL_FILEPATH) {

using namespace fl;
using fl::audio::test::makeSample;

namespace {

} // anonymous namespace

FL_TEST_CASE("AudioProcessor - update with valid sample doesn't crash") {
    AudioProcessor processor;
    AudioSample sample = makeSample(440.0f, 1000);
    processor.update(sample);
    // After update, the processor should have a valid context
    FL_CHECK(processor.getContext() != nullptr);
}

FL_TEST_CASE("AudioProcessor - setSampleRate / getSampleRate round-trip") {
    AudioProcessor processor;
    FL_CHECK_EQ(processor.getSampleRate(), 44100);
    processor.setSampleRate(22050);
    FL_CHECK_EQ(processor.getSampleRate(), 22050);
}

FL_TEST_CASE("AudioProcessor - onEnergy callback fires") {
    AudioProcessor processor;
    float lastRMS = -1.0f;
    processor.onEnergy([&lastRMS](float rms) { lastRMS = rms; });

    AudioSample sample = makeSample(440.0f, 1000, 10000.0f);
    processor.update(sample);

    FL_CHECK_GT(lastRMS, 0.0f);
}

FL_TEST_CASE("AudioProcessor - onBass/onMid/onTreble callbacks fire") {
    AudioProcessor processor;
    float lastBass = -1.0f;
    float lastMid = -1.0f;
    float lastTreble = -1.0f;
    processor.onBass([&lastBass](float level) { lastBass = level; });
    processor.onMid([&lastMid](float level) { lastMid = level; });
    processor.onTreble([&lastTreble](float level) { lastTreble = level; });

    // Feed a multi-frequency signal
    fl::vector<fl::i16> data;
    data.reserve(512);
    for (int i = 0; i < 512; ++i) {
        float t = static_cast<float>(i) / 44100.0f;
        float bass = 5000.0f * fl::sinf(2.0f * FL_M_PI * 200.0f * t);
        float mid = 5000.0f * fl::sinf(2.0f * FL_M_PI * 1000.0f * t);
        float treble = 5000.0f * fl::sinf(2.0f * FL_M_PI * 4000.0f * t);
        fl::i32 combined = static_cast<fl::i32>(bass + mid + treble);
        if (combined > 32767) combined = 32767;
        if (combined < -32768) combined = -32768;
        data.push_back(static_cast<fl::i16>(combined));
    }
    AudioSample sample(data, 1000);
    processor.update(sample);

    // The signal contains explicit 200Hz bass + 1000Hz mid + 4000Hz treble energy.
    // All three bands should report positive energy individually.
    FL_CHECK_GT(lastBass, 0.0f);
    FL_CHECK_GT(lastMid, 0.0f);
    FL_CHECK_GT(lastTreble, 0.0f);
}

FL_TEST_CASE("AudioProcessor - signal conditioning enabled by default") {
    AudioProcessor processor;
    // Signal conditioning is enabled by default
    const auto& stats = processor.getSignalConditionerStats();
    FL_CHECK_EQ(stats.samplesProcessed, 0u);

    AudioSample sample = makeSample(440.0f, 1000);
    processor.update(sample);

    FL_CHECK_GT(processor.getSignalConditionerStats().samplesProcessed, 0u);
}

FL_TEST_CASE("AudioProcessor - reset clears all state") {
    AudioProcessor processor;
    float lastRMS = -1.0f;
    processor.onEnergy([&lastRMS](float rms) { lastRMS = rms; });

    AudioSample sample = makeSample(440.0f, 1000);
    processor.update(sample);
    FL_CHECK_GT(lastRMS, 0.0f);

    processor.reset();
    // After reset, stats should be cleared
    FL_CHECK_EQ(processor.getSignalConditionerStats().samplesProcessed, 0u);
}

FL_TEST_CASE("AudioProcessor - lazy detector creation") {
    AudioProcessor processor;
    // Without callbacks registered, just update() should work fine
    AudioSample sample = makeSample(440.0f, 1000);
    processor.update(sample);

    // Now register a callback - should trigger detector creation
    int beatCount = 0;
    processor.onBeat([&beatCount]() { beatCount++; });

    processor.update(sample);
    // After registering onBeat and updating, context should still be valid
    FL_CHECK(processor.getContext() != nullptr);
}

FL_TEST_CASE("FrequencyBands - getBassNorm/getMidNorm/getTrebleNorm exist and return 0-1") {
    AudioProcessor processor;
    // Disable signal conditioning so our test signal passes through cleanly
    processor.setSignalConditioningEnabled(false);
    // Force lazy creation of FrequencyBands detector before feeding data
    processor.getBassLevel();

    // Feed a bass-heavy signal for multiple frames to let filters converge
    for (int i = 0; i < 30; ++i) {
        AudioSample sample = makeSample(200.0f, i * 23, 20000.0f);
        processor.update(sample);
    }

    // Access FrequencyBands through AudioProcessor's polling getters
    float bassLevel = processor.getBassLevel();
    float midLevel = processor.getMidLevel();
    float trebleLevel = processor.getTrebleLevel();

    // All normalized values should be in [0.0, 1.0]
    FL_CHECK_GE(bassLevel, 0.0f);
    FL_CHECK_LE(bassLevel, 1.0f);
    FL_CHECK_GE(midLevel, 0.0f);
    FL_CHECK_LE(midLevel, 1.0f);
    FL_CHECK_GE(trebleLevel, 0.0f);
    FL_CHECK_LE(trebleLevel, 1.0f);

    // Bass should be meaningfully detected with a 200Hz signal
    FL_CHECK_GT(bassLevel, 0.0f);
}

FL_TEST_CASE("FrequencyBands - normalization makes bands self-referential") {
    AudioProcessor processor;
    processor.setSignalConditioningEnabled(false);
    // Force lazy creation of FrequencyBands detector before feeding data
    processor.getBassLevel();

    // Feed a signal with strong bass (200Hz, amplitude 20000) and weak treble (6000Hz, amplitude 2000)
    // With correct dt (~11.6ms), need ~2x more frames for filters to converge.
    for (int i = 0; i < 80; ++i) {
        fl::vector<fl::i16> data;
        data.reserve(512);
        for (int s = 0; s < 512; ++s) {
            float t = static_cast<float>(s) / 44100.0f;
            float bass = 20000.0f * fl::sinf(2.0f * FL_M_PI * 200.0f * t);
            float treble = 2000.0f * fl::sinf(2.0f * FL_M_PI * 6000.0f * t);
            fl::i32 combined = static_cast<fl::i32>(bass + treble);
            if (combined > 32767) combined = 32767;
            if (combined < -32768) combined = -32768;
            data.push_back(static_cast<fl::i16>(combined));
        }
        AudioSample sample(data, i * 12);
        processor.update(sample);
    }

    float bassLevel = processor.getBassLevel();
    float trebleLevel = processor.getTrebleLevel();

    // Both bands should be meaningfully above 0 due to per-band normalization.
    // Without normalization, treble would be near-zero relative to bass.
    FL_CHECK_GT(bassLevel, 0.1f);
    FL_CHECK_GT(trebleLevel, 0.1f);
}

FL_TEST_CASE("FrequencyBands - reset clears normalization filters") {
    AudioProcessor processor;
    processor.setSignalConditioningEnabled(false);
    // Force lazy creation of FrequencyBands detector before feeding data
    processor.getBassLevel();

    // Feed signal for multiple frames
    for (int i = 0; i < 30; ++i) {
        AudioSample sample = makeSample(200.0f, i * 23, 20000.0f);
        processor.update(sample);
    }

    // Verify bass is detected
    FL_CHECK_GT(processor.getBassLevel(), 0.0f);

    // Reset and check levels return to zero
    processor.reset();
    // After reset, re-access triggers lazy creation with fresh state
    // Feed silence to get a zero reading
    fl::vector<fl::i16> silence(512, 0);
    AudioSample silenceSample(silence, 0);
    processor.update(silenceSample);
    float bassAfterReset = processor.getBassLevel();
    // Bass should be at or very near zero after reset + silence
    FL_CHECK_LT(bassAfterReset, 0.01f);
}

FL_TEST_CASE("AudioProcessor - getBassRaw/getMidRaw/getTrebleRaw exist") {
    AudioProcessor processor;
    processor.setSignalConditioningEnabled(false);
    // Force lazy creation of FrequencyBands detector before feeding data
    processor.getBassRaw();

    // Feed a multi-frequency signal
    fl::vector<fl::i16> data;
    data.reserve(512);
    for (int s = 0; s < 512; ++s) {
        float t = static_cast<float>(s) / 44100.0f;
        float bass = 10000.0f * fl::sinf(2.0f * FL_M_PI * 200.0f * t);
        float mid = 10000.0f * fl::sinf(2.0f * FL_M_PI * 1000.0f * t);
        float treble = 5000.0f * fl::sinf(2.0f * FL_M_PI * 6000.0f * t);
        fl::i32 combined = static_cast<fl::i32>(bass + mid + treble);
        if (combined > 32767) combined = 32767;
        if (combined < -32768) combined = -32768;
        data.push_back(static_cast<fl::i16>(combined));
    }
    AudioSample sample(data, 1000);
    processor.update(sample);

    FL_CHECK_GT(processor.getBassRaw(), 0.0f);
    FL_CHECK_GT(processor.getMidRaw(), 0.0f);
    FL_CHECK_GT(processor.getTrebleRaw(), 0.0f);
}

FL_TEST_CASE("AudioProcessor - getBassLevel uses normalized values") {
    AudioProcessor processor;
    processor.setSignalConditioningEnabled(false);
    // Force lazy creation of FrequencyBands detector before feeding data
    processor.getBassLevel();

    // Feed bass-heavy + weak-treble signal for multiple frames
    // With correct dt (~11.6ms), need ~2x more frames for filters to converge.
    for (int i = 0; i < 80; ++i) {
        fl::vector<fl::i16> data;
        data.reserve(512);
        for (int s = 0; s < 512; ++s) {
            float t = static_cast<float>(s) / 44100.0f;
            float bass = 20000.0f * fl::sinf(2.0f * FL_M_PI * 200.0f * t);
            float treble = 2000.0f * fl::sinf(2.0f * FL_M_PI * 6000.0f * t);
            fl::i32 combined = static_cast<fl::i32>(bass + treble);
            if (combined > 32767) combined = 32767;
            if (combined < -32768) combined = -32768;
            data.push_back(static_cast<fl::i16>(combined));
        }
        AudioSample sample(data, i * 12);
        processor.update(sample);
    }

    // Treble should be meaningfully above 0.1 — proves normalization is active.
    // Without normalization, treble would be near-zero relative to bass.
    FL_CHECK_GT(processor.getTrebleLevel(), 0.1f);
}

// ==========================================================================
// Adversarial tests for per-band normalization in FrequencyBands detector
// ==========================================================================

FL_TEST_CASE("FrequencyBands - callback delivers raw values not normalized") {
    // BUG: onBass/onMid/onTreble callbacks fire with raw smoothed values,
    // but getBassLevel() returns normalized values. Users get inconsistent
    // results depending on which API they use.
    AudioProcessor processor;
    processor.setSignalConditioningEnabled(false);

    float callbackBass = -1.0f;
    processor.onBass([&callbackBass](float level) { callbackBass = level; });

    // Feed strong bass signal to build up raw energy
    for (int i = 0; i < 50; ++i) {
        AudioSample sample = makeSample(200.0f, i * 23, 20000.0f);
        processor.update(sample);
    }

    float polledBass = processor.getBassLevel();

    // Both should be positive
    FL_CHECK_GT(callbackBass, 0.0f);
    FL_CHECK_GT(polledBass, 0.0f);

    // Callback value is raw (can be > 1), polled value is normalized [0,1].
    // Document this divergence: they are NOT the same value.
    FL_CHECK_LE(polledBass, 1.0f);
    // Raw callback value is typically much larger than the normalized polled value
    // because it's unnormalized FFT energy.
    // This test documents the API inconsistency.
}

FL_TEST_CASE("FrequencyBands - signal above FFT max leaks via spectral leakage") {
    // The FFT range is [100, 10000] Hz but treble range is [4000, 20000] Hz.
    // A pure 15kHz signal is above FFT max but still present in PCM data.
    // BUG/LIMITATION: The CQ kernel's high-frequency bins have short windows
    // that can alias/leak energy from out-of-range frequencies. This test
    // documents that a 15kHz signal DOES produce non-trivial treble energy
    // even though it's "outside" the FFT range.
    AudioProcessor processor;
    processor.setSignalConditioningEnabled(false);
    processor.getBassLevel(); // Force lazy creation

    // Feed a 15kHz signal (above FFT max of 10kHz)
    for (int i = 0; i < 30; ++i) {
        AudioSample sample = makeSample(15000.0f, i * 23, 20000.0f);
        processor.update(sample);
    }

    float trebleRaw = processor.getTrebleRaw();
    // Spectral leakage means this is NOT zero — documenting the actual behavior.
    // This is a known limitation: the FFT range doesn't act as a hard cutoff.
    FL_CHECK_GT(trebleRaw, 1.0f);

    // Bass should be much lower than treble for a 15kHz signal
    float bassRaw = processor.getBassRaw();
    FL_CHECK_LT(bassRaw, trebleRaw);
}

FL_TEST_CASE("FrequencyBands - normalized values stay in [0,1] under rapid signal changes") {
    // Adversarial: alternate between silence and max-amplitude signal
    // every frame. The normalization filter should never produce values
    // outside [0, 1] despite the whiplash.
    AudioProcessor processor;
    processor.setSignalConditioningEnabled(false);
    processor.getBassLevel(); // Force lazy creation

    for (int i = 0; i < 100; ++i) {
        AudioSample sample;
        if (i % 2 == 0) {
            sample = makeSample(200.0f, i * 23, 30000.0f);
        } else {
            sample = fl::audio::test::makeSilence(i * 23);
        }
        processor.update(sample);

        float bass = processor.getBassLevel();
        float mid = processor.getMidLevel();
        float treble = processor.getTrebleLevel();

        FL_CHECK_GE(bass, 0.0f);
        FL_CHECK_LE(bass, 1.0f);
        FL_CHECK_GE(mid, 0.0f);
        FL_CHECK_LE(mid, 1.0f);
        FL_CHECK_GE(treble, 0.0f);
        FL_CHECK_LE(treble, 1.0f);
    }
}

FL_TEST_CASE("FrequencyBands - first frame after reset normalizes against floor") {
    // After reset, filters start at 0. The first non-zero signal gets
    // normalized against the floor (0.001). With fast attack tau the
    // running max snaps to the input, but the very first frame could
    // produce a spike if the floor is too low relative to the signal.
    AudioProcessor processor;
    processor.setSignalConditioningEnabled(false);
    processor.getBassLevel(); // Force lazy creation

    // Build up some state
    for (int i = 0; i < 20; ++i) {
        AudioSample sample = makeSample(200.0f, i * 23, 20000.0f);
        processor.update(sample);
    }

    // Reset everything
    processor.reset();

    // First frame with a signal — should still be in [0, 1]
    processor.getBassLevel(); // Re-trigger lazy creation
    AudioSample sample = makeSample(200.0f, 0, 20000.0f);
    processor.update(sample);

    float bass = processor.getBassLevel();
    FL_CHECK_GE(bass, 0.0f);
    FL_CHECK_LE(bass, 1.0f);
}

FL_TEST_CASE("FrequencyBands - raw values not clamped to [0,1]") {
    // getBassRaw() should return the actual FFT energy, which can be > 1.0
    // unlike getBassLevel() which is normalized and clamped.
    AudioProcessor processor;
    processor.setSignalConditioningEnabled(false);
    processor.getBassRaw(); // Force lazy creation

    // Feed a very strong bass signal
    for (int i = 0; i < 30; ++i) {
        AudioSample sample = makeSample(200.0f, i * 23, 30000.0f);
        processor.update(sample);
    }

    float bassRaw = processor.getBassRaw();
    float bassNorm = processor.getBassLevel();

    // Raw can be any positive value
    FL_CHECK_GT(bassRaw, 0.0f);
    // Normalized is always in [0, 1]
    FL_CHECK_GE(bassNorm, 0.0f);
    FL_CHECK_LE(bassNorm, 1.0f);
}

FL_TEST_CASE("FrequencyBands - silence produces zero for both raw and normalized") {
    AudioProcessor processor;
    processor.setSignalConditioningEnabled(false);
    processor.getBassLevel(); // Force lazy creation

    // Feed pure silence
    for (int i = 0; i < 30; ++i) {
        AudioSample sample = fl::audio::test::makeSilence(i * 23);
        processor.update(sample);
    }

    FL_CHECK_LT(processor.getBassRaw(), 0.001f);
    FL_CHECK_LT(processor.getMidRaw(), 0.001f);
    FL_CHECK_LT(processor.getTrebleRaw(), 0.001f);
    FL_CHECK_LT(processor.getBassLevel(), 0.001f);
    FL_CHECK_LT(processor.getMidLevel(), 0.001f);
    FL_CHECK_LT(processor.getTrebleLevel(), 0.001f);
}

FL_TEST_CASE("FrequencyBands - signal drop from loud to quiet normalizes correctly") {
    // After establishing a high running max with loud signal, switching to
    // a quiet signal should produce low normalized values (not near 1.0).
    // The running max decays slowly (4s tau) so the quiet signal should
    // read as a fraction of the peak.
    AudioProcessor processor;
    processor.setSignalConditioningEnabled(false);
    processor.getBassLevel(); // Force lazy creation

    // Phase 1: Loud bass for many frames to establish high running max
    for (int i = 0; i < 60; ++i) {
        AudioSample sample = makeSample(200.0f, i * 23, 30000.0f);
        processor.update(sample);
    }
    float loudBass = processor.getBassLevel();
    float loudRaw = processor.getBassRaw();

    // Phase 2: Much quieter bass (1/10th amplitude)
    for (int i = 60; i < 80; ++i) {
        AudioSample sample = makeSample(200.0f, i * 23, 3000.0f);
        processor.update(sample);
    }
    float quietBass = processor.getBassLevel();
    float quietRaw = processor.getBassRaw();

    // Raw value should drop significantly
    FL_CHECK_LT(quietRaw, loudRaw * 0.5f);
    // Normalized value should also drop because running max is still high
    FL_CHECK_LT(quietBass, loudBass);
}

FL_TEST_CASE("FrequencyBands - pure mid-frequency signal has dominant mid band") {
    // A 1kHz signal should produce dominant mid-band energy.
    // Bass and treble should be significantly lower.
    AudioProcessor processor;
    processor.setSignalConditioningEnabled(false);
    processor.getBassLevel(); // Force lazy creation

    for (int i = 0; i < 50; ++i) {
        AudioSample sample = makeSample(1000.0f, i * 23, 20000.0f);
        processor.update(sample);
    }

    float bassRaw = processor.getBassRaw();
    float midRaw = processor.getMidRaw();
    float trebleRaw = processor.getTrebleRaw();

    // Mid should be dominant
    FL_CHECK_GT(midRaw, bassRaw);
    FL_CHECK_GT(midRaw, trebleRaw);
}

FL_TEST_CASE("FrequencyBands - normalization converges for steady signal") {
    // For a constant-amplitude signal, the normalized value should converge
    // to ~1.0 over time as the running max tracks the steady level.
    // With correct dt (~11.6ms for 512 samples at 44100Hz), filters need
    // ~2x more frames to converge than with the old hardcoded 23ms dt.
    AudioProcessor processor;
    processor.setSignalConditioningEnabled(false);
    processor.getBassLevel(); // Force lazy creation

    // Feed steady bass signal for many frames
    for (int i = 0; i < 200; ++i) {
        AudioSample sample = makeSample(200.0f, i * 12, 20000.0f);
        processor.update(sample);
    }

    float bassNorm = processor.getBassLevel();
    // After convergence, normalized value should be near 1.0
    // (the running max should closely track the steady signal level)
    FL_CHECK_GT(bassNorm, 0.8f);
}

FL_TEST_CASE("AudioProcessor - onBeat callback fires with periodic bass") {
    AudioProcessor processor;
    int beatCount = 0;
    processor.onBeat([&beatCount]() { beatCount++; });

    // Feed silence to establish baseline
    fl::vector<fl::i16> silenceData(512, 0);
    for (int i = 0; i < 20; ++i) {
        AudioSample silence(silenceData, i * 23);
        processor.update(silence);
    }

    // Feed periodic bass bursts at ~120 BPM
    fl::u32 timestamp = 500;
    for (int beat = 0; beat < 12; ++beat) {
        // Bass burst
        fl::vector<fl::i16> bassData;
        bassData.reserve(512);
        for (int s = 0; s < 512; ++s) {
            float phase = 2.0f * FL_M_PI * 200.0f * s / 44100.0f;
            bassData.push_back(static_cast<fl::i16>(20000.0f * fl::sinf(phase)));
        }
        AudioSample bassSample(bassData, timestamp);
        processor.update(bassSample);

        // Silence between beats
        for (int frame = 1; frame < 22; ++frame) {
            timestamp += 23;
            AudioSample silence(silenceData, timestamp);
            processor.update(silence);
        }
        timestamp += 23;
    }

    // At least some beats should have been detected
    FL_CHECK_GT(beatCount, 2);
}

// ==========================================================================
// Adversarial tests for computed dt (not hardcoded kFrameDt)
// ==========================================================================

FL_TEST_CASE("FrequencyBands - dt differs between buffer sizes") {
    // Verify that dt is actually computed from pcm size, not hardcoded.
    // Two processors with same sample rate but different buffer sizes:
    //   256 samples → dt = 256/44100 = 5.8ms
    //   512 samples → dt = 512/44100 = 11.6ms
    // Both feed identical steady signals. With the ExponentialSmoother,
    // larger dt → higher alpha → the smoother tracks the input faster.
    // After just a few frames, the 512-sample processor should have a
    // higher raw smoothed value because it converges faster.
    AudioProcessor proc256;
    AudioProcessor proc512;
    proc256.setSignalConditioningEnabled(false);
    proc512.setSignalConditioningEnabled(false);
    proc256.getBassRaw();
    proc512.getBassRaw();

    // Feed just 3 frames so the smoother hasn't fully converged
    for (int i = 0; i < 3; ++i) {
        AudioSample s256 = makeSample(200.0f, i * 6, 20000.0f, 256);
        proc256.update(s256);

        AudioSample s512 = makeSample(200.0f, i * 12, 20000.0f, 512);
        proc512.update(s512);
    }

    float raw256 = proc256.getBassRaw();
    float raw512 = proc512.getBassRaw();

    // Both should be positive
    FL_CHECK_GT(raw256, 0.0f);
    FL_CHECK_GT(raw512, 0.0f);

    // The two raw values should NOT be identical — this proves dt varies
    // with buffer size (if hardcoded, they could still differ due to FFT
    // resolution differences, but this at minimum validates the fix runs).
    FL_CHECK_NE(raw256, raw512);
}

FL_TEST_CASE("FrequencyBands - dt scales with sample rate") {
    // Same buffer size (512), different sample rates.
    // At 22050 Hz: dt = 512/22050 = 23.2ms
    // At 44100 Hz: dt = 512/44100 = 11.6ms
    // Lower sample rate → larger dt per frame → faster filter decay.
    //
    // Strategy: Build up running max, then drop to quiet signal.
    // Larger dt → faster decay → higher normalized value after drop.
    AudioProcessor procHigh;
    AudioProcessor procLow;
    procHigh.setSignalConditioningEnabled(false);
    procLow.setSignalConditioningEnabled(false);
    procHigh.setSampleRate(44100);
    procLow.setSampleRate(22050);
    procHigh.getBassLevel();
    procLow.getBassLevel();

    // Phase 1: Build up running max
    for (int i = 0; i < 100; ++i) {
        AudioSample sHigh = makeSample(200.0f, i * 12, 20000.0f, 512, 44100.0f);
        procHigh.update(sHigh);

        AudioSample sLow = makeSample(200.0f, i * 23, 20000.0f, 512, 22050.0f);
        procLow.update(sLow);
    }

    // Phase 2: Drop to quiet signal
    for (int i = 0; i < 30; ++i) {
        AudioSample sHigh = makeSample(200.0f, (100 + i) * 12, 2000.0f, 512, 44100.0f);
        procHigh.update(sHigh);

        AudioSample sLow = makeSample(200.0f, (100 + i) * 23, 2000.0f, 512, 22050.0f);
        procLow.update(sLow);
    }

    float bassHigh = procHigh.getBassLevel();
    float bassLow = procLow.getBassLevel();

    FL_CHECK_GT(bassHigh, 0.0f);
    FL_CHECK_GT(bassLow, 0.0f);

    // Low sample rate (larger dt) → faster running max decay →
    // quiet signal is larger fraction of decayed max → HIGHER normalized value.
    FL_CHECK_GT(bassLow, bassHigh);
}

FL_TEST_CASE("FrequencyBands - burst mode identical timestamps still converge") {
    // Simulate I2S DMA burst: 10 samples arrive with identical timestamp.
    // Each still represents 11.6ms of audio (512/44100).
    // Normalized bass should converge above 0.5.
    AudioProcessor processor;
    processor.setSignalConditioningEnabled(false);
    processor.getBassLevel();

    const fl::u32 burstTimestamp = 1000;
    for (int i = 0; i < 10; ++i) {
        AudioSample sample = makeSample(200.0f, burstTimestamp, 20000.0f);
        processor.update(sample);
    }

    float bassNorm = processor.getBassLevel();
    // Should have converged meaningfully even with identical timestamps
    FL_CHECK_GT(bassNorm, 0.5f);
}

} // FL_TEST_FILE
