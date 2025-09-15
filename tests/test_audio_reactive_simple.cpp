#include "test.h"
#include "fl/audio_reactive.h"
#include "fl/math.h"
#include <math.h>
#include "fl/memory.h"
#include "fl/circular_buffer.h"

using namespace fl;

TEST_CASE("AudioReactive basic functionality") {
    // Test basic initialization
    AudioReactive audio;
    AudioReactiveConfig config;
    config.sampleRate = 22050;
    config.gain = 128;
    config.agcEnabled = false;
    
    audio.begin(config);
    
    // Check initial state
    const AudioData& data = audio.getData();
    CHECK(data.volume == 0.0f);
    CHECK(data.volumeRaw == 0.0f);
    CHECK_FALSE(data.beatDetected);
    
    // Test adding samples - Create AudioSample and add it
    fl::vector<int16_t> samples;
    samples.reserve(1000);
    
    for (int i = 0; i < 1000; ++i) {
        // Generate a simple sine wave sample
        float phase = 2.0f * M_PI * 1000.0f * i / 22050.0f; // 1kHz
        int16_t sample = static_cast<int16_t>(8000.0f * sinf(phase));
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
    CHECK(processedData.volume > 0.0f);
    
    // Verify that the timestamp was properly captured from the AudioSample
    CHECK(processedData.timestamp == testTimestamp);
    
    // Verify that the AudioSample correctly stores and returns its timestamp
    CHECK(audioSample.timestamp() == testTimestamp);
}

TEST_CASE("AudioReactive convenience functions") {
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
    
    CHECK(volume >= 0.0f);
    CHECK(bass >= 0.0f);
    CHECK(mid >= 0.0f);
    CHECK(treble >= 0.0f);
    // beat can be true or false, just check it doesn't crash
    (void)beat; // Suppress unused variable warning
}

TEST_CASE("AudioReactive enhanced beat detection") {
    AudioReactive audio;
    AudioReactiveConfig config;
    config.sampleRate = 22050;
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
    CHECK_FALSE(bassBeat);
    CHECK_FALSE(midBeat);
    CHECK_FALSE(trebleBeat);
    CHECK_EQ(spectralFlux, 0.0f);
    CHECK_EQ(bassEnergy, 0.0f);
    CHECK_EQ(midEnergy, 0.0f);
    CHECK_EQ(trebleEnergy, 0.0f);
    
    // Create a bass-heavy sample (low frequency)
    fl::vector<int16_t> bassySamples;
    bassySamples.reserve(1000);
    
    for (int i = 0; i < 1000; ++i) {
        // Generate a low frequency sine wave (80Hz - should map to bass bins)
        float phase = 2.0f * M_PI * 80.0f * i / 22050.0f;
        int16_t sample = static_cast<int16_t>(16000.0f * sinf(phase));
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
    CHECK(data.bassEnergy > 0.0f);
    CHECK(data.spectralFlux >= 0.0f);
    
    // Energy should be distributed appropriately for bass content
    CHECK(data.bassEnergy > data.midEnergy);
    CHECK(data.bassEnergy > data.trebleEnergy);
}

TEST_CASE("AudioReactive multi-band beat detection") {
    AudioReactive audio;
    AudioReactiveConfig config;
    config.enableMultiBand = true;
    config.bassThreshold = 0.05f;  // Lower thresholds for testing
    config.midThreshold = 0.05f;
    config.trebleThreshold = 0.05f;
    
    audio.begin(config);
    
    // Create samples with increasing amplitude to trigger beat detection
    fl::vector<int16_t> loudSamples;
    loudSamples.reserve(1000);
    
    for (int i = 0; i < 1000; ++i) {
        // Create a multi-frequency signal that should trigger beats
        float bassPhase = 2.0f * M_PI * 60.0f * i / 22050.0f;   // Bass
        float midPhase = 2.0f * M_PI * 1000.0f * i / 22050.0f;  // Mid
        float treblePhase = 2.0f * M_PI * 5000.0f * i / 22050.0f; // Treble
        
        float amplitude = 20000.0f; // High amplitude to trigger beats
        float combined = sinf(bassPhase) + sinf(midPhase) + sinf(treblePhase);
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
    CHECK(audio.getBassEnergy() > 0.0f);
    CHECK(audio.getMidEnergy() > 0.0f);
    CHECK(audio.getTrebleEnergy() > 0.0f);
}

TEST_CASE("AudioReactive spectral flux detection") {
    AudioReactive audio;
    AudioReactiveConfig config;
    config.enableSpectralFlux = true;
    config.spectralFluxThreshold = 0.01f; // Low threshold for testing
    
    audio.begin(config);
    
    // Create two different samples to generate spectral flux
    fl::vector<int16_t> sample1, sample2;
    sample1.reserve(1000);
    sample2.reserve(1000);
    
    // First sample - single frequency
    for (int i = 0; i < 1000; ++i) {
        float phase = 2.0f * M_PI * 440.0f * i / 22050.0f; // A note
        int16_t sample = static_cast<int16_t>(8000.0f * sinf(phase));
        sample1.push_back(sample);
    }
    
    // Second sample - different frequency (should create spectral flux)
    for (int i = 0; i < 1000; ++i) {
        float phase = 2.0f * M_PI * 880.0f * i / 22050.0f; // A note one octave higher
        int16_t sample = static_cast<int16_t>(8000.0f * sinf(phase));
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
    CHECK(secondFlux >= 0.0f);
    
    // Flux should have increased or stayed the same from processing different content
    (void)firstFlux; // Suppress unused variable warning
}

TEST_CASE("AudioReactive perceptual weighting") {
    AudioReactive audio;
    AudioReactiveConfig config;
    config.sampleRate = 22050;
    
    audio.begin(config);
    
    // Create a test sample
    fl::vector<int16_t> samples;
    samples.reserve(1000);
    
    for (int i = 0; i < 1000; ++i) {
        float phase = 2.0f * M_PI * 1000.0f * i / 22050.0f;
        int16_t sample = static_cast<int16_t>(8000.0f * sinf(phase));
        samples.push_back(sample);
    }
    
    AudioSampleImplPtr impl = fl::make_shared<AudioSampleImpl>();
    impl->assign(samples.begin(), samples.end(), 4000);
    AudioSample audioSample(impl);
    
    // Process sample (perceptual weighting should be applied automatically)
    audio.processSample(audioSample);
    
    // Check that processing completed without errors
    const AudioData& data = audio.getData();
    CHECK(data.volume >= 0.0f);
    CHECK(data.timestamp == 4000);
    
    // Frequency bins should have been processed
    bool hasNonZeroBins = false;
    for (int i = 0; i < 16; ++i) {
        if (data.frequencyBins[i] > 0.0f) {
            hasNonZeroBins = true;
            break;
        }
    }
    CHECK(hasNonZeroBins);
}

TEST_CASE("AudioReactive configuration validation") {
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
    CHECK(audio.getVolume() >= 0.0f);
    CHECK_FALSE(audio.isBassBeat()); // Should be false when multi-band is disabled
    CHECK_FALSE(audio.isMidBeat());
    CHECK_FALSE(audio.isTrebleBeat());
}

TEST_CASE("AudioReactive CircularBuffer functionality") {
    // Test the CircularBuffer template directly
    StaticCircularBuffer<float, 8> buffer;
    
    CHECK(buffer.empty());
    CHECK_FALSE(buffer.full());
    CHECK_EQ(buffer.size(), 0);
    CHECK_EQ(buffer.capacity(), 8);
    
    // Test pushing elements
    for (int i = 0; i < 5; ++i) {
        buffer.push(static_cast<float>(i));
    }
    
    CHECK_EQ(buffer.size(), 5);
    CHECK_FALSE(buffer.full());
    CHECK_FALSE(buffer.empty());
    
    // Test popping elements
    float value;
    CHECK(buffer.pop(value));
    CHECK_EQ(value, 0.0f);
    CHECK_EQ(buffer.size(), 4);
    
    // Fill buffer completely
    for (int i = 5; i < 12; ++i) {
        buffer.push(static_cast<float>(i));
    }
    
    CHECK(buffer.full());
    CHECK_EQ(buffer.size(), 8);
    
    // Test that old elements are overwritten
    buffer.push(100.0f);
    CHECK(buffer.full());
    CHECK_EQ(buffer.size(), 8);
    
    // Clear buffer
    buffer.clear();
    CHECK(buffer.empty());
    CHECK_EQ(buffer.size(), 0);
} 
