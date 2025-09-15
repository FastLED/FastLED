#pragma once

#include "fl/fft.h"
#include "fl/math.h"
#include "fl/vector.h"
#include "fl/stdint.h"
#include "fl/int.h"
#include "fl/audio.h"
#include "fl/array.h"
#include "fl/unique_ptr.h"
#include "fl/sketch_macros.h"
#include "crgb.h"
#include "fl/colorutils.h"

namespace fl {

// Forward declarations for enhanced beat detection
class SpectralFluxDetector;
class PerceptualWeighting;

// Audio data structure - matches original WLED output with extensions
struct AudioData {
    float volume = 0.0f;                    // Overall volume level (0-255)
    float volumeRaw = 0.0f;                 // Raw volume without smoothing
    float peak = 0.0f;                      // Peak level (0-255) 
    bool beatDetected = false;              // Beat detection flag
    float frequencyBins[16] = {0};          // 16 frequency bins (matches WLED NUM_GEQ_CHANNELS)
    float dominantFrequency = 0.0f;         // Major peak frequency (Hz)
    float magnitude = 0.0f;                 // FFT magnitude of dominant frequency
    fl::u32 timestamp = 0;                 // millis() when data was captured
    
    // Enhanced beat detection fields
    bool bassBeatDetected = false;          // Bass-specific beat detection
    bool midBeatDetected = false;           // Mid-range beat detection
    bool trebleBeatDetected = false;        // Treble beat detection
    float spectralFlux = 0.0f;              // Current spectral flux value
    float bassEnergy = 0.0f;                // Energy in bass frequencies (0-1)
    float midEnergy = 0.0f;                 // Energy in mid frequencies (6-7)
    float trebleEnergy = 0.0f;              // Energy in treble frequencies (14-15)
};

struct AudioReactiveConfig {
    fl::u8 gain = 128;              // Input gain (0-255)
    fl::u8 sensitivity = 128;       // AGC sensitivity
    bool agcEnabled = true;          // Auto gain control
    bool noiseGate = true;           // Noise gate
    fl::u8 attack = 50;             // Attack time (ms) - how fast to respond to increases
    fl::u8 decay = 200;             // Decay time (ms) - how slow to respond to decreases
    u16 sampleRate = 22050;     // Sample rate (Hz)
    fl::u8 scalingMode = 3;         // 0=none, 1=log, 2=linear, 3=sqrt

    // Enhanced beat detection configuration
    bool enableSpectralFlux = true;     // Enable spectral flux-based beat detection
    bool enableMultiBand = true;        // Enable multi-band beat detection
    float spectralFluxThreshold = 0.1f; // Threshold for spectral flux detection
    float bassThreshold = 0.15f;        // Threshold for bass beat detection
    float midThreshold = 0.12f;         // Threshold for mid beat detection
    float trebleThreshold = 0.08f;      // Threshold for treble beat detection
};

class AudioReactive {
public:
    AudioReactive();
    ~AudioReactive();
    
    // Setup
    void begin(const AudioReactiveConfig& config = AudioReactiveConfig{});
    void setConfig(const AudioReactiveConfig& config);
    
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
    
    // Enhanced beat detection accessors
    bool isBassBeat() const;
    bool isMidBeat() const;
    bool isTrebleBeat() const;
    float getSpectralFlux() const;
    float getBassEnergy() const;
    float getMidEnergy() const;
    float getTrebleEnergy() const;
    
    // Effect helpers
    fl::u8 volumeToScale255() const;
    CRGB volumeToColor(const CRGBPalette16& palette) const;
    fl::u8 frequencyToScale255(fl::u8 binIndex) const;

private:
    // Internal processing methods
    void processFFT(const AudioSample& sample);
    void mapFFTBinsToFrequencyChannels();
    void updateVolumeAndPeak(const AudioSample& sample);
    void detectBeat(fl::u32 currentTimeMs);
    void smoothResults();
    void applyScaling();
    void applyGain();
    
    // Enhanced beat detection methods
    void detectEnhancedBeats(fl::u32 currentTimeMs);
    void calculateBandEnergies();
    void updateSpectralFlux();
    void applyPerceptualWeighting();
    
    // Helper methods
    float mapFrequencyBin(int fromBin, int toBin);
    float computeRMS(const fl::vector<fl::i16>& samples);
    
    // Configuration
    AudioReactiveConfig mConfig;
    
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
    
    // Enhanced beat detection components
    fl::unique_ptr<SpectralFluxDetector> mSpectralFluxDetector;
    fl::unique_ptr<PerceptualWeighting> mPerceptualWeighting;
    
    // Enhanced beat detection state
    fl::array<float, 16> mPreviousMagnitudes;
};

// Spectral flux-based onset detection for enhanced beat detection
class SpectralFluxDetector {
public:
    SpectralFluxDetector();
    ~SpectralFluxDetector();
    
    void reset();
    bool detectOnset(const float* currentBins, const float* previousBins);
    float calculateSpectralFlux(const float* currentBins, const float* previousBins);
    void setThreshold(float threshold);
    float getThreshold() const;
    
private:
    float mFluxThreshold;
    fl::array<float, 16> mPreviousMagnitudes;
    
#if SKETCH_HAS_LOTS_OF_MEMORY
    fl::array<float, 32> mFluxHistory;      // For advanced smoothing
    fl::size mHistoryIndex;
    float calculateAdaptiveThreshold();
#endif
};

// Multi-band beat detection for different frequency ranges
struct BeatDetectors {
    BeatDetectors();
    ~BeatDetectors();
    
    void reset();
    void detectBeats(const float* frequencyBins, AudioData& audioData);
    void setThresholds(float bassThresh, float midThresh, float trebleThresh);
    
private:
#if SKETCH_HAS_LOTS_OF_MEMORY
    SpectralFluxDetector bass;     // 20-200 Hz (bins 0-1)
    SpectralFluxDetector mid;      // 200-2000 Hz (bins 6-7)
    SpectralFluxDetector treble;   // 2000-20000 Hz (bins 14-15)
#else
    SpectralFluxDetector combined; // Single detector for memory-constrained
#endif
    
    // Energy tracking for band-specific thresholds
    float mBassEnergy;
    float mMidEnergy; 
    float mTrebleEnergy;
    float mPreviousBassEnergy;
    float mPreviousMidEnergy;
    float mPreviousTrebleEnergy;
};

// Perceptual audio weighting for psychoacoustic processing
class PerceptualWeighting {
public:
    PerceptualWeighting();
    ~PerceptualWeighting();
    
    void applyAWeighting(AudioData& data) const;
    void applyLoudnessCompensation(AudioData& data, float referenceLevel) const;
    
private:
    // A-weighting coefficients for 16-bin frequency analysis
    static constexpr float A_WEIGHTING_COEFFS[16] = {
        0.5f, 0.6f, 0.8f, 1.0f, 1.2f, 1.3f, 1.4f, 1.4f,
        1.3f, 1.2f, 1.0f, 0.8f, 0.6f, 0.4f, 0.2f, 0.1f
    };
    
#if SKETCH_HAS_LOTS_OF_MEMORY
    fl::array<float, 16> mLoudnessHistory;  // For dynamic compensation
    fl::size mHistoryIndex;
#endif
};

} // namespace fl
