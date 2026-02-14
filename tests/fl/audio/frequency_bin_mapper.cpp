#include "fl/audio/frequency_bin_mapper.h"
#include "fl/audio/spectral_equalizer.h"
#include "test_helpers.hpp"
#include "fl/fft.h"
#include "fl/stl/vector.h"
#include "fl/stl/math.h"
#include "fl/math_macros.h"
#include "test.h"

using namespace fl;
using namespace fl::test;

// Helper: Generate synthetic FFT bins with known frequency content
static vector<float> generateSyntheticFFT(size numBins, float peakFrequency, u32 sampleRate) {
    vector<float> fftBins(numBins, 0.0f);

    // Calculate which FFT bin corresponds to the peak frequency
    // FFT bin index = (frequency / sampleRate) * fftSize
    // fftSize = numBins * 2 (FFT produces fftSize/2 bins)
    float fftSize = static_cast<float>(numBins) * 2.0f;
    float binIndex = (peakFrequency / static_cast<float>(sampleRate)) * fftSize;

    u32 peakBin = static_cast<u32>(binIndex);
    if (peakBin < numBins) {
        // Create a peak with some width
        fftBins[peakBin] = 1000.0f;  // Peak magnitude
        if (peakBin > 0) {
            fftBins[peakBin - 1] = 500.0f;  // Adjacent bin
        }
        if (peakBin < numBins - 1) {
            fftBins[peakBin + 1] = 500.0f;  // Adjacent bin
        }
    }

    return fftBins;
}

FL_TEST_CASE("FrequencyBinMapper - Default 16-bin configuration") {
    FrequencyBinMapper mapper;

    // Verify default configuration
    const auto& config = mapper.getConfig();
    FL_CHECK_EQ(static_cast<size>(config.mode), 16u);
    FL_CHECK_EQ(config.minFrequency, 20.0f);
    FL_CHECK_EQ(config.maxFrequency, 16000.0f);
    FL_CHECK_EQ(config.sampleRate, 22050u);
    FL_CHECK_EQ(config.fftBinCount, 256u);
    FL_CHECK(config.useLogSpacing);

    // Verify number of bins
    FL_CHECK_EQ(mapper.getNumBins(), 16u);
}

FL_TEST_CASE("FrequencyBinMapper - 32-bin configuration") {
    FrequencyBinMapperConfig config;
    config.mode = FrequencyBinMode::Bins32;
    config.sampleRate = 44100;
    config.fftBinCount = 512;

    FrequencyBinMapper mapper(config);

    FL_CHECK_EQ(mapper.getNumBins(), 32u);
    FL_CHECK_EQ(mapper.getConfig().sampleRate, 44100u);
    FL_CHECK_EQ(mapper.getConfig().fftBinCount, 512u);
}

FL_TEST_CASE("FrequencyBinMapper - Logarithmic bin boundaries") {
    FrequencyBinMapperConfig config;
    config.mode = FrequencyBinMode::Bins16;
    config.minFrequency = 20.0f;
    config.maxFrequency = 16000.0f;
    config.useLogSpacing = true;

    FrequencyBinMapper mapper(config);

    // Verify logarithmic spacing (each bin should have roughly constant log width)
    float prevLogWidth = 0.0f;
    for (size i = 0; i < 15; ++i) {
        auto range = mapper.getBinFrequencyRange(i);

        // Check that range is valid
        FL_CHECK(range.minFreq > 0.0f);
        FL_CHECK(range.maxFreq > range.minFreq);

        // Calculate log width: log(max/min)
        float logWidth = fl::logf(range.maxFreq / range.minFreq);

        // All bins should have similar log width (within 10% tolerance)
        if (i > 0) {
            float ratio = logWidth / prevLogWidth;
            FL_CHECK(ratio > 0.9f);
            FL_CHECK(ratio < 1.1f);
        }

        prevLogWidth = logWidth;
    }

    // Verify first bin starts at minFrequency (allow small numerical error)
    auto firstRange = mapper.getBinFrequencyRange(0);
    FL_CHECK(fl::abs(firstRange.minFreq - 20.0f) < 0.1f);

    auto lastRange = mapper.getBinFrequencyRange(15);
    FL_CHECK_GT(lastRange.maxFreq, 5000.0f);
}

FL_TEST_CASE("FrequencyBinMapper - Linear bin boundaries") {
    FrequencyBinMapperConfig config;
    config.mode = FrequencyBinMode::Bins16;
    config.minFrequency = 0.0f;
    config.maxFrequency = 8000.0f;
    config.useLogSpacing = false;

    FrequencyBinMapper mapper(config);

    // Verify linear spacing (all bins should have equal width)
    float expectedWidth = 8000.0f / 16.0f;  // 500 Hz per bin

    for (size i = 0; i < 16; ++i) {
        auto range = mapper.getBinFrequencyRange(i);

        float width = range.maxFreq - range.minFreq;

        // Width should be constant (within 1 Hz tolerance)
        FL_CHECK(fl::abs(width - expectedWidth) < 1.0f);
    }
}

FL_TEST_CASE("FrequencyBinMapper - Bass frequency mapping") {
    FrequencyBinMapperConfig config;
    config.mode = FrequencyBinMode::Bins16;
    config.sampleRate = 22050;
    config.fftBinCount = 256;

    FrequencyBinMapper mapper(config);

    // Generate FFT with bass peak (50 Hz)
    vector<float> fftBins = generateSyntheticFFT(256, 50.0f, 22050);

    // Map to frequency bins
    vector<float> freqBins(16, 0.0f);
    mapper.mapBins(fftBins, freqBins);

    // Bass energy should be highest
    float bassEnergy = mapper.getBassEnergy(freqBins);
    float midEnergy = mapper.getMidEnergy(freqBins);
    float trebleEnergy = mapper.getTrebleEnergy(freqBins);

    FL_CHECK(bassEnergy > midEnergy);
    FL_CHECK(bassEnergy > trebleEnergy);
    FL_CHECK(bassEnergy > 0.0f);
}

FL_TEST_CASE("FrequencyBinMapper - Mid frequency mapping") {
    FrequencyBinMapperConfig config;
    config.mode = FrequencyBinMode::Bins16;
    config.sampleRate = 22050;
    config.fftBinCount = 256;

    FrequencyBinMapper mapper(config);

    // Generate FFT with mid peak (500 Hz)
    vector<float> fftBins = generateSyntheticFFT(256, 500.0f, 22050);

    // Map to frequency bins
    vector<float> freqBins(16, 0.0f);
    mapper.mapBins(fftBins, freqBins);

    // Mid energy should be highest
    float bassEnergy = mapper.getBassEnergy(freqBins);
    float midEnergy = mapper.getMidEnergy(freqBins);
    float trebleEnergy = mapper.getTrebleEnergy(freqBins);

    FL_CHECK(midEnergy > bassEnergy);
    FL_CHECK(midEnergy > trebleEnergy);
    FL_CHECK(midEnergy > 0.0f);
}

FL_TEST_CASE("FrequencyBinMapper - Treble frequency mapping") {
    FrequencyBinMapperConfig config;
    config.mode = FrequencyBinMode::Bins16;
    config.sampleRate = 22050;
    config.fftBinCount = 256;

    FrequencyBinMapper mapper(config);

    // Generate FFT with treble peak (8000 Hz)
    vector<float> fftBins = generateSyntheticFFT(256, 8000.0f, 22050);

    // Map to frequency bins
    vector<float> freqBins(16, 0.0f);
    mapper.mapBins(fftBins, freqBins);

    // Treble energy should be highest
    float bassEnergy = mapper.getBassEnergy(freqBins);
    float midEnergy = mapper.getMidEnergy(freqBins);
    float trebleEnergy = mapper.getTrebleEnergy(freqBins);

    FL_CHECK(trebleEnergy > bassEnergy);
    FL_CHECK(trebleEnergy > midEnergy);
    FL_CHECK(trebleEnergy > 0.0f);
}

FL_TEST_CASE("FrequencyBinMapper - Uniform FFT bins") {
    FrequencyBinMapper mapper;

    // Generate uniform FFT (white noise spectrum)
    vector<float> fftBins(256, 100.0f);  // All bins equal magnitude

    // Map to frequency bins
    vector<float> freqBins(16, 0.0f);
    mapper.mapBins(fftBins, freqBins);

    // All output bins should be non-zero
    for (size i = 0; i < 16; ++i) {
        FL_CHECK(freqBins[i] > 0.0f);
    }

    // For logarithmic spacing, all output bins should have similar energy
    // (since they span equal log widths of the spectrum)
    float avgEnergy = 0.0f;
    for (size i = 0; i < 16; ++i) {
        avgEnergy += freqBins[i];
    }
    avgEnergy /= 16.0f;

    // Each bin should be within 20% of average (tolerant to edge effects)
    for (size i = 1; i < 15; ++i) {  // Skip edge bins
        float ratio = freqBins[i] / avgEnergy;
        FL_CHECK(ratio > 0.8f);
        FL_CHECK(ratio < 1.2f);
    }
}

FL_TEST_CASE("FrequencyBinMapper - Empty FFT bins") {
    FrequencyBinMapper mapper;

    // Generate empty FFT (silence)
    vector<float> fftBins(256, 0.0f);

    // Map to frequency bins
    vector<float> freqBins(16, 99.0f);  // Initialize to non-zero
    mapper.mapBins(fftBins, freqBins);

    // All output bins should be zero
    for (size i = 0; i < 16; ++i) {
        FL_CHECK_EQ(freqBins[i], 0.0f);
    }

    // Bass/mid/treble energy should be zero
    FL_CHECK_EQ(mapper.getBassEnergy(freqBins), 0.0f);
    FL_CHECK_EQ(mapper.getMidEnergy(freqBins), 0.0f);
    FL_CHECK_EQ(mapper.getTrebleEnergy(freqBins), 0.0f);
}

FL_TEST_CASE("FrequencyBinMapper - Statistics tracking") {
    FrequencyBinMapper mapper;

    // Initial stats should be zero
    const auto& stats1 = mapper.getStats();
    FL_CHECK_EQ(stats1.binMappingCount, 0u);
    FL_CHECK_EQ(stats1.lastFFTBinsUsed, 0u);
    FL_CHECK_EQ(stats1.maxMagnitude, 0.0f);

    // Map some bins
    vector<float> fftBins(256, 50.0f);
    fftBins[100] = 1000.0f;  // Peak

    vector<float> freqBins(16);
    mapper.mapBins(fftBins, freqBins);

    // Stats should be updated
    const auto& stats2 = mapper.getStats();
    FL_CHECK_EQ(stats2.binMappingCount, 1u);
    FL_CHECK(stats2.lastFFTBinsUsed > 0u);
    FL_CHECK_EQ(stats2.maxMagnitude, 1000.0f);

    // Map again
    mapper.mapBins(fftBins, freqBins);

    const auto& stats3 = mapper.getStats();
    FL_CHECK_EQ(stats3.binMappingCount, 2u);
}

FL_TEST_CASE("FrequencyBinMapper - 32-bin mode mapping") {
    FrequencyBinMapperConfig config;
    config.mode = FrequencyBinMode::Bins32;
    config.sampleRate = 22050;
    config.fftBinCount = 512;  // Larger FFT for higher resolution

    FrequencyBinMapper mapper(config);

    FL_CHECK_EQ(mapper.getNumBins(), 32u);

    // Generate FFT
    vector<float> fftBins(512, 10.0f);

    // Map to 32 bins
    vector<float> freqBins(32, 0.0f);
    mapper.mapBins(fftBins, freqBins);

    // All 32 bins should be populated
    for (size i = 0; i < 32; ++i) {
        FL_CHECK(freqBins[i] > 0.0f);
    }
}

FL_TEST_CASE("FrequencyBinMapper - Reconfiguration") {
    FrequencyBinMapper mapper;

    // Start with 16 bins
    FL_CHECK_EQ(mapper.getNumBins(), 16u);

    // Reconfigure to 32 bins
    FrequencyBinMapperConfig config;
    config.mode = FrequencyBinMode::Bins32;
    config.fftBinCount = 512;
    mapper.configure(config);

    FL_CHECK_EQ(mapper.getNumBins(), 32u);
    FL_CHECK_EQ(mapper.getConfig().fftBinCount, 512u);

    // Stats should be reset
    const auto& stats = mapper.getStats();
    FL_CHECK_EQ(stats.binMappingCount, 0u);
}

FL_TEST_CASE("FrequencyBinMapper - Bin boundary coverage") {
    FrequencyBinMapper mapper;

    // Verify all 16 bins have valid frequency ranges
    for (size i = 0; i < 16; ++i) {
        auto range = mapper.getBinFrequencyRange(i);

        // Range should be valid (allow small numerical error for first bin)
        FL_CHECK(range.minFreq >= 19.9f);  // Slightly below 20 Hz due to FP precision
        FL_CHECK(range.maxFreq <= 16100.0f);  // Slightly above 16k Hz due to FP precision
        FL_CHECK(range.maxFreq > range.minFreq);

        // Adjacent bins should be contiguous
        if (i > 0) {
            auto prevRange = mapper.getBinFrequencyRange(i - 1);
            FL_CHECK(fl::abs(prevRange.maxFreq - range.minFreq) < 0.1f);
        }
    }

    // Invalid bin index should return zero range
    auto invalidRange = mapper.getBinFrequencyRange(99);
    FL_CHECK_EQ(invalidRange.minFreq, 0.0f);
    FL_CHECK_EQ(invalidRange.maxFreq, 0.0f);
}

// FBM-1: Log Spacing - First Bin Narrower Than Last (Hz width)
FL_TEST_CASE("FrequencyBinMapper - log spacing first bin narrower than last") {
    FrequencyBinMapperConfig config;
    config.mode = FrequencyBinMode::Bins16;
    config.minFrequency = 20.0f;
    config.maxFrequency = 16000.0f;
    config.useLogSpacing = true;

    FrequencyBinMapper mapper(config);

    auto firstRange = mapper.getBinFrequencyRange(0);
    auto lastRange = mapper.getBinFrequencyRange(15);

    float firstBinWidth = firstRange.maxFreq - firstRange.minFreq;
    float lastBinWidth = lastRange.maxFreq - lastRange.minFreq;

    // Log spacing: first bin (bass) spans much fewer Hz than last bin (treble)
    FL_CHECK_GT(firstBinWidth, 0.0f);
    FL_CHECK_GT(lastBinWidth, 0.0f);
    FL_CHECK_LT(firstBinWidth, lastBinWidth);

    // The difference should be dramatic (orders of magnitude for 20-16000 Hz range)
    FL_CHECK_GT(lastBinWidth / firstBinWidth, 10.0f);
}

// FBM-2: Single-Bin Peak Isolation
FL_TEST_CASE("FrequencyBinMapper - single bin peak isolation") {
    FrequencyBinMapperConfig config;
    config.mode = FrequencyBinMode::Bins16;
    config.sampleRate = 22050;
    config.fftBinCount = 256;
    config.useLogSpacing = true;

    FrequencyBinMapper mapper(config);

    // Place energy in exactly one FFT bin (bin 50 of 256)
    vector<float> fftBins(256, 0.0f);
    fftBins[50] = 1000.0f;

    vector<float> freqBins(16, 0.0f);
    mapper.mapBins(fftBins, freqBins);

    // Count how many output bins got non-zero energy
    int nonZeroCount = 0;
    for (size i = 0; i < 16; ++i) {
        if (freqBins[i] > 0.0f) nonZeroCount++;
    }

    // Single FFT bin should map to exactly one output bin
    FL_CHECK_EQ(nonZeroCount, 1);
}

// FBM-3: Sample Rate Change Affects Mapping
FL_TEST_CASE("FrequencyBinMapper - sample rate affects mapping") {
    // Same FFT data, different sample rates -> different output
    vector<float> fftBins(256, 0.0f);
    fftBins[50] = 1000.0f; // Energy at bin 50

    // Config 1: sampleRate = 22050
    FrequencyBinMapperConfig config1;
    config1.mode = FrequencyBinMode::Bins16;
    config1.sampleRate = 22050;
    config1.fftBinCount = 256;
    FrequencyBinMapper mapper1(config1);

    vector<float> output1(16, 0.0f);
    mapper1.mapBins(fftBins, output1);

    // Config 2: sampleRate = 44100 (doubled)
    FrequencyBinMapperConfig config2;
    config2.mode = FrequencyBinMode::Bins16;
    config2.sampleRate = 44100;
    config2.fftBinCount = 256;
    FrequencyBinMapper mapper2(config2);

    vector<float> output2(16, 0.0f);
    mapper2.mapBins(fftBins, output2);

    // Same FFT bin 50 corresponds to different frequencies at different sample rates:
    // At 22050 Hz: bin 50 = 50 * 22050 / 512 = ~2153 Hz
    // At 44100 Hz: bin 50 = 50 * 44100 / 512 = ~4307 Hz
    // These should map to different output bins
    bool outputsDiffer = false;
    for (size i = 0; i < 16; ++i) {
        if (fl::abs(output1[i] - output2[i]) > 0.01f) {
            outputsDiffer = true;
            break;
        }
    }
    FL_CHECK(outputsDiffer);
}

FL_TEST_CASE("FrequencyBinMapper - Small output buffer handling") {
    FrequencyBinMapper mapper;

    vector<float> fftBins(256, 100.0f);

    // Provide buffer smaller than required (8 bins instead of 16)
    vector<float> smallBuffer(8, 0.0f);

    // Should handle gracefully (warning logged, but no crash)
    mapper.mapBins(fftBins, smallBuffer);

    // Buffer should still contain valid values (implementation handles gracefully)
    FL_CHECK_EQ(smallBuffer.size(), 8u);  // Buffer size unchanged
}

FL_TEST_CASE("FrequencyBinMapper - Bass/mid/treble separation") {
    FrequencyBinMapper mapper;

    // Verify bass range (bins 0-1)
    auto bassRange0 = mapper.getBinFrequencyRange(0);
    auto bassRange1 = mapper.getBinFrequencyRange(1);

    FL_CHECK(bassRange0.minFreq >= 19.9f);  // Allow FP precision tolerance
    FL_CHECK(bassRange1.maxFreq <= 200.0f);  // Bass should be below 200 Hz

    // Verify mid range (bins 6-7)
    auto midRange6 = mapper.getBinFrequencyRange(6);
    auto midRange7 = mapper.getBinFrequencyRange(7);

    FL_CHECK(midRange6.minFreq >= 200.0f);
    FL_CHECK(midRange7.maxFreq <= 2000.0f);  // Mid should be 200-2000 Hz

    // Verify treble range (bins 14-15)
    auto trebleRange14 = mapper.getBinFrequencyRange(14);
    auto trebleRange15 = mapper.getBinFrequencyRange(15);

    FL_CHECK(trebleRange14.minFreq >= 2000.0f);
    FL_CHECK(trebleRange15.maxFreq <= 16000.0f);  // Treble should be 2000-16000 Hz
}

// Integration tests with SpectralEqualizer

FL_TEST_CASE("FrequencyBinMapper + SpectralEqualizer - Basic integration") {
    // Setup FFT
    FFT fft;
    FFT_Args fftArgs(512, 16, 20.0f, 16000.0f, 22050);

    // Setup frequency bin mapper
    FrequencyBinMapperConfig mapperConfig;
    mapperConfig.mode = FrequencyBinMode::Bins16;
    mapperConfig.sampleRate = 22050;
    mapperConfig.fftBinCount = 256;  // 512 / 2
    FrequencyBinMapper mapper(mapperConfig);

    // Setup spectral equalizer (flat curve)
    SpectralEqualizerConfig eqConfig;
    eqConfig.curve = EqualizationCurve::Flat;
    eqConfig.numBands = 16;
    SpectralEqualizer eq(eqConfig);

    // Generate test audio (1kHz tone)
    vector<i16> audioSamples = generateTone(512, 1000.0f, 22050.0f, 10000);

    // Run FFT
    FFTBins fftBins(16);
    fft.run(audioSamples, &fftBins, fftArgs);

    // Map FFT bins to frequency channels
    vector<float> frequencyBins(16);
    mapper.mapBins(fftBins.bins_raw, frequencyBins);

    // Verify we got frequency bins
    bool hasEnergy = false;
    for (size i = 0; i < frequencyBins.size(); ++i) {
        if (frequencyBins[i] > 0.0f) {
            hasEnergy = true;
            break;
        }
    }
    FL_CHECK(hasEnergy);

    // Apply equalization (flat curve should not change values)
    vector<float> equalizedBins(16);
    eq.apply(frequencyBins, equalizedBins);

    // Flat EQ should produce identical output
    for (size i = 0; i < 16; ++i) {
        FL_CHECK_EQ(frequencyBins[i], equalizedBins[i]);
    }
}

FL_TEST_CASE("FrequencyBinMapper + SpectralEqualizer - A-weighting integration") {
    // Setup FFT
    FFT fft;
    FFT_Args fftArgs(512, 16, 20.0f, 16000.0f, 22050);

    // Setup frequency bin mapper
    FrequencyBinMapperConfig mapperConfig;
    mapperConfig.mode = FrequencyBinMode::Bins16;
    mapperConfig.sampleRate = 22050;
    mapperConfig.fftBinCount = 256;
    FrequencyBinMapper mapper(mapperConfig);

    // Setup spectral equalizer with A-weighting
    SpectralEqualizerConfig eqConfig;
    eqConfig.curve = EqualizationCurve::AWeighting;
    eqConfig.numBands = 16;
    SpectralEqualizer eq(eqConfig);

    // Generate test audio with multiple tones
    vector<i16> audioSamples(512, 0);
    auto bass = generateTone(512, 50.0f, 22050.0f, 5000);    // Bass
    auto mid = generateTone(512, 500.0f, 22050.0f, 5000);    // Mid
    auto treble = generateTone(512, 4000.0f, 22050.0f, 5000); // Treble

    // Mix tones
    for (size i = 0; i < 512; ++i) {
        i32 mixed = static_cast<i32>(bass[i]) + static_cast<i32>(mid[i]) + static_cast<i32>(treble[i]);
        mixed /= 3;  // Average
        if (mixed > 32767) mixed = 32767;
        if (mixed < -32768) mixed = -32768;
        audioSamples[i] = static_cast<i16>(mixed);
    }

    // Run FFT
    FFTBins fftBins(16);
    fft.run(audioSamples, &fftBins, fftArgs);

    // Map to frequency bins
    vector<float> frequencyBins(16);
    mapper.mapBins(fftBins.bins_raw, frequencyBins);

    // Apply A-weighting
    vector<float> equalizedBins(16);
    eq.apply(frequencyBins, equalizedBins);

    // Get band energies before and after EQ
    float bassEnergyBefore = mapper.getBassEnergy(frequencyBins);
    float midEnergyBefore = mapper.getMidEnergy(frequencyBins);

    float bassEnergyAfter = mapper.getBassEnergy(equalizedBins);
    float midEnergyAfter = mapper.getMidEnergy(equalizedBins);

    // A-weighting should boost mid more than bass
    // (mid gain > bass gain means midAfter/midBefore > bassAfter/bassBefore)
    if (bassEnergyBefore > 0.0f && midEnergyBefore > 0.0f) {
        float bassBoost = bassEnergyAfter / bassEnergyBefore;
        float midBoost = midEnergyAfter / midEnergyBefore;
        FL_CHECK(midBoost > bassBoost);
    }
}

FL_TEST_CASE("FrequencyBinMapper + SpectralEqualizer - 32-bin mode") {
    // Setup FFT for higher resolution
    FFT fft;
    FFT_Args fftArgs(1024, 32, 20.0f, 16000.0f, 44100);

    // Setup frequency bin mapper (32 bins)
    FrequencyBinMapperConfig mapperConfig;
    mapperConfig.mode = FrequencyBinMode::Bins32;
    mapperConfig.sampleRate = 44100;
    mapperConfig.fftBinCount = 512;  // 1024 / 2
    FrequencyBinMapper mapper(mapperConfig);

    // Setup spectral equalizer (32 bins with A-weighting)
    SpectralEqualizerConfig eqConfig;
    eqConfig.curve = EqualizationCurve::AWeighting;
    eqConfig.numBands = 32;
    SpectralEqualizer eq(eqConfig);

    // Generate test audio
    vector<i16> audioSamples = generateTone(1024, 2000.0f, 44100.0f, 15000);

    // Run FFT
    FFTBins fftBins(32);
    fft.run(audioSamples, &fftBins, fftArgs);

    // Map to frequency bins
    vector<float> frequencyBins(32);
    mapper.mapBins(fftBins.bins_raw, frequencyBins);

    FL_CHECK_EQ(mapper.getNumBins(), 32u);

    // Apply equalization
    vector<float> equalizedBins(32);
    eq.apply(frequencyBins, equalizedBins);

    // Verify all 32 bins were processed
    bool allProcessed = true;
    for (size i = 0; i < 32; ++i) {
        if (frequencyBins[i] > 0.0f && equalizedBins[i] == 0.0f) {
            allProcessed = false;
            break;
        }
    }
    FL_CHECK(allProcessed);
}

FL_TEST_CASE("FrequencyBinMapper + SpectralEqualizer - Complete pipeline") {
    // This test demonstrates a complete Phase 2 processing pipeline:
    // Audio -> FFT -> FrequencyBinMapper -> SpectralEqualizer -> Analysis

    // Setup components
    FFT fft;
    FFT_Args fftArgs(512, 16, 20.0f, 16000.0f, 22050);

    FrequencyBinMapperConfig mapperConfig;
    mapperConfig.mode = FrequencyBinMode::Bins16;
    mapperConfig.sampleRate = 22050;
    mapperConfig.fftBinCount = 256;
    mapperConfig.useLogSpacing = true;  // Logarithmic spacing
    FrequencyBinMapper mapper(mapperConfig);

    SpectralEqualizerConfig eqConfig;
    eqConfig.curve = EqualizationCurve::AWeighting;
    eqConfig.numBands = 16;
    eqConfig.applyMakeupGain = true;
    eqConfig.makeupGainTarget = 0.8f;
    SpectralEqualizer eq(eqConfig);

    // Generate rich audio content (multi-band)
    vector<i16> audioSamples(512, 0);
    for (size i = 0; i < 512; ++i) {
        float t = static_cast<float>(i) / 22050.0f;
        float sample = 3000.0f * fl::sinf(2.0f * FL_M_PI * 50.0f * t);    // Bass (50 Hz)
        sample += 3000.0f * fl::sinf(2.0f * FL_M_PI * 500.0f * t);        // Mid (500 Hz)
        sample += 3000.0f * fl::sinf(2.0f * FL_M_PI * 4000.0f * t);       // Treble (4000 Hz)
        audioSamples[i] = static_cast<i16>(sample);
    }

    // Stage 1: FFT
    FFTBins fftBins(16);
    fft.run(audioSamples, &fftBins, fftArgs);

    // Stage 2: Frequency bin mapping
    vector<float> frequencyBins(16);
    mapper.mapBins(fftBins.bins_raw, frequencyBins);

    // Stage 3: Spectral equalization
    vector<float> equalizedBins(16);
    eq.apply(frequencyBins, equalizedBins);

    // Stage 4: Extract band energies
    float bassEnergy = mapper.getBassEnergy(equalizedBins);
    float midEnergy = mapper.getMidEnergy(equalizedBins);

    // Verify all stages produced valid output
    // At least bass and mid should have energy (treble might be attenuated)
    FL_CHECK(bassEnergy > 0.0f);
    FL_CHECK(midEnergy > 0.0f);

    // Check total energy is present
    float totalEnergy = 0.0f;
    for (size i = 0; i < equalizedBins.size(); ++i) {
        totalEnergy += equalizedBins[i];
    }
    FL_CHECK(totalEnergy > 0.0f);

    // Verify statistics were tracked
    const auto& mapperStats = mapper.getStats();
    FL_CHECK(mapperStats.binMappingCount > 0);

    const auto& eqStats = eq.getStats();
    FL_CHECK(eqStats.applicationsCount > 0);
    FL_CHECK(eqStats.lastMakeupGain > 0.0f);
}
