/// @file hexwave.cpp
/// @brief Tests for HexWave bandlimited audio oscillator

#include "test.h"
#include "fl/hexwave.h"
#include "fl/stl/math.h"

using namespace fl;

namespace {

// Helper to check if samples are within valid range [-1.0, 1.0] with some tolerance
bool samplesInRange(const float* samples, int32_t count, float tolerance = 1.5f) {
    for (int32_t i = 0; i < count; ++i) {
        if (samples[i] < -tolerance || samples[i] > tolerance) {
            return false;
        }
    }
    return true;
}

// Helper to check if waveform has significant variation (not all zeros or constant)
bool hasVariation(const float* samples, int32_t count) {
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

FL_TEST_CASE("HexWave - basic initialization and generation") {
    auto engine = IHexWaveEngine::create(32, 16);
    FL_CHECK_TRUE(engine->isValid());

    auto osc = IHexWaveOscillator::create(engine, HexWaveShape::Sawtooth);
    FL_CHECK_TRUE(osc != nullptr);

    // Generate some samples
    float samples[256];
    float freq = 440.0f / 44100.0f;  // 440 Hz at 44.1 kHz
    osc->generateSamples(samples, 256, freq);

    // Verify samples are in reasonable range and have variation
    FL_CHECK_TRUE(samplesInRange(samples, 256));
    FL_CHECK_TRUE(hasVariation(samples, 256));
}

FL_TEST_CASE("HexWave - waveform shapes") {
    auto engine = IHexWaveEngine::create();

    float samples[512];
    float freq = 100.0f / 44100.0f;  // Low frequency for clearer waveform

    // Test all predefined shapes generate valid output
    auto saw = IHexWaveOscillator::create(engine, HexWaveShape::Sawtooth);
    FL_CHECK_TRUE(saw != nullptr);
    saw->generateSamples(samples, 512, freq);
    FL_CHECK_TRUE(samplesInRange(samples, 512));
    FL_CHECK_TRUE(hasVariation(samples, 512));

    auto square = IHexWaveOscillator::create(engine, HexWaveShape::Square);
    FL_CHECK_TRUE(square != nullptr);
    square->generateSamples(samples, 512, freq);
    FL_CHECK_TRUE(samplesInRange(samples, 512));
    FL_CHECK_TRUE(hasVariation(samples, 512));

    auto triangle = IHexWaveOscillator::create(engine, HexWaveShape::Triangle);
    FL_CHECK_TRUE(triangle != nullptr);
    triangle->generateSamples(samples, 512, freq);
    FL_CHECK_TRUE(samplesInRange(samples, 512));
    FL_CHECK_TRUE(hasVariation(samples, 512));
}

FL_TEST_CASE("HexWave - custom parameters") {
    auto engine = IHexWaveEngine::create();

    // Create with custom parameters
    HexWaveParams params(1, 0.3f, 0.5f, 0.1f);
    auto osc = IHexWaveOscillator::create(engine, params);
    FL_CHECK_TRUE(osc != nullptr);

    // Verify params are stored correctly
    HexWaveParams retrieved = osc->getParams();
    FL_CHECK_EQ(retrieved.reflect, 1);
    FL_CHECK_EQ(retrieved.peakTime, 0.3f);
    FL_CHECK_EQ(retrieved.halfHeight, 0.5f);
    FL_CHECK_EQ(retrieved.zeroWait, 0.1f);

    // Generate samples
    float samples[256];
    osc->generateSamples(samples, 256, 0.01f);
    FL_CHECK_TRUE(samplesInRange(samples, 256));
}

FL_TEST_CASE("HexWave - shape change at runtime") {
    auto engine = IHexWaveEngine::create();

    auto osc = IHexWaveOscillator::create(engine, HexWaveShape::Sawtooth);
    FL_CHECK_TRUE(osc != nullptr);

    float samples[256];
    float freq = 0.01f;

    // Generate with sawtooth
    osc->generateSamples(samples, 256, freq);
    FL_CHECK_TRUE(hasVariation(samples, 256));

    // Change to square and generate more
    osc->setShape(HexWaveShape::Square);
    osc->generateSamples(samples, 256, freq);
    FL_CHECK_TRUE(hasVariation(samples, 256));

    // Change to triangle and generate more
    osc->setShape(HexWaveShape::Triangle);
    osc->generateSamples(samples, 256, freq);
    FL_CHECK_TRUE(hasVariation(samples, 256));
}

FL_TEST_CASE("HexWave - span interface") {
    auto engine = IHexWaveEngine::create();

    auto osc = IHexWaveOscillator::create(engine, HexWaveShape::Triangle);
    FL_CHECK_TRUE(osc != nullptr);

    float buffer[128];
    fl::span<float> samples(buffer, 128);

    osc->generateSamples(samples, 0.01f);
    FL_CHECK_TRUE(samplesInRange(buffer, 128));
    FL_CHECK_TRUE(hasVariation(buffer, 128));
}

FL_TEST_CASE("HexWave - reset functionality") {
    auto engine = IHexWaveEngine::create();

    auto osc = IHexWaveOscillator::create(engine, HexWaveShape::Sawtooth);
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

FL_TEST_CASE("HexWave - multiple engines") {
    // Create two separate engines with different settings
    auto engine1 = IHexWaveEngine::create(32, 16);
    auto engine2 = IHexWaveEngine::create(16, 8);

    FL_CHECK_TRUE(engine1->isValid());
    FL_CHECK_TRUE(engine2->isValid());
    FL_CHECK_EQ(engine1->getWidth(), 32);
    FL_CHECK_EQ(engine2->getWidth(), 16);

    // Create oscillators from each engine
    auto osc1 = IHexWaveOscillator::create(engine1, HexWaveShape::Sawtooth);
    auto osc2 = IHexWaveOscillator::create(engine2, HexWaveShape::Square);

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

FL_TEST_CASE("HexWave - oscillator keeps engine alive") {
    IHexWaveOscillatorPtr osc;
    {
        // Create engine in inner scope
        auto engine = IHexWaveEngine::create(32, 16);
        FL_CHECK_TRUE(engine->isValid());

        // Create oscillator that holds reference to engine
        osc = IHexWaveOscillator::create(engine, HexWaveShape::Triangle);
        FL_CHECK_TRUE(osc != nullptr);
    }
    // Engine should still be alive through oscillator's reference

    // Oscillator should still work
    float samples[64];
    osc->generateSamples(samples, 64, 0.01f);
    FL_CHECK_TRUE(samplesInRange(samples, 64));
    FL_CHECK_TRUE(hasVariation(samples, 64));
}
