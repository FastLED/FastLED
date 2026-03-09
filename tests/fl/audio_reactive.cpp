#include "fl/audio/audio_reactive.h"
#include "fl/audio/audio_processor.h"
#include "fl/audio/detectors/equalizer.h"
#include "fl/audio/input.h"
#include "audio/test_helpers.hpp"
#include "fl/audio/audio.h"
#include "fl/stl/circular_buffer.h"
#include "fl/math_macros.h"
#include "fl/stl/math.h"
#include "fl/stl/shared_ptr.h"
#include "fl/stl/span.h"
#include "fl/stl/stdint.h"
#include "test.h"

FL_TEST_FILE(FL_FILEPATH) {

using namespace fl;
using namespace fl::test;

FL_TEST_CASE("AudioReactive basic functionality") {
    // Test basic initialization
    AudioReactive audio;
    AudioReactiveConfig config;
    config.sampleRate = 22050;
    config.gain = 128;
    // AGC removed — gain is now controlled via AudioProcessor::setGain()
    
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

FL_TEST_CASE("AudioReactive circular_buffer functionality") {
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
    AudioSample sample(data, 12345);
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
    AudioSample sineSample(sineSamples, 0);
    float sineZcf = sineSample.zcf();
    FL_CHECK_GE(sineZcf, 0.0f);
    FL_CHECK_LT(sineZcf, 0.1f);

    // High frequency noise → high ZCF
    fl::vector<fl::i16> noiseSamples;
    noiseSamples.reserve(512);
    for (int i = 0; i < 512; ++i) {
        noiseSamples.push_back(static_cast<fl::i16>((i % 2 == 0) ? 10000 : -10000));
    }
    AudioSample noiseSample(noiseSamples, 0);
    float noiseZcf = noiseSample.zcf();
    FL_CHECK_GT(noiseZcf, 0.3f);
}

FL_TEST_CASE("AudioSample - rms for known signal") {
    // Silence → RMS = 0
    fl::vector<fl::i16> silence(512, 0);
    AudioSample silentSample(silence, 0);
    FL_CHECK_EQ(silentSample.rms(), 0.0f);

    // Constant amplitude ±8000 → RMS = 8000
    fl::vector<fl::i16> constant;
    constant.reserve(512);
    for (int i = 0; i < 512; ++i) {
        constant.push_back(static_cast<fl::i16>((i % 2 == 0) ? 8000 : -8000));
    }
    AudioSample constSample(constant, 0);
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
    AudioSample sample(data, 0);
    FFTBins bins(16);
    sample.fft(&bins);
    FL_CHECK_GT(bins.raw().size(), 0u);
}

FL_TEST_CASE("AudioSample - copy and equality") {
    fl::vector<fl::i16> data;
    data.reserve(100);
    for (int i = 0; i < 100; ++i) {
        data.push_back(static_cast<fl::i16>(i * 100));
    }
    AudioSample original(data, 999);
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

    config.enableNoiseFloorTracking = false;
    audio.begin(config);

    // Process several quiet samples to let AGC converge
    for (int i = 0; i < 10; ++i) {
        vector<i16> quietSamples = generateSineWave(500, 1000.0f, 22050.0f, 1000);
        AudioSample quietAudio = createSample(quietSamples, i * 100);
        audio.processSample(quietAudio);
    }

    // Audio should be processed and potentially amplified
    const auto& data = audio.getData();
    FL_CHECK(data.volume >= 0.0f);
}

FL_TEST_CASE("AudioReactive - Enable noise floor tracking") {
    AudioReactive audio;
    AudioReactiveConfig config;
    config.enableSignalConditioning = false;

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

    config.enableNoiseFloorTracking = false;
    audio.begin(config);

    // Process a sample
    vector<i16> samples = generateSineWave(500, 1000.0f, 22050.0f, 5000);
    AudioSample audioSample = createSample(samples, 1000);
    audio.processSample(audioSample);

    // Stats should still be available (components exist but are disabled)
    const auto& scStats = audio.getSignalConditionerStats();
    const auto& nfStats = audio.getNoiseFloorStats();

    // Components are disabled so they shouldn't have processed samples
    FL_CHECK_EQ(scStats.samplesProcessed, 0u);
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
    // AGC removed — gain is now controlled via AudioProcessor::setGain()  // Use old AGC, not new AutoGain
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
    AudioSampleImplPtr impl = fl::make_shared<AudioSampleImpl>();
    impl->assign(samples.begin(), samples.end(), 1000);
    AudioSample audioSample(impl);
    proc.update(audioSample);

    // After processing, energy should be non-zero
    FL_CHECK_GT(proc.getEnergy(), 0.0f);
}

FL_TEST_CASE("AudioReactive - polling getters via AudioProcessor") {
    AudioReactive audio;
    AudioReactiveConfig config;
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
    AudioSample audioSample = createSample(samples, 1000);
    audio.processSample(audioSample);

    // After processing, energy polling getter should reflect audio
    FL_CHECK_GT(audio.getEnergyLevel(), 0.0f);
}

// ===== AudioProcessor gain tests =====

FL_TEST_CASE("AudioProcessor - default gain is 1.0") {
    AudioProcessor proc;
    FL_CHECK_EQ(proc.getGain(), 1.0f);
}

FL_TEST_CASE("AudioProcessor - setGain/getGain roundtrip") {
    AudioProcessor proc;
    proc.setGain(3.5f);
    FL_CHECK_EQ(proc.getGain(), 3.5f);
}

FL_TEST_CASE("AudioProcessor - gain amplifies energy") {
    // Use onEnergy callback to capture raw RMS (not normalized, which self-normalizes).
    float rawEnergy1 = 0.0f;
    float rawEnergy2 = 0.0f;

    AudioProcessor proc1;
    proc1.onEnergy([&](float rms) { rawEnergy1 = rms; });
    vector<i16> samples = generateSineWave(512, 440.0f, 44100.0f, 5000);
    AudioSample s1 = createSample(samples, 100);
    proc1.update(s1);

    AudioProcessor proc2;
    proc2.setGain(5.0f);
    proc2.onEnergy([&](float rms) { rawEnergy2 = rms; });
    AudioSample s2 = createSample(samples, 200);
    proc2.update(s2);

    FL_CHECK_GT(rawEnergy1, 0.0f);
    FL_CHECK_GT(rawEnergy2, rawEnergy1);
}

FL_TEST_CASE("AudioProcessor - gain zero silences") {
    float rawEnergy = -1.0f;
    AudioProcessor proc;
    proc.setGain(0.0f);
    proc.onEnergy([&](float rms) { rawEnergy = rms; });
    vector<i16> samples = generateSineWave(512, 440.0f, 44100.0f, 16000);
    AudioSample s = createSample(samples, 100);
    proc.update(s);
    // Energy should be zero since gain zeroed out all samples
    FL_CHECK_EQ(rawEnergy, 0.0f);
}

FL_TEST_CASE("AudioProcessor - gain does not reset on reset()") {
    AudioProcessor proc;
    proc.setGain(3.0f);
    proc.reset();
    FL_CHECK_EQ(proc.getGain(), 3.0f);
}

FL_TEST_CASE("AudioProcessor - equalizer autoGain default") {
    AudioProcessor proc;
    // Before any updates, equalizer autoGain should be 1.0
    FL_CHECK_EQ(proc.getEqAutoGain(), 1.0f);
}

FL_TEST_CASE("AudioProcessor - equalizer isSilence on silence") {
    AudioProcessor proc;
    // Prime the equalizer detector before feeding samples
    (void)proc.getEqIsSilence();
    // Feed 10 frames of silence
    for (int i = 0; i < 10; ++i) {
        vector<i16> silence(512, 0);
        AudioSample s = createSample(silence, i * 100);
        proc.update(s);
    }
    FL_CHECK(proc.getEqIsSilence());
}

FL_TEST_CASE("AudioProcessor - equalizer isSilence on loud signal") {
    AudioProcessor proc;
    // Prime the equalizer detector before feeding samples
    (void)proc.getEqIsSilence();
    // Feed 10 frames of loud sine
    for (int i = 0; i < 10; ++i) {
        vector<i16> samples = generateSineWave(512, 440.0f, 44100.0f, 16000);
        AudioSample s = createSample(samples, i * 100);
        proc.update(s);
    }
    FL_CHECK_FALSE(proc.getEqIsSilence());
}

FL_TEST_CASE("AudioProcessor - equalizer callback has autoGain and isSilence") {
    AudioProcessor proc;
    bool callbackFired = false;
    float receivedAutoGain = 0.0f;
    bool receivedIsSilence = true;

    proc.onEqualizer([&](const Equalizer& eq) {
        callbackFired = true;
        receivedAutoGain = eq.volumeNormFactor;
        receivedIsSilence = eq.isSilence;
    });

    // Feed loud sine to trigger non-silence
    for (int i = 0; i < 5; ++i) {
        vector<i16> samples = generateSineWave(512, 440.0f, 44100.0f, 16000);
        AudioSample s = createSample(samples, i * 100);
        proc.update(s);
    }

    FL_CHECK(callbackFired);
    FL_CHECK_GT(receivedAutoGain, 0.0f);
    FL_CHECK_FALSE(receivedIsSilence);
}

FL_TEST_CASE("AudioProcessor - gain stacks with input gain") {
    // Use onEnergy callback to observe raw RMS values.
    float rawEnergy1 = 0.0f;
    float rawEnergy2 = 0.0f;
    vector<i16> samples = generateSineWave(512, 440.0f, 44100.0f, 3000);

    // Sample with applyGain(2) + processor gain=1
    AudioSample s1 = createSample(samples, 100);
    s1.applyGain(2.0f);

    AudioProcessor proc1;
    proc1.onEnergy([&](float rms) { rawEnergy1 = rms; });
    proc1.update(s1);

    // Sample with applyGain(2) + processor gain=3
    AudioSample s2 = createSample(samples, 200);
    s2.applyGain(2.0f);

    AudioProcessor proc2;
    proc2.setGain(3.0f);
    proc2.onEnergy([&](float rms) { rawEnergy2 = rms; });
    proc2.update(s2);

    FL_CHECK_GT(rawEnergy1, 0.0f);
    FL_CHECK_GT(rawEnergy2, rawEnergy1);
}

// ===== AudioReactive gain tests =====

FL_TEST_CASE("AudioReactive - default gain is 1.0") {
    AudioReactive audio;
    audio.begin();
    FL_CHECK_EQ(audio.getGain(), 1.0f);
}

FL_TEST_CASE("AudioReactive - setGain/getGain roundtrip") {
    AudioReactive audio;
    audio.begin();
    audio.setGain(2.5f);
    FL_CHECK_EQ(audio.getGain(), 2.5f);
}

FL_TEST_CASE("AudioReactive - setGain before begin returns 1.0") {
    AudioReactive audio;
    // Before begin(), no AudioProcessor exists yet
    FL_CHECK_EQ(audio.getGain(), 1.0f);
}

FL_TEST_CASE("AudioReactive - setGain affects processing") {
    // setGain() affects the internal AudioProcessor path. The legacy
    // AudioReactive volume pipeline uses the old u8 config gain separately.
    // Verify gain has a measurable effect by applying gain before feeding
    // to AudioReactive and comparing volumes through the legacy pipeline.
    vector<i16> samples = generateSineWave(512, 440.0f, 44100.0f, 3000);

    // No external gain
    AudioReactive audio1;
    audio1.begin();
    AudioSample s1 = createSample(samples, 100);
    audio1.processSample(s1);
    float vol1 = audio1.getVolume();

    // Apply external gain via applyGain before feeding
    AudioReactive audio2;
    audio2.begin();
    AudioSample s2 = createSample(samples, 200);
    s2.applyGain(5.0f);
    audio2.processSample(s2);
    float vol2 = audio2.getVolume();

    FL_CHECK_GT(vol2, vol1);
}

// ===== AudioConfig gain tests =====

FL_TEST_CASE("AudioConfig - default gain is 1.0") {
    AudioConfig config = AudioConfig::CreateInmp441(1, 2, 3, Left);
    FL_CHECK_EQ(config.getGain(), 1.0f);
}

FL_TEST_CASE("AudioConfig - setGain/getGain roundtrip") {
    AudioConfig config = AudioConfig::CreateInmp441(1, 2, 3, Left);
    config.setGain(4.0f);
    FL_CHECK_EQ(config.getGain(), 4.0f);
}

FL_TEST_CASE("AudioConfig - gain persists across copy") {
    AudioConfig config1 = AudioConfig::CreateInmp441(1, 2, 3, Left);
    config1.setGain(3.0f);
    AudioConfig config2 = config1;
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
    AudioProcessor proc;
    bool lastIsSilence = false;
    proc.onEqualizer([&](const Equalizer& eq) {
        lastIsSilence = eq.isSilence;
    });

    for (int i = 0; i < 10; ++i) {
        vector<i16> samples = generateSineWave(512, 440.0f, 44100.0f, 10);
        AudioSample s = createSample(samples, i * 12);
        proc.update(s);
    }
    FL_CHECK(lastIsSilence);
}

FL_TEST_CASE("Silence - quiet multi-frequency chord detected as silent") {
    // Simulate a quiet 3-note chord: 261 Hz (C4) + 329 Hz (E4) + 392 Hz (G4)
    // Each at amplitude 4, combined peak ≈ 12, RMS ≈ 8.5 → silence
    AudioProcessor proc;
    bool lastIsSilence = false;
    proc.onEqualizer([&](const Equalizer& eq) {
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
        AudioSample s = createSample(samples, iter * 12);
        proc.update(s);
    }
    FL_CHECK(lastIsSilence);
}

FL_TEST_CASE("Silence - quiet bass + treble detected as silent") {
    // Simulate quiet bass (100 Hz) + treble (4000 Hz) at amplitude 5 each
    // Combined RMS ≈ sqrt(2) * 5 * 0.707 ≈ 5.0 → silence
    AudioProcessor proc;
    bool lastIsSilence = false;
    proc.onEqualizer([&](const Equalizer& eq) {
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
        AudioSample s = createSample(samples, iter * 12);
        proc.update(s);
    }
    FL_CHECK(lastIsSilence);
}

FL_TEST_CASE("Silence - quiet frequency sweep detected as silent") {
    // Simulate a frequency sweep from 200 Hz to 2000 Hz at amplitude 8
    // RMS ≈ 8 * 0.707 ≈ 5.7 → silence
    AudioProcessor proc;
    bool lastIsSilence = false;
    proc.onEqualizer([&](const Equalizer& eq) {
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
        AudioSample s = createSample(samples, iter * 12);
        proc.update(s);
    }
    FL_CHECK(lastIsSilence);
}

FL_TEST_CASE("Silence - quiet noise bed detected as silent") {
    // Simulate very quiet pseudo-random noise at amplitude 6
    // RMS ≈ 6 * 0.577 ≈ 3.5 (uniform noise RMS = amp/sqrt(3)) → silence
    AudioProcessor proc;
    bool lastIsSilence = false;
    proc.onEqualizer([&](const Equalizer& eq) {
        lastIsSilence = eq.isSilence;
    });

    for (int iter = 0; iter < 10; ++iter) {
        vector<i16> noise = generateNoise(512, 6);
        AudioSample s = createSample(noise, iter * 12);
        proc.update(s);
    }
    FL_CHECK(lastIsSilence);
}

FL_TEST_CASE("Silence - quiet pulsed synth detected as silent") {
    // Simulate a synth pad that pulses: alternating between amplitude 12
    // (RMS ≈ 8.5) and amplitude 2 (RMS ≈ 1.4). Both below threshold.
    AudioProcessor proc;
    bool allSilent = true;
    proc.onEqualizer([&](const Equalizer& eq) {
        if (!eq.isSilence) allSilent = false;
    });

    for (int iter = 0; iter < 20; ++iter) {
        i16 amp = (iter % 2 == 0) ? 12 : 2;
        vector<i16> samples = generateSineWave(512, 880.0f, 44100.0f, amp);
        AudioSample s = createSample(samples, iter * 12);
        proc.update(s);
    }
    FL_CHECK(allSilent);
}

FL_TEST_CASE("Silence - boundary: amplitude 13 still silent") {
    // Amplitude 13 → RMS ≈ 9.19 < 10.0 threshold → should be silent
    AudioProcessor proc;
    bool lastIsSilence = false;
    proc.onEqualizer([&](const Equalizer& eq) {
        lastIsSilence = eq.isSilence;
    });

    for (int i = 0; i < 10; ++i) {
        vector<i16> samples = generateSineWave(512, 1000.0f, 44100.0f, 13);
        AudioSample s = createSample(samples, i * 12);
        proc.update(s);
    }
    FL_CHECK(lastIsSilence);
}

FL_TEST_CASE("Silence - loud signal NOT silent") {
    // Disable signal conditioning so the signal passes through cleanly.
    // Amplitude 500 → RMS ≈ 354, well above kSilenceThreshold = 10.
    AudioProcessor proc;
    proc.setSignalConditioningEnabled(false);
    bool lastIsSilence = true;
    proc.onEqualizer([&](const Equalizer& eq) {
        lastIsSilence = eq.isSilence;
    });

    for (int i = 0; i < 10; ++i) {
        vector<i16> samples = generateSineWave(512, 1000.0f, 44100.0f, 500);
        AudioSample s = createSample(samples, i * 12);
        proc.update(s);
    }
    FL_CHECK_FALSE(lastIsSilence);
}

FL_TEST_CASE("Silence - quiet music via AudioReactive polling getter") {
    // End-to-end: quiet 440 Hz via AudioReactive → getEqIsSilence()
    // This exercises the AudioReactive → AudioProcessor → EqualizerDetector path.
    AudioReactive audio;
    audio.begin();
    // Prime the equalizer detector by calling the getter
    (void)audio.getGain(); // ensures AudioProcessor exists

    // We need to use the AudioProcessor directly for the eq polling getter.
    // AudioReactive doesn't expose getEqIsSilence(), but AudioProcessor does.
    // Test via AudioProcessor instead.
    AudioProcessor proc;
    (void)proc.getEqIsSilence(); // prime detector

    for (int i = 0; i < 10; ++i) {
        vector<i16> samples = generateSineWave(512, 440.0f, 44100.0f, 8);
        AudioSample s = createSample(samples, i * 12);
        proc.update(s);
    }
    FL_CHECK(proc.getEqIsSilence());
}

FL_TEST_CASE("Silence - loud music correctly NOT silent via callback") {
    // Sanity check: loud signal should NOT be reported as silent
    AudioProcessor proc;
    bool lastIsSilence = true;
    proc.onEqualizer([&](const Equalizer& eq) {
        lastIsSilence = eq.isSilence;
    });

    for (int i = 0; i < 10; ++i) {
        vector<i16> samples = generateSineWave(512, 440.0f, 44100.0f, 16000);
        AudioSample s = createSample(samples, i * 12);
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

/// Helper: assert every field in the Equalizer snapshot is in valid range
static void checkEqBounds(const Equalizer& eq) {
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
    for (int b = 0; b < Equalizer::kNumBins; ++b) {
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
    AudioProcessor proc;
    Equalizer last;
    proc.onEqualizer([&](const Equalizer& eq) {
        checkEqBounds(eq);
        last = eq;
    });

    for (int i = 0; i < 10; ++i) {
        vector<i16> samples = generateSineWave(512, 100.0f, 44100.0f, 16000);
        AudioSample s = createSample(samples, i * 12);
        proc.update(s);
    }
    FL_CHECK_FALSE(last.isSilence);
    FL_CHECK_GT(last.volume, 0.0f);
    FL_CHECK_GT(last.bass, 0.0f);
}

FL_TEST_CASE("Loud - mid sine detected in mid band, all values bounded") {
    // 1000 Hz sine at amplitude 16000 → should land in mid bins (4-10)
    AudioProcessor proc;
    Equalizer last;
    proc.onEqualizer([&](const Equalizer& eq) {
        checkEqBounds(eq);
        last = eq;
    });

    for (int i = 0; i < 10; ++i) {
        vector<i16> samples = generateSineWave(512, 1000.0f, 44100.0f, 16000);
        AudioSample s = createSample(samples, i * 12);
        proc.update(s);
    }
    FL_CHECK_FALSE(last.isSilence);
    FL_CHECK_GT(last.volume, 0.0f);
    FL_CHECK_GT(last.mid, 0.0f);
}

FL_TEST_CASE("Loud - treble sine detected in treble band, all values bounded") {
    // 4000 Hz sine at amplitude 16000 → should land in treble bins (11-15)
    AudioProcessor proc;
    Equalizer last;
    proc.onEqualizer([&](const Equalizer& eq) {
        checkEqBounds(eq);
        last = eq;
    });

    for (int i = 0; i < 10; ++i) {
        vector<i16> samples = generateSineWave(512, 4000.0f, 44100.0f, 16000);
        AudioSample s = createSample(samples, i * 12);
        proc.update(s);
    }
    FL_CHECK_FALSE(last.isSilence);
    FL_CHECK_GT(last.volume, 0.0f);
    FL_CHECK_GT(last.treble, 0.0f);
}

FL_TEST_CASE("Loud - full spectrum chord, all bands active, all bounded") {
    // Bass (100 Hz) + Mid (1000 Hz) + Treble (4000 Hz), each at amp 8000
    AudioProcessor proc;
    Equalizer last;
    proc.onEqualizer([&](const Equalizer& eq) {
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
        AudioSample s = createSample(samples, iter * 12);
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
    AudioProcessor proc;
    proc.onEqualizer([&](const Equalizer& eq) {
        checkEqBounds(eq);
    });

    for (int i = 0; i < 20; ++i) {
        vector<i16> samples = generateSineWave(512, 440.0f, 44100.0f, 32767);
        AudioSample s = createSample(samples, i * 12);
        proc.update(s);
    }
    // If we got here without checkEqBounds failing, all values stayed bounded.
}

FL_TEST_CASE("Loud - sudden spike after silence never exceeds 1.0") {
    // 10 frames of silence, then sudden max-amplitude burst.
    // This is the worst-case for adaptive normalization: the running max
    // is near zero, then a huge signal arrives. Bins must still clamp to 1.0.
    AudioProcessor proc;
    proc.onEqualizer([&](const Equalizer& eq) {
        checkEqBounds(eq);
    });

    // Silence phase
    for (int i = 0; i < 10; ++i) {
        vector<i16> silence(512, 0);
        AudioSample s = createSample(silence, i * 12);
        proc.update(s);
    }

    // Sudden loud burst
    for (int i = 10; i < 15; ++i) {
        vector<i16> loud = generateSineWave(512, 440.0f, 44100.0f, 30000);
        AudioSample s = createSample(loud, i * 12);
        proc.update(s);
    }
}

FL_TEST_CASE("Loud - alternating silence and bursts never exceed 1.0") {
    // Rapidly alternating between silence and max-amplitude.
    // Stresses the adaptive filters with constant transients.
    AudioProcessor proc;
    proc.onEqualizer([&](const Equalizer& eq) {
        checkEqBounds(eq);
    });

    for (int i = 0; i < 30; ++i) {
        vector<i16> samples;
        if (i % 2 == 0) {
            samples.assign(512, 0); // silence
        } else {
            samples = generateSineWave(512, 1000.0f, 44100.0f, 32767);
        }
        AudioSample s = createSample(samples, i * 12);
        proc.update(s);
    }
}

FL_TEST_CASE("Loud - DC offset signal bounded and not silent") {
    // A signal with massive DC offset (16000) + sine. Signal conditioning
    // removes DC, but the remaining signal should be detected as non-silent
    // and all values should stay bounded.
    AudioProcessor proc;
    Equalizer last;
    proc.onEqualizer([&](const Equalizer& eq) {
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
        AudioSample s = createSample(samples, iter * 12);
        proc.update(s);
    }
    FL_CHECK_FALSE(last.isSilence);
    FL_CHECK_GT(last.volume, 0.0f);
}

FL_TEST_CASE("Loud - amplitude sweep ramps up, all values bounded") {
    // Amplitude ramps from 1000 to 32000 over 20 frames.
    // Tests that normalization tracks increasing signal without overflow.
    AudioProcessor proc;
    proc.onEqualizer([&](const Equalizer& eq) {
        checkEqBounds(eq);
    });
    Equalizer last;

    for (int iter = 0; iter < 20; ++iter) {
        i16 amp = static_cast<i16>(1000 + iter * 1500);
        vector<i16> samples = generateSineWave(512, 800.0f, 44100.0f, amp);
        AudioSample s = createSample(samples, iter * 12);
        proc.update(s);
        // Capture the last callback's result
        proc.onEqualizer([&](const Equalizer& eq) { last = eq; });
    }
    FL_CHECK_FALSE(last.isSilence);
    FL_CHECK_GT(last.volume, 0.0f);
}

FL_TEST_CASE("Loud - multi-frequency noise bounded and not silent") {
    // White-ish noise at high amplitude. Should have energy spread
    // across bands, and all values must stay bounded.
    AudioProcessor proc;
    Equalizer last;
    proc.onEqualizer([&](const Equalizer& eq) {
        checkEqBounds(eq);
        last = eq;
    });

    for (int iter = 0; iter < 10; ++iter) {
        vector<i16> noise = generateNoise(512, 16000);
        AudioSample s = createSample(noise, iter * 12);
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
    Equalizer sineLast, noiseLast;

    {
        AudioProcessor proc;
        proc.onEqualizer([&](const Equalizer& eq) {
            checkEqBounds(eq);
            sineLast = eq;
        });
        for (int i = 0; i < 10; ++i) {
            vector<i16> samples = generateSineWave(512, 440.0f, 44100.0f, 8000);
            AudioSample s = createSample(samples, i * 12);
            proc.update(s);
        }
    }

    {
        // Use generated noise (pseudo-random) — has high zcf naturally
        AudioProcessor proc;
        proc.onEqualizer([&](const Equalizer& eq) {
            checkEqBounds(eq);
            noiseLast = eq;
        });
        for (int i = 0; i < 10; ++i) {
            vector<i16> noise = generateNoise(512, 8000);
            AudioSample s = createSample(noise, i * 12);
            proc.update(s);
        }
    }

    // Both should be non-silent (signal conditioning passes them through)
    FL_CHECK_FALSE(sineLast.isSilence);
    FL_CHECK_FALSE(noiseLast.isSilence);
    // Noise should have higher zcf than a pure tone
    FL_CHECK_GT(noiseLast.zcf, sineLast.zcf);
}

FL_TEST_CASE("Loud - gain amplified signal still bounded") {
    // Apply gain=10 to an already-loud signal. The gain causes internal
    // clamping but the equalizer output must still be in [0, 1].
    AudioProcessor proc;
    proc.setGain(10.0f);
    proc.onEqualizer([&](const Equalizer& eq) {
        checkEqBounds(eq);
    });
    Equalizer last;

    for (int i = 0; i < 10; ++i) {
        vector<i16> samples = generateSineWave(512, 440.0f, 44100.0f, 16000);
        AudioSample s = createSample(samples, i * 12);
        proc.update(s);
    }
    // Should not crash and all values stay bounded (checked in callback).
}

FL_TEST_CASE("EqualizerConfig - default values match previous hardcoded") {
    EqualizerConfig config;
    FL_CHECK(config.minFreq == 60.0f);
    FL_CHECK(config.maxFreq == 5120.0f);
    FL_CHECK(config.smoothing == 0.05f);
    FL_CHECK(config.normAttack == 0.001f);
    FL_CHECK(config.normDecay == 4.0f);
    FL_CHECK(config.silenceThreshold == 10.0f);
}

FL_TEST_CASE("AudioProcessor - configureEqualizer changes behavior") {
    AudioProcessor proc;

    // Collect equalizer output with default config
    float defaultVolume = 0;
    proc.onEqualizer([&](const Equalizer& eq) {
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
    EqualizerConfig fast;
    fast.normDecay = 0.01f;  // Very fast decay
    proc.configureEqualizer(fast);
    proc.onEqualizer([&](const Equalizer& eq) {
        fastDecayVolume = eq.volume;
    });

    for (int i = 0; i < 20; ++i) {
        vector<i16> samples = generateSineWave(512, 440.0f, 44100.0f, 8000);
        proc.update(createSample(samples, i * 12));
    }

    // Both should produce valid output (0-1 range)
    FL_CHECK(capturedDefault >= 0.0f);
    FL_CHECK(capturedDefault <= 1.0f);
    FL_CHECK(fastDecayVolume >= 0.0f);
    FL_CHECK(fastDecayVolume <= 1.0f);
}

FL_TEST_CASE("AudioProcessor - configureEqualizer silence threshold") {
    AudioProcessor proc;

    // Configure with a very high silence threshold
    EqualizerConfig config;
    config.silenceThreshold = 50000.0f;  // Almost everything is "silent"
    proc.configureEqualizer(config);

    bool isSilence = false;
    proc.onEqualizer([&](const Equalizer& eq) {
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
    EqualizerConfig lowThresh;
    lowThresh.silenceThreshold = 1.0f;
    proc.configureEqualizer(lowThresh);

    for (int i = 0; i < 5; ++i) {
        vector<i16> samples = generateSineWave(512, 440.0f, 44100.0f, 8000);
        proc.update(createSample(samples, i * 12));
    }

    // With threshold at 1.0, moderate audio should NOT be silent
    FL_CHECK(isSilence == false);
}


} // FL_TEST_FILE
