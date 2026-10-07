#pragma once

#include "fl/audio/fft/fft.h"
#include "fl/stl/int.h"
#include "fl/audio/audio.h"  // IWYU pragma: keep
#include "fl/audio/audio_context.h"
#include "fl/audio/signal_conditioner.h"
#include "fl/audio/noise_floor_tracker.h"
#include "fl/audio/frequency_bin_mapper.h"
#include "fl/audio/silence_envelope.h"
#include "fl/audio/spectral_equalizer.h"
#include "fl/stl/array.h"
#include "fl/stl/span.h"
#include "fl/stl/shared_ptr.h"
#include "fl/stl/unique_ptr.h"
#include "fl/system/sketch_macros.h"
#include "fl/gfx/colorutils.h"  // IWYU pragma: keep
#include "fl/gfx/crgb.h"  // for CRGB  // IWYU pragma: keep
#include "fl/stl/vector.h"
#include "fl/stl/noexcept.h"

namespace fl {
namespace audio {

class Processor;

// Forward declarations for enhanced beat detection
class SpectralFluxDetector;
class PerceptualWeighting;
namespace detector {
class MusicalBeat;
class MultiBandBeat;
}

// Audio data structure - matches original WLED output with extensions
struct Data {
    float volume = 0.0f;                    // Overall volume level (0.0-1.0, adaptive normalized)
    float volumeRaw = 0.0f;                 // Raw volume without smoothing (0.0-1.0)
    float peak = 0.0f;                      // Peak level (0.0-1.0)
    bool beatDetected = false;              // Beat detection flag
    float frequencyBins[16] = {0};          // 16 frequency bins (matches WLED NUM_GEQ_CHANNELS)
    float dominantFrequency = 0.0f;         // Major peak frequency (Hz)
    float magnitude = 0.0f;                 // fft::FFT magnitude of dominant frequency
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

struct ReactiveConfig {
    fl::u8 gain = 128;              // Input gain (0-255)
    fl::u8 sensitivity = 128;       // AGC sensitivity
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

    // Signal conditioning configuration
    bool enableSignalConditioning = true;  // Enable DC removal, spike filter, noise gate
    bool enableNoiseFloorTracking = true;   // Enable noise floor tracking

    // Frequency bin mapping configuration
    bool enableLogBinSpacing = true;       // Use logarithmic frequency bin spacing (vs linear)

    // Spectral equalizer configuration (optional middleware)
    bool enableSpectralEqualizer = false;  // Enable spectral EQ (A-weighting, custom curves)

    // Musical beat detection configuration (Phase 3 middleware)
    bool enableMusicalBeatDetection = false; // Enable advanced beat tracking with BPM
    bool enableMultiBandBeats = false;       // Enable per-band beat detection
    float musicalBeatMinBPM = 60.0f;        // Minimum BPM for musical beats
    float musicalBeatMaxBPM = 180.0f;       // Maximum BPM for musical beats
    float musicalBeatConfidence = 0.5f;     // Minimum confidence for beat validation
};

class Reactive {
public:
    Reactive() FL_NO_EXCEPT;
    ~Reactive() FL_NO_EXCEPT;
    
    // Setup
    void begin(const ReactiveConfig& config = ReactiveConfig{}) FL_NO_EXCEPT;
    void setConfig(const ReactiveConfig& config) FL_NO_EXCEPT;
    
    // Process audio sample - this does all the work immediately
    void processSample(const Sample& sample) FL_NO_EXCEPT;
    
    // Optional: update smoothing without new sample data  
    void update(fl::u32 currentTimeMs) FL_NO_EXCEPT;
    
    // Data access
    const Data& getData() const FL_NO_EXCEPT;
    const Data& getSmoothedData() const FL_NO_EXCEPT;
    
    // Convenience accessors
    float getVolume() const FL_NO_EXCEPT;
    float getBass() const FL_NO_EXCEPT;    // Average of bins 0-1
    float getMid() const FL_NO_EXCEPT;     // Average of bins 6-7
    float getTreble() const FL_NO_EXCEPT;  // Average of bins 14-15
    bool isBeat() const FL_NO_EXCEPT;
    
    // Enhanced beat detection accessors
    bool isBassBeat() const FL_NO_EXCEPT;
    bool isMidBeat() const FL_NO_EXCEPT;
    bool isTrebleBeat() const FL_NO_EXCEPT;
    float getSpectralFlux() const FL_NO_EXCEPT;
    float getBassEnergy() const FL_NO_EXCEPT;
    float getMidEnergy() const FL_NO_EXCEPT;
    float getTrebleEnergy() const FL_NO_EXCEPT;
    
    // ----- Polling Getters (float 0.0-1.0, bool, or integer) -----
    // These forward to an internal Processor for detector-based analysis.

    // Vocal Detection
    float getVocalConfidence() FL_NO_EXCEPT;

    // Beat Detection (detector-based)
    float getBeatConfidence() FL_NO_EXCEPT;
    float getBPM() FL_NO_EXCEPT;

    // Energy Analysis
    float getEnergyLevel() FL_NO_EXCEPT;
    float getPeakLevel() FL_NO_EXCEPT;

    // Frequency Bands
    float getBassLevel() FL_NO_EXCEPT;
    float getMidLevel() FL_NO_EXCEPT;
    float getTrebleLevel() FL_NO_EXCEPT;

    // Silence Detection
    bool isSilent() FL_NO_EXCEPT;
    u32 getSilenceDuration() FL_NO_EXCEPT;

    // Transient Detection
    float getTransientStrength() FL_NO_EXCEPT;

    // Dynamics Analysis
    float getDynamicTrend() FL_NO_EXCEPT;  // -1.0 to 1.0
    bool isCrescendo() FL_NO_EXCEPT;
    bool isDiminuendo() FL_NO_EXCEPT;

    // Pitch Detection
    float getPitchConfidence() FL_NO_EXCEPT;
    float getPitchHz() FL_NO_EXCEPT;

    // Tempo Analysis
    float getTempoConfidence() FL_NO_EXCEPT;
    float getTempoBPM() FL_NO_EXCEPT;

    // Buildup Detection
    float getBuildupIntensity() FL_NO_EXCEPT;
    float getBuildupProgress() FL_NO_EXCEPT;

    // Drop Detection
    float getDropImpact() FL_NO_EXCEPT;

    // Percussion Detection
    bool isKick() FL_NO_EXCEPT;
    bool isSnare() FL_NO_EXCEPT;
    bool isHiHat() FL_NO_EXCEPT;
    bool isTom() FL_NO_EXCEPT;

    // Note Detection
    u8 getCurrentNote() FL_NO_EXCEPT;
    float getNoteVelocity() FL_NO_EXCEPT;
    float getNoteConfidence() FL_NO_EXCEPT;

    // Downbeat Detection
    float getDownbeatConfidence() FL_NO_EXCEPT;
    float getMeasurePhase() FL_NO_EXCEPT;
    u8 getCurrentBeatNumber() FL_NO_EXCEPT;

    // Backbeat Detection
    float getBackbeatConfidence() FL_NO_EXCEPT;
    float getBackbeatStrength() FL_NO_EXCEPT;

    // Chord Detection
    float getChordConfidence() FL_NO_EXCEPT;

    // Key Detection
    float getKeyConfidence() FL_NO_EXCEPT;

    // Mood Analysis
    float getMoodArousal() FL_NO_EXCEPT;
    float getMoodValence() FL_NO_EXCEPT;  // -1.0 to 1.0

    // Gain control - delegates to internal Processor
    void setGain(float gain) FL_NO_EXCEPT;
    float getGain() const FL_NO_EXCEPT;

    // Effect helpers
    fl::u8 volumeToScale255() const FL_NO_EXCEPT;
    CRGB volumeToColor(const CRGBPalette16& palette) const FL_NO_EXCEPT;
    fl::u8 frequencyToScale255(fl::u8 binIndex) const FL_NO_EXCEPT;

    // Signal conditioning stats
    const SignalConditioner::Stats& getSignalConditionerStats() const FL_NO_EXCEPT;
    const NoiseFloorTracker::Stats& getNoiseFloorStats() const FL_NO_EXCEPT;

    // Spectral equalizer stats (optional middleware - must be enabled first)
    bool isSpectralEqualizerEnabled() const FL_NO_EXCEPT;
    const SpectralEqualizer::Stats& getSpectralEqualizerStats() const FL_NO_EXCEPT;

private:
    // Internal processing methods
    void processFFT(const Sample& sample) FL_NO_EXCEPT;
    void mapFFTBinsToFrequencyChannels() FL_NO_EXCEPT;
    void updateVolumeAndPeak(const Sample& sample) FL_NO_EXCEPT;
    void detectBeat(fl::u32 currentTimeMs) FL_NO_EXCEPT;
    void smoothResults() FL_NO_EXCEPT;
    void applyScaling() FL_NO_EXCEPT;
    void applyGain() FL_NO_EXCEPT;
    
    // Enhanced beat detection methods
    void detectEnhancedBeats(fl::u32 currentTimeMs) FL_NO_EXCEPT;
    void calculateBandEnergies() FL_NO_EXCEPT;
    void updateSpectralFlux() FL_NO_EXCEPT;
    void applyAWeighting() FL_NO_EXCEPT;
    void applyLoudnessCompensation() FL_NO_EXCEPT;
    void applySpectralEqualization() FL_NO_EXCEPT;
    
    // Helper methods
    float mapFrequencyBin(int fromBin, int toBin) FL_NO_EXCEPT;
    float computeRMS(const fl::vector<fl::i16>& samples) FL_NO_EXCEPT;
    
    // Configuration
    ReactiveConfig mConfig;
    
    // FFT processing (via shared Context for caching)
    shared_ptr<Context> mContext;
    fft::Bins mFFTBins;
    
    // Audio data  
    Data mCurrentData;
    Data mSmoothedData;
    
    // Processing state  
    fl::u32 mLastBeatTime = 0;
    static constexpr fl::u32 BEAT_COOLDOWN = 100;   // 100ms minimum between beats
    
    // Volume tracking for beat detection
    float mPreviousVolume = 0.0f;
    float mVolumeThreshold = 0.04f;
    
    // Pink noise compensation: √(f_center / f_ref) normalized so geometric mean ≈ 1.0.
    // Compensates for 1/f spectral tilt of natural audio (music, speech).
    // Computed dynamically from bin centers in begin().
    float mPinkNoiseGains[16] = {};
    bool mPinkNoiseComputed = false;
    
    // Signal conditioning components
    SignalConditioner mSignalConditioner;
    NoiseFloorTracker mNoiseFloorTracker;
    FrequencyBinMapper mFrequencyBinMapper;

    // Enhanced beat detection components
    fl::unique_ptr<SpectralFluxDetector> mSpectralFluxDetector;
    fl::unique_ptr<PerceptualWeighting> mPerceptualWeighting;

    // Musical beat detection components (Phase 3 middleware)
    fl::unique_ptr<detector::MusicalBeat> mMusicalBeatDetector;
    fl::unique_ptr<detector::MultiBandBeat> mMultiBandBeatDetector;
    fl::unique_ptr<SpectralEqualizer> mSpectralEqualizer;

    // Enhanced beat detection state
    fl::array<float, 16> mPreviousMagnitudes;

    // Silence-gate envelopes for spectral metrics (FastLED#2253).
    // During silence (Context::isSilent() true), these fast-decay the
    // dominantFrequency / magnitude / spectralFlux outputs toward zero so
    // the FFT noise floor does not lock onto arbitrary bins. During audio
    // they are pass-through. Tau = 0.2 s — spectral metrics are brittle
    // and should snap to zero quickly.
    SilenceEnvelope mDominantFrequencyEnvelope;
    SilenceEnvelope mMagnitudeEnvelope;
    SilenceEnvelope mSpectralFluxEnvelope;

    // Internal Processor for detector-based polling getters
    fl::unique_ptr<Processor> mAudioProcessor;
    Processor& ensureAudioProcessor() FL_NO_EXCEPT;
};

// Spectral flux-based onset detection for enhanced beat detection
class SpectralFluxDetector {
public:
    SpectralFluxDetector() FL_NO_EXCEPT;
    ~SpectralFluxDetector() FL_NO_EXCEPT;
    
    void reset() FL_NO_EXCEPT;
    bool detectOnset(span<const float, 16> currentBins) FL_NO_EXCEPT;
    float calculateSpectralFlux(span<const float, 16> currentBins, span<const float, 16> previousBins) FL_NO_EXCEPT;
    void setThreshold(float threshold) FL_NO_EXCEPT;
    float getThreshold() const FL_NO_EXCEPT;
    
private:
    float mFluxThreshold;
    fl::array<float, 16> mPreviousMagnitudes;
    
#if SKETCH_HAS_LARGE_MEMORY
    fl::array<float, 32> mFluxHistory;      // For advanced smoothing
    fl::size mHistoryIndex;
    float calculateAdaptiveThreshold() FL_NO_EXCEPT;
#endif
};

// Multi-band beat detection for different frequency ranges
struct BeatDetectors {
    BeatDetectors() FL_NO_EXCEPT;
    ~BeatDetectors() FL_NO_EXCEPT;
    
    void reset() FL_NO_EXCEPT;
    void detectBeats(span<const float, 16> frequencyBins, Data& audioData) FL_NO_EXCEPT;
    void setThresholds(float bassThresh, float midThresh, float trebleThresh) FL_NO_EXCEPT;
    
private:
#if SKETCH_HAS_LARGE_MEMORY
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
    PerceptualWeighting() FL_NO_EXCEPT;
    ~PerceptualWeighting() FL_NO_EXCEPT;
    
    void applyAWeighting(Data& data) const FL_NO_EXCEPT;
    void applyLoudnessCompensation(Data& data, float referenceLevel) const FL_NO_EXCEPT;
    
private:
    // A-weighting coefficients for 16-bin frequency analysis
    static constexpr float A_WEIGHTING_COEFFS[16] = {
        0.5f, 0.6f, 0.8f, 1.0f, 1.2f, 1.3f, 1.4f, 1.4f,
        1.3f, 1.2f, 1.0f, 0.8f, 0.6f, 0.4f, 0.2f, 0.1f
    };
    
#if SKETCH_HAS_LARGE_MEMORY
    fl::array<float, 16> mLoudnessHistory;  // For dynamic compensation
    fl::size mHistoryIndex;
#endif
};

} // namespace audio
} // namespace fl
