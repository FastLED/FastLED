#pragma once

#include "fl/fft.h"
#include "fl/math.h"
#include "fl/vector.h"
#include "fl/stdint.h"
#include "fl/int.h"
#include "fl/audio.h"
#include "crgb.h"
#include "fl/colorutils.h"

namespace fl {

// Audio data structure - matches original WLED output
struct AudioData {
    float volume = 0.0f;                    // Overall volume level (0-255)
    float volumeRaw = 0.0f;                 // Raw volume without smoothing
    float peak = 0.0f;                      // Peak level (0-255) 
    bool beatDetected = false;              // Beat detection flag
    float frequencyBins[16] = {0};          // 16 frequency bins (matches WLED NUM_GEQ_CHANNELS)
    float dominantFrequency = 0.0f;         // Major peak frequency (Hz)
    float magnitude = 0.0f;                 // FFT magnitude of dominant frequency
    fl::u32 timestamp = 0;                 // millis() when data was captured
};

struct AudioConfig {
    uint8_t gain = 128;              // Input gain (0-255)
    uint8_t sensitivity = 128;       // AGC sensitivity
    bool agcEnabled = true;          // Auto gain control
    bool noiseGate = true;           // Noise gate
    uint8_t attack = 50;             // Attack time (ms) - how fast to respond to increases
    uint8_t decay = 200;             // Decay time (ms) - how slow to respond to decreases
    u16 sampleRate = 22050;     // Sample rate (Hz)
    uint8_t scalingMode = 3;         // 0=none, 1=log, 2=linear, 3=sqrt
};

class AudioReactive {
public:
    AudioReactive();
    ~AudioReactive();
    
    // Setup
    void begin(const AudioConfig& config = AudioConfig{});
    void setConfig(const AudioConfig& config);
    
    // Process audio sample - this does all the work immediately
    void processSample(const AudioSample& sample);
    
    // Optional: update smoothing without new sample data  
    void update(fl::u32 currentTimeMs);
    
    // Data access
    const AudioData& getData() const;
    const AudioData& getSmoothedData() const;
    
    // Convenience accessors
    float getVolume() const;
    float getBass() const;    // Average of bins 0-1
    float getMid() const;     // Average of bins 6-7 
    float getTreble() const;  // Average of bins 14-15
    bool isBeat() const;
    
    // Effect helpers
    uint8_t volumeToScale255() const;
    CRGB volumeToColor(const CRGBPalette16& palette) const;
    uint8_t frequencyToScale255(uint8_t binIndex) const;

private:
    // Internal processing methods
    void processFFT(const AudioSample& sample);
    void mapFFTBinsToFrequencyChannels();
    void updateVolumeAndPeak(const AudioSample& sample);
    void detectBeat(fl::u32 currentTimeMs);
    void smoothResults();
    void applyScaling();
    void applyGain();
    
    // Helper methods
    float mapFrequencyBin(int fromBin, int toBin);
    float computeRMS(const fl::vector<int16_t>& samples);
    
    // Configuration
    AudioConfig mConfig;
    
    // FFT processing
    FFT mFFT;
    FFTBins mFFTBins;
    
    // Audio data  
    AudioData mCurrentData;
    AudioData mSmoothedData;
    
    // Processing state  
    fl::u32 mLastBeatTime = 0;
    static constexpr fl::u32 BEAT_COOLDOWN = 100;   // 100ms minimum between beats
    
    // Volume tracking for beat detection
    float mPreviousVolume = 0.0f;
    float mVolumeThreshold = 10.0f;
    
    // Pink noise compensation (from WLED)
    static constexpr float PINK_NOISE_COMPENSATION[16] = {
        1.70f, 1.71f, 1.73f, 1.78f, 1.68f, 1.56f, 1.55f, 1.63f,
        1.79f, 1.62f, 1.80f, 2.06f, 2.47f, 3.35f, 6.83f, 9.55f
    };
    
    // AGC state
    float mAGCMultiplier = 1.0f;
    float mMaxSample = 0.0f;
    float mAverageLevel = 0.0f;
};


} // namespace fl
