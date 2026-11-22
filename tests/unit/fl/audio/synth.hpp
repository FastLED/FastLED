/// @file synth.cpp
/// @brief Tests for bandlimited audio synthesizer oscillator

#include "fl/audio/synth.h"
#include "fl/math/math.h"

using namespace fl;

namespace {

// Helper to check if samples are within valid range [-1.0, 1.0] with some tolerance
static bool samplesInRange(const float* samples, int32_t count, float tolerance = 1.5f) {
    for (int32_t i = 0; i < count; ++i) {
        if (samples[i] < -tolerance || samples[i] > tolerance) {
            return false;
        }
    }
    return true;
}

// Helper to check if waveform has significant variation (not all zeros or constant)
static bool hasVariation(const float* samples, int32_t count) {
    if (count < 2) return false;

    float minVal = samples[0];
    float maxVal = samples[0];
    for (int32_t i = 1; i < count; ++i) {
        if (samples[i] < minVal) minVal = samples[i];
        if (samples[i] > maxVal) maxVal = samples[i];
    }

    // Waveform should have at least some variation
    return (maxVal - minVal) > 0.1f;
}

}  // anonymous namespace

FL_TEST_CASE("Synth - basic initialization and generation") {
    auto engine = audio::ISynthEngine::create(32, 16);
    FL_CHECK_TRUE(engine->isValid());

    auto osc = audio::ISynthOscillator::create(engine, audio::SynthShape::Sawtooth);
    FL_CHECK_TRUE(osc != nullptr);

    // Generate some samples
    float samples[256];
    float freq = 440.0f / 44100.0f;  // 440 Hz at 44.1 kHz
    osc->generateSamples(samples, 256, freq);

    // Verify samples are in reasonable range and have variation
    FL_CHECK_TRUE(samplesInRange(samples, 256));
    FL_CHECK_TRUE(hasVariation(samples, 256));
}

FL_TEST_CASE("Synth - waveform shapes") {
    auto engine = audio::ISynthEngine::create();

    float samples[512];
    float freq = 100.0f / 44100.0f;  // Low frequency for clearer waveform

    // Test all predefined shapes generate valid output
    auto saw = audio::ISynthOscillator::create(engine, audio::SynthShape::Sawtooth);
    FL_CHECK_TRUE(saw != nullptr);
    saw->generateSamples(samples, 512, freq);
    FL_CHECK_TRUE(samplesInRange(samples, 512));
    FL_CHECK_TRUE(hasVariation(samples, 512));

    auto square = audio::ISynthOscillator::create(engine, audio::SynthShape::Square);
    FL_CHECK_TRUE(square != nullptr);
    square->generateSamples(samples, 512, freq);
    FL_CHECK_TRUE(samplesInRange(samples, 512));
    FL_CHECK_TRUE(hasVariation(samples, 512));

    auto triangle = audio::ISynthOscillator::create(engine, audio::SynthShape::Triangle);
    FL_CHECK_TRUE(triangle != nullptr);
    triangle->generateSamples(samples, 512, freq);
    FL_CHECK_TRUE(samplesInRange(samples, 512));
    FL_CHECK_TRUE(hasVariation(samples, 512));
}

FL_TEST_CASE("Synth - custom parameters") {
    auto engine = audio::ISynthEngine::create();

    // Create with custom parameters
    audio::SynthParams params(1, 0.3f, 0.5f, 0.1f);
    auto osc = audio::ISynthOscillator::create(engine, params);
    FL_CHECK_TRUE(osc != nullptr);

    // Verify params are stored correctly
    audio::SynthParams retrieved = osc->getParams();
    FL_CHECK_EQ(retrieved.reflect, 1);
    FL_CHECK_EQ(retrieved.peakTime, 0.3f);
    FL_CHECK_EQ(retrieved.halfHeight, 0.5f);
    FL_CHECK_EQ(retrieved.zeroWait, 0.1f);

    // Generate samples
    float samples[256];
    osc->generateSamples(samples, 256, 0.01f);
    FL_CHECK_TRUE(samplesInRange(samples, 256));
}

FL_TEST_CASE("Synth - shape change at runtime") {
    auto engine = audio::ISynthEngine::create();

    auto osc = audio::ISynthOscillator::create(engine, audio::SynthShape::Sawtooth);
    FL_CHECK_TRUE(osc != nullptr);

    float samples[256];
    float freq = 0.01f;

    // Generate with sawtooth
    osc->generateSamples(samples, 256, freq);
    FL_CHECK_TRUE(hasVariation(samples, 256));

    // Change to square and generate more
    osc->setShape(audio::SynthShape::Square);
    osc->generateSamples(samples, 256, freq);
    FL_CHECK_TRUE(hasVariation(samples, 256));

    // Change to triangle and generate more
    osc->setShape(audio::SynthShape::Triangle);
    osc->generateSamples(samples, 256, freq);
    FL_CHECK_TRUE(hasVariation(samples, 256));
}

FL_TEST_CASE("Synth - span interface") {
    auto engine = audio::ISynthEngine::create();

    auto osc = audio::ISynthOscillator::create(engine, audio::SynthShape::Triangle);
    FL_CHECK_TRUE(osc != nullptr);

    float buffer[128];
    fl::span<float> samples(buffer, 128);

    osc->generateSamples(samples, 0.01f);
    FL_CHECK_TRUE(samplesInRange(buffer, 128));
    FL_CHECK_TRUE(hasVariation(buffer, 128));
}

FL_TEST_CASE("Synth - reset functionality") {
    auto engine = audio::ISynthEngine::create();

    auto osc = audio::ISynthOscillator::create(engine, audio::SynthShape::Sawtooth);
    FL_CHECK_TRUE(osc != nullptr);

    float samples1[64];
    float samples2[64];
    float freq = 0.02f;

    // Generate some samples
    osc->generateSamples(samples1, 64, freq);

    // Reset and generate again - should start from same position
    osc->reset();
    osc->generateSamples(samples2, 64, freq);

    // After reset, samples should be similar (same starting point)
    // Note: Not exactly equal due to internal state, but should be close
    bool similar = true;
    for (int i = 0; i < 64; ++i) {
        if (fl::fabs(samples1[i] - samples2[i]) > 0.01f) {
            similar = false;
            break;
        }
    }
    FL_CHECK_TRUE(similar);
}

FL_TEST_CASE("Synth - multiple engines") {
    // Create two separate engines with different settings
    auto engine1 = audio::ISynthEngine::create(32, 16);
    auto engine2 = audio::ISynthEngine::create(16, 8);

    FL_CHECK_TRUE(engine1->isValid());
    FL_CHECK_TRUE(engine2->isValid());
    FL_CHECK_EQ(engine1->getWidth(), 32);
    FL_CHECK_EQ(engine2->getWidth(), 16);

    // Create oscillators from each engine
    auto osc1 = audio::ISynthOscillator::create(engine1, audio::SynthShape::Sawtooth);
    auto osc2 = audio::ISynthOscillator::create(engine2, audio::SynthShape::Square);

    FL_CHECK_TRUE(osc1 != nullptr);
    FL_CHECK_TRUE(osc2 != nullptr);

    // Both oscillators should work independently
    float samples1[128];
    float samples2[128];
    float freq = 0.01f;

    osc1->generateSamples(samples1, 128, freq);
    osc2->generateSamples(samples2, 128, freq);

    FL_CHECK_TRUE(samplesInRange(samples1, 128));
    FL_CHECK_TRUE(samplesInRange(samples2, 128));
    FL_CHECK_TRUE(hasVariation(samples1, 128));
    FL_CHECK_TRUE(hasVariation(samples2, 128));

    // The waveforms should be different (sawtooth vs square)
    bool different = false;
    for (int i = 0; i < 128; ++i) {
        if (fl::fabs(samples1[i] - samples2[i]) > 0.1f) {
            different = true;
            break;
        }
    }
    FL_CHECK_TRUE(different);
}

FL_TEST_CASE("Synth - oscillator keeps engine alive") {
    audio::ISynthOscillatorPtr osc;
    {
        // Create engine in inner scope
        auto engine = audio::ISynthEngine::create(32, 16);
        FL_CHECK_TRUE(engine->isValid());

        // Create oscillator that holds reference to engine
        osc = audio::ISynthOscillator::create(engine, audio::SynthShape::Triangle);
        FL_CHECK_TRUE(osc != nullptr);
    }
    // Engine should still be alive through oscillator's reference

    // Oscillator should still work
    float samples[64];
    osc->generateSamples(samples, 64, 0.01f);
    FL_CHECK_TRUE(samplesInRange(samples, 64));
    FL_CHECK_TRUE(hasVariation(samples, 64));
}

FL_TEST_CASE("Synth - audio::SynthParams::fromShape returns correct values") {
    // Sawtooth: reflect=1, peak=0, half=0, wait=0
    audio::SynthParams saw = audio::SynthParams::fromShape(audio::SynthShape::Sawtooth);
    FL_CHECK_EQ(saw.reflect, 1);
    FL_CHECK_EQ(saw.peakTime, 0.0f);
    FL_CHECK_EQ(saw.halfHeight, 0.0f);
    FL_CHECK_EQ(saw.zeroWait, 0.0f);

    // Square: reflect=1, peak=0, half=1, wait=0
    audio::SynthParams sq = audio::SynthParams::fromShape(audio::SynthShape::Square);
    FL_CHECK_EQ(sq.reflect, 1);
    FL_CHECK_EQ(sq.peakTime, 0.0f);
    FL_CHECK_EQ(sq.halfHeight, 1.0f);
    FL_CHECK_EQ(sq.zeroWait, 0.0f);

    // Triangle: reflect=1, peak=0.5, half=0, wait=0
    audio::SynthParams tri = audio::SynthParams::fromShape(audio::SynthShape::Triangle);
    FL_CHECK_EQ(tri.reflect, 1);
    FL_CHECK_EQ(tri.peakTime, 0.5f);
    FL_CHECK_EQ(tri.halfHeight, 0.0f);
    FL_CHECK_EQ(tri.zeroWait, 0.0f);

    // AlternatingSaw: reflect=0, peak=0, half=0, wait=0
    audio::SynthParams altSaw = audio::SynthParams::fromShape(audio::SynthShape::AlternatingSaw);
    FL_CHECK_EQ(altSaw.reflect, 0);
    FL_CHECK_EQ(altSaw.peakTime, 0.0f);
    FL_CHECK_EQ(altSaw.halfHeight, 0.0f);
    FL_CHECK_EQ(altSaw.zeroWait, 0.0f);
}
