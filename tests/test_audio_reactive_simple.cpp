#include "test.h"
#include "fl/audio_reactive.h"
#include "fl/math.h"
#include <math.h>

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
    
    // Create AudioSample from our generated samples
    AudioSampleImplPtr impl = AudioSampleImplPtr::New();
    impl->assign(samples.begin(), samples.end());
    AudioSample audioSample(impl);
    
    // Process the audio sample directly (use a fake timestamp)
    audio.processSample(audioSample, 1000); // 1 second
    
    // Check that we detected some audio
    const AudioData& processedData = audio.getData();
    CHECK(processedData.volume > 0.0f);
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
