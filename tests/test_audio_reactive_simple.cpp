#include "test.h"
#include "fl/audio_reactive.h"
#include "fl/math.h"
#include <math.h>
#include "fl/memory.h"

using namespace fl;

TEST_CASE("AudioReactive basic functionality") {
    // Test basic initialization
    AudioReactive audio;
    AudioConfig config;
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
    AudioConfig config;
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
