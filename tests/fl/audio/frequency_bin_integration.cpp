// standalone test
#include "fl/audio/frequency_bin_mapper.h"
#include "fl/audio/spectral_equalizer.h"
#include "fl/fft.h"
#include "fl/stl/vector.h"
#include "fl/stl/shared_ptr.h"
#include "fl/stl/math.h"
#include "fl/math_macros.h"
#include "test.h"

using namespace fl;

// Helper: Generate synthetic audio with specific frequency content
static vector<i16> generateTone(size count, float frequency, float sampleRate, i16 amplitude) {
    vector<i16> samples;
    samples.reserve(count);
    for (size i = 0; i < count; ++i) {
        float phase = 2.0f * FL_M_PI * frequency * static_cast<float>(i) / sampleRate;
        i16 sample = static_cast<i16>(amplitude * fl::sinf(phase));
        samples.push_back(sample);
    }
    return samples;
}

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
