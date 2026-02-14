#include "fl/audio/spectral_equalizer.h"
#include "fl/stl/vector.h"
#include "fl/stl/math.h"
#include "fl/math_macros.h"
#include "test.h"

using namespace fl;

// Helper: Generate uniform frequency bins (white noise)
static vector<float> generateUniformBins(size count, float magnitude) {
    return vector<float>(count, magnitude);
}

// Helper: Calculate average of bins
static float calculateAverage(span<const float> bins) {
    if (bins.size() == 0) {
        return 0.0f;
    }

    float sum = 0.0f;
    for (size i = 0; i < bins.size(); ++i) {
        sum += bins[i];
    }
    return sum / static_cast<float>(bins.size());
}

FL_TEST_CASE("SpectralEqualizer - Default flat configuration") {
    SpectralEqualizer eq;

    // Verify default configuration
    const auto& config = eq.getConfig();
    FL_CHECK(config.curve == EqualizationCurve::Flat);
    FL_CHECK_EQ(config.numBands, 16u);
    FL_CHECK_FALSE(config.applyMakeupGain);
    FL_CHECK_FALSE(config.enableCompression);

    // Verify flat gains (all 1.0)
    auto gains = eq.getGains();
    FL_CHECK_EQ(gains.size(), 16u);
    for (size i = 0; i < gains.size(); ++i) {
        FL_CHECK_EQ(gains[i], 1.0f);
    }
}

FL_TEST_CASE("SpectralEqualizer - Flat curve (no EQ)") {
    SpectralEqualizerConfig config;
    config.curve = EqualizationCurve::Flat;
    config.numBands = 16;

    SpectralEqualizer eq(config);

    // Generate input bins
    vector<float> inputBins = generateUniformBins(16, 100.0f);

    // Apply equalization
    vector<float> outputBins(16);
    eq.apply(inputBins, outputBins);

    // Output should match input (no change)
    for (size i = 0; i < 16; ++i) {
        FL_CHECK_EQ(outputBins[i], inputBins[i]);
    }

    // Stats should be updated
    const auto& stats = eq.getStats();
    FL_CHECK_EQ(stats.applicationsCount, 1u);
    FL_CHECK_EQ(stats.lastInputPeak, 100.0f);
    FL_CHECK_EQ(stats.lastOutputPeak, 100.0f);
    FL_CHECK_EQ(stats.lastMakeupGain, 1.0f);
}

FL_TEST_CASE("SpectralEqualizer - A-weighting curve (16 bands)") {
    SpectralEqualizerConfig config;
    config.curve = EqualizationCurve::AWeighting;
    config.numBands = 16;

    SpectralEqualizer eq(config);

    // Verify A-weighting gains are not flat
    auto gains = eq.getGains();
    bool hasVariation = false;
    for (size i = 1; i < gains.size(); ++i) {
        if (fl::abs(gains[i] - gains[0]) > 0.01f) {
            hasVariation = true;
            break;
        }
    }
    FL_CHECK(hasVariation);

    // Verify mid frequencies (bins 6-7) have higher gain than bass (bins 0-1)
    float bassGain = (gains[0] + gains[1]) / 2.0f;
    float midGain = (gains[6] + gains[7]) / 2.0f;
    FL_CHECK(midGain > bassGain);

    // Apply to uniform input
    vector<float> inputBins = generateUniformBins(16, 100.0f);
    vector<float> outputBins(16);
    eq.apply(inputBins, outputBins);

    // Output should have mid frequencies boosted
    float inputMid = (inputBins[6] + inputBins[7]) / 2.0f;
    float outputMid = (outputBins[6] + outputBins[7]) / 2.0f;
    FL_CHECK(outputMid > inputMid);
}

FL_TEST_CASE("SpectralEqualizer - A-weighting curve (32 bands)") {
    SpectralEqualizerConfig config;
    config.curve = EqualizationCurve::AWeighting;
    config.numBands = 32;

    SpectralEqualizer eq(config);

    // Verify gains array is correct size
    auto gains = eq.getGains();
    FL_CHECK_EQ(gains.size(), 32u);

    // Verify gains have variation
    bool hasVariation = false;
    for (size i = 1; i < gains.size(); ++i) {
        if (fl::abs(gains[i] - gains[0]) > 0.01f) {
            hasVariation = true;
            break;
        }
    }
    FL_CHECK(hasVariation);
}

FL_TEST_CASE("SpectralEqualizer - Custom gains") {
    SpectralEqualizer eq;

    // Set custom gains (boost first half, attenuate second half)
    vector<float> customGains(16);
    for (size i = 0; i < 8; ++i) {
        customGains[i] = 2.0f;  // Boost
    }
    for (size i = 8; i < 16; ++i) {
        customGains[i] = 0.5f;  // Attenuate
    }

    eq.setCustomGains(customGains);

    // Verify curve switched to Custom
    FL_CHECK(eq.getConfig().curve == EqualizationCurve::Custom);

    // Verify gains were applied
    auto gains = eq.getGains();
    for (size i = 0; i < 8; ++i) {
        FL_CHECK_EQ(gains[i], 2.0f);
    }
    for (size i = 8; i < 16; ++i) {
        FL_CHECK_EQ(gains[i], 0.5f);
    }

    // Apply to uniform input
    vector<float> inputBins = generateUniformBins(16, 100.0f);
    vector<float> outputBins(16);
    eq.apply(inputBins, outputBins);

    // Verify gains were applied correctly
    for (size i = 0; i < 8; ++i) {
        FL_CHECK_EQ(outputBins[i], 200.0f);  // 100 * 2.0
    }
    for (size i = 8; i < 16; ++i) {
        FL_CHECK_EQ(outputBins[i], 50.0f);   // 100 * 0.5
    }
}

FL_TEST_CASE("SpectralEqualizer - Makeup gain") {
    SpectralEqualizerConfig config;
    config.curve = EqualizationCurve::Custom;
    config.numBands = 16;
    config.applyMakeupGain = true;
    config.makeupGainTarget = 1.0f;  // Maintain original level

    // Set custom gains that reduce overall level
    config.customGains.resize(16, 0.5f);  // Reduce all bins by 50%

    SpectralEqualizer eq(config);

    // Apply to uniform input
    vector<float> inputBins = generateUniformBins(16, 100.0f);
    vector<float> outputBins(16);
    eq.apply(inputBins, outputBins);

    // Without makeup gain, output would be 50.0
    // With makeup gain, output should be boosted back toward 100.0
    float avgOutput = calculateAverage(outputBins);
    FL_CHECK(avgOutput > 50.0f);  // Boosted from 50
    FL_CHECK(avgOutput < 150.0f); // But not over-boosted

    // Verify makeup gain was applied
    const auto& stats = eq.getStats();
    FL_CHECK(stats.lastMakeupGain > 1.0f);  // Gain was increased
}

FL_TEST_CASE("SpectralEqualizer - Compression") {
    SpectralEqualizerConfig config;
    config.numBands = 16;
    config.enableCompression = true;
    config.compressionThreshold = 50.0f;
    config.compressionRatio = 2.0f;

    SpectralEqualizer eq(config);

    // Generate input with both quiet and loud signals
    vector<float> inputBins(16);
    for (size i = 0; i < 8; ++i) {
        inputBins[i] = 30.0f;  // Below threshold
    }
    for (size i = 8; i < 16; ++i) {
        inputBins[i] = 100.0f; // Above threshold (50 excess)
    }

    vector<float> outputBins(16);
    eq.apply(inputBins, outputBins);

    // Quiet signals should pass through unchanged
    for (size i = 0; i < 8; ++i) {
        FL_CHECK_EQ(outputBins[i], 30.0f);
    }

    // Loud signals should be compressed
    // Input: 100, Threshold: 50, Excess: 50, Compressed: 50 + 50/2 = 75
    for (size i = 8; i < 16; ++i) {
        FL_CHECK_EQ(outputBins[i], 75.0f);
    }
}

FL_TEST_CASE("SpectralEqualizer - Statistics tracking") {
    SpectralEqualizer eq;

    // Initial stats should be zero
    const auto& stats1 = eq.getStats();
    FL_CHECK_EQ(stats1.applicationsCount, 0u);

    // Apply once
    vector<float> inputBins = generateUniformBins(16, 100.0f);
    inputBins[5] = 500.0f;  // Peak

    vector<float> outputBins(16);
    eq.apply(inputBins, outputBins);

    // Stats should be updated
    const auto& stats2 = eq.getStats();
    FL_CHECK_EQ(stats2.applicationsCount, 1u);
    FL_CHECK_EQ(stats2.lastInputPeak, 500.0f);
    FL_CHECK_EQ(stats2.lastOutputPeak, 500.0f);  // Flat curve
    FL_CHECK(stats2.avgInputLevel > 0.0f);

    // Reset stats
    eq.resetStats();
    const auto& stats3 = eq.getStats();
    FL_CHECK_EQ(stats3.applicationsCount, 0u);
}

FL_TEST_CASE("SpectralEqualizer - Zero input handling") {
    SpectralEqualizer eq;

    // All zeros
    vector<float> inputBins(16, 0.0f);
    vector<float> outputBins(16, 99.0f);  // Initialize to non-zero

    eq.apply(inputBins, outputBins);

    // Output should be all zeros
    for (size i = 0; i < 16; ++i) {
        FL_CHECK_EQ(outputBins[i], 0.0f);
    }

    // Stats should reflect zero levels
    const auto& stats = eq.getStats();
    FL_CHECK_EQ(stats.lastInputPeak, 0.0f);
    FL_CHECK_EQ(stats.lastOutputPeak, 0.0f);
    FL_CHECK_EQ(stats.avgInputLevel, 0.0f);
}

FL_TEST_CASE("SpectralEqualizer - Reconfiguration") {
    SpectralEqualizer eq;

    // Start with flat curve
    FL_CHECK(eq.getConfig().curve == EqualizationCurve::Flat);

    // Reconfigure to A-weighting
    SpectralEqualizerConfig config;
    config.curve = EqualizationCurve::AWeighting;
    config.numBands = 16;
    eq.configure(config);

    FL_CHECK(eq.getConfig().curve == EqualizationCurve::AWeighting);

    // Verify gains changed
    auto gains = eq.getGains();
    bool hasVariation = false;
    for (size i = 1; i < gains.size(); ++i) {
        if (fl::abs(gains[i] - gains[0]) > 0.01f) {
            hasVariation = true;
            break;
        }
    }
    FL_CHECK(hasVariation);

    // Stats should be reset
    const auto& stats = eq.getStats();
    FL_CHECK_EQ(stats.applicationsCount, 0u);
}

FL_TEST_CASE("SpectralEqualizer - Input size mismatch") {
    SpectralEqualizerConfig config;
    config.numBands = 16;

    SpectralEqualizer eq(config);

    // Provide wrong size input (8 bins instead of 16)
    vector<float> inputBins = generateUniformBins(8, 100.0f);
    vector<float> outputBins(16);

    // Should handle gracefully (warning logged, but no crash)
    eq.apply(inputBins, outputBins);

    // Application count should NOT increment (failed)
    const auto& stats = eq.getStats();
    FL_CHECK_EQ(stats.applicationsCount, 0u);
}

FL_TEST_CASE("SpectralEqualizer - Output buffer too small") {
    SpectralEqualizer eq;

    vector<float> inputBins = generateUniformBins(16, 100.0f);

    // Provide buffer smaller than required
    vector<float> smallBuffer(8);

    // Should handle gracefully (warning logged, but no crash)
    eq.apply(inputBins, smallBuffer);

    // Application count should NOT increment (failed)
    const auto& stats = eq.getStats();
    FL_CHECK_EQ(stats.applicationsCount, 0u);
}

FL_TEST_CASE("SpectralEqualizer - Custom gains size mismatch") {
    SpectralEqualizer eq;

    // Try to set custom gains with wrong size
    vector<float> badGains(8, 2.0f);  // Only 8 gains, need 16

    eq.setCustomGains(badGains);

    // Should remain on flat curve (warning logged)
    FL_CHECK(eq.getConfig().curve == EqualizationCurve::Flat);
}

FL_TEST_CASE("SpectralEqualizer - Gain frequency response") {
    SpectralEqualizerConfig config;
    config.curve = EqualizationCurve::AWeighting;
    config.numBands = 16;

    SpectralEqualizer eq(config);

    // Generate input with energy in specific bands
    vector<float> inputBins(16, 0.0f);
    inputBins[0] = 100.0f;   // Bass
    inputBins[6] = 100.0f;   // Mid
    inputBins[14] = 100.0f;  // Treble

    vector<float> outputBins(16);
    eq.apply(inputBins, outputBins);

    // Mid should be boosted more than bass/treble (A-weighting)
    FL_CHECK(outputBins[6] > outputBins[0]);
    FL_CHECK(outputBins[6] > outputBins[14]);
}

FL_TEST_CASE("SpectralEqualizer - Makeup gain clamping") {
    SpectralEqualizerConfig config;
    config.numBands = 16;
    config.applyMakeupGain = true;
    config.makeupGainTarget = 1.0f;
    config.customGains.resize(16, 0.01f);  // Very low gains (would need 100x makeup)
    config.curve = EqualizationCurve::Custom;

    SpectralEqualizer eq(config);

    vector<float> inputBins = generateUniformBins(16, 100.0f);
    vector<float> outputBins(16);
    eq.apply(inputBins, outputBins);

    // Makeup gain should be clamped to max 10.0
    const auto& stats = eq.getStats();
    FL_CHECK(stats.lastMakeupGain <= 10.0f);
}

FL_TEST_CASE("SpectralEqualizer - Compression with various ratios") {
    // Test 2:1 compression
    {
        SpectralEqualizerConfig config;
        config.numBands = 16;
        config.enableCompression = true;
        config.compressionThreshold = 50.0f;
        config.compressionRatio = 2.0f;

        SpectralEqualizer eq(config);

        vector<float> inputBins(16, 100.0f);  // 50 dB over threshold
        vector<float> outputBins(16);
        eq.apply(inputBins, outputBins);

        // Output: 50 + (100-50)/2 = 75
        FL_CHECK_EQ(outputBins[0], 75.0f);
    }

    // Test 4:1 compression (more aggressive)
    {
        SpectralEqualizerConfig config;
        config.numBands = 16;
        config.enableCompression = true;
        config.compressionThreshold = 50.0f;
        config.compressionRatio = 4.0f;

        SpectralEqualizer eq(config);

        vector<float> inputBins(16, 100.0f);  // 50 dB over threshold
        vector<float> outputBins(16);
        eq.apply(inputBins, outputBins);

        // Output: 50 + (100-50)/4 = 62.5
        FL_CHECK_EQ(outputBins[0], 62.5f);
    }
}

FL_TEST_CASE("SpectralEqualizer - A-weighting unsupported band count") {
    SpectralEqualizerConfig config;
    config.curve = EqualizationCurve::AWeighting;
    config.numBands = 24;  // Not 16 or 32

    SpectralEqualizer eq(config);

    // Should fall back to flat gains
    auto gains = eq.getGains();
    for (size i = 0; i < gains.size(); ++i) {
        FL_CHECK_EQ(gains[i], 1.0f);
    }
}
