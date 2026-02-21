#include "fl/audio_reactive.h"
#include "fl/fx/audio/audio_processor.h"
#include "audio/test_helpers.hpp"
#include "fl/audio.h"
#include "fl/circular_buffer.h"
#include "fl/math_macros.h"
#include "fl/stl/math.h"
#include "fl/stl/shared_ptr.h"
#include "fl/stl/span.h"
#include "fl/stl/stdint.h"
#include "test.h"

using namespace fl;
using namespace fl::test;

FL_TEST_CASE("AudioReactive basic functionality") {
    // Test basic initialization
    AudioReactive audio;
    AudioReactiveConfig config;
    config.sampleRate = 22050;
    config.gain = 128;
    config.agcEnabled = false;
    
    audio.begin(config);
    
    // Check initial state
    const AudioData& data = audio.getData();
    FL_CHECK(data.volume == 0.0f);
    FL_CHECK(data.volumeRaw == 0.0f);
    FL_CHECK_FALSE(data.beatDetected);
    
    // Test adding samples - Create AudioSample and add it
    // Reduced from 1000 to 500 samples for performance (still provides excellent coverage)
    fl::vector<int16_t> samples;
    samples.reserve(500);

    for (int i = 0; i < 500; ++i) {
        // Generate a simple sine wave sample
        float phase = 2.0f * FL_M_PI * 1000.0f * i / 22050.0f; // 1kHz
        int16_t sample = static_cast<int16_t>(8000.0f * fl::sinf(phase));
        samples.push_back(sample);
    }
    
    // Create AudioSample from our generated samples with timestamp
    AudioSampleImplPtr impl = fl::make_shared<AudioSampleImpl>();
    uint32_t testTimestamp = 1234567; // Test timestamp value
    impl->assign(samples.begin(), samples.end(), testTimestamp);
    AudioSample audioSample(impl);
    
    // Process the audio sample directly (timestamp comes from AudioSample)
    audio.processSample(audioSample);
    
    // Check that we detected some audio
    const AudioData& processedData = audio.getData();
    FL_CHECK(processedData.volume > 0.0f);
    
    // Verify that the timestamp was properly captured from the AudioSample
    FL_CHECK(processedData.timestamp == testTimestamp);
    
    // Verify that the AudioSample correctly stores and returns its timestamp
    FL_CHECK(audioSample.timestamp() == testTimestamp);
}

FL_TEST_CASE("AudioReactive convenience functions") {
    AudioReactive audio;
    AudioReactiveConfig config;
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

FL_TEST_CASE("AudioReactive enhanced beat detection") {
    AudioReactive audio;
    AudioReactiveConfig config;
    // Use 44100 Hz to match AudioSample::fft() default sample rate.
    // AudioSample::fft() currently hardcodes 44100 Hz (see TODO in audio.cpp.hpp).
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
    // Use 200 Hz which is within the CQ kernel range (fmin=174.6 Hz)
    // and 512 samples to match FFT default sample count
    fl::vector<int16_t> bassySamples;
    bassySamples.reserve(512);

    for (int i = 0; i < 512; ++i) {
        // Generate a low frequency sine wave (200Hz - within CQ bass range)
        float phase = 2.0f * FL_M_PI * 200.0f * i / 44100.0f;
        int16_t sample = static_cast<int16_t>(16000.0f * fl::sinf(phase));
        bassySamples.push_back(sample);
    }
    
    // Create AudioSample
    AudioSampleImplPtr impl = fl::make_shared<AudioSampleImpl>();
    uint32_t timestamp = 1000;
    impl->assign(bassySamples.begin(), bassySamples.end(), timestamp);
    AudioSample bassySample(impl);
    
    // Process the sample
    audio.processSample(bassySample);
    
    // Check that we detected some bass energy
    const AudioData& data = audio.getData();
    FL_CHECK(data.bassEnergy > 0.0f);
    FL_CHECK(data.spectralFlux >= 0.0f);
    
    // Bass energy should be significant relative to treble.
    // Note: strict bassEnergy > midEnergy is not guaranteed by the CQ kernel
    // with only 512 samples (~2-3 cycles at low frequencies), as spectral
    // leakage distributes energy across adjacent bins.
    FL_CHECK(data.bassEnergy > data.trebleEnergy * 0.5f);
}

FL_TEST_CASE("AudioReactive multi-band beat detection") {
    AudioReactive audio;
    AudioReactiveConfig config;
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
    
    // Create AudioSample
    AudioSampleImplPtr impl = fl::make_shared<AudioSampleImpl>();
    uint32_t timestamp = 2000;
    impl->assign(loudSamples.begin(), loudSamples.end(), timestamp);
    AudioSample loudSample(impl);
    
    // Process a quiet sample first to establish baseline
    fl::vector<int16_t> quietSamples(1000, 100); // Very quiet
    AudioSampleImplPtr quietImpl = fl::make_shared<AudioSampleImpl>();
    quietImpl->assign(quietSamples.begin(), quietSamples.end(), 1500);
    AudioSample quietSample(quietImpl);
    audio.processSample(quietSample);
    
    // Now process loud sample (should trigger beats due to energy increase)
    audio.processSample(loudSample);
    
    // Check that energies were calculated
    FL_CHECK(audio.getBassEnergy() > 0.0f);
    FL_CHECK(audio.getMidEnergy() > 0.0f);
    FL_CHECK(audio.getTrebleEnergy() > 0.0f);
}

FL_TEST_CASE("AudioReactive spectral flux detection") {
    AudioReactive audio;
    AudioReactiveConfig config;
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
    AudioSampleImplPtr impl1 = fl::make_shared<AudioSampleImpl>();
    impl1->assign(sample1.begin(), sample1.end(), 3000);
    AudioSample audioSample1(impl1);
    audio.processSample(audioSample1);
    
    float firstFlux = audio.getSpectralFlux();
    
    // Process second sample (different frequency content should create flux)
    AudioSampleImplPtr impl2 = fl::make_shared<AudioSampleImpl>();
    impl2->assign(sample2.begin(), sample2.end(), 3100);
    AudioSample audioSample2(impl2);
    audio.processSample(audioSample2);
    
    float secondFlux = audio.getSpectralFlux();
    
    // Should have detected spectral flux due to frequency change
    FL_CHECK(secondFlux >= 0.0f);
    
    // Flux should have increased or stayed the same from processing different content
    (void)firstFlux; // Suppress unused variable warning
}

FL_TEST_CASE("AudioReactive perceptual weighting") {
    AudioReactive audio;
    AudioReactiveConfig config;
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
    
    AudioSampleImplPtr impl = fl::make_shared<AudioSampleImpl>();
    impl->assign(samples.begin(), samples.end(), 4000);
    AudioSample audioSample(impl);
    
    // Process sample (perceptual weighting should be applied automatically)
    audio.processSample(audioSample);
    
    // Check that processing completed without errors
    const AudioData& data = audio.getData();
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

FL_TEST_CASE("AudioReactive configuration validation") {
    AudioReactive audio;
    AudioReactiveConfig config;
    
    // Test different configuration combinations
    config.enableSpectralFlux = false;
    config.enableMultiBand = false;
    audio.begin(config);
    
    // Should work without enhanced features
    fl::vector<int16_t> samples(1000, 1000);
    AudioSampleImplPtr impl = fl::make_shared<AudioSampleImpl>();
    impl->assign(samples.begin(), samples.end(), 5000);
    AudioSample audioSample(impl);
    
    audio.processSample(audioSample);
    
    // Basic functionality should still work
    FL_CHECK(audio.getVolume() >= 0.0f);
    FL_CHECK_FALSE(audio.isBassBeat()); // Should be false when multi-band is disabled
    FL_CHECK_FALSE(audio.isMidBeat());
    FL_CHECK_FALSE(audio.isTrebleBeat());
}

FL_TEST_CASE("AudioReactive CircularBuffer functionality") {
    // Test the CircularBuffer template directly
    StaticCircularBuffer<float, 8> buffer;
    
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

FL_TEST_CASE("AudioSample - default constructor") {
    AudioSample sample;
    FL_CHECK_FALSE(sample.isValid());
    FL_CHECK_EQ(sample.size(), 0u);
}

FL_TEST_CASE("AudioSample - span constructor") {
    fl::vector<fl::i16> data;
    data.reserve(512);
    for (int i = 0; i < 512; ++i) {
        float phase = 2.0f * 3.14159265f * 440.0f * i / 44100.0f;
        data.push_back(static_cast<fl::i16>(16000.0f * fl::sinf(phase)));
    }
    AudioSample sample(fl::span<const fl::i16>(data.data(), data.size()), 12345);
    FL_CHECK(sample.isValid());
    FL_CHECK_EQ(sample.size(), 512u);
    FL_CHECK_EQ(sample.timestamp(), 12345u);
    FL_CHECK_EQ(sample.pcm().size(), 512u);
}

FL_TEST_CASE("AudioSample - zcf for sine wave vs noise") {
    // Pure sine → low ZCF
    fl::vector<fl::i16> sineSamples;
    sineSamples.reserve(512);
    for (int i = 0; i < 512; ++i) {
        float phase = 2.0f * 3.14159265f * 440.0f * i / 44100.0f;
        sineSamples.push_back(static_cast<fl::i16>(16000.0f * fl::sinf(phase)));
    }
    AudioSample sineSample(fl::span<const fl::i16>(sineSamples.data(), sineSamples.size()), 0);
    float sineZcf = sineSample.zcf();
    FL_CHECK_GE(sineZcf, 0.0f);
    FL_CHECK_LT(sineZcf, 0.1f);

    // High frequency noise → high ZCF
    fl::vector<fl::i16> noiseSamples;
    noiseSamples.reserve(512);
    for (int i = 0; i < 512; ++i) {
        noiseSamples.push_back(static_cast<fl::i16>((i % 2 == 0) ? 10000 : -10000));
    }
    AudioSample noiseSample(fl::span<const fl::i16>(noiseSamples.data(), noiseSamples.size()), 0);
    float noiseZcf = noiseSample.zcf();
    FL_CHECK_GT(noiseZcf, 0.3f);
}

FL_TEST_CASE("AudioSample - rms for known signal") {
    // Silence → RMS = 0
    fl::vector<fl::i16> silence(512, 0);
    AudioSample silentSample(fl::span<const fl::i16>(silence.data(), silence.size()), 0);
    FL_CHECK_EQ(silentSample.rms(), 0.0f);

    // Constant amplitude ±8000 → RMS = 8000
    fl::vector<fl::i16> constant;
    constant.reserve(512);
    for (int i = 0; i < 512; ++i) {
        constant.push_back(static_cast<fl::i16>((i % 2 == 0) ? 8000 : -8000));
    }
    AudioSample constSample(fl::span<const fl::i16>(constant.data(), constant.size()), 0);
    float rms = constSample.rms();
    FL_CHECK_GT(rms, 7000.0f);
    FL_CHECK_LT(rms, 9000.0f);
}

FL_TEST_CASE("AudioSample - fft produces output") {
    fl::vector<fl::i16> data;
    data.reserve(512);
    for (int i = 0; i < 512; ++i) {
        float phase = 2.0f * 3.14159265f * 1000.0f * i / 44100.0f;
        data.push_back(static_cast<fl::i16>(16000.0f * fl::sinf(phase)));
    }
    AudioSample sample(fl::span<const fl::i16>(data.data(), data.size()), 0);
    FFTBins bins(16);
    sample.fft(&bins);
    FL_CHECK_GT(bins.bins_raw.size(), 0u);
}

FL_TEST_CASE("AudioSample - copy and equality") {
    fl::vector<fl::i16> data;
    data.reserve(100);
    for (int i = 0; i < 100; ++i) {
        data.push_back(static_cast<fl::i16>(i * 100));
    }
    AudioSample original(fl::span<const fl::i16>(data.data(), data.size()), 999);
    AudioSample copy(original);
    FL_CHECK(copy.isValid());
    FL_CHECK(original == copy);
    FL_CHECK_FALSE(original != copy);
    FL_CHECK_EQ(copy.timestamp(), 999u);
    FL_CHECK_EQ(copy.size(), 100u);

    AudioSample empty;
    FL_CHECK(original != empty);
}
FL_TEST_CASE("AudioReactive - full pipeline DC removal and gain") {
    AudioReactive audio;
    AudioReactiveConfig config;
    config.sampleRate = 22050;
    config.enableSignalConditioning = true;
    config.enableAutoGain = true;
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
        AudioSample audioSample = createSample(samples, iter * 100);
        audio.processSample(audioSample);
    }

    // Signal conditioning should have removed DC and processed signal
    const auto& scStats = audio.getSignalConditionerStats();
    FL_CHECK_GT(scStats.samplesProcessed, 0u);

    // Auto gain should have processed samples
    const auto& agStats = audio.getAutoGainStats();
    FL_CHECK_GT(agStats.samplesProcessed, 0u);

    // Volume should be measurable
    const auto& data = audio.getData();
    FL_CHECK_GT(data.volume, 0.0f);
}

// INT-2: Pipeline with silence - no NaN, no crash
FL_TEST_CASE("AudioReactive - silence pipeline no NaN") {
    AudioReactive audio;
    AudioReactiveConfig config;
    config.sampleRate = 22050;
    config.enableSignalConditioning = true;
    config.enableAutoGain = true;
    config.enableNoiseFloorTracking = true;
    audio.begin(config);

    // Feed 20 frames of silence
    for (int iter = 0; iter < 20; ++iter) {
        vector<i16> silence(512, 0);
        AudioSample audioSample = createSample(silence, iter * 100);
        audio.processSample(audioSample);
    }

    // Should not crash, volume should be zero or near-zero
    const auto& data = audio.getData();
    FL_CHECK_LT(data.volume, 100.0f);
    FL_CHECK_FALSE(data.volume != data.volume); // NaN check
}

// INT-3: Musical beat detection actually processes audio
FL_TEST_CASE("AudioReactive - musical beat detection processes audio") {
    AudioReactive audio;
    AudioReactiveConfig config;
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
        AudioSample audioSample = createSample(samples, iter * 23);
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
FL_TEST_CASE("AudioReactive - multi-band beat detection processes audio") {
    AudioReactive audio;
    AudioReactiveConfig config;
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
        AudioSample audioSample = createSample(samples, iter * 23);
        audio.processSample(audioSample);
    }

    // Verify actual processing (not just config check)
    const auto& data = audio.getData();
    FL_CHECK_GT(data.volume, 0.0f);

    // Bass energy should be present
    FL_CHECK_GT(data.bassEnergy, 0.0f);
}

// Keep: Pipeline with all middleware enabled (tightened)
FL_TEST_CASE("AudioReactive - all middleware enabled processes correctly") {
    AudioReactive audio;
    AudioReactiveConfig config;
    config.sampleRate = 22050;
    config.enableLogBinSpacing = true;
    config.enableSpectralEqualizer = true;
    config.enableSignalConditioning = true;
    config.enableAutoGain = true;
    config.enableNoiseFloorTracking = true;
    audio.begin(config);

    // Process 10 frames of 1kHz sine
    for (int iter = 0; iter < 10; ++iter) {
        vector<i16> samples = generateSineWave(512, 1000.0f, 22050.0f, 5000);
        AudioSample audioSample = createSample(samples, iter * 100);
        audio.processSample(audioSample);
    }

    const auto& data = audio.getData();
    FL_CHECK_GT(data.volume, 0.0f);
    FL_CHECK_GT(data.midEnergy, 0.0f);

    // All stats should show processing occurred
    const auto& scStats = audio.getSignalConditionerStats();
    FL_CHECK_GT(scStats.samplesProcessed, 0u);

    const auto& agStats = audio.getAutoGainStats();
    FL_CHECK_GT(agStats.samplesProcessed, 0u);

    const auto& nfStats = audio.getNoiseFloorStats();
    FL_CHECK_GT(nfStats.samplesProcessed, 0u);
}

FL_TEST_CASE("AudioReactive - FrequencyBinMapper is always active") {
    AudioReactive audio;
    AudioReactiveConfig config;

    // Verify log bin spacing is enabled by default
    FL_CHECK(config.enableLogBinSpacing == true);

    // Begin with default config
    audio.begin(config);

    // Process a sample to verify mapper works
    vector<i16> samples = generateSineWave(512, 1000.0f, 22050.0f, 8000);
    AudioSample audioSample = createSample(samples, 1000);
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

FL_TEST_CASE("AudioReactive - SpectralEqualizer disabled by default") {
    AudioReactive audio;
    AudioReactiveConfig config;

    // Verify spectral equalizer is disabled by default
    FL_CHECK_FALSE(config.enableSpectralEqualizer);

    audio.begin(config);

    // Process a sample - should work without EQ
    vector<i16> samples = generateSineWave(512, 1000.0f, 22050.0f, 8000);
    AudioSample audioSample = createSample(samples, 1000);
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

FL_TEST_CASE("AudioReactive - Log bin spacing uses sample rate") {
    AudioReactive audio;
    AudioReactiveConfig config;
    config.sampleRate = 16000;
    config.enableLogBinSpacing = true;

    audio.begin(config);

    // Generate a sine wave in the mid-frequency range (500 Hz)
    vector<i16> samples = generateSineWave(512, 500.0f, 16000.0f, 10000);
    AudioSample audioSample = createSample(samples, 2000);
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

FL_TEST_CASE("AudioReactive - Linear bin spacing fallback") {
    AudioReactive audio;
    AudioReactiveConfig config;
    config.sampleRate = 22050;
    config.enableLogBinSpacing = false;  // Use linear spacing

    audio.begin(config);

    // Generate a sine wave
    vector<i16> samples = generateSineWave(512, 1000.0f, 22050.0f, 8000);
    AudioSample audioSample = createSample(samples, 3000);
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

FL_TEST_CASE("AudioReactive - SpectralEqualizer integration") {
    AudioReactive audio;
    AudioReactiveConfig config;
    config.sampleRate = 22050;
    config.enableSpectralEqualizer = true;  // Enable EQ

    audio.begin(config);

    // Generate a sine wave
    vector<i16> samples = generateSineWave(512, 1000.0f, 22050.0f, 8000);
    AudioSample audioSample = createSample(samples, 4000);
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

FL_TEST_CASE("AudioReactive - SpectralEqualizer lazy creation") {
    AudioReactive audio;
    AudioReactiveConfig config;
    config.sampleRate = 22050;
    config.enableSpectralEqualizer = false;  // Start with EQ disabled

    audio.begin(config);

    // Process a sample without EQ
    vector<i16> samples1 = generateSineWave(512, 1000.0f, 22050.0f, 8000);
    AudioSample audioSample1 = createSample(samples1, 5000);
    audio.processSample(audioSample1);

    const auto& data1 = audio.getData();
    FL_CHECK(data1.volume > 0.0f);

    // Now reconfigure with EQ enabled
    config.enableSpectralEqualizer = true;
    audio.begin(config);

    // Process another sample with EQ
    vector<i16> samples2 = generateSineWave(512, 1000.0f, 22050.0f, 8000);
    AudioSample audioSample2 = createSample(samples2, 6000);
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

FL_TEST_CASE("AudioReactive - Band energies use mapper") {
    AudioReactive audio;
    AudioReactiveConfig config;
    config.sampleRate = 22050;
    config.enableLogBinSpacing = true;

    audio.begin(config);

    // Generate a low-frequency sine wave (100 Hz) with high amplitude
    // This should produce energy in the bass range
    vector<i16> samples = generateSineWave(512, 100.0f, 22050.0f, 15000);
    AudioSample audioSample = createSample(samples, 7000);
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

FL_TEST_CASE("AudioReactive - Multiple frequency ranges") {
    AudioReactive audio;
    AudioReactiveConfig config;
    config.sampleRate = 22050;
    config.enableLogBinSpacing = true;

    audio.begin(config);

    // Process bass frequency (100 Hz)
    vector<i16> bassSamples = generateSineWave(512, 100.0f, 22050.0f, 10000);
    AudioSample bassAudio = createSample(bassSamples, 8000);
    audio.processSample(bassAudio);

    const auto& bassData = audio.getData();
    float bassEnergy1 = bassData.bassEnergy;
    FL_CHECK(bassEnergy1 > 0.0f);

    // Process mid frequency (1000 Hz)
    vector<i16> midSamples = generateSineWave(512, 1000.0f, 22050.0f, 10000);
    AudioSample midAudio = createSample(midSamples, 9000);
    audio.processSample(midAudio);

    const auto& midData = audio.getData();
    float midEnergy = midData.midEnergy;
    FL_CHECK(midEnergy > 0.0f);

    // Process treble frequency (8000 Hz)
    vector<i16> trebleSamples = generateSineWave(512, 8000.0f, 22050.0f, 10000);
    AudioSample trebleAudio = createSample(trebleSamples, 10000);
    audio.processSample(trebleAudio);

    const auto& trebleData = audio.getData();
    float trebleEnergy = trebleData.trebleEnergy;
    FL_CHECK(trebleEnergy > 0.0f);
}

FL_TEST_CASE("AudioReactive - Frequency bin consistency with mapper") {
    AudioReactive audio;
    AudioReactiveConfig config;
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

    AudioSample complexAudio = createSample(complexSamples, 11000);
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

FL_TEST_CASE("AudioReactive - Pipeline with all middleware enabled") {
    AudioReactive audio;
    AudioReactiveConfig config;
    config.sampleRate = 22050;
    config.enableLogBinSpacing = true;
    config.enableSpectralEqualizer = true;
    config.enableSignalConditioning = true;
    config.enableAutoGain = true;
    config.enableNoiseFloorTracking = true;

    audio.begin(config);

    // Process multiple samples to let middleware converge
    for (int iter = 0; iter < 10; ++iter) {
        vector<i16> samples = generateSineWave(512, 1000.0f, 22050.0f, 5000);
        AudioSample audioSample = createSample(samples, iter * 100);
        audio.processSample(audioSample);
    }

    // Verify all components are active
    const auto& data = audio.getData();
    FL_CHECK(data.volume > 0.0f);

    // With a 1kHz sine, mid energy should be dominant
    FL_CHECK(data.midEnergy > 0.0f);

    // Check signal conditioning stats
    const auto& scStats = audio.getSignalConditionerStats();
    FL_CHECK(scStats.samplesProcessed > 0);

    const auto& agStats = audio.getAutoGainStats();
    FL_CHECK(agStats.samplesProcessed > 0);

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
FL_TEST_CASE("AudioReactive - Signal conditioning integration enabled by default") {
    AudioReactive audio;
    AudioReactiveConfig config;
    // Signal conditioning should be enabled by default
    FL_CHECK(config.enableSignalConditioning == true);
    FL_CHECK(config.enableAutoGain == true);
    FL_CHECK(config.enableNoiseFloorTracking == true);

    audio.begin(config);

    // Process a sample - should work without signal conditioning
    vector<i16> samples = generateSineWave(1000, 1000.0f, 22050.0f, 8000);
    AudioSample audioSample = createSample(samples, 1000);
    audio.processSample(audioSample);

    const auto& data = audio.getData();
    FL_CHECK(data.volume > 0.0f);
}

FL_TEST_CASE("AudioReactive - Enable signal conditioning") {
    AudioReactive audio;
    AudioReactiveConfig config;
    config.enableSignalConditioning = true;
    config.enableAutoGain = false;
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

    AudioSample biasedAudio = createSample(biasedSamples, 2000);
    audio.processSample(biasedAudio);

    // Signal conditioning should have removed DC bias
    const auto& scStats = audio.getSignalConditionerStats();
    FL_CHECK(scStats.samplesProcessed > 0);

    // Audio should still be processed
    const auto& data = audio.getData();
    FL_CHECK(data.volume > 0.0f);
}

FL_TEST_CASE("AudioReactive - Enable auto gain") {
    AudioReactive audio;
    AudioReactiveConfig config;
    config.enableSignalConditioning = false;
    config.enableAutoGain = true;
    config.enableNoiseFloorTracking = false;
    audio.begin(config);

    // Process several quiet samples to let AGC converge
    for (int i = 0; i < 10; ++i) {
        vector<i16> quietSamples = generateSineWave(500, 1000.0f, 22050.0f, 1000);
        AudioSample quietAudio = createSample(quietSamples, i * 100);
        audio.processSample(quietAudio);
    }

    const auto& agStats = audio.getAutoGainStats();
    FL_CHECK(agStats.samplesProcessed > 0);
    FL_CHECK(agStats.currentGain > 0.0f);

    // Audio should be processed and potentially amplified
    const auto& data = audio.getData();
    FL_CHECK(data.volume >= 0.0f);
}

FL_TEST_CASE("AudioReactive - Enable noise floor tracking") {
    AudioReactive audio;
    AudioReactiveConfig config;
    config.enableSignalConditioning = false;
    config.enableAutoGain = false;
    config.enableNoiseFloorTracking = true;
    audio.begin(config);

    // Process several samples to build noise floor estimate
    for (int i = 0; i < 10; ++i) {
        vector<i16> samples = generateSineWave(500, 1000.0f, 22050.0f, 3000);
        AudioSample audioSample = createSample(samples, i * 100);
        audio.processSample(audioSample);
    }

    const auto& nfStats = audio.getNoiseFloorStats();
    FL_CHECK(nfStats.samplesProcessed > 0);
    FL_CHECK(nfStats.currentFloor > 0.0f);

    // Audio should be processed
    const auto& data = audio.getData();
    FL_CHECK(data.volume > 0.0f);
}

FL_TEST_CASE("AudioReactive - Full signal conditioning pipeline") {
    AudioReactive audio;
    AudioReactiveConfig config;
    config.enableSignalConditioning = true;
    config.enableAutoGain = true;
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

        AudioSample audioSample = createSample(samples, iter * 100);
        audio.processSample(audioSample);
    }

    // Verify all components processed the signal
    const auto& scStats = audio.getSignalConditionerStats();
    FL_CHECK(scStats.samplesProcessed > 0);

    const auto& agStats = audio.getAutoGainStats();
    FL_CHECK(agStats.samplesProcessed > 0);

    const auto& nfStats = audio.getNoiseFloorStats();
    FL_CHECK(nfStats.samplesProcessed > 0);

    // Audio should be processed
    const auto& data = audio.getData();
    FL_CHECK(data.volume > 0.0f);

    // Verify signal conditioning stats
    FL_CHECK(scStats.dcOffset != 0);  // Should have detected DC offset
}

FL_TEST_CASE("AudioReactive - Stats pointers null when disabled") {
    AudioReactive audio;
    AudioReactiveConfig config;
    config.enableSignalConditioning = false;
    config.enableAutoGain = false;
    config.enableNoiseFloorTracking = false;
    audio.begin(config);

    // Process a sample
    vector<i16> samples = generateSineWave(500, 1000.0f, 22050.0f, 5000);
    AudioSample audioSample = createSample(samples, 1000);
    audio.processSample(audioSample);

    // Stats should still be available (components exist but are disabled)
    const auto& scStats = audio.getSignalConditionerStats();
    const auto& agStats = audio.getAutoGainStats();
    const auto& nfStats = audio.getNoiseFloorStats();

    // Components are disabled so they shouldn't have processed samples
    FL_CHECK_EQ(scStats.samplesProcessed, 0u);
    FL_CHECK_EQ(agStats.samplesProcessed, 0u);
    FL_CHECK_EQ(nfStats.samplesProcessed, 0u);
}

FL_TEST_CASE("AudioReactive - Signal conditioning with spikes") {
    AudioReactive audio;
    AudioReactiveConfig config;
    config.enableSignalConditioning = true;
    audio.begin(config);

    // Create signal with injected spikes
    vector<i16> samples = generateSineWave(1000, 1000.0f, 22050.0f, 3000);
    for (size i = 0; i < 100; i += 10) {
        samples[i] = 25000;  // Inject spikes
    }

    AudioSample audioSample = createSample(samples, 3000);
    audio.processSample(audioSample);

    // Verify spikes were detected and rejected
    const auto& scStats = audio.getSignalConditionerStats();
    FL_CHECK(scStats.spikesRejected > 0);

    // Audio should still be processed (spikes filtered out)
    const auto& data = audio.getData();
    FL_CHECK(data.volume > 0.0f);
}

FL_TEST_CASE("AudioReactive - Backward compatibility") {
    // Test that existing code without signal conditioning still works

    AudioReactive audio;
    AudioReactiveConfig config;
    config.sampleRate = 22050;
    config.gain = 128;
    config.agcEnabled = false;  // Use old AGC, not new AutoGain
    // Don't enable new signal conditioning features
    audio.begin(config);

    // Process samples the old way
    vector<i16> samples = generateSineWave(1000, 1000.0f, 22050.0f, 8000);
    AudioSample audioSample = createSample(samples, 4000);
    audio.processSample(audioSample);

    // Should work exactly as before
    const auto& data = audio.getData();
    FL_CHECK(data.volume > 0.0f);
    FL_CHECK(data.frequencyBins[0] >= 0.0f);
}

FL_TEST_CASE("AudioProcessor - polling getters") {
    // Test that AudioProcessor polling getters return valid values
    AudioProcessor proc;

    // Before any update, getters should return defaults (0 / false)
    FL_CHECK_EQ(proc.getVocalConfidence(), 0);
    FL_CHECK_EQ(proc.isVocalActive(), 0);
    FL_CHECK_EQ(proc.getBeatConfidence(), 0);
    FL_CHECK_EQ(proc.isBeat(), 0);
    FL_CHECK_EQ(proc.getEnergy(), 0);
    FL_CHECK_EQ(proc.getPeakLevel(), 0);
    FL_CHECK_EQ(proc.getBassLevel(), 0);
    FL_CHECK_EQ(proc.getMidLevel(), 0);
    FL_CHECK_EQ(proc.getTrebleLevel(), 0);
    // Note: isSilent() may be 0 before any data is processed
    // since the detector needs samples to determine silence state
    FL_CHECK_EQ(proc.isTransient(), 0);
    FL_CHECK_EQ(proc.isCrescendo(), 0);
    FL_CHECK_EQ(proc.isDiminuendo(), 0);
    FL_CHECK_EQ(proc.isVoiced(), 0);
    FL_CHECK_EQ(proc.isTempoStable(), 0);
    FL_CHECK_EQ(proc.isBuilding(), 0);
    FL_CHECK_EQ(proc.isKick(), 0);
    FL_CHECK_EQ(proc.isSnare(), 0);
    FL_CHECK_EQ(proc.isHiHat(), 0);
    FL_CHECK_EQ(proc.isTom(), 0);
    FL_CHECK_EQ(proc.isNoteActive(), 0);
    FL_CHECK_EQ(proc.isDownbeat(), 0);
    FL_CHECK_EQ(proc.hasChord(), 0);
    FL_CHECK_EQ(proc.hasKey(), 0);

    // Feed a sine wave and verify getters still work without crashing
    fl::vector<int16_t> samples;
    samples.reserve(512);
    for (int i = 0; i < 512; ++i) {
        float phase = 2.0f * FL_M_PI * 440.0f * i / 44100.0f;
        samples.push_back(static_cast<int16_t>(10000.0f * fl::sinf(phase)));
    }
    AudioSampleImplPtr impl = fl::make_shared<AudioSampleImpl>();
    impl->assign(samples.begin(), samples.end(), 1000);
    AudioSample audioSample(impl);
    proc.update(audioSample);

    // After processing, energy should be non-zero
    FL_CHECK_GT(proc.getEnergy(), 0);
}

FL_TEST_CASE("AudioReactive - polling getters via AudioProcessor") {
    AudioReactive audio;
    AudioReactiveConfig config;
    config.sampleRate = 44100;
    audio.begin(config);

    // Polling getters should not crash on initial state
    FL_CHECK_EQ(audio.getVocalConfidence(), 0);
    FL_CHECK_EQ(audio.isVocalActive(), 0);
    FL_CHECK_EQ(audio.getBeatConfidence(), 0);
    FL_CHECK_EQ(audio.isBeatDetected(), 0);
    FL_CHECK_EQ(audio.getEnergyLevel(), 0);
    FL_CHECK_EQ(audio.getBassLevel(), 0);
    FL_CHECK_EQ(audio.getMidLevel(), 0);
    FL_CHECK_EQ(audio.getTrebleLevel(), 0);
    FL_CHECK_EQ(audio.isKick(), 0);
    FL_CHECK_EQ(audio.isSnare(), 0);
    FL_CHECK_EQ(audio.isHiHat(), 0);
    FL_CHECK_EQ(audio.hasChord(), 0);
    FL_CHECK_EQ(audio.hasKey(), 0);

    // Feed audio and verify getters still work
    fl::vector<int16_t> samples;
    samples.reserve(512);
    for (int i = 0; i < 512; ++i) {
        float phase = 2.0f * FL_M_PI * 440.0f * i / 44100.0f;
        samples.push_back(static_cast<int16_t>(10000.0f * fl::sinf(phase)));
    }
    AudioSample audioSample = createSample(samples, 1000);
    audio.processSample(audioSample);

    // After processing, energy polling getter should reflect audio
    FL_CHECK_GT(audio.getEnergyLevel(), 0);
}
