#include "test.h"

#include "fl/audio.h"
#include "fl/fft.h"
#include "fl/math.h"

using namespace fl;

TEST_CASE("AudioSample FFT integration") {
    // Create a simple sine wave
    const int sample_size = 512;
    fl::vector<int16_t> pcm_data;
    
    // Generate a 440Hz sine wave (A note)
    for (int i = 0; i < sample_size; ++i) {
        float t = static_cast<float>(i) / 44100.0f; // Assume 44.1kHz sample rate
        float frequency = 440.0f; // A note
        float amplitude = 0.5f;
        float sample = amplitude * sin(2.0f * PI * frequency * t);
        pcm_data.push_back(static_cast<int16_t>(sample * 32767.0f));
    }
    
    // Create AudioSample
    AudioSampleImplPtr impl = NewPtr<AudioSampleImpl>();
    impl->assign(pcm_data.begin(), pcm_data.end());
    AudioSample audio_sample(impl);
    
    // Verify basic properties
    CHECK(audio_sample.isValid());
    CHECK(audio_sample.size() == sample_size);
    
    // Test FFT functionality
    FFTBins fft_bins(32);
    audio_sample.fft(&fft_bins);
    
    // Verify FFT output has expected structure
    CHECK(fft_bins.bins_raw.size() == 32);
    CHECK(fft_bins.bins_db.size() == 32);
    
    // Since we generated a 440Hz tone, we should see a peak in the appropriate frequency bin
    // The exact bin depends on the FFT configuration, but there should be significantly
    // more energy in some bins than others
    float max_energy = 0.0f;
    int max_bin = 0;
    for (int i = 0; i < 32; ++i) {
        if (fft_bins.bins_raw[i] > max_energy) {
            max_energy = fft_bins.bins_raw[i];
            max_bin = i;
        }
    }
    
    // Verify we got a significant peak (should be much higher than background)
    CHECK(max_energy > 100.0f); // Arbitrary threshold for sine wave energy
    
    FASTLED_WARN("FFT peak at bin " << max_bin << " with energy " << max_energy);
}

TEST_CASE("AudioSample drum tone reactive test") {
    // Generate a drum tone - short burst of low frequency content with decay
    const int sample_size = 1024;
    fl::vector<int16_t> drum_pcm;
    
    // Drum characteristics:
    // - Low fundamental frequency (60-80Hz)
    // - Quick attack and exponential decay
    // - Some harmonic content and noise
    const float sample_rate = 44100.0f;
    const float fundamental_freq = 70.0f; // Bass drum frequency
    const float decay_time = 0.02f; // 20ms decay time
    
    for (int i = 0; i < sample_size; ++i) {
        float t = static_cast<float>(i) / sample_rate;
        
        // Exponential decay envelope
        float envelope = exp(-t / decay_time);
        
        // Fundamental frequency
        float fundamental = sin(2.0f * PI * fundamental_freq * t);
        
        // Add some harmonics for realistic drum sound
        float harmonic2 = 0.3f * sin(2.0f * PI * fundamental_freq * 2.0f * t);
        float harmonic3 = 0.1f * sin(2.0f * PI * fundamental_freq * 3.0f * t);
        
        // Add a bit of noise for texture (fix RAND_MAX conversion issue)
        float noise = 0.05f * (static_cast<float>(rand()) / static_cast<float>(RAND_MAX) - 0.5f);
        
        // Combine all components
        float sample = envelope * (fundamental + harmonic2 + harmonic3 + noise);
        
        // Scale to int16 range
        drum_pcm.push_back(static_cast<int16_t>(sample * 32767.0f * 0.8f));
    }
    
    // Create AudioSample from drum tone
    AudioSampleImplPtr impl = NewPtr<AudioSampleImpl>();
    impl->assign(drum_pcm.begin(), drum_pcm.end());
    AudioSample drum_sample(impl);
    
    // Verify drum sample is valid
    CHECK(drum_sample.isValid());
    CHECK(drum_sample.size() == sample_size);
    
    // Test audio reactive components
    
    // 1. Test RMS (Root Mean Square) - should be significant due to drum hit
    float rms_value = drum_sample.rms();
    CHECK(rms_value > 1000.0f); // Should have significant energy
    FASTLED_WARN("Drum RMS value: " << rms_value);
    
    // 2. Test Zero Crossing Factor - drums should have relatively low ZCF
    float zcf_value = drum_sample.zcf();
    CHECK(zcf_value < 0.3f); // Low frequency content should have low zero crossing rate
    FASTLED_WARN("Drum ZCF value: " << zcf_value);
    
    // 3. Test FFT response - should show energy in low frequency bins
    FFTBins fft_result(64);
    drum_sample.fft(&fft_result);
    
    // Verify low frequency bins have more energy than high frequency bins
    float low_freq_energy = 0.0f;
    float high_freq_energy = 0.0f;
    
    // Sum energy in low frequency bins (first quarter)
    for (int i = 0; i < 16; ++i) {
        low_freq_energy += fft_result.bins_raw[i];
    }
    
    // Sum energy in high frequency bins (last quarter)
    for (int i = 48; i < 64; ++i) {
        high_freq_energy += fft_result.bins_raw[i];
    }
    
    // Drum should have significantly more low frequency energy
    CHECK(low_freq_energy > high_freq_energy * 2.0f);
    FASTLED_WARN("Low freq energy: " << low_freq_energy << ", High freq energy: " << high_freq_energy);
    
    // 4. Test SoundLevelMeter response
    SoundLevelMeter sound_meter(33.0f, 0.0f); // Standard settings
    sound_meter.processBlock(drum_sample.pcm());
    
    double dbfs = sound_meter.getDBFS();
    double spl = sound_meter.getSPL();
    
    // DBFS should be negative but not too quiet (drum hit should be audible)
    CHECK(dbfs > -40.0f); // Should be louder than -40dBFS
    CHECK(dbfs < 0.0f);   // Should be negative (proper dBFS)
    
    // SPL should be reasonable for a drum hit
    CHECK(spl >= 33.0f);   // Should register as audible sound (floor is 33.0f)
    
    FASTLED_WARN("Drum dBFS: " << dbfs << ", SPL: " << spl);
    
    // 5. Test that the drum sample can be distinguished from silence
    fl::vector<int16_t> silence(sample_size, 0);
    AudioSampleImplPtr silence_impl = NewPtr<AudioSampleImpl>();
    silence_impl->assign(silence.begin(), silence.end());
    AudioSample silence_sample(silence_impl);
    
    // Drum should have much higher RMS than silence
    float silence_rms = silence_sample.rms();
    CHECK(drum_sample.rms() > silence_rms * 1000.0f);
    
    // Drum and silence should not be equal
    CHECK(drum_sample != silence_sample);
    
    FASTLED_WARN("Test completed: Drum tone successfully generated and audio reactive components responded correctly");
}
