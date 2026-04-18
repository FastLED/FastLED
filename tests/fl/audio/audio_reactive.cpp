#include "fl/audio/audio_reactive.h"
#include "fl/audio/audio_processor.h"
#include "fl/audio/detector/equalizer.h"
#include "fl/audio/input.h"
#include "tests/fl/audio/test_helpers.hpp"
#include "fl/audio/audio.h"
#include "fl/stl/circular_buffer.h"
#include "fl/math/math.h"
#include "fl/math/math.h"
#include "fl/stl/shared_ptr.h"
#include "fl/stl/span.h"
#include "fl/stl/stdint.h"
#include "test.h"

FL_TEST_FILE(FL_FILEPATH) {

using namespace fl;
using namespace fl::test;

FL_TEST_CASE("audio::Reactive basic functionality") {
    // Test basic initialization
    audio::Reactive audio;
    audio::ReactiveConfig config;
    config.sampleRate = 22050;
    config.gain = 128;
    // AGC removed — gain is now controlled via Processor::setGain()
    
    audio.begin(config);
    
    // Check initial state
    const audio::Data& data = audio.getData();
    FL_CHECK(data.volume == 0.0f);
    FL_CHECK(data.volumeRaw == 0.0f);
    FL_CHECK_FALSE(data.beatDetected);
    
    // Test adding samples - Create audio::Sample and add it
    // Reduced from 1000 to 500 samples for performance (still provides excellent coverage)
    fl::vector<int16_t> samples;
    samples.reserve(500);

    for (int i = 0; i < 500; ++i) {
        // Generate a simple sine wave sample
        float phase = 2.0f * FL_M_PI * 1000.0f * i / 22050.0f; // 1kHz
        int16_t sample = static_cast<int16_t>(8000.0f * fl::sinf(phase));
        samples.push_back(sample);
    }
    
    // Create audio::Sample from our generated samples with timestamp
    audio::SampleImplPtr impl = fl::make_shared<audio::SampleImpl>();
    uint32_t testTimestamp = 1234567; // Test timestamp value
    impl->assign(samples.begin(), samples.end(), testTimestamp);
    audio::Sample audioSample(impl);
    
    // Process the audio sample directly (timestamp comes from audio::Sample)
    audio.processSample(audioSample);
    
    // Check that we detected some audio
    const audio::Data& processedData = audio.getData();
    FL_CHECK(processedData.volume > 0.0f);
    FL_CHECK_LE(processedData.volume, 1.0f);
    FL_CHECK_LE(processedData.volumeRaw, 1.0f);
    FL_CHECK_LE(processedData.peak, 1.0f);

    // Verify that the timestamp was properly captured from the audio::Sample
    FL_CHECK(processedData.timestamp == testTimestamp);
    
    // Verify that the audio::Sample correctly stores and returns its timestamp
    FL_CHECK(audioSample.timestamp() == testTimestamp);
}

FL_TEST_CASE("audio::Reactive convenience functions") {
    audio::Reactive audio;
    audio::ReactiveConfig config;
    config.sampleRate = 22050;
    audio.begin(config);
    
    // Test convenience accessors don't crash
    float volume = audio.getVolume();
    float bass = audio.getBass();
    float mid = audio.getMid();
    float treble = audio.getTreble();
    bool beat = audio.isBeat();
    
    FL_CHECK(volume >= 0.0f);
    FL_CHECK(bass >= 0.0f);
    FL_CHECK(mid >= 0.0f);
    FL_CHECK(treble >= 0.0f);
    // beat can be true or false, just check it doesn't crash
    (void)beat; // Suppress unused variable warning
}

FL_TEST_CASE("audio::Reactive enhanced beat detection") {
    audio::Reactive audio;
    audio::ReactiveConfig config;
    // Use 44100 Hz to match Sample::fft() default sample rate.
    // Sample::fft() currently hardcodes 44100 Hz (see TODO in audio.cpp.hpp).
    config.sampleRate = 44100;
    config.enableSpectralFlux = true;
    config.enableMultiBand = true;
    config.spectralFluxThreshold = 0.05f;
    config.bassThreshold = 0.1f;
    config.midThreshold = 0.08f;
    config.trebleThreshold = 0.06f;

    audio.begin(config);

    // Test enhanced beat detection accessors
    bool bassBeat = audio.isBassBeat();
    bool midBeat = audio.isMidBeat();
    bool trebleBeat = audio.isTrebleBeat();
    float spectralFlux = audio.getSpectralFlux();
    float bassEnergy = audio.getBassEnergy();
    float midEnergy = audio.getMidEnergy();
    float trebleEnergy = audio.getTrebleEnergy();

    // Initial state should be false/zero
    FL_CHECK_FALSE(bassBeat);
    FL_CHECK_FALSE(midBeat);
    FL_CHECK_FALSE(trebleBeat);
    FL_CHECK_EQ(spectralFlux, 0.0f);
    FL_CHECK_EQ(bassEnergy, 0.0f);
    FL_CHECK_EQ(midEnergy, 0.0f);
    FL_CHECK_EQ(trebleEnergy, 0.0f);

    // Create a bass-heavy sample (low frequency)
    // Use 200 Hz which is within the CQ kernel range (fmin=90 Hz)
    // and 512 samples to match audio::fft::FFT default sample count
    fl::vector<int16_t> bassySamples;
    bassySamples.reserve(512);

    for (int i = 0; i < 512; ++i) {
        // Generate a low frequency sine wave (200Hz - within CQ bass range)
        float phase = 2.0f * FL_M_PI * 200.0f * i / 44100.0f;
        int16_t sample = static_cast<int16_t>(16000.0f * fl::sinf(phase));
        bassySamples.push_back(sample);
    }
    
    // Create audio::Sample
    audio::SampleImplPtr impl = fl::make_shared<audio::SampleImpl>();
    uint32_t timestamp = 1000;
    impl->assign(bassySamples.begin(), bassySamples.end(), timestamp);
    audio::Sample bassySample(impl);
    
    // Process the sample
    audio.processSample(bassySample);
    
    // Check that we detected some bass energy
    const audio::Data& data = audio.getData();
    FL_CHECK(data.bassEnergy > 0.0f);
    FL_CHECK(data.spectralFlux >= 0.0f);
}

FL_TEST_CASE("audio::Reactive multi-band beat detection") {
    audio::Reactive audio;
    audio::ReactiveConfig config;
    config.enableMultiBand = true;
    config.bassThreshold = 0.05f;  // Lower thresholds for testing
    config.midThreshold = 0.05f;
    config.trebleThreshold = 0.05f;
    
    audio.begin(config);
    
    // Create samples with increasing amplitude to trigger beat detection
    // Reduced from 1000 to 500 samples for performance (still provides excellent coverage)
    fl::vector<int16_t> loudSamples;
    loudSamples.reserve(500);

    for (int i = 0; i < 500; ++i) {
        // Create a multi-frequency signal that should trigger beats
        float bassPhase = 2.0f * FL_M_PI * 60.0f * i / 22050.0f;   // Bass
        float midPhase = 2.0f * FL_M_PI * 1000.0f * i / 22050.0f;  // Mid
        float treblePhase = 2.0f * FL_M_PI * 5000.0f * i / 22050.0f; // Treble

        float amplitude = 20000.0f; // High amplitude to trigger beats
        float combined = fl::sinf(bassPhase) + fl::sinf(midPhase) + fl::sinf(treblePhase);
        int16_t sample = static_cast<int16_t>(amplitude * combined / 3.0f);
        loudSamples.push_back(sample);
    }
    
    // Create audio::Sample
    audio::SampleImplPtr impl = fl::make_shared<audio::SampleImpl>();
    uint32_t timestamp = 2000;
    impl->assign(loudSamples.begin(), loudSamples.end(), timestamp);
    audio::Sample loudSample(impl);
    
    // Process a quiet sample first to establish baseline
    fl::vector<int16_t> quietSamples(1000, 100); // Very quiet
    audio::SampleImplPtr quietImpl = fl::make_shared<audio::SampleImpl>();
    quietImpl->assign(quietSamples.begin(), quietSamples.end(), 1500);
    audio::Sample quietSample(quietImpl);
    audio.processSample(quietSample);
    
    // Now process loud sample (should trigger beats due to energy increase)
    audio.processSample(loudSample);
    
    // Check that energies were calculated
    FL_CHECK(audio.getBassEnergy() > 0.0f);
    FL_CHECK(audio.getMidEnergy() > 0.0f);
    FL_CHECK(audio.getTrebleEnergy() > 0.0f);
}

FL_TEST_CASE("audio::Reactive spectral flux detection") {
    audio::Reactive audio;
    audio::ReactiveConfig config;
    config.enableSpectralFlux = true;
    config.spectralFluxThreshold = 0.01f; // Low threshold for testing
    
    audio.begin(config);
    
    // Create two different samples to generate spectral flux
    // Reduced from 1000 to 500 samples for performance (still provides excellent coverage)
    fl::vector<int16_t> sample1, sample2;
    sample1.reserve(500);
    sample2.reserve(500);

    // First sample - single frequency
    for (int i = 0; i < 500; ++i) {
        float phase = 2.0f * FL_M_PI * 440.0f * i / 22050.0f; // A note
        int16_t sample = static_cast<int16_t>(8000.0f * fl::sinf(phase));
        sample1.push_back(sample);
    }

    // Second sample - different frequency (should create spectral flux)
    for (int i = 0; i < 500; ++i) {
        float phase = 2.0f * FL_M_PI * 880.0f * i / 22050.0f; // A note one octave higher
        int16_t sample = static_cast<int16_t>(8000.0f * fl::sinf(phase));
        sample2.push_back(sample);
    }
    
    // Process first sample
    audio::SampleImplPtr impl1 = fl::make_shared<audio::SampleImpl>();
    impl1->assign(sample1.begin(), sample1.end(), 3000);
    audio::Sample audioSample1(impl1);
    audio.processSample(audioSample1);
    
    float firstFlux = audio.getSpectralFlux();
    
    // Process second sample (different frequency content should create flux)
    audio::SampleImplPtr impl2 = fl::make_shared<audio::SampleImpl>();
    impl2->assign(sample2.begin(), sample2.end(), 3100);
    audio::Sample audioSample2(impl2);
    audio.processSample(audioSample2);
    
    float secondFlux = audio.getSpectralFlux();
    
    // Should have detected spectral flux due to frequency change
    FL_CHECK(secondFlux >= 0.0f);
    
    // Flux should have increased or stayed the same from processing different content
    (void)firstFlux; // Suppress unused variable warning
}

FL_TEST_CASE("audio::Reactive perceptual weighting") {
    audio::Reactive audio;
    audio::ReactiveConfig config;
    config.sampleRate = 22050;
    
    audio.begin(config);
    
    // Create a test sample
    // Reduced from 1000 to 500 samples for performance (still provides excellent coverage)
    fl::vector<int16_t> samples;
    samples.reserve(500);

    for (int i = 0; i < 500; ++i) {
        float phase = 2.0f * FL_M_PI * 1000.0f * i / 22050.0f;
        int16_t sample = static_cast<int16_t>(8000.0f * fl::sinf(phase));
        samples.push_back(sample);
    }
    
    audio::SampleImplPtr impl = fl::make_shared<audio::SampleImpl>();
    impl->assign(samples.begin(), samples.end(), 4000);
    audio::Sample audioSample(impl);
    
    // Process sample (perceptual weighting should be applied automatically)
    audio.processSample(audioSample);
    
    // Check that processing completed without errors
    const audio::Data& data = audio.getData();
    FL_CHECK(data.volume >= 0.0f);
    FL_CHECK(data.timestamp == 4000);
    
    // Frequency bins should have been processed
    bool hasNonZeroBins = false;
    for (int i = 0; i < 16; ++i) {
        if (data.frequencyBins[i] > 0.0f) {
            hasNonZeroBins = true;
            break;
        }
    }
    FL_CHECK(hasNonZeroBins);
}

FL_TEST_CASE("audio::Reactive configuration validation") {
    audio::Reactive audio;
    audio::ReactiveConfig config;
    
    // Test different configuration combinations
    config.enableSpectralFlux = false;
    config.enableMultiBand = false;
    audio.begin(config);
    
    // Should work without enhanced features
    fl::vector<int16_t> samples(1000, 1000);
    audio::SampleImplPtr impl = fl::make_shared<audio::SampleImpl>();
    impl->assign(samples.begin(), samples.end(), 5000);
    audio::Sample audioSample(impl);
    
    audio.processSample(audioSample);
    
    // Basic functionality should still work
    FL_CHECK(audio.getVolume() >= 0.0f);
    FL_CHECK_FALSE(audio.isBassBeat()); // Should be false when multi-band is disabled
    FL_CHECK_FALSE(audio.isMidBeat());
    FL_CHECK_FALSE(audio.isTrebleBeat());
}

FL_TEST_CASE("audio::Reactive circular_buffer functionality") {
    // Test the circular_buffer template directly
    circular_buffer<float, 8> buffer;
    
    FL_CHECK(buffer.empty());
    FL_CHECK_FALSE(buffer.full());
    FL_CHECK_EQ(buffer.size(), 0);
    FL_CHECK_EQ(buffer.capacity(), 8);
    
    // Test pushing elements
    for (int i = 0; i < 5; ++i) {
        buffer.push(static_cast<float>(i));
    }
    
    FL_CHECK_EQ(buffer.size(), 5);
    FL_CHECK_FALSE(buffer.full());
    FL_CHECK_FALSE(buffer.empty());
    
    // Test popping elements
    float value;
    FL_CHECK(buffer.pop(value));
    FL_CHECK_EQ(value, 0.0f);
    FL_CHECK_EQ(buffer.size(), 4);
    
    // Fill buffer completely
    for (int i = 5; i < 12; ++i) {
        buffer.push(static_cast<float>(i));
    }
    
    FL_CHECK(buffer.full());
    FL_CHECK_EQ(buffer.size(), 8);
    
    // Test that old elements are overwritten
    buffer.push(100.0f);
    FL_CHECK(buffer.full());
    FL_CHECK_EQ(buffer.size(), 8);
    
    // Clear buffer
    buffer.clear();
    FL_CHECK(buffer.empty());
    FL_CHECK_EQ(buffer.size(), 0);
}

FL_TEST_CASE("audio::Sample - default constructor") {
    audio::Sample sample;
    FL_CHECK_FALSE(sample.isValid());
    FL_CHECK_EQ(sample.size(), 0u);
}

FL_TEST_CASE("audio::Sample - span constructor") {
    fl::vector<fl::i16> data;
    data.reserve(512);
    for (int i = 0; i < 512; ++i) {
        float phase = 2.0f * 3.14159265f * 440.0f * i / 44100.0f;
        data.push_back(static_cast<fl::i16>(16000.0f * fl::sinf(phase)));
    }
    audio::Sample sample(data, 12345);
    FL_CHECK(sample.isValid());
    FL_CHECK_EQ(sample.size(), 512u);
    FL_CHECK_EQ(sample.timestamp(), 12345u);
    FL_CHECK_EQ(sample.pcm().size(), 512u);
}

FL_TEST_CASE("audio::Sample - zcf for sine wave vs noise") {
    // Pure sine → low ZCF
    fl::vector<fl::i16> sineSamples;
    sineSamples.reserve(512);
    for (int i = 0; i < 512; ++i) {
        float phase = 2.0f * 3.14159265f * 440.0f * i / 44100.0f;
        sineSamples.push_back(static_cast<fl::i16>(16000.0f * fl::sinf(phase)));
    }
    audio::Sample sineSample(sineSamples, 0);
    float sineZcf = sineSample.zcf();
    FL_CHECK_GE(sineZcf, 0.0f);
    FL_CHECK_LT(sineZcf, 0.1f);

    // High frequency noise → high ZCF
    fl::vector<fl::i16> noiseSamples;
    noiseSamples.reserve(512);
    for (int i = 0; i < 512; ++i) {
        noiseSamples.push_back(static_cast<fl::i16>((i % 2 == 0) ? 10000 : -10000));
    }
    audio::Sample noiseSample(noiseSamples, 0);
    float noiseZcf = noiseSample.zcf();
    FL_CHECK_GT(noiseZcf, 0.3f);
}

FL_TEST_CASE("audio::Sample - rms for known signal") {
    // Silence → RMS = 0
    fl::vector<fl::i16> silence(512, 0);
    audio::Sample silentSample(silence, 0);
    FL_CHECK_EQ(silentSample.rms(), 0.0f);

    // Constant amplitude ±8000 → RMS = 8000
    fl::vector<fl::i16> constant;
    constant.reserve(512);
    for (int i = 0; i < 512; ++i) {
        constant.push_back(static_cast<fl::i16>((i % 2 == 0) ? 8000 : -8000));
    }
    audio::Sample constSample(constant, 0);
    float rms = constSample.rms();
    FL_CHECK_GT(rms, 7000.0f);
    FL_CHECK_LT(rms, 9000.0f);
}

FL_TEST_CASE("audio::Sample - fft produces output") {
    fl::vector<fl::i16> data;
    data.reserve(512);
    for (int i = 0; i < 512; ++i) {
        float phase = 2.0f * 3.14159265f * 1000.0f * i / 44100.0f;
        data.push_back(static_cast<fl::i16>(16000.0f * fl::sinf(phase)));
    }
    audio::Sample sample(data, 0);
    audio::fft::Bins bins(16);
    sample.fft(&bins);
    FL_CHECK_GT(bins.raw().size(), 0u);
}

FL_TEST_CASE("audio::Sample - copy and equality") {
    fl::vector<fl::i16> data;
    data.reserve(100);
    for (int i = 0; i < 100; ++i) {
        data.push_back(static_cast<fl::i16>(i * 100));
    }
    audio::Sample original(data, 999);
    audio::Sample copy(original);
    FL_CHECK(copy.isValid());
    FL_CHECK(original == copy);
    FL_CHECK_FALSE(original != copy);
    FL_CHECK_EQ(copy.timestamp(), 999u);
    FL_CHECK_EQ(copy.size(), 100u);

    audio::Sample empty;
    FL_CHECK(original != empty);
}
FL_TEST_CASE("audio::Reactive - full pipeline DC removal and gain") {
    audio::Reactive audio;
    audio::ReactiveConfig config;
    config.sampleRate = 22050;
    config.enableSignalConditioning = true;

    config.enableNoiseFloorTracking = true;
    config.enableLogBinSpacing = true;

    audio.begin(config);

    // Signal: 500 amplitude sine at 1kHz with 3000 DC offset
    for (int iter = 0; iter < 20; ++iter) {
        vector<i16> samples;
        samples.reserve(512);
        for (size i = 0; i < 512; ++i) {
            float phase = 2.0f * FL_M_PI * 1000.0f * static_cast<float>(i) / 22050.0f;
            i32 val = static_cast<i32>(500.0f * fl::sinf(phase)) + 3000;
            if (val > 32767) val = 32767;
            if (val < -32768) val = -32768;
            samples.push_back(static_cast<i16>(val));
        }
        audio::Sample audioSample = createSample(samples, iter * 100);
        audio.processSample(audioSample);
    }

    // Signal conditioning should have removed DC and processed signal
    const auto& scStats = audio.getSignalConditionerStats();
    FL_CHECK_GT(scStats.samplesProcessed, 0u);

    // Volume should be measurable
    const auto& data = audio.getData();
    FL_CHECK_GT(data.volume, 0.0f);
}

// INT-2: Pipeline with silence - no NaN, no crash
FL_TEST_CASE("audio::Reactive - silence pipeline no NaN") {
    audio::Reactive audio;
    audio::ReactiveConfig config;
    config.sampleRate = 22050;
    config.enableSignalConditioning = true;

    config.enableNoiseFloorTracking = true;
    audio.begin(config);

    // Feed 20 frames of silence
    for (int iter = 0; iter < 20; ++iter) {
        vector<i16> silence(512, 0);
        audio::Sample audioSample = createSample(silence, iter * 100);
        audio.processSample(audioSample);
    }

    // Should not crash, volume should be zero or near-zero
    const auto& data = audio.getData();
    FL_CHECK_LE(data.volume, 1.0f);
    FL_CHECK_FALSE(data.volume != data.volume); // NaN check
}

// INT-3: Musical beat detection actually processes audio
FL_TEST_CASE("audio::Reactive - musical beat detection processes audio") {
    audio::Reactive audio;
    audio::ReactiveConfig config;
    config.sampleRate = 22050;
    config.enableMusicalBeatDetection = true;
    config.enableSpectralFlux = true;
    config.musicalBeatMinBPM = 60.0f;
    config.musicalBeatMaxBPM = 180.0f;
    config.musicalBeatConfidence = 0.3f;
    audio.begin(config);

    // Feed actual audio signal (not just config check!)
    // Process 20 frames of a 440 Hz tone with varying amplitude
    for (int iter = 0; iter < 20; ++iter) {
        float amplitude = (iter % 4 == 0) ? 15000.0f : 1000.0f;
        vector<i16> samples = generateSineWave(512, 440.0f, 22050.0f,
                                               static_cast<i16>(amplitude));
        audio::Sample audioSample = createSample(samples, iter * 23);
        audio.processSample(audioSample);
    }

    // Verify actual processing happened (not just config)
    const auto& data = audio.getData();
    FL_CHECK_GT(data.volume, 0.0f);

    // Frequency bins should have energy from the 440 Hz tone
    bool hasBinData = false;
    for (int i = 0; i < 16; ++i) {
        if (data.frequencyBins[i] > 0.0f) {
            hasBinData = true;
            break;
        }
    }
    FL_CHECK(hasBinData);
}

// INT-4: Multi-band beat detection actually processes audio
FL_TEST_CASE("audio::Reactive - multi-band beat detection processes audio") {
    audio::Reactive audio;
    audio::ReactiveConfig config;
    config.sampleRate = 22050;
    config.enableMultiBandBeats = true;
    config.enableSpectralFlux = true;
    config.bassThreshold = 0.15f;
    config.midThreshold = 0.12f;
    config.trebleThreshold = 0.08f;
    audio.begin(config);

    // Feed bass-heavy signal alternating with silence (simulating kick drum)
    for (int iter = 0; iter < 20; ++iter) {
        vector<i16> samples;
        if (iter % 5 == 0) {
            // Bass burst
            samples = generateSineWave(512, 100.0f, 22050.0f, 15000);
        } else {
            // Quiet
            samples = generateSineWave(512, 100.0f, 22050.0f, 500);
        }
        audio::Sample audioSample = createSample(samples, iter * 23);
        audio.processSample(audioSample);
    }

    // Verify actual processing (not just config check)
    const auto& data = audio.getData();
    FL_CHECK_GT(data.volume, 0.0f);

    // Bass energy should be present
    FL_CHECK_GT(data.bassEnergy, 0.0f);
}

// Keep: Pipeline with all middleware enabled (tightened)
FL_TEST_CASE("audio::Reactive - all middleware enabled processes correctly") {
    audio::Reactive audio;
    audio::ReactiveConfig config;
    // Sample::fft() hardcodes DefaultSampleRate (44100), so the config
    // must match to avoid frequency-label mismatch (TODO: fix audio::Sample).
    config.sampleRate = 44100;
    config.enableLogBinSpacing = true;
    config.enableSpectralEqualizer = true;
    config.enableSignalConditioning = true;

    config.enableNoiseFloorTracking = true;
    audio.begin(config);

    // Process 10 frames of 700Hz sine at high amplitude.
    // Sample rate must be 44100 to match Sample::fft()'s hardcoded rate.
    // Amplitude 25000 ensures signal passes the noise gate threshold.
    for (int iter = 0; iter < 10; ++iter) {
        vector<i16> samples = generateSineWave(512, 700.0f, 44100.0f, 25000);
        audio::Sample audioSample = createSample(samples, iter * 100);
        audio.processSample(audioSample);
    }

    const auto& data = audio.getData();
    FL_CHECK_GT(data.volume, 0.0f);
    // Verify audio::fft::FFT pipeline produces energy in at least one band.
    // Specific band energies depend on CQ bin layout vs FrequencyBinMapper
    // constants — checking total energy is more robust.
    float totalEnergy = data.bassEnergy + data.midEnergy + data.trebleEnergy;
    FL_CHECK_GT(totalEnergy, 0.0f);

    // All stats should show processing occurred
    const auto& scStats = audio.getSignalConditionerStats();
    FL_CHECK_GT(scStats.samplesProcessed, 0u);

    const auto& nfStats = audio.getNoiseFloorStats();
    FL_CHECK_GT(nfStats.samplesProcessed, 0u);
}

FL_TEST_CASE("audio::Reactive - FrequencyBinMapper is always active") {
    audio::Reactive audio;
    audio::ReactiveConfig config;

    // Verify log bin spacing is enabled by default
    FL_CHECK(config.enableLogBinSpacing == true);

    // Begin with default config
    audio.begin(config);

    // Process a sample to verify mapper works
    vector<i16> samples = generateSineWave(512, 1000.0f, 22050.0f, 8000);
    audio::Sample audioSample = createSample(samples, 1000);
    audio.processSample(audioSample);

    // Verify frequency bins are populated
    const auto& data = audio.getData();
    FL_CHECK(data.volume > 0.0f);

    // Verify frequency bins contain energy from the 1kHz sine
    bool hasBinData = false;
    for (int i = 0; i < 16; ++i) {
        if (data.frequencyBins[i] > 0.0f) {
            hasBinData = true;
            break;
        }
    }
    FL_CHECK(hasBinData);
}

FL_TEST_CASE("audio::Reactive - SpectralEqualizer disabled by default") {
    audio::Reactive audio;
    audio::ReactiveConfig config;

    // Verify spectral equalizer is disabled by default
    FL_CHECK_FALSE(config.enableSpectralEqualizer);

    audio.begin(config);

    // Process a sample - should work without EQ
    vector<i16> samples = generateSineWave(512, 1000.0f, 22050.0f, 8000);
    audio::Sample audioSample = createSample(samples, 1000);
    audio.processSample(audioSample);

    const auto& data = audio.getData();
    FL_CHECK(data.volume > 0.0f);

    bool hasBinData = false;
    for (int i = 0; i < 16; ++i) {
        if (data.frequencyBins[i] > 0.0f) {
            hasBinData = true;
            break;
        }
    }
    FL_CHECK(hasBinData);
}

FL_TEST_CASE("audio::Reactive - Log bin spacing uses sample rate") {
    audio::Reactive audio;
    audio::ReactiveConfig config;
    config.sampleRate = 16000;
    config.enableLogBinSpacing = true;

    audio.begin(config);

    // Generate a sine wave in the mid-frequency range (500 Hz)
    vector<i16> samples = generateSineWave(512, 500.0f, 16000.0f, 10000);
    audio::Sample audioSample = createSample(samples, 2000);
    audio.processSample(audioSample);

    // Verify frequency bins are populated
    const auto& data = audio.getData();
    FL_CHECK(data.volume > 0.0f);

    // Check that at least some bins are non-zero
    bool hasBinData = false;
    for (int i = 0; i < 16; ++i) {
        if (data.frequencyBins[i] > 0.0f) {
            hasBinData = true;
            break;
        }
    }
    FL_CHECK(hasBinData);
}

FL_TEST_CASE("audio::Reactive - Linear bin spacing fallback") {
    audio::Reactive audio;
    audio::ReactiveConfig config;
    config.sampleRate = 22050;
    config.enableLogBinSpacing = false;  // Use linear spacing

    audio.begin(config);

    // Generate a sine wave
    vector<i16> samples = generateSineWave(512, 1000.0f, 22050.0f, 8000);
    audio::Sample audioSample = createSample(samples, 3000);
    audio.processSample(audioSample);

    // Verify frequency bins are populated
    const auto& data = audio.getData();
    FL_CHECK(data.volume > 0.0f);

    // Check that bins contain data
    bool hasBinData = false;
    for (int i = 0; i < 16; ++i) {
        if (data.frequencyBins[i] > 0.0f) {
            hasBinData = true;
            break;
        }
    }
    FL_CHECK(hasBinData);
}

FL_TEST_CASE("audio::Reactive - SpectralEqualizer integration") {
    audio::Reactive audio;
    audio::ReactiveConfig config;
    config.sampleRate = 22050;
    config.enableSpectralEqualizer = true;  // Enable EQ

    audio.begin(config);

    // Generate a sine wave
    vector<i16> samples = generateSineWave(512, 1000.0f, 22050.0f, 8000);
    audio::Sample audioSample = createSample(samples, 4000);
    audio.processSample(audioSample);

    // Verify frequency bins are populated (EQ modifies values but doesn't zero them out)
    const auto& data = audio.getData();
    FL_CHECK(data.volume > 0.0f);

    bool hasBinData = false;
    for (int i = 0; i < 16; ++i) {
        if (data.frequencyBins[i] > 0.0f) {
            hasBinData = true;
            break;
        }
    }
    FL_CHECK(hasBinData);
}

FL_TEST_CASE("audio::Reactive - SpectralEqualizer lazy creation") {
    audio::Reactive audio;
    audio::ReactiveConfig config;
    config.sampleRate = 22050;
    config.enableSpectralEqualizer = false;  // Start with EQ disabled

    audio.begin(config);

    // Process a sample without EQ
    vector<i16> samples1 = generateSineWave(512, 1000.0f, 22050.0f, 8000);
    audio::Sample audioSample1 = createSample(samples1, 5000);
    audio.processSample(audioSample1);

    const auto& data1 = audio.getData();
    FL_CHECK(data1.volume > 0.0f);

    // Now reconfigure with EQ enabled
    config.enableSpectralEqualizer = true;
    audio.begin(config);

    // Process another sample with EQ
    vector<i16> samples2 = generateSineWave(512, 1000.0f, 22050.0f, 8000);
    audio::Sample audioSample2 = createSample(samples2, 6000);
    audio.processSample(audioSample2);

    const auto& data2 = audio.getData();
    FL_CHECK(data2.volume > 0.0f);

    bool hasBinData = false;
    for (int i = 0; i < 16; ++i) {
        if (data2.frequencyBins[i] > 0.0f) {
            hasBinData = true;
            break;
        }
    }
    FL_CHECK(hasBinData);
}

FL_TEST_CASE("audio::Reactive - Band energies use mapper") {
    audio::Reactive audio;
    audio::ReactiveConfig config;
    config.sampleRate = 22050;
    config.enableLogBinSpacing = true;

    audio.begin(config);

    // Generate a low-frequency sine wave (100 Hz) with high amplitude
    // This should produce energy in the bass range
    vector<i16> samples = generateSineWave(512, 100.0f, 22050.0f, 15000);
    audio::Sample audioSample = createSample(samples, 7000);
    audio.processSample(audioSample);

    // Check that bassEnergy > 0
    const auto& data = audio.getData();
    FL_CHECK(data.bassEnergy > 0.0f);

    // Check that getData() contains valid band energies
    // Bass bins (0-1) should have energy
    bool hasBassData = false;
    for (int i = 0; i < 2; ++i) {
        if (data.frequencyBins[i] > 0.0f) {
            hasBassData = true;
            break;
        }
    }
    FL_CHECK(hasBassData);
}

FL_TEST_CASE("audio::Reactive - Multiple frequency ranges") {
    audio::Reactive audio;
    audio::ReactiveConfig config;
    config.sampleRate = 22050;
    config.enableLogBinSpacing = true;

    audio.begin(config);

    // Process bass frequency (100 Hz)
    vector<i16> bassSamples = generateSineWave(512, 100.0f, 22050.0f, 10000);
    audio::Sample bassAudio = createSample(bassSamples, 8000);
    audio.processSample(bassAudio);

    const auto& bassData = audio.getData();
    float bassEnergy1 = bassData.bassEnergy;
    FL_CHECK(bassEnergy1 > 0.0f);

    // Process mid frequency (370 Hz at 22050 sampleRate).
    // Sample::fft() uses 44100 Hz internally, so the audio::fft::FFT interprets
    // this as ~740 Hz, which lands in CQ bins 6-7 (the mid range).
    vector<i16> midSamples = generateSineWave(512, 370.0f, 22050.0f, 15000);
    audio::Sample midAudio = createSample(midSamples, 9000);
    audio.processSample(midAudio);

    const auto& midData = audio.getData();
    float midEnergy = midData.midEnergy;
    FL_CHECK(midEnergy > 0.0f);

    // Process treble frequency (8000 Hz)
    vector<i16> trebleSamples = generateSineWave(512, 8000.0f, 22050.0f, 10000);
    audio::Sample trebleAudio = createSample(trebleSamples, 10000);
    audio.processSample(trebleAudio);

    const auto& trebleData = audio.getData();
    float trebleEnergy = trebleData.trebleEnergy;
    FL_CHECK(trebleEnergy > 0.0f);
}

FL_TEST_CASE("audio::Reactive - Frequency bin consistency with mapper") {
    audio::Reactive audio;
    audio::ReactiveConfig config;
    config.sampleRate = 22050;
    config.enableLogBinSpacing = true;

    audio.begin(config);

    // Process a full-spectrum signal (mix of frequencies)
    vector<i16> complexSamples = generateSineWave(512, 100.0f, 22050.0f, 3000);
    vector<i16> mid = generateSineWave(512, 1000.0f, 22050.0f, 3000);
    vector<i16> treble = generateSineWave(512, 5000.0f, 22050.0f, 3000);

    // Mix signals
    for (size i = 0; i < 512; ++i) {
        i32 mixed = static_cast<i32>(complexSamples[i]) +
                    static_cast<i32>(mid[i]) +
                    static_cast<i32>(treble[i]);
        // Clamp to i16 range
        if (mixed > 32767) mixed = 32767;
        if (mixed < -32768) mixed = -32768;
        complexSamples[i] = static_cast<i16>(mixed);
    }

    audio::Sample complexAudio = createSample(complexSamples, 11000);
    audio.processSample(complexAudio);

    // Verify all frequency bands have energy
    const auto& data = audio.getData();
    FL_CHECK(data.bassEnergy > 0.0f);
    FL_CHECK(data.midEnergy > 0.0f);
    FL_CHECK(data.trebleEnergy > 0.0f);

    // Verify frequency bins are populated across the spectrum
    int nonZeroBins = 0;
    for (int i = 0; i < 16; ++i) {
        if (data.frequencyBins[i] > 0.0f) {
            nonZeroBins++;
        }
    }
    FL_CHECK(nonZeroBins > 0);
}

FL_TEST_CASE("audio::Reactive - Pipeline with all middleware enabled") {
    audio::Reactive audio;
    audio::ReactiveConfig config;
    // Sample::fft() hardcodes DefaultSampleRate (44100), so the config
    // must match to avoid frequency-label mismatch (TODO: fix audio::Sample).
    config.sampleRate = 44100;
    config.enableLogBinSpacing = true;
    config.enableSpectralEqualizer = true;
    config.enableSignalConditioning = true;

    config.enableNoiseFloorTracking = true;

    audio.begin(config);

    // Process multiple samples to let middleware converge.
    // Use 700Hz sine at 44100 Hz (matches Sample::fft()) with high amplitude.
    for (int iter = 0; iter < 10; ++iter) {
        vector<i16> samples = generateSineWave(512, 700.0f, 44100.0f, 25000);
        audio::Sample audioSample = createSample(samples, iter * 100);
        audio.processSample(audioSample);
    }

    // Verify all components are active
    const auto& data = audio.getData();
    FL_CHECK(data.volume > 0.0f);

    // Verify audio::fft::FFT pipeline produces energy in at least one band
    float totalEnergy = data.bassEnergy + data.midEnergy + data.trebleEnergy;
    FL_CHECK(totalEnergy > 0.0f);

    // Check signal conditioning stats
    const auto& scStats = audio.getSignalConditionerStats();
    FL_CHECK(scStats.samplesProcessed > 0);

    const auto& nfStats = audio.getNoiseFloorStats();
    FL_CHECK(nfStats.samplesProcessed > 0);

    // Check frequency bins
    bool hasBinData = false;
    for (int i = 0; i < 16; ++i) {
        if (data.frequencyBins[i] > 0.0f) {
            hasBinData = true;
            break;
        }
    }
    FL_CHECK(hasBinData);
}
FL_TEST_CASE("audio::Reactive - Signal conditioning integration enabled by default") {
    audio::Reactive audio;
    audio::ReactiveConfig config;
    // Signal conditioning should be enabled by default
    FL_CHECK(config.enableSignalConditioning == true);
    FL_CHECK(config.enableNoiseFloorTracking == true);

    audio.begin(config);

    // Process a sample - should work without signal conditioning
    vector<i16> samples = generateSineWave(1000, 1000.0f, 22050.0f, 8000);
    audio::Sample audioSample = createSample(samples, 1000);
    audio.processSample(audioSample);

    const auto& data = audio.getData();
    FL_CHECK(data.volume > 0.0f);
}

FL_TEST_CASE("audio::Reactive - Enable signal conditioning") {
    audio::Reactive audio;
    audio::ReactiveConfig config;
    config.enableSignalConditioning = true;

    config.enableNoiseFloorTracking = false;
    audio.begin(config);

    // Create sample with DC bias
    vector<i16> biasedSamples = generateSineWave(1000, 1000.0f, 22050.0f, 5000);
    for (size i = 0; i < biasedSamples.size(); ++i) {
        i32 biased = static_cast<i32>(biasedSamples[i]) + 2000; // Add DC bias
        if (biased > 32767) biased = 32767;
        if (biased < -32768) biased = -32768;
        biasedSamples[i] = static_cast<i16>(biased);
    }

    audio::Sample biasedAudio = createSample(biasedSamples, 2000);
    audio.processSample(biasedAudio);

    // Signal conditioning should have removed DC bias
    const auto& scStats = audio.getSignalConditionerStats();
    FL_CHECK(scStats.samplesProcessed > 0);

    // Audio should still be processed
    const auto& data = audio.getData();
    FL_CHECK(data.volume > 0.0f);
}

FL_TEST_CASE("audio::Reactive - Enable auto gain") {
    audio::Reactive audio;
    audio::ReactiveConfig config;
    config.enableSignalConditioning = false;

    config.enableNoiseFloorTracking = false;
    audio.begin(config);

    // Process several quiet samples to let AGC converge
    for (int i = 0; i < 10; ++i) {
        vector<i16> quietSamples = generateSineWave(500, 1000.0f, 22050.0f, 1000);
        audio::Sample quietAudio = createSample(quietSamples, i * 100);
        audio.processSample(quietAudio);
    }

    // Audio should be processed and potentially amplified
    const auto& data = audio.getData();
    FL_CHECK(data.volume >= 0.0f);
}

FL_TEST_CASE("audio::Reactive - Enable noise floor tracking") {
    audio::Reactive audio;
    audio::ReactiveConfig config;
    config.enableSignalConditioning = false;

    config.enableNoiseFloorTracking = true;
    audio.begin(config);

    // Process several samples to build noise floor estimate
    for (int i = 0; i < 10; ++i) {
        vector<i16> samples = generateSineWave(500, 1000.0f, 22050.0f, 3000);
        audio::Sample audioSample = createSample(samples, i * 100);
        audio.processSample(audioSample);
    }

    const auto& nfStats = audio.getNoiseFloorStats();
    FL_CHECK(nfStats.samplesProcessed > 0);
    FL_CHECK(nfStats.currentFloor > 0.0f);

    // Audio should be processed
    const auto& data = audio.getData();
    FL_CHECK(data.volume > 0.0f);
}

FL_TEST_CASE("audio::Reactive - Full signal conditioning pipeline") {
    audio::Reactive audio;
    audio::ReactiveConfig config;
    config.enableSignalConditioning = true;

    config.enableNoiseFloorTracking = true;
    audio.begin(config);

    // Create complex test signal:
    // - Sine wave with DC bias
    // - Varying amplitude
    for (int iter = 0; iter < 20; ++iter) {
        i16 amplitude = static_cast<i16>(2000 + iter * 200); // Gradually increasing
        vector<i16> samples = generateSineWave(500, 1000.0f, 22050.0f, amplitude);

        // Add DC bias
        for (size i = 0; i < samples.size(); ++i) {
            i32 biased = static_cast<i32>(samples[i]) + 1000;
            if (biased > 32767) biased = 32767;
            if (biased < -32768) biased = -32768;
            samples[i] = static_cast<i16>(biased);
        }

        audio::Sample audioSample = createSample(samples, iter * 100);
        audio.processSample(audioSample);
    }

    // Verify all components processed the signal
    const auto& scStats = audio.getSignalConditionerStats();
    FL_CHECK(scStats.samplesProcessed > 0);

    const auto& nfStats = audio.getNoiseFloorStats();
    FL_CHECK(nfStats.samplesProcessed > 0);

    // Audio should be processed
    const auto& data = audio.getData();
    FL_CHECK(data.volume > 0.0f);

    // Verify signal conditioning stats
    FL_CHECK(scStats.dcOffset != 0);  // Should have detected DC offset
}

FL_TEST_CASE("audio::Reactive - Stats pointers null when disabled") {
    audio::Reactive audio;
    audio::ReactiveConfig config;
    config.enableSignalConditioning = false;

    config.enableNoiseFloorTracking = false;
    audio.begin(config);

    // Process a sample
    vector<i16> samples = generateSineWave(500, 1000.0f, 22050.0f, 5000);
    audio::Sample audioSample = createSample(samples, 1000);
    audio.processSample(audioSample);

    // Stats should still be available (components exist but are disabled)
    const auto& scStats = audio.getSignalConditionerStats();
    const auto& nfStats = audio.getNoiseFloorStats();

    // Components are disabled so they shouldn't have processed samples
    FL_CHECK_EQ(scStats.samplesProcessed, 0u);
    FL_CHECK_EQ(nfStats.samplesProcessed, 0u);
}

FL_TEST_CASE("audio::Reactive - Signal conditioning with spikes") {
    audio::Reactive audio;
    audio::ReactiveConfig config;
    config.enableSignalConditioning = true;
    audio.begin(config);

    // Create signal with injected spikes
    vector<i16> samples = generateSineWave(1000, 1000.0f, 22050.0f, 3000);
    for (size i = 0; i < 100; i += 10) {
        samples[i] = 25000;  // Inject spikes
    }

    audio::Sample audioSample = createSample(samples, 3000);
    audio.processSample(audioSample);

    // Verify spikes were detected and rejected
    const auto& scStats = audio.getSignalConditionerStats();
    FL_CHECK(scStats.spikesRejected > 0);

    // Audio should still be processed (spikes filtered out)
    const auto& data = audio.getData();
    FL_CHECK(data.volume > 0.0f);
}

FL_TEST_CASE("audio::Reactive - Backward compatibility") {
    // Test that existing code without signal conditioning still works

    audio::Reactive audio;
    audio::ReactiveConfig config;
    config.sampleRate = 22050;
    config.gain = 128;
    // AGC removed — gain is now controlled via Processor::setGain()  // Use old AGC, not new audio::AutoGain
    // Don't enable new signal conditioning features
    audio.begin(config);

    // Process samples the old way
    vector<i16> samples = generateSineWave(1000, 1000.0f, 22050.0f, 8000);
    audio::Sample audioSample = createSample(samples, 4000);
    audio.processSample(audioSample);

    // Should work exactly as before
    const auto& data = audio.getData();
    FL_CHECK(data.volume > 0.0f);
    FL_CHECK(data.frequencyBins[0] >= 0.0f);
}

FL_TEST_CASE("audio::Processor - polling getters") {
    // Test that audio::Processor polling getters return valid values
    audio::Processor proc;

    // Before any update, getters should return defaults (0.0f / false)
    FL_CHECK_EQ(proc.getVocalConfidence(), 0.0f);
    FL_CHECK_EQ(proc.getBeatConfidence(), 0.0f);
    FL_CHECK_EQ(proc.getEnergy(), 0.0f);
    FL_CHECK_EQ(proc.getPeakLevel(), 0.0f);
    FL_CHECK_EQ(proc.getBassLevel(), 0.0f);
    FL_CHECK_EQ(proc.getMidLevel(), 0.0f);
    FL_CHECK_EQ(proc.getTrebleLevel(), 0.0f);
    // Note: isSilent() may be false before any data is processed
    // since the detector needs samples to determine silence state
    FL_CHECK_EQ(proc.getTransientStrength(), 0.0f);
    FL_CHECK_FALSE(proc.isCrescendo());
    FL_CHECK_FALSE(proc.isDiminuendo());
    FL_CHECK_EQ(proc.getPitchConfidence(), 0.0f);
    FL_CHECK_EQ(proc.getTempoConfidence(), 0.0f);
    FL_CHECK_EQ(proc.getBuildupIntensity(), 0.0f);
    FL_CHECK_FALSE(proc.isKick());
    FL_CHECK_FALSE(proc.isSnare());
    FL_CHECK_FALSE(proc.isHiHat());
    FL_CHECK_FALSE(proc.isTom());
    FL_CHECK_EQ(proc.getNoteConfidence(), 0.0f);
    FL_CHECK_EQ(proc.getDownbeatConfidence(), 0.0f);
    FL_CHECK_EQ(proc.getChordConfidence(), 0.0f);
    FL_CHECK_EQ(proc.getKeyConfidence(), 0.0f);

    // Feed a sine wave and verify getters still work without crashing
    fl::vector<int16_t> samples;
    samples.reserve(512);
    for (int i = 0; i < 512; ++i) {
        float phase = 2.0f * FL_M_PI * 440.0f * i / 44100.0f;
        samples.push_back(static_cast<int16_t>(10000.0f * fl::sinf(phase)));
    }
    audio::SampleImplPtr impl = fl::make_shared<audio::SampleImpl>();
    impl->assign(samples.begin(), samples.end(), 1000);
    audio::Sample audioSample(impl);
    proc.update(audioSample);

    // After processing, energy should be non-zero
    FL_CHECK_GT(proc.getEnergy(), 0.0f);
}

FL_TEST_CASE("audio::Reactive - polling getters via audio::Processor") {
    audio::Reactive audio;
    audio::ReactiveConfig config;
    config.sampleRate = 44100;
    audio.begin(config);

    // Polling getters should not crash on initial state
    FL_CHECK_EQ(audio.getVocalConfidence(), 0.0f);
    FL_CHECK_EQ(audio.getBeatConfidence(), 0.0f);
    FL_CHECK_EQ(audio.getEnergyLevel(), 0.0f);
    FL_CHECK_EQ(audio.getBassLevel(), 0.0f);
    FL_CHECK_EQ(audio.getMidLevel(), 0.0f);
    FL_CHECK_EQ(audio.getTrebleLevel(), 0.0f);
    FL_CHECK_FALSE(audio.isKick());
    FL_CHECK_FALSE(audio.isSnare());
    FL_CHECK_FALSE(audio.isHiHat());
    FL_CHECK_EQ(audio.getChordConfidence(), 0.0f);
    FL_CHECK_EQ(audio.getKeyConfidence(), 0.0f);

    // Feed audio and verify getters still work
    fl::vector<int16_t> samples;
    samples.reserve(512);
    for (int i = 0; i < 512; ++i) {
        float phase = 2.0f * FL_M_PI * 440.0f * i / 44100.0f;
        samples.push_back(static_cast<int16_t>(10000.0f * fl::sinf(phase)));
    }
    audio::Sample audioSample = createSample(samples, 1000);
    audio.processSample(audioSample);

    // After processing, energy polling getter should reflect audio
    FL_CHECK_GT(audio.getEnergyLevel(), 0.0f);
}

// ===== audio::Processor gain tests =====

FL_TEST_CASE("audio::Processor - default gain is 1.0") {
    audio::Processor proc;
    FL_CHECK_EQ(proc.getGain(), 1.0f);
}

FL_TEST_CASE("audio::Processor - setGain/getGain roundtrip") {
    audio::Processor proc;
    proc.setGain(3.5f);
    FL_CHECK_EQ(proc.getGain(), 3.5f);
}

FL_TEST_CASE("audio::Processor - gain amplifies energy") {
    // Use onEnergy callback to capture raw RMS (not normalized, which self-normalizes).
    float rawEnergy1 = 0.0f;
    float rawEnergy2 = 0.0f;

    audio::Processor proc1;
    proc1.onEnergy([&](float rms) { rawEnergy1 = rms; });
    vector<i16> samples = generateSineWave(512, 440.0f, 44100.0f, 5000);
    audio::Sample s1 = createSample(samples, 100);
    proc1.update(s1);

    audio::Processor proc2;
    proc2.setGain(5.0f);
    proc2.onEnergy([&](float rms) { rawEnergy2 = rms; });
    audio::Sample s2 = createSample(samples, 200);
    proc2.update(s2);

    FL_CHECK_GT(rawEnergy1, 0.0f);
    FL_CHECK_GT(rawEnergy2, rawEnergy1);
}

FL_TEST_CASE("audio::Processor - gain zero silences") {
    float rawEnergy = -1.0f;
    audio::Processor proc;
    proc.setGain(0.0f);
    proc.onEnergy([&](float rms) { rawEnergy = rms; });
    vector<i16> samples = generateSineWave(512, 440.0f, 44100.0f, 16000);
    audio::Sample s = createSample(samples, 100);
    proc.update(s);
    // Energy should be zero since gain zeroed out all samples
    FL_CHECK_EQ(rawEnergy, 0.0f);
}

FL_TEST_CASE("audio::Processor - gain does not reset on reset()") {
    audio::Processor proc;
    proc.setGain(3.0f);
    proc.reset();
    FL_CHECK_EQ(proc.getGain(), 3.0f);
}

FL_TEST_CASE("audio::Processor - equalizer autoGain default") {
    audio::Processor proc;
    // Before any updates, equalizer autoGain should be 1.0
    FL_CHECK_EQ(proc.getEqAutoGain(), 1.0f);
}

FL_TEST_CASE("audio::Processor - equalizer isSilence on silence") {
    audio::Processor proc;
    // Prime the equalizer detector before feeding samples
    (void)proc.getEqIsSilence();
    // Feed 10 frames of silence
    for (int i = 0; i < 10; ++i) {
        vector<i16> silence(512, 0);
        audio::Sample s = createSample(silence, i * 100);
        proc.update(s);
    }
    FL_CHECK(proc.getEqIsSilence());
}

FL_TEST_CASE("audio::Processor - equalizer isSilence on loud signal") {
    audio::Processor proc;
    // Prime the equalizer detector before feeding samples
    (void)proc.getEqIsSilence();
    // Feed 10 frames of loud sine
    for (int i = 0; i < 10; ++i) {
        vector<i16> samples = generateSineWave(512, 440.0f, 44100.0f, 16000);
        audio::Sample s = createSample(samples, i * 100);
        proc.update(s);
    }
    FL_CHECK_FALSE(proc.getEqIsSilence());
}

FL_TEST_CASE("audio::Processor - equalizer callback has autoGain and isSilence") {
    audio::Processor proc;
    bool callbackFired = false;
    float receivedAutoGain = 0.0f;
    bool receivedIsSilence = true;

    proc.onEqualizer([&](const audio::detector::Equalizer& eq) {
        callbackFired = true;
        receivedAutoGain = eq.volumeNormFactor;
        receivedIsSilence = eq.isSilence;
    });

    // Feed loud sine to trigger non-silence
    for (int i = 0; i < 5; ++i) {
        vector<i16> samples = generateSineWave(512, 440.0f, 44100.0f, 16000);
        audio::Sample s = createSample(samples, i * 100);
        proc.update(s);
    }

    FL_CHECK(callbackFired);
    FL_CHECK_GT(receivedAutoGain, 0.0f);
    FL_CHECK_FALSE(receivedIsSilence);
}

FL_TEST_CASE("audio::Processor - gain stacks with input gain") {
    // Use onEnergy callback to observe raw RMS values.
    float rawEnergy1 = 0.0f;
    float rawEnergy2 = 0.0f;
    vector<i16> samples = generateSineWave(512, 440.0f, 44100.0f, 3000);

    // Sample with applyGain(2) + processor gain=1
    audio::Sample s1 = createSample(samples, 100);
    s1.applyGain(2.0f);

    audio::Processor proc1;
    proc1.onEnergy([&](float rms) { rawEnergy1 = rms; });
    proc1.update(s1);

    // Sample with applyGain(2) + processor gain=3
    audio::Sample s2 = createSample(samples, 200);
    s2.applyGain(2.0f);

    audio::Processor proc2;
    proc2.setGain(3.0f);
    proc2.onEnergy([&](float rms) { rawEnergy2 = rms; });
    proc2.update(s2);

    FL_CHECK_GT(rawEnergy1, 0.0f);
    FL_CHECK_GT(rawEnergy2, rawEnergy1);
}

// ===== audio::Reactive gain tests =====

FL_TEST_CASE("audio::Reactive - default gain is 1.0") {
    audio::Reactive audio;
    audio.begin();
    FL_CHECK_EQ(audio.getGain(), 1.0f);
}

FL_TEST_CASE("audio::Reactive - setGain/getGain roundtrip") {
    audio::Reactive audio;
    audio.begin();
    audio.setGain(2.5f);
    FL_CHECK_EQ(audio.getGain(), 2.5f);
}

FL_TEST_CASE("audio::Reactive - setGain before begin returns 1.0") {
    audio::Reactive audio;
    // Before begin(), no audio::Processor exists yet
    FL_CHECK_EQ(audio.getGain(), 1.0f);
}

FL_TEST_CASE("audio::Reactive - setGain affects processing") {
    // Verify gain has a measurable effect on raw volume.
    // Note: Data::volume uses adaptive normalization (converges to ~1.0
    // regardless of amplitude), so we check volumeRaw which preserves
    // the absolute amplitude relationship.
    vector<i16> samples = generateSineWave(512, 440.0f, 44100.0f, 3000);

    // No external gain
    audio::Reactive audio1;
    audio1.begin();
    audio::Sample s1 = createSample(samples, 100);
    audio1.processSample(s1);
    float vol1 = audio1.getData().volumeRaw;

    // Apply external gain via applyGain before feeding
    audio::Reactive audio2;
    audio2.begin();
    audio::Sample s2 = createSample(samples, 200);
    s2.applyGain(5.0f);
    audio2.processSample(s2);
    float vol2 = audio2.getData().volumeRaw;

    FL_CHECK_GT(vol2, vol1);
}

// ===== audio::Config gain tests =====

FL_TEST_CASE("audio::Config - default gain is 1.0") {
    audio::Config config = audio::Config::CreateInmp441(1, 2, 3, audio::AudioChannel::Left);
    FL_CHECK_EQ(config.getGain(), 1.0f);
}

FL_TEST_CASE("audio::Config - setGain/getGain roundtrip") {
    audio::Config config = audio::Config::CreateInmp441(1, 2, 3, audio::AudioChannel::Left);
    config.setGain(4.0f);
    FL_CHECK_EQ(config.getGain(), 4.0f);
}

FL_TEST_CASE("audio::Config - gain persists across copy") {
    audio::Config config1 = audio::Config::CreateInmp441(1, 2, 3, audio::AudioChannel::Left);
    config1.setGain(3.0f);
    audio::Config config2 = config1;
    FL_CHECK_EQ(config2.getGain(), 3.0f);
}

// ===== Adversarial: very quiet synth music → isSilence tests =====
//
// EqualizerDetector::kSilenceThreshold = 10.0f raw RMS.
// A sine with amplitude A has RMS ≈ A * 0.707.
// So amplitude ≤ 13 → RMS ≈ 9.2 → silence.
// These tests simulate realistic "quiet music" scenarios: multi-frequency
// chords, sweeps, noise beds — all at amplitudes below the threshold —
// and assert the equalizer correctly reports isSilence == true.

FL_TEST_CASE("Silence - quiet single sine detected as silent") {
    // A single 440 Hz tone at amplitude 10 → RMS ≈ 7.07
    audio::Processor proc;
    bool lastIsSilence = false;
    proc.onEqualizer([&](const audio::detector::Equalizer& eq) {
        lastIsSilence = eq.isSilence;
    });

    for (int i = 0; i < 10; ++i) {
        vector<i16> samples = generateSineWave(512, 440.0f, 44100.0f, 10);
        audio::Sample s = createSample(samples, i * 12);
        proc.update(s);
    }
    FL_CHECK(lastIsSilence);
}

FL_TEST_CASE("Silence - quiet multi-frequency chord detected as silent") {
    // Simulate a quiet 3-note chord: 261 Hz (C4) + 329 Hz (E4) + 392 Hz (G4)
    // Each at amplitude 4, combined peak ≈ 12, RMS ≈ 8.5 → silence
    audio::Processor proc;
    bool lastIsSilence = false;
    proc.onEqualizer([&](const audio::detector::Equalizer& eq) {
        lastIsSilence = eq.isSilence;
    });

    for (int iter = 0; iter < 10; ++iter) {
        vector<i16> samples;
        samples.reserve(512);
        for (int i = 0; i < 512; ++i) {
            float t = static_cast<float>(i) / 44100.0f;
            float val = 4.0f * fl::sinf(2.0f * FL_M_PI * 261.63f * t)  // C4
                      + 4.0f * fl::sinf(2.0f * FL_M_PI * 329.63f * t)  // E4
                      + 4.0f * fl::sinf(2.0f * FL_M_PI * 392.00f * t); // G4
            // Clamp (combined peak ≈ ±12, well within i16 range)
            samples.push_back(static_cast<i16>(val));
        }
        audio::Sample s = createSample(samples, iter * 12);
        proc.update(s);
    }
    FL_CHECK(lastIsSilence);
}

FL_TEST_CASE("Silence - quiet bass + treble detected as silent") {
    // Simulate quiet bass (100 Hz) + treble (4000 Hz) at amplitude 5 each
    // Combined RMS ≈ sqrt(2) * 5 * 0.707 ≈ 5.0 → silence
    audio::Processor proc;
    bool lastIsSilence = false;
    proc.onEqualizer([&](const audio::detector::Equalizer& eq) {
        lastIsSilence = eq.isSilence;
    });

    for (int iter = 0; iter < 10; ++iter) {
        vector<i16> samples;
        samples.reserve(512);
        for (int i = 0; i < 512; ++i) {
            float t = static_cast<float>(i) / 44100.0f;
            float val = 5.0f * fl::sinf(2.0f * FL_M_PI * 100.0f * t)
                      + 5.0f * fl::sinf(2.0f * FL_M_PI * 4000.0f * t);
            samples.push_back(static_cast<i16>(val));
        }
        audio::Sample s = createSample(samples, iter * 12);
        proc.update(s);
    }
    FL_CHECK(lastIsSilence);
}

FL_TEST_CASE("Silence - quiet frequency sweep detected as silent") {
    // Simulate a frequency sweep from 200 Hz to 2000 Hz at amplitude 8
    // RMS ≈ 8 * 0.707 ≈ 5.7 → silence
    audio::Processor proc;
    bool lastIsSilence = false;
    proc.onEqualizer([&](const audio::detector::Equalizer& eq) {
        lastIsSilence = eq.isSilence;
    });

    for (int iter = 0; iter < 10; ++iter) {
        vector<i16> samples;
        samples.reserve(512);
        for (int i = 0; i < 512; ++i) {
            // Linear frequency sweep within this frame
            float frac = static_cast<float>(i) / 512.0f;
            float freq = 200.0f + (2000.0f - 200.0f) * frac;
            float t = static_cast<float>(i) / 44100.0f;
            float val = 8.0f * fl::sinf(2.0f * FL_M_PI * freq * t);
            samples.push_back(static_cast<i16>(val));
        }
        audio::Sample s = createSample(samples, iter * 12);
        proc.update(s);
    }
    FL_CHECK(lastIsSilence);
}

FL_TEST_CASE("Silence - quiet noise bed detected as silent") {
    // Simulate very quiet pseudo-random noise at amplitude 6
    // RMS ≈ 6 * 0.577 ≈ 3.5 (uniform noise RMS = amp/sqrt(3)) → silence
    audio::Processor proc;
    bool lastIsSilence = false;
    proc.onEqualizer([&](const audio::detector::Equalizer& eq) {
        lastIsSilence = eq.isSilence;
    });

    for (int iter = 0; iter < 10; ++iter) {
        vector<i16> noise = generateNoise(512, 6);
        audio::Sample s = createSample(noise, iter * 12);
        proc.update(s);
    }
    FL_CHECK(lastIsSilence);
}

FL_TEST_CASE("Silence - quiet pulsed synth detected as silent") {
    // Simulate a synth pad that pulses: alternating between amplitude 12
    // (RMS ≈ 8.5) and amplitude 2 (RMS ≈ 1.4). audio::Both below threshold.
    audio::Processor proc;
    bool allSilent = true;
    proc.onEqualizer([&](const audio::detector::Equalizer& eq) {
        if (!eq.isSilence) allSilent = false;
    });

    for (int iter = 0; iter < 20; ++iter) {
        i16 amp = (iter % 2 == 0) ? 12 : 2;
        vector<i16> samples = generateSineWave(512, 880.0f, 44100.0f, amp);
        audio::Sample s = createSample(samples, iter * 12);
        proc.update(s);
    }
    FL_CHECK(allSilent);
}

FL_TEST_CASE("Silence - boundary: amplitude 13 still silent") {
    // Amplitude 13 → RMS ≈ 9.19 < 10.0 threshold → should be silent
    audio::Processor proc;
    bool lastIsSilence = false;
    proc.onEqualizer([&](const audio::detector::Equalizer& eq) {
        lastIsSilence = eq.isSilence;
    });

    for (int i = 0; i < 10; ++i) {
        vector<i16> samples = generateSineWave(512, 1000.0f, 44100.0f, 13);
        audio::Sample s = createSample(samples, i * 12);
        proc.update(s);
    }
    FL_CHECK(lastIsSilence);
}

FL_TEST_CASE("Silence - loud signal NOT silent") {
    // Disable signal conditioning so the signal passes through cleanly.
    // Amplitude 500 → RMS ≈ 354, well above kSilenceThreshold = 10.
    audio::Processor proc;
    proc.setSignalConditioningEnabled(false);
    bool lastIsSilence = true;
    proc.onEqualizer([&](const audio::detector::Equalizer& eq) {
        lastIsSilence = eq.isSilence;
    });

    for (int i = 0; i < 10; ++i) {
        vector<i16> samples = generateSineWave(512, 1000.0f, 44100.0f, 500);
        audio::Sample s = createSample(samples, i * 12);
        proc.update(s);
    }
    FL_CHECK_FALSE(lastIsSilence);
}

FL_TEST_CASE("Silence - quiet music via audio::Reactive polling getter") {
    // End-to-end: quiet 440 Hz via audio::Reactive → getEqIsSilence()
    // This exercises the audio::Reactive → audio::Processor → audio::detector::EqualizerDetector path.
    audio::Reactive audio;
    audio.begin();
    // Prime the equalizer detector by calling the getter
    (void)audio.getGain(); // ensures audio::Processor exists

    // We need to use the audio::Processor directly for the eq polling getter.
    // audio::Reactive doesn't expose getEqIsSilence(), but audio::Processor does.
    // Test via audio::Processor instead.
    audio::Processor proc;
    (void)proc.getEqIsSilence(); // prime detector

    for (int i = 0; i < 10; ++i) {
        vector<i16> samples = generateSineWave(512, 440.0f, 44100.0f, 8);
        audio::Sample s = createSample(samples, i * 12);
        proc.update(s);
    }
    FL_CHECK(proc.getEqIsSilence());
}

FL_TEST_CASE("Silence - loud music correctly NOT silent via callback") {
    // Sanity check: loud signal should NOT be reported as silent
    audio::Processor proc;
    bool lastIsSilence = true;
    proc.onEqualizer([&](const audio::detector::Equalizer& eq) {
        lastIsSilence = eq.isSilence;
    });

    for (int i = 0; i < 10; ++i) {
        vector<i16> samples = generateSineWave(512, 440.0f, 44100.0f, 16000);
        audio::Sample s = createSample(samples, i * 12);
        proc.update(s);
    }
    FL_CHECK_FALSE(lastIsSilence);
}

// ===== Adversarial: loud synth music tests =====
//
// These tests verify the equalizer produces correct, bounded output for
// loud signals. All normalized values (bass, mid, treble, volume, bins)
// must stay in [0.0, 1.0] — never exceeding 1.0 even for sudden spikes
// or clipping-level signals.
//
// Frequency band mapping (WLED-style, EQ range 60-5120 Hz):
//   Bass:   bins 0-3   (~60-320 Hz)
//   Mid:    bins 4-10  (~320-2560 Hz)
//   Treble: bins 11-15 (~2560-5120 Hz)

/// Helper: assert every field in the audio::detector::Equalizer snapshot is in valid range
static void checkEqBounds(const audio::detector::Equalizer& eq) {
    FL_CHECK_GE(eq.bass, 0.0f);
    FL_CHECK_LE(eq.bass, 1.0f);
    FL_CHECK_GE(eq.mid, 0.0f);
    FL_CHECK_LE(eq.mid, 1.0f);
    FL_CHECK_GE(eq.treble, 0.0f);
    FL_CHECK_LE(eq.treble, 1.0f);
    FL_CHECK_GE(eq.volume, 0.0f);
    FL_CHECK_LE(eq.volume, 1.0f);
    FL_CHECK_GE(eq.zcf, 0.0f);
    FL_CHECK_LE(eq.zcf, 1.0f);
    FL_CHECK_GE(eq.volumeNormFactor, 0.0f);
    // Check all 16 bins
    for (int b = 0; b < audio::detector::Equalizer::kNumBins; ++b) {
        FL_CHECK_GE(eq.bins[b], 0.0f);
        FL_CHECK_LE(eq.bins[b], 1.0f);
    }
    // NaN check: a value != itself means NaN
    FL_CHECK_FALSE(eq.bass != eq.bass);
    FL_CHECK_FALSE(eq.mid != eq.mid);
    FL_CHECK_FALSE(eq.treble != eq.treble);
    FL_CHECK_FALSE(eq.volume != eq.volume);
    FL_CHECK_FALSE(eq.volumeNormFactor != eq.volumeNormFactor);
}

FL_TEST_CASE("Loud - bass sine detected in bass band, all values bounded") {
    // 100 Hz sine at amplitude 16000 → should land in bass bins (0-3)
    audio::Processor proc;
    audio::detector::Equalizer last;
    proc.onEqualizer([&](const audio::detector::Equalizer& eq) {
        checkEqBounds(eq);
        last = eq;
    });

    for (int i = 0; i < 10; ++i) {
        vector<i16> samples = generateSineWave(512, 100.0f, 44100.0f, 16000);
        audio::Sample s = createSample(samples, i * 12);
        proc.update(s);
    }
    FL_CHECK_FALSE(last.isSilence);
    FL_CHECK_GT(last.volume, 0.0f);
    FL_CHECK_GT(last.bass, 0.0f);
}

FL_TEST_CASE("Loud - mid sine detected in mid band, all values bounded") {
    // 1000 Hz sine at amplitude 16000 → should land in mid bins (4-10)
    audio::Processor proc;
    audio::detector::Equalizer last;
    proc.onEqualizer([&](const audio::detector::Equalizer& eq) {
        checkEqBounds(eq);
        last = eq;
    });

    for (int i = 0; i < 10; ++i) {
        vector<i16> samples = generateSineWave(512, 1000.0f, 44100.0f, 16000);
        audio::Sample s = createSample(samples, i * 12);
        proc.update(s);
    }
    FL_CHECK_FALSE(last.isSilence);
    FL_CHECK_GT(last.volume, 0.0f);
    FL_CHECK_GT(last.mid, 0.0f);
}

FL_TEST_CASE("Loud - treble sine detected in treble band, all values bounded") {
    // 4000 Hz sine at amplitude 16000 → should land in treble bins (11-15)
    audio::Processor proc;
    audio::detector::Equalizer last;
    proc.onEqualizer([&](const audio::detector::Equalizer& eq) {
        checkEqBounds(eq);
        last = eq;
    });

    for (int i = 0; i < 10; ++i) {
        vector<i16> samples = generateSineWave(512, 4000.0f, 44100.0f, 16000);
        audio::Sample s = createSample(samples, i * 12);
        proc.update(s);
    }
    FL_CHECK_FALSE(last.isSilence);
    FL_CHECK_GT(last.volume, 0.0f);
    FL_CHECK_GT(last.treble, 0.0f);
}

FL_TEST_CASE("Loud - full spectrum chord, all bands active, all bounded") {
    // Bass (100 Hz) + Mid (1000 Hz) + Treble (4000 Hz), each at amp 8000
    audio::Processor proc;
    audio::detector::Equalizer last;
    proc.onEqualizer([&](const audio::detector::Equalizer& eq) {
        checkEqBounds(eq);
        last = eq;
    });

    for (int iter = 0; iter < 10; ++iter) {
        vector<i16> samples;
        samples.reserve(512);
        for (int i = 0; i < 512; ++i) {
            float t = static_cast<float>(i) / 44100.0f;
            float val = 8000.0f * fl::sinf(2.0f * FL_M_PI * 100.0f * t)
                      + 8000.0f * fl::sinf(2.0f * FL_M_PI * 1000.0f * t)
                      + 8000.0f * fl::sinf(2.0f * FL_M_PI * 4000.0f * t);
            i32 clamped = static_cast<i32>(val);
            if (clamped > 32767) clamped = 32767;
            if (clamped < -32768) clamped = -32768;
            samples.push_back(static_cast<i16>(clamped));
        }
        audio::Sample s = createSample(samples, iter * 12);
        proc.update(s);
    }
    FL_CHECK_FALSE(last.isSilence);
    FL_CHECK_GT(last.bass, 0.0f);
    FL_CHECK_GT(last.mid, 0.0f);
    FL_CHECK_GT(last.treble, 0.0f);
    FL_CHECK_GT(last.volume, 0.0f);
}

FL_TEST_CASE("Loud - clipping-level signal never exceeds 1.0") {
    // Max amplitude i16 (32767) — worst-case clipping. Every frame.
    audio::Processor proc;
    proc.onEqualizer([&](const audio::detector::Equalizer& eq) {
        checkEqBounds(eq);
    });

    for (int i = 0; i < 20; ++i) {
        vector<i16> samples = generateSineWave(512, 440.0f, 44100.0f, 32767);
        audio::Sample s = createSample(samples, i * 12);
        proc.update(s);
    }
    // If we got here without checkEqBounds failing, all values stayed bounded.
}

FL_TEST_CASE("Loud - sudden spike after silence never exceeds 1.0") {
    // 10 frames of silence, then sudden max-amplitude burst.
    // This is the worst-case for adaptive normalization: the running max
    // is near zero, then a huge signal arrives. Bins must still clamp to 1.0.
    audio::Processor proc;
    proc.onEqualizer([&](const audio::detector::Equalizer& eq) {
        checkEqBounds(eq);
    });

    // Silence phase
    for (int i = 0; i < 10; ++i) {
        vector<i16> silence(512, 0);
        audio::Sample s = createSample(silence, i * 12);
        proc.update(s);
    }

    // Sudden loud burst
    for (int i = 10; i < 15; ++i) {
        vector<i16> loud = generateSineWave(512, 440.0f, 44100.0f, 30000);
        audio::Sample s = createSample(loud, i * 12);
        proc.update(s);
    }
}

FL_TEST_CASE("Loud - alternating silence and bursts never exceed 1.0") {
    // Rapidly alternating between silence and max-amplitude.
    // Stresses the adaptive filters with constant transients.
    audio::Processor proc;
    proc.onEqualizer([&](const audio::detector::Equalizer& eq) {
        checkEqBounds(eq);
    });

    for (int i = 0; i < 30; ++i) {
        vector<i16> samples;
        if (i % 2 == 0) {
            samples.assign(512, 0); // silence
        } else {
            samples = generateSineWave(512, 1000.0f, 44100.0f, 32767);
        }
        audio::Sample s = createSample(samples, i * 12);
        proc.update(s);
    }
}

FL_TEST_CASE("Loud - DC offset signal bounded and not silent") {
    // A signal with massive DC offset (16000) + sine. Signal conditioning
    // removes DC, but the remaining signal should be detected as non-silent
    // and all values should stay bounded.
    audio::Processor proc;
    audio::detector::Equalizer last;
    proc.onEqualizer([&](const audio::detector::Equalizer& eq) {
        checkEqBounds(eq);
        last = eq;
    });

    for (int iter = 0; iter < 10; ++iter) {
        vector<i16> samples;
        samples.reserve(512);
        for (int i = 0; i < 512; ++i) {
            float t = static_cast<float>(i) / 44100.0f;
            i32 val = 16000 + static_cast<i32>(10000.0f * fl::sinf(2.0f * FL_M_PI * 440.0f * t));
            if (val > 32767) val = 32767;
            if (val < -32768) val = -32768;
            samples.push_back(static_cast<i16>(val));
        }
        audio::Sample s = createSample(samples, iter * 12);
        proc.update(s);
    }
    FL_CHECK_FALSE(last.isSilence);
    FL_CHECK_GT(last.volume, 0.0f);
}

FL_TEST_CASE("Loud - amplitude sweep ramps up, all values bounded") {
    // Amplitude ramps from 1000 to 32000 over 20 frames.
    // Tests that normalization tracks increasing signal without overflow.
    audio::Processor proc;
    proc.onEqualizer([&](const audio::detector::Equalizer& eq) {
        checkEqBounds(eq);
    });
    audio::detector::Equalizer last;

    for (int iter = 0; iter < 20; ++iter) {
        i16 amp = static_cast<i16>(1000 + iter * 1500);
        vector<i16> samples = generateSineWave(512, 800.0f, 44100.0f, amp);
        audio::Sample s = createSample(samples, iter * 12);
        proc.update(s);
        // Capture the last callback's result
        proc.onEqualizer([&](const audio::detector::Equalizer& eq) { last = eq; });
    }
    FL_CHECK_FALSE(last.isSilence);
    FL_CHECK_GT(last.volume, 0.0f);
}

FL_TEST_CASE("Loud - multi-frequency noise bounded and not silent") {
    // White-ish noise at high amplitude. Should have energy spread
    // across bands, and all values must stay bounded.
    audio::Processor proc;
    audio::detector::Equalizer last;
    proc.onEqualizer([&](const audio::detector::Equalizer& eq) {
        checkEqBounds(eq);
        last = eq;
    });

    for (int iter = 0; iter < 10; ++iter) {
        vector<i16> noise = generateNoise(512, 16000);
        audio::Sample s = createSample(noise, iter * 12);
        proc.update(s);
    }
    FL_CHECK_FALSE(last.isSilence);
    FL_CHECK_GT(last.volume, 0.0f);
    // Noise should have broad spectral content
    FL_CHECK_GT(last.bass, 0.0f);
    FL_CHECK_GT(last.mid, 0.0f);
}

FL_TEST_CASE("Loud - low zcf for pure sine, higher zcf for noise") {
    // A pure sine should have low zero-crossing factor.
    // High-frequency noise should have higher zcf.
    // Use a high-frequency sine (near Nyquist) as the "noisy" signal
    // to avoid triggering signal conditioning artifacts.
    audio::detector::Equalizer sineLast, noiseLast;

    {
        audio::Processor proc;
        proc.onEqualizer([&](const audio::detector::Equalizer& eq) {
            checkEqBounds(eq);
            sineLast = eq;
        });
        for (int i = 0; i < 10; ++i) {
            vector<i16> samples = generateSineWave(512, 440.0f, 44100.0f, 8000);
            audio::Sample s = createSample(samples, i * 12);
            proc.update(s);
        }
    }

    {
        // Use generated noise (pseudo-random) — has high zcf naturally
        audio::Processor proc;
        proc.onEqualizer([&](const audio::detector::Equalizer& eq) {
            checkEqBounds(eq);
            noiseLast = eq;
        });
        for (int i = 0; i < 10; ++i) {
            vector<i16> noise = generateNoise(512, 8000);
            audio::Sample s = createSample(noise, i * 12);
            proc.update(s);
        }
    }

    // audio::Both should be non-silent (signal conditioning passes them through)
    FL_CHECK_FALSE(sineLast.isSilence);
    FL_CHECK_FALSE(noiseLast.isSilence);
    // Noise should have higher zcf than a pure tone
    FL_CHECK_GT(noiseLast.zcf, sineLast.zcf);
}

FL_TEST_CASE("Loud - gain amplified signal still bounded") {
    // Apply gain=10 to an already-loud signal. The gain causes internal
    // clamping but the equalizer output must still be in [0, 1].
    audio::Processor proc;
    proc.setGain(10.0f);
    proc.onEqualizer([&](const audio::detector::Equalizer& eq) {
        checkEqBounds(eq);
    });
    audio::detector::Equalizer last;

    for (int i = 0; i < 10; ++i) {
        vector<i16> samples = generateSineWave(512, 440.0f, 44100.0f, 16000);
        audio::Sample s = createSample(samples, i * 12);
        proc.update(s);
    }
    // Should not crash and all values stay bounded (checked in callback).
}

// Regression test: BUG1 / GitHub #2193 — dominant frequency used linear bin
// centers instead of log-spaced CQ bin centers. For a 1 kHz tone the old
// linear formula reported ~6600-7500 Hz; the correct log formula gives ~1000-1400 Hz.
FL_TEST_CASE("audio::Reactive - dominant frequency uses log-spaced bin centers") {
    audio::Reactive audio;
    audio::ReactiveConfig config;
    config.sampleRate = 44100;
    audio.begin(config);

    // Feed a 1 kHz tone — well-resolved with 512 samples (~11.6 cycles)
    for (int i = 0; i < 20; ++i) {
        audio::Sample s = makeSample(1000.0f, i * 12, 16000.0f, 512, 44100.0f);
        audio.processSample(s);
    }

    const auto& data = audio.getData();
    // With log-spaced bins, a 1 kHz tone lands in a CQ bin with center
    // frequency in the low thousands. The old linear formula mapped the
    // same bin to ~9000+ Hz. Accept anything under 4000 Hz as correct.
    FL_CHECK_GT(data.dominantFrequency, 100.0f);
    FL_CHECK_LT(data.dominantFrequency, 4000.0f);
}

FL_TEST_CASE("audio::detector::EqualizerConfig - default values match previous hardcoded") {
    audio::detector::EqualizerConfig config;
    FL_CHECK(config.minFreq == 90.0f);
    FL_CHECK(config.maxFreq == 5120.0f);
    FL_CHECK(config.smoothing == 0.05f);
    FL_CHECK(config.normAttack == 0.001f);
    FL_CHECK(config.normDecay == 4.0f);
    FL_CHECK(config.silenceThreshold == 10.0f);
}

FL_TEST_CASE("audio::Processor - configureEqualizer changes behavior") {
    audio::Processor proc;

    // Collect equalizer output with default config
    float defaultVolume = 0;
    proc.onEqualizer([&](const audio::detector::Equalizer& eq) {
        defaultVolume = eq.volume;
    });

    // Feed several frames to build up normalization state
    for (int i = 0; i < 20; ++i) {
        vector<i16> samples = generateSineWave(512, 440.0f, 44100.0f, 8000);
        proc.update(createSample(samples, i * 12));
    }
    float capturedDefault = defaultVolume;

    // Now reconfigure with very fast normalization decay
    proc.reset();
    float fastDecayVolume = 0;
    audio::detector::EqualizerConfig fast;
    fast.normDecay = 0.01f;  // Very fast decay
    proc.configureEqualizer(fast);
    proc.onEqualizer([&](const audio::detector::Equalizer& eq) {
        fastDecayVolume = eq.volume;
    });

    for (int i = 0; i < 20; ++i) {
        vector<i16> samples = generateSineWave(512, 440.0f, 44100.0f, 8000);
        proc.update(createSample(samples, i * 12));
    }

    // audio::Both should produce valid output (0-1 range)
    FL_CHECK(capturedDefault >= 0.0f);
    FL_CHECK(capturedDefault <= 1.0f);
    FL_CHECK(fastDecayVolume >= 0.0f);
    FL_CHECK(fastDecayVolume <= 1.0f);
}

FL_TEST_CASE("audio::Processor - configureEqualizer silence threshold") {
    audio::Processor proc;

    // Configure with a very high silence threshold
    audio::detector::EqualizerConfig config;
    config.silenceThreshold = 50000.0f;  // Almost everything is "silent"
    proc.configureEqualizer(config);

    bool isSilence = false;
    proc.onEqualizer([&](const audio::detector::Equalizer& eq) {
        isSilence = eq.isSilence;
    });

    // Feed moderate-amplitude audio
    for (int i = 0; i < 5; ++i) {
        vector<i16> samples = generateSineWave(512, 440.0f, 44100.0f, 8000);
        proc.update(createSample(samples, i * 12));
    }

    // With threshold at 50000, moderate audio should be detected as silent
    FL_CHECK(isSilence == true);

    // Now reconfigure with a very low threshold
    audio::detector::EqualizerConfig lowThresh;
    lowThresh.silenceThreshold = 1.0f;
    proc.configureEqualizer(lowThresh);

    for (int i = 0; i < 5; ++i) {
        vector<i16> samples = generateSineWave(512, 440.0f, 44100.0f, 8000);
        proc.update(createSample(samples, i * 12));
    }

    // With threshold at 1.0, moderate audio should NOT be silent
    FL_CHECK(isSilence == false);
}

FL_TEST_CASE("audio::Reactive - spectral metrics decay to zero on silence (FastLED#2253)") {
    // Drive Reactive with a loud 440 Hz sine so dominantFrequency / magnitude
    // / spectralFlux all take on non-zero values, then switch to zero-PCM
    // with Context::setSilent(true). The SilenceEnvelope (tau=0.2s) should
    // snap the three fields to ~0 within 1 second (5 * tau).
    audio::Reactive reactive;
    audio::ReactiveConfig config;
    config.sampleRate = 44100;
    config.enableNoiseFloorTracking = true;  // Required for isSilent() to fire.
    reactive.begin(config);

    // Phase 1: 1 s of loud 440 Hz sine at full-scale amplitude.
    constexpr fl::size kSamplesPerFrame = 512;
    constexpr float kSampleRate = 44100.0f;
    constexpr int kFramesPerSecond =
        static_cast<int>(kSampleRate / static_cast<float>(kSamplesPerFrame));  // ~86

    for (int i = 0; i < kFramesPerSecond; ++i) {
        audio::Sample s = makeSample(440.0f, i * 12, 16000.0f,
                                     kSamplesPerFrame, kSampleRate);
        reactive.processSample(s);
    }

    // Sanity: all three metrics should be non-trivial after loud drive.
    {
        const auto& data = reactive.getData();
        FL_CHECK_GT(data.dominantFrequency, 0.0f);
        FL_CHECK_GT(data.magnitude, 0.0f);
        // Flux may be near zero in steady state; don't gate on it here.
        (void)data.spectralFlux;
    }

    // Phase 2: 1 s of zero-PCM silence. The pipeline's NoiseFloorTracker
    // will flag each frame as silent, and the envelope should decay all
    // three spectral metrics toward zero.
    for (int i = 0; i < kFramesPerSecond; ++i) {
        audio::Sample s = makeSilence(
            (kFramesPerSecond + i) * 12, kSamplesPerFrame);
        reactive.processSample(s);
    }

    // With tau=0.2s and ~1 s of silence (~5*tau), outputs should be
    // within the envelope's isGated() epsilon (1e-4) of zero.
    const auto& data = reactive.getData();
    FL_CHECK_LT(data.dominantFrequency, 0.01f);
    FL_CHECK_LT(data.magnitude, 0.01f);
    FL_CHECK_LT(data.spectralFlux, 0.01f);
}

FL_TEST_CASE("audio::Reactive - spectral metrics are pass-through during loud audio (FastLED#2253)") {
    // Sanity: with live audio, the SilenceEnvelope must be a strict
    // pass-through — dominantFrequency / magnitude / spectralFlux should
    // track the raw FFT outputs exactly, not be smoothed or attenuated.
    audio::Reactive reactive;
    audio::ReactiveConfig config;
    config.sampleRate = 44100;
    config.enableNoiseFloorTracking = true;
    reactive.begin(config);

    // Feed loud sine — during audio isSilent() is false and the envelope
    // returns currentValue unchanged.
    for (int i = 0; i < 30; ++i) {
        audio::Sample s = makeSample(1000.0f, i * 12, 16000.0f, 512, 44100.0f);
        reactive.processSample(s);
    }

    const auto& data = reactive.getData();
    // Loud 1 kHz drive must yield a sensible dominant frequency in the
    // audible range and a positive magnitude — same contract as the
    // pre-gate behavior.
    FL_CHECK_GT(data.dominantFrequency, 100.0f);
    FL_CHECK_LT(data.dominantFrequency, 4000.0f);
    FL_CHECK_GT(data.magnitude, 0.0f);
}

} // FL_TEST_FILE
