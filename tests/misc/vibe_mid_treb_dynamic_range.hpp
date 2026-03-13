// Repro test for VibeDetector mid/treble stuck at 1.00
// https://github.com/FastLED/FastLED/issues/2193#issuecomment-4019752955
//
// Simulates realistic music patterns where mid/treble energy varies
// meaningfully, then checks whether getMid()/getTreb() have enough
// dynamic range for LED visualization (> 0.3 peak-to-trough).

#include "test.h"
#include "fl/audio/audio.h"
#include "fl/audio/audio_context.h"
#include "fl/audio/detectors/vibe.h"
#include "fl/stl/int.h"
#include "fl/log.h"
#include "fl/math_macros.h"
#include "fl/stl/math.h"
#include "fl/stl/random.h"
#include "fl/stl/shared_ptr.h"
#include "fl/stl/vector.h"

using namespace fl;

namespace {

// Clamp and cast a float to i16
i16 clampI16(float v) {
    if (v > 32767.0f) v = 32767.0f;
    if (v < -32768.0f) v = -32768.0f;
    return static_cast<i16>(v);
}

// Generate a multi-frequency AudioSample (bass + mid + treble mixed)
AudioSample makeMix(float bassAmp, float midAmp, float trebAmp,
                    u32 timestamp, int count = 512,
                    float sampleRate = 44100.0f) {
    vector<i16> data(count, 0);
    // Bass: 200 Hz (well inside 20-3688 Hz band)
    // Mid: 5000 Hz (well inside 3688-7356 Hz band)
    // Treble: 9000 Hz (well inside 7356-11025 Hz band)
    const float freqs[3] = {200.0f, 5000.0f, 9000.0f};
    const float amps[3] = {bassAmp, midAmp, trebAmp};

    for (int band = 0; band < 3; ++band) {
        for (int i = 0; i < count; ++i) {
            float phase = 2.0f * FL_M_PI * freqs[band] * i / sampleRate;
            float sample = amps[band] * fl::sinf(phase);
            data[i] = clampI16(static_cast<float>(data[i]) + sample);
        }
    }
    return AudioSample(data, timestamp);
}

// Generate a multi-frequency AudioSample with broadband noise floor.
// Models a MEMS mic (INMP441): self-noise adds continuous energy to all
// FFT bins. noiseAmp is the peak amplitude of the white noise.
// Uses deterministic PRNG seeded from timestamp for reproducibility.
AudioSample makeMixWithNoise(float bassAmp, float midAmp, float trebAmp,
                             float noiseAmp, u32 timestamp,
                             int count = 512,
                             float sampleRate = 44100.0f) {
    vector<i16> data(count, 0);
    const float freqs[3] = {200.0f, 5000.0f, 9000.0f};
    const float amps[3] = {bassAmp, midAmp, trebAmp};

    // Add tonal content
    for (int band = 0; band < 3; ++band) {
        for (int i = 0; i < count; ++i) {
            float phase = 2.0f * FL_M_PI * freqs[band] * i / sampleRate;
            float sample = amps[band] * fl::sinf(phase);
            data[i] = clampI16(static_cast<float>(data[i]) + sample);
        }
    }

    // Add broadband white noise (deterministic per-timestamp)
    fl_random rng(static_cast<u16>(timestamp * 7 + 31));
    for (int i = 0; i < count; ++i) {
        // Map u16 [0, 65535] to float [-1, 1]
        float noise = (static_cast<float>(rng.random16()) / 32767.5f) - 1.0f;
        data[i] = clampI16(static_cast<float>(data[i]) + noiseAmp * noise);
    }
    return AudioSample(data, timestamp);
}

// Generate a sample with spectral leakage: a bass tone near the mid band
// boundary (3500 Hz — just below the 3688 Hz boundary) plus actual mid
// content plus noise. The 512-sample FFT window causes the 3500 Hz tone
// to leak significant energy into the mid bin.
AudioSample makeMixWithLeakage(float bassAmp, float leakageBassAmp,
                               float midAmp, float trebAmp,
                               float noiseAmp, u32 timestamp,
                               int count = 512,
                               float sampleRate = 44100.0f) {
    vector<i16> data(count, 0);

    // Normal bass (200 Hz)
    for (int i = 0; i < count; ++i) {
        float phase = 2.0f * FL_M_PI * 200.0f * i / sampleRate;
        data[i] = clampI16(static_cast<float>(data[i]) +
                           bassAmp * fl::sinf(phase));
    }

    // Leakage bass: 3500 Hz — nominally in bass band (20-3688 Hz) but
    // FFT spectral leakage bleeds energy into the mid bin (3688-7356 Hz).
    // A 512-sample Hann window at 44100 Hz has main-lobe width ≈ 172 Hz,
    // so 3500 Hz is only 188 Hz from the 3688 Hz boundary — well within
    // the leakage zone. With rectangular window (which kiss_fft uses),
    // sidelobes are even worse.
    for (int i = 0; i < count; ++i) {
        float phase = 2.0f * FL_M_PI * 3500.0f * i / sampleRate;
        data[i] = clampI16(static_cast<float>(data[i]) +
                           leakageBassAmp * fl::sinf(phase));
    }

    // Actual mid content (5000 Hz)
    for (int i = 0; i < count; ++i) {
        float phase = 2.0f * FL_M_PI * 5000.0f * i / sampleRate;
        data[i] = clampI16(static_cast<float>(data[i]) +
                           midAmp * fl::sinf(phase));
    }

    // Treble (9000 Hz)
    for (int i = 0; i < count; ++i) {
        float phase = 2.0f * FL_M_PI * 9000.0f * i / sampleRate;
        data[i] = clampI16(static_cast<float>(data[i]) +
                           trebAmp * fl::sinf(phase));
    }

    // Broadband noise
    fl_random rng(static_cast<u16>(timestamp * 7 + 31));
    for (int i = 0; i < count; ++i) {
        float noise = (static_cast<float>(rng.random16()) / 32767.5f) - 1.0f;
        data[i] = clampI16(static_cast<float>(data[i]) + noiseAmp * noise);
    }
    return AudioSample(data, timestamp);
}

} // namespace

// Test 1: Alternating loud/quiet mid signal should produce visible dynamic range
//
// This simulates a musical passage where mid energy alternates between
// loud (vocals, harmonics) and quiet (gaps between phrases).
// On real hardware, getMid() stays at 1.00 regardless.
FL_TEST_CASE("VibeDetector mid dynamic range with alternating signal") {
    VibeDetector detector;

    // Phase 1: Establish baseline with moderate mid energy (100 frames)
    for (int i = 0; i < 100; ++i) {
        auto sample = makeMix(5000.0f, 8000.0f, 3000.0f, i * 12);
        auto ctx = make_shared<AudioContext>(sample);
        ctx->setSampleRate(44100);
        detector.update(ctx);
    }

    // Phase 2: Alternate between loud mid and quiet mid, track range
    float midMin = 999.0f;
    float midMax = -999.0f;

    for (int cycle = 0; cycle < 10; ++cycle) {
        // 5 frames of loud mid
        for (int i = 0; i < 5; ++i) {
            auto sample = makeMix(5000.0f, 25000.0f, 3000.0f,
                                  (100 + cycle * 10 + i) * 12);
            auto ctx = make_shared<AudioContext>(sample);
            ctx->setSampleRate(44100);
            detector.update(ctx);

            float mid = detector.getMid();
            if (mid < midMin) midMin = mid;
            if (mid > midMax) midMax = mid;
        }

        // 5 frames of quiet mid
        for (int i = 0; i < 5; ++i) {
            auto sample = makeMix(5000.0f, 1000.0f, 3000.0f,
                                  (105 + cycle * 10 + i) * 12);
            auto ctx = make_shared<AudioContext>(sample);
            ctx->setSampleRate(44100);
            detector.update(ctx);

            float mid = detector.getMid();
            if (mid < midMin) midMin = mid;
            if (mid > midMax) midMax = mid;
        }
    }

    float midRange = midMax - midMin;
    FASTLED_WARN("Mid dynamic range: min=" << midMin << " max=" << midMax
                 << " range=" << midRange);

    // For LED visualization, we need at least 0.3 dynamic range.
    // The bug report shows mid stuck at exactly 1.00, meaning range ≈ 0.0.
    // A threshold of 0.3 is the minimum for any visible LED response.
    FL_CHECK_GT(midRange, 0.3f);
}

// Test 2: Same test for treble
FL_TEST_CASE("VibeDetector treble dynamic range with alternating signal") {
    VibeDetector detector;

    // Phase 1: Establish baseline (100 frames)
    for (int i = 0; i < 100; ++i) {
        auto sample = makeMix(5000.0f, 5000.0f, 5000.0f, i * 12);
        auto ctx = make_shared<AudioContext>(sample);
        ctx->setSampleRate(44100);
        detector.update(ctx);
    }

    // Phase 2: Alternate loud/quiet treble
    float trebMin = 999.0f;
    float trebMax = -999.0f;

    for (int cycle = 0; cycle < 10; ++cycle) {
        // 5 frames loud treble (cymbals, sibilance)
        for (int i = 0; i < 5; ++i) {
            auto sample = makeMix(5000.0f, 5000.0f, 25000.0f,
                                  (100 + cycle * 10 + i) * 12);
            auto ctx = make_shared<AudioContext>(sample);
            ctx->setSampleRate(44100);
            detector.update(ctx);

            float treb = detector.getTreb();
            if (treb < trebMin) trebMin = treb;
            if (treb > trebMax) trebMax = treb;
        }

        // 5 frames quiet treble
        for (int i = 0; i < 5; ++i) {
            auto sample = makeMix(5000.0f, 5000.0f, 1000.0f,
                                  (105 + cycle * 10 + i) * 12);
            auto ctx = make_shared<AudioContext>(sample);
            ctx->setSampleRate(44100);
            detector.update(ctx);

            float treb = detector.getTreb();
            if (treb < trebMin) trebMin = treb;
            if (treb > trebMax) trebMax = treb;
        }
    }

    float trebRange = trebMax - trebMin;
    FASTLED_WARN("Treble dynamic range: min=" << trebMin << " max=" << trebMax
                 << " range=" << trebRange);

    FL_CHECK_GT(trebRange, 0.3f);
}

// Test 3: Realistic music pattern — gradual mid/treble variation
// Real music doesn't have 25x amplitude swings between frames. Instead,
// mid/treble energy varies by ~2-3x over several frames. This is the
// scenario where the bug manifests: the EMA tracks the gradual changes
// too closely, so mImm/mLongAvg stays near 1.0.
FL_TEST_CASE("VibeDetector mid dynamic range with gradual variation (realistic)") {
    VibeDetector detector;

    // Phase 1: Establish baseline with steady mid energy (200 frames ≈ 6.7s)
    for (int i = 0; i < 200; ++i) {
        auto sample = makeMix(5000.0f, 8000.0f, 3000.0f, i * 12);
        auto ctx = make_shared<AudioContext>(sample);
        ctx->setSampleRate(44100);
        detector.update(ctx);
    }

    // Phase 2: Gradual mid variation — amplitude varies between 4000 and 12000
    // This is a realistic 3x range (6 dB) that real music exhibits
    float midMin = 999.0f;
    float midMax = -999.0f;

    for (int frame = 0; frame < 200; ++frame) {
        // Slowly vary mid amplitude using a sine envelope (period ~60 frames)
        float t = static_cast<float>(frame) / 60.0f;
        float envelope = 0.5f + 0.5f * fl::sinf(2.0f * FL_M_PI * t);
        float midAmp = 4000.0f + 8000.0f * envelope; // 4000 to 12000

        auto sample = makeMix(5000.0f, midAmp, 3000.0f, (200 + frame) * 12);
        auto ctx = make_shared<AudioContext>(sample);
        ctx->setSampleRate(44100);
        detector.update(ctx);

        float mid = detector.getMid();
        if (mid < midMin) midMin = mid;
        if (mid > midMax) midMax = mid;
    }

    float midRange = midMax - midMin;
    FASTLED_WARN("Realistic mid dynamic range: min=" << midMin << " max="
                 << midMax << " range=" << midRange);

    // With gradual 3x variation, we need at least 0.3 dynamic range
    // for any visible LED response. The bug report shows 0.0 range.
    FL_CHECK_GT(midRange, 0.3f);
}

// Test 4: Most realistic scenario — continuous mid signal with brief gaps
// This models vocals: sustained energy with short pauses between phrases
FL_TEST_CASE("VibeDetector mid with sustained signal and brief gaps") {
    VibeDetector detector;

    // Phase 1: 200 frames of steady mid (long warm-up so mLongAvg stabilizes)
    for (int i = 0; i < 200; ++i) {
        auto sample = makeMix(5000.0f, 10000.0f, 3000.0f, i * 12);
        auto ctx = make_shared<AudioContext>(sample);
        ctx->setSampleRate(44100);
        detector.update(ctx);
    }

    // Phase 2: 30 frames sustained, 3 frames quiet, repeat
    // This is realistic vocal phrasing: ~1 second of singing, ~100ms gap
    float midMin = 999.0f;
    float midMax = -999.0f;

    for (int cycle = 0; cycle < 10; ++cycle) {
        // 30 frames of mid energy
        for (int i = 0; i < 30; ++i) {
            auto sample = makeMix(5000.0f, 10000.0f, 3000.0f,
                                  (200 + cycle * 33 + i) * 12);
            auto ctx = make_shared<AudioContext>(sample);
            ctx->setSampleRate(44100);
            detector.update(ctx);

            float mid = detector.getMid();
            if (mid < midMin) midMin = mid;
            if (mid > midMax) midMax = mid;
        }

        // 3 frames of low mid (brief gap between phrases)
        for (int i = 0; i < 3; ++i) {
            auto sample = makeMix(5000.0f, 500.0f, 3000.0f,
                                  (230 + cycle * 33 + i) * 12);
            auto ctx = make_shared<AudioContext>(sample);
            ctx->setSampleRate(44100);
            detector.update(ctx);

            float mid = detector.getMid();
            if (mid < midMin) midMin = mid;
            if (mid > midMax) midMax = mid;
        }
    }

    float midRange = midMax - midMin;
    FASTLED_WARN("Sustained mid with gaps: min=" << midMin << " max=" << midMax
                 << " range=" << midRange);

    // Even with brief gaps in otherwise sustained signal,
    // we need some dynamic range for visualization
    FL_CHECK_GT(midRange, 0.3f);
}

// Test 5: Bass should have good dynamic range (control - this should pass)
FL_TEST_CASE("VibeDetector bass dynamic range with alternating signal (control)") {
    VibeDetector detector;

    // Phase 1: Establish baseline (100 frames)
    for (int i = 0; i < 100; ++i) {
        auto sample = makeMix(8000.0f, 5000.0f, 3000.0f, i * 12);
        auto ctx = make_shared<AudioContext>(sample);
        ctx->setSampleRate(44100);
        detector.update(ctx);
    }

    // Phase 2: Alternate loud/quiet bass (simulates kick drum pattern)
    float bassMin = 999.0f;
    float bassMax = -999.0f;

    for (int cycle = 0; cycle < 10; ++cycle) {
        // 5 frames loud bass (kick)
        for (int i = 0; i < 5; ++i) {
            auto sample = makeMix(25000.0f, 5000.0f, 3000.0f,
                                  (100 + cycle * 10 + i) * 12);
            auto ctx = make_shared<AudioContext>(sample);
            ctx->setSampleRate(44100);
            detector.update(ctx);

            float bass = detector.getBass();
            if (bass < bassMin) bassMin = bass;
            if (bass > bassMax) bassMax = bass;
        }

        // 5 frames quiet bass
        for (int i = 0; i < 5; ++i) {
            auto sample = makeMix(1000.0f, 5000.0f, 3000.0f,
                                  (105 + cycle * 10 + i) * 12);
            auto ctx = make_shared<AudioContext>(sample);
            ctx->setSampleRate(44100);
            detector.update(ctx);

            float bass = detector.getBass();
            if (bass < bassMin) bassMin = bass;
            if (bass > bassMax) bassMax = bass;
        }
    }

    float bassRange = bassMax - bassMin;
    FASTLED_WARN("Bass dynamic range: min=" << bassMin << " max=" << bassMax
                 << " range=" << bassRange);

    // Bass should have good dynamic range (this is the control test)
    FL_CHECK_GT(bassRange, 0.3f);
}

// ============================================================================
// MEMS mic simulation tests: noise floor + spectral leakage
// ============================================================================

// Test 6: Mid dynamic range with MEMS noise floor
// INMP441 self-noise adds ~150 amplitude broadband noise to every sample.
// This means mid/treble bins are never zero — they sit at the noise floor.
// The EMA tracks noise floor + signal, compressing the relative range.
FL_TEST_CASE("VibeDetector mid dynamic range with MEMS noise floor") {
    VibeDetector detector;
    const float noiseAmp = 150.0f; // Realistic INMP441 noise floor

    // Phase 1: Establish baseline (200 frames with noise)
    for (int i = 0; i < 200; ++i) {
        auto sample = makeMixWithNoise(5000.0f, 8000.0f, 3000.0f, noiseAmp,
                                       i * 12);
        auto ctx = make_shared<AudioContext>(sample);
        ctx->setSampleRate(44100);
        detector.update(ctx);
    }

    // Phase 2: Gradual mid variation (4000-12000) with noise floor
    float midMin = 999.0f;
    float midMax = -999.0f;

    for (int frame = 0; frame < 200; ++frame) {
        float t = static_cast<float>(frame) / 60.0f;
        float envelope = 0.5f + 0.5f * fl::sinf(2.0f * FL_M_PI * t);
        float midAmp = 4000.0f + 8000.0f * envelope;

        auto sample = makeMixWithNoise(5000.0f, midAmp, 3000.0f, noiseAmp,
                                        (200 + frame) * 12);
        auto ctx = make_shared<AudioContext>(sample);
        ctx->setSampleRate(44100);
        detector.update(ctx);

        float mid = detector.getMid();
        if (mid < midMin) midMin = mid;
        if (mid > midMax) midMax = mid;
    }

    float midRange = midMax - midMin;
    FASTLED_WARN("Mid with MEMS noise: min=" << midMin << " max=" << midMax
                 << " range=" << midRange);

    FL_CHECK_GT(midRange, 0.3f);
}

// Test 7: Treble dynamic range with MEMS noise floor
// Treble band (7356-11025 Hz) has the most noise floor contamination
// relative to signal because treble content is typically lower amplitude.
FL_TEST_CASE("VibeDetector treble dynamic range with MEMS noise floor") {
    VibeDetector detector;
    const float noiseAmp = 150.0f;

    // Phase 1: Establish baseline
    for (int i = 0; i < 200; ++i) {
        auto sample = makeMixWithNoise(5000.0f, 5000.0f, 3000.0f, noiseAmp,
                                       i * 12);
        auto ctx = make_shared<AudioContext>(sample);
        ctx->setSampleRate(44100);
        detector.update(ctx);
    }

    // Phase 2: Gradual treble variation (1500-4500, 3x range) with noise
    // Treble amplitudes are typically lower than bass/mid in real music
    float trebMin = 999.0f;
    float trebMax = -999.0f;

    for (int frame = 0; frame < 200; ++frame) {
        float t = static_cast<float>(frame) / 60.0f;
        float envelope = 0.5f + 0.5f * fl::sinf(2.0f * FL_M_PI * t);
        float trebAmp = 1500.0f + 3000.0f * envelope;

        auto sample = makeMixWithNoise(5000.0f, 5000.0f, trebAmp, noiseAmp,
                                        (200 + frame) * 12);
        auto ctx = make_shared<AudioContext>(sample);
        ctx->setSampleRate(44100);
        detector.update(ctx);

        float treb = detector.getTreb();
        if (treb < trebMin) trebMin = treb;
        if (treb > trebMax) trebMax = treb;
    }

    float trebRange = trebMax - trebMin;
    FASTLED_WARN("Treble with MEMS noise: min=" << trebMin << " max="
                 << trebMax << " range=" << trebRange);

    FL_CHECK_GT(trebRange, 0.3f);
}

// Test 8: Mid dynamic range with spectral leakage from bass
// A bass tone at 3500 Hz (just below the 3688 Hz mid boundary) leaks
// energy into the mid bin. When bass amplitude varies (kick drum pattern),
// the mid bin sees variation from leakage — but this is "fake" mid energy.
// The test checks whether actual mid content changes are still visible
// above the leakage-contaminated baseline.
FL_TEST_CASE("VibeDetector mid dynamic range with spectral leakage") {
    VibeDetector detector;
    const float noiseAmp = 150.0f;
    const float leakageBass = 8000.0f; // Strong bass tone near mid boundary

    // Phase 1: Establish baseline with leakage bass + moderate mid
    for (int i = 0; i < 200; ++i) {
        auto sample = makeMixWithLeakage(
            5000.0f,      // normal bass (200 Hz)
            leakageBass,  // leakage bass (3500 Hz, bleeds into mid)
            6000.0f,      // actual mid content (5000 Hz)
            3000.0f,      // treble
            noiseAmp,
            i * 12);
        auto ctx = make_shared<AudioContext>(sample);
        ctx->setSampleRate(44100);
        detector.update(ctx);
    }

    // Phase 2: Vary actual mid content while leakage bass stays constant.
    // Mid bin energy = leakage(~constant) + actual_mid(varying) + noise.
    // If leakage dominates, the mid variation is a small fraction of total,
    // and mImm/mLongAvg stays near 1.0.
    float midMin = 999.0f;
    float midMax = -999.0f;

    for (int frame = 0; frame < 200; ++frame) {
        float t = static_cast<float>(frame) / 60.0f;
        float envelope = 0.5f + 0.5f * fl::sinf(2.0f * FL_M_PI * t);
        // Mid varies 2000-10000 (5x range) — needs to overcome leakage floor
        float midAmp = 2000.0f + 8000.0f * envelope;

        auto sample = makeMixWithLeakage(
            5000.0f, leakageBass, midAmp, 3000.0f, noiseAmp,
            (200 + frame) * 12);
        auto ctx = make_shared<AudioContext>(sample);
        ctx->setSampleRate(44100);
        detector.update(ctx);

        float mid = detector.getMid();
        if (mid < midMin) midMin = mid;
        if (mid > midMax) midMax = mid;
    }

    float midRange = midMax - midMin;
    FASTLED_WARN("Mid with spectral leakage: min=" << midMin << " max="
                 << midMax << " range=" << midRange);

    FL_CHECK_GT(midRange, 0.3f);
}

// Test 9: Worst case — high noise + heavy leakage + gradual treble variation
// This is the scenario most likely to reproduce the "stuck at 1.00" bug.
// Treble has the weakest signal, highest relative noise contamination,
// and receives leakage from both bass (3500 Hz sidelobes extend far)
// and mid (7000 Hz tone near the 7356 Hz treble boundary).
FL_TEST_CASE("VibeDetector treble worst case: noise + leakage + gradual") {
    VibeDetector detector;
    const float noiseAmp = 200.0f; // Slightly elevated noise (noisy environment)

    // Phase 1: Long warm-up with leakage-contaminated signal
    for (int i = 0; i < 300; ++i) {
        vector<i16> data(512, 0);

        // Bass (200 Hz) — constant
        for (int s = 0; s < 512; ++s) {
            float phase = 2.0f * FL_M_PI * 200.0f * s / 44100.0f;
            data[s] = clampI16(static_cast<float>(data[s]) +
                               6000.0f * fl::sinf(phase));
        }
        // Mid-range tone (5000 Hz) — constant
        for (int s = 0; s < 512; ++s) {
            float phase = 2.0f * FL_M_PI * 5000.0f * s / 44100.0f;
            data[s] = clampI16(static_cast<float>(data[s]) +
                               5000.0f * fl::sinf(phase));
        }
        // Leakage tone near treble boundary (7000 Hz — 356 Hz below 7356 Hz)
        for (int s = 0; s < 512; ++s) {
            float phase = 2.0f * FL_M_PI * 7000.0f * s / 44100.0f;
            data[s] = clampI16(static_cast<float>(data[s]) +
                               6000.0f * fl::sinf(phase));
        }
        // Moderate treble (9000 Hz)
        for (int s = 0; s < 512; ++s) {
            float phase = 2.0f * FL_M_PI * 9000.0f * s / 44100.0f;
            data[s] = clampI16(static_cast<float>(data[s]) +
                               3000.0f * fl::sinf(phase));
        }
        // Noise
        fl_random rng(static_cast<u16>(i * 7 + 31));
        for (int s = 0; s < 512; ++s) {
            float noise = (static_cast<float>(rng.random16()) / 32767.5f) - 1.0f;
            data[s] = clampI16(static_cast<float>(data[s]) + noiseAmp * noise);
        }

        auto sample = AudioSample(data, i * 12);
        auto ctx = make_shared<AudioContext>(sample);
        ctx->setSampleRate(44100);
        detector.update(ctx);
    }

    // Phase 2: Gradually vary treble content (1000-5000 amplitude, 5x range)
    // while leakage + noise create a high baseline in the treble bin.
    float trebMin = 999.0f;
    float trebMax = -999.0f;

    for (int frame = 0; frame < 300; ++frame) {
        float t = static_cast<float>(frame) / 80.0f;
        float envelope = 0.5f + 0.5f * fl::sinf(2.0f * FL_M_PI * t);
        float trebAmp = 1000.0f + 4000.0f * envelope;

        vector<i16> data(512, 0);

        // Bass
        for (int s = 0; s < 512; ++s) {
            float phase = 2.0f * FL_M_PI * 200.0f * s / 44100.0f;
            data[s] = clampI16(static_cast<float>(data[s]) +
                               6000.0f * fl::sinf(phase));
        }
        // Mid
        for (int s = 0; s < 512; ++s) {
            float phase = 2.0f * FL_M_PI * 5000.0f * s / 44100.0f;
            data[s] = clampI16(static_cast<float>(data[s]) +
                               5000.0f * fl::sinf(phase));
        }
        // Leakage tone (7000 Hz — constant, bleeds into treble)
        for (int s = 0; s < 512; ++s) {
            float phase = 2.0f * FL_M_PI * 7000.0f * s / 44100.0f;
            data[s] = clampI16(static_cast<float>(data[s]) +
                               6000.0f * fl::sinf(phase));
        }
        // Actual treble content (varying)
        for (int s = 0; s < 512; ++s) {
            float phase = 2.0f * FL_M_PI * 9000.0f * s / 44100.0f;
            data[s] = clampI16(static_cast<float>(data[s]) +
                               trebAmp * fl::sinf(phase));
        }
        // Noise
        fl_random rng(static_cast<u16>((300 + frame) * 7 + 31));
        for (int s = 0; s < 512; ++s) {
            float noise = (static_cast<float>(rng.random16()) / 32767.5f) - 1.0f;
            data[s] = clampI16(static_cast<float>(data[s]) + noiseAmp * noise);
        }

        auto sample = AudioSample(data, (300 + frame) * 12);
        auto ctx = make_shared<AudioContext>(sample);
        ctx->setSampleRate(44100);
        detector.update(ctx);

        float treb = detector.getTreb();
        if (treb < trebMin) trebMin = treb;
        if (treb > trebMax) trebMax = treb;
    }

    float trebRange = trebMax - trebMin;
    FASTLED_WARN("Treble worst case (noise+leakage+gradual): min=" << trebMin
                 << " max=" << trebMax << " range=" << trebRange);

    // This is the hardest test. Leakage + noise create a high baseline
    // in the treble bin. The 1000-5000 amplitude variation in actual
    // treble content must be visible above this contaminated floor.
    FL_CHECK_GT(trebRange, 0.3f);
}
