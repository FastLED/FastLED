/*  FastLED - Real-Time Beat Detection for EDM
    ---------------------------------------------------------
    Implements onset detection and beat tracking algorithms optimized
    for Electronic Dance Music (EDM) on embedded platforms (ESP32).

    Based on research from "Real-Time Onset Detection & Beat Tracking
    Algorithms for EDM and Embedded Systems" - implements SuperFlux-based
    onset detection with comb filter tempo tracking.

    Overview:
      This module provides real-time beat detection with:
      • SuperFlux onset detection with vibrato suppression
      • Multi-band spectral flux (bass/mid/high weighting)
      • Adaptive whitening for polyphonic music
      • Comb filter tempo tracking (100-150 BPM for EDM)
      • Real-time peak picking with adaptive thresholds
      • 4ms processing budget target for ESP32

    Key Features:
      • Optimized for EDM characteristics (kick drums, hi-hats, side-chaining)
      • Configurable multi-band analysis (24 mel bands recommended)
      • Real-time operation with minimal latency
      • Fixed-point arithmetic options for efficiency
      • Ring buffer management for streaming audio

    Usage:
      BeatDetectorConfig cfg;
      cfg.sample_rate_hz = 48000;
      cfg.frame_size = 512;
      cfg.hop_size = 256;
      cfg.tempo_range_bpm = {100, 150};

      BeatDetector detector(cfg);
      detector.onBeat = [](float confidence, float bpm) {
        // Handle beat events
      };

      // Process audio in chunks
      detector.processFrame(audioBuffer, bufferSize);

    License: MIT (same spirit as FastLED)
*/

#pragma once

#include "FastLED.h"

#if SKETCH_HAS_LOTS_OF_MEMORY

#include "fl/stdint.h"
#include "fl/function.h"
#include "fl/vector.h"
#include "fl/algorithm.h"
#include "fl/math.h"

namespace fl {

// ---------- Enumerations ----------

/// @brief Onset detection function types
enum class OnsetDetectionFunction : uint8_t {
    Energy = 0,         ///< Simple time-domain energy (fastest, least accurate)
    SpectralFlux,       ///< Positive magnitude difference (good baseline)
    SuperFlux,          ///< Spectral flux with maximum filter & delayed difference (best for EDM)
    HFC,                ///< High-frequency content (good for cymbals/hi-hats)
    ComplexDomain,      ///< Phase-aware detection (robust to vibrato)
    MultiBand           ///< Multi-band spectral flux with weighting (EDM optimized)
};

/// @brief Peak picking strategy
enum class PeakPickingMode : uint8_t {
    LocalMaximum = 0,   ///< Simple local maximum within window
    AdaptiveThreshold,  ///< Threshold based on local mean + delta
    SuperFluxPeaks      ///< Pre/post/avg windows with minimum distance (best for EDM)
};

/// @brief Tempo tracking algorithm
enum class TempoTrackerType : uint8_t {
    None = 0,           ///< No tempo tracking, onset detection only
    CombFilter,         ///< Comb filter with autocorrelation (recommended for EDM)
    Autocorrelation,    ///< Simple autocorrelation of ODF
    DynamicProgramming  ///< DP-based beat tracking (higher latency, handles tempo changes)
};

// ---------- Configuration Structures ----------

/// @brief Frequency band configuration for multi-band analysis
struct FrequencyBand {
    float low_hz;       ///< Lower frequency bound in Hz
    float high_hz;      ///< Upper frequency bound in Hz
    float weight;       ///< Weight for this band in the final ODF (0.0-1.0)
};

/// @brief Configuration for beat detector
struct BeatDetectorConfig {
    // Audio Parameters
    float sample_rate_hz = 48000.0f;    ///< Input sample rate in Hz
    int frame_size = 512;               ///< Analysis window size in samples (256-1024)
    int hop_size = 256;                 ///< Step size between frames in samples

    // Onset Detection
    OnsetDetectionFunction odf_type = OnsetDetectionFunction::SuperFlux;  ///< ODF algorithm
    int num_bands = 24;                 ///< Number of mel bands for spectral analysis (3-138)
    bool log_compression = true;        ///< Apply log compression to magnitude spectrum
    int superflux_mu = 3;               ///< SuperFlux delay parameter (frames to compare, 1-5)
    int max_filter_radius = 2;          ///< SuperFlux maximum filter radius (bins, 1-5)

    // Multi-band Configuration (only used if odf_type == MultiBand)
    fl::vector<FrequencyBand> bands;    ///< Custom frequency bands (defaults to 3-band: bass/mid/high)

    // Peak Picking
    PeakPickingMode peak_mode = PeakPickingMode::SuperFluxPeaks;  ///< Peak picking strategy
    float peak_threshold_delta = 0.07f; ///< Threshold above local mean (0.05-0.15)
    int peak_pre_max_ms = 30;           ///< Pre-window for local maximum (ms)
    int peak_post_max_ms = 30;          ///< Post-window for local maximum (ms)
    int peak_pre_avg_ms = 100;          ///< Pre-window for mean calculation (ms)
    int peak_post_avg_ms = 70;          ///< Post-window for mean calculation (ms)
    int min_inter_onset_ms = 30;        ///< Minimum time between onsets (ms, 30-50)

    // Tempo Tracking
    TempoTrackerType tempo_tracker = TempoTrackerType::CombFilter;  ///< Tempo tracking algorithm
    float tempo_min_bpm = 100.0f;       ///< Minimum tempo in BPM (EDM: 100-140)
    float tempo_max_bpm = 150.0f;       ///< Maximum tempo in BPM (EDM: 100-140)
    float tempo_rayleigh_sigma = 120.0f;///< Rayleigh prior center (BPM, typical human tempo)
    int tempo_acf_window_sec = 4;       ///< Autocorrelation window for tempo estimation (seconds)

    // Adaptive Whitening (improves polyphonic detection)
    bool adaptive_whitening = false;    ///< Enable adaptive spectral whitening
    float whitening_alpha = 0.95f;      ///< Smoothing factor for running maximum (0.9-0.99)

    // Optimization
    bool use_fixed_point = false;       ///< Use fixed-point arithmetic (faster, less precise)
    int fft_size = 512;                 ///< FFT size (must be power of 2, >= frame_size)

    // Constructor with EDM-optimized defaults
    BeatDetectorConfig() {
        // Default 3-band configuration: bass (kick), mid, high (hi-hats)
        bands.push_back({60.0f, 160.0f, 1.5f});   // Bass/kick emphasis
        bands.push_back({160.0f, 2000.0f, 1.0f}); // Mid
        bands.push_back({2000.0f, 8000.0f, 1.2f});// High/hi-hats emphasis
    }
};

// ---------- Results & Events ----------

/// @brief Onset detection result
struct OnsetEvent {
    uint32_t frame_index;   ///< Frame number where onset occurred
    float confidence;       ///< Onset strength/confidence (0.0-1.0+)
    float timestamp_ms;     ///< Timestamp in milliseconds from start
};

/// @brief Beat tracking result
struct BeatEvent {
    uint32_t frame_index;   ///< Frame number where beat occurred
    float confidence;       ///< Beat confidence (0.0-1.0)
    float timestamp_ms;     ///< Timestamp in milliseconds from start
    float bpm;              ///< Current tempo estimate in BPM
    float phase;            ///< Beat phase within bar (0.0-1.0, if known)
};

/// @brief Tempo estimation result
struct TempoEstimate {
    float bpm;              ///< Estimated tempo in beats per minute
    float confidence;       ///< Confidence in tempo estimate (0.0-1.0)
    int period_samples;     ///< Beat period in samples
};

// ---------- Onset Detection Function (ODF) Processor ----------

/// @brief Low-level onset detection function computation
/// @details Computes novelty curves from audio/spectral data
class OnsetDetectionProcessor {
public:
    /// @brief Construct ODF processor
    /// @param cfg Configuration
    explicit OnsetDetectionProcessor(const BeatDetectorConfig& cfg);

    /// @brief Process a single frame and compute onset detection value
    /// @param magnitude_spectrum FFT magnitude spectrum (linear, not dB)
    /// @param spectrum_size Number of bins in spectrum
    /// @return Onset detection function value (novelty)
    float processSpectrum(const float* magnitude_spectrum, int spectrum_size);

    /// @brief Process time-domain frame (for energy-based ODF)
    /// @param frame Time-domain samples
    /// @param n Number of samples
    /// @return Onset detection function value
    float processTimeDomain(const float* frame, int n);

    /// @brief Reset internal state
    void reset();

    /// @brief Get current configuration
    const BeatDetectorConfig& config() const { return _cfg; }

    /// @brief Update configuration
    void setConfig(const BeatDetectorConfig& cfg);

private:
    BeatDetectorConfig _cfg;

    // Spectral memory for difference calculations
    static constexpr int MAX_SPECTRUM_SIZE = 1024;
    static constexpr int MAX_HISTORY = 5;  // For SuperFlux mu parameter
    float _spectrumHistory[MAX_HISTORY][MAX_SPECTRUM_SIZE];
    int _historyIndex;
    int _historyCount;

    // Adaptive whitening state
    float _runningMax[MAX_SPECTRUM_SIZE];

    // Previous energy for time-domain ODF
    float _lastEnergy;

    // Internal methods
    float computeSpectralFlux(const float* mag, int size);
    float computeSuperFlux(const float* mag, int size);
    float computeHFC(const float* mag, int size);
    float computeMultiBand(const float* mag, int size);
    void applyAdaptiveWhitening(float* mag, int size);
    void applyLogCompression(float* mag, int size);
    void applyMaximumFilter(float* mag, int size, int radius);
    int freqToBin(float freq_hz, int fft_size, float sample_rate) const;
};

// ---------- Peak Picker ----------

/// @brief Peak detection in onset detection function
class PeakPicker {
public:
    /// @brief Construct peak picker
    /// @param cfg Configuration
    explicit PeakPicker(const BeatDetectorConfig& cfg);

    /// @brief Add new ODF value and check for peak
    /// @param odf_value Current onset detection function value
    /// @param frame_index Current frame number
    /// @param timestamp_ms Current timestamp in milliseconds
    /// @return OnsetEvent if peak detected, or nullptr
    fl::vector<OnsetEvent> process(float odf_value, uint32_t frame_index, float timestamp_ms);

    /// @brief Reset internal state
    void reset();

    /// @brief Get current configuration
    const BeatDetectorConfig& config() const { return _cfg; }

    /// @brief Update configuration
    void setConfig(const BeatDetectorConfig& cfg);

private:
    BeatDetectorConfig _cfg;

    // Ring buffer for ODF history
    static constexpr int MAX_ODF_BUFFER = 512;
    float _odfBuffer[MAX_ODF_BUFFER];
    uint32_t _frameBuffer[MAX_ODF_BUFFER];
    float _timestampBuffer[MAX_ODF_BUFFER];
    int _bufferIndex;
    int _bufferCount;

    // Frame index tracking
    uint32_t _lastOnsetFrame;

    // Window sizes in frames
    int _preMaxFrames;
    int _postMaxFrames;
    int _preAvgFrames;
    int _postAvgFrames;
    int _minInterOnsetFrames;

    // Internal methods
    bool isLocalMaximum(int center_idx) const;
    float computeLocalMean(int center_idx) const;
    bool meetsMinDistance(uint32_t frame_index) const;
    int msToFrames(int ms) const;
    int wrapIndex(int idx) const;
};

// ---------- Tempo Tracker ----------

/// @brief Tempo estimation and beat phase tracking
class TempoTracker {
public:
    /// @brief Construct tempo tracker
    /// @param cfg Configuration
    explicit TempoTracker(const BeatDetectorConfig& cfg);

    /// @brief Update tempo tracker with onset event
    /// @param onset Onset event
    void addOnset(const OnsetEvent& onset);

    /// @brief Update tempo tracker with ODF value (for autocorrelation)
    /// @param odf_value Onset detection function value
    /// @param timestamp_ms Current timestamp
    void addODFValue(float odf_value, float timestamp_ms);

    /// @brief Get current tempo estimate
    /// @return Tempo estimate
    TempoEstimate getTempo() const;

    /// @brief Check if current frame is a predicted beat
    /// @param timestamp_ms Current timestamp in milliseconds
    /// @return Beat event if beat detected, or empty vector
    fl::vector<BeatEvent> checkBeat(float timestamp_ms);

    /// @brief Reset internal state
    void reset();

    /// @brief Get current configuration
    const BeatDetectorConfig& config() const { return _cfg; }

    /// @brief Update configuration
    void setConfig(const BeatDetectorConfig& cfg);

private:
    BeatDetectorConfig _cfg;

    // Autocorrelation state
    static constexpr int MAX_ACF_LAG = 512;
    static constexpr int MAX_ODF_HISTORY = 2048;
    float _odfHistory[MAX_ODF_HISTORY];
    int _odfHistoryIndex;
    int _odfHistoryCount;

    // Tempo estimate state
    float _currentBPM;
    float _tempoConfidence;
    int _periodSamples;
    float _beatPhase;  // Current phase within beat cycle (0.0-1.0)
    float _lastBeatTime;

    // Comb filter state
    float _combFilter[MAX_ACF_LAG];

    // Internal methods
    void updateTempoEstimate();
    void computeAutocorrelation(float* acf, int max_lag);
    void applyCombFilter(float* acf, int max_lag);
    void applyRayleighWeighting(float* acf, int max_lag);
    int findPeakLag(const float* acf, int max_lag) const;
    int bpmToLag(float bpm) const;
    float lagToBPM(int lag) const;
    int bpmToSamples(float bpm) const;
};

// ---------- Main Beat Detector ----------

/// @brief Complete beat detection and tracking system
/// @details Integrates onset detection, peak picking, and tempo tracking
class BeatDetector {
public:
    /// @brief Construct beat detector with configuration
    /// @param cfg Configuration parameters
    explicit BeatDetector(const BeatDetectorConfig& cfg);

    /// @brief Callback invoked when an onset is detected
    /// @param confidence Onset confidence (0.0-1.0+)
    /// @param timestamp_ms Timestamp in milliseconds
    function<void(float confidence, float timestamp_ms)> onOnset;

    /// @brief Callback invoked when a beat is detected
    /// @param confidence Beat confidence (0.0-1.0)
    /// @param bpm Current tempo estimate in BPM
    /// @param timestamp_ms Timestamp in milliseconds
    function<void(float confidence, float bpm, float timestamp_ms)> onBeat;

    /// @brief Callback invoked when tempo estimate changes
    /// @param bpm New tempo in BPM
    /// @param confidence Confidence in tempo estimate (0.0-1.0)
    function<void(float bpm, float confidence)> onTempoChange;

    /// @brief Process an audio frame
    /// @param frame Audio samples (normalized float, -1.0 to +1.0)
    /// @param n Number of samples
    /// @details This is the main entry point for beat detection. Call this
    ///          repeatedly with consecutive audio chunks. Callbacks will be
    ///          invoked as beats/onsets are detected.
    void processFrame(const float* frame, int n);

    /// @brief Process pre-computed FFT magnitude spectrum
    /// @param magnitude_spectrum FFT magnitude (linear scale, not dB)
    /// @param spectrum_size Number of bins
    /// @param timestamp_ms Current timestamp in milliseconds
    /// @details Use this if you've already computed an FFT for other purposes
    void processSpectrum(const float* magnitude_spectrum, int spectrum_size, float timestamp_ms);

    /// @brief Get current tempo estimate
    /// @return Tempo estimate
    TempoEstimate getTempo() const;

    /// @brief Get the most recent ODF value
    /// @return Current onset detection function value
    float getCurrentODF() const { return _currentODF; }

    /// @brief Reset all internal state
    void reset();

    /// @brief Get current configuration
    const BeatDetectorConfig& config() const { return _cfg; }

    /// @brief Update configuration (affects subsequent processing)
    void setConfig(const BeatDetectorConfig& cfg);

    /// @brief Get total number of frames processed
    uint32_t getFrameCount() const { return _frameCount; }

    /// @brief Get total number of onsets detected
    uint32_t getOnsetCount() const { return _onsetCount; }

    /// @brief Get total number of beats detected
    uint32_t getBeatCount() const { return _beatCount; }

private:
    BeatDetectorConfig _cfg;

    // Processing components
    OnsetDetectionProcessor _odfProcessor;
    PeakPicker _peakPicker;
    TempoTracker _tempoTracker;

    // State tracking
    uint32_t _frameCount;
    uint32_t _onsetCount;
    uint32_t _beatCount;
    float _currentODF;
    float _lastTempoBPM;

    // Internal methods
    float getTimestampMs() const;
};

// ---------- Utility Functions ----------

/// @brief Convert BPM to period in samples
/// @param bpm Tempo in beats per minute
/// @param sample_rate Sample rate in Hz
/// @return Period in samples
inline int bpmToSamples(float bpm, float sample_rate) {
    return static_cast<int>((60.0f * sample_rate) / bpm);
}

/// @brief Convert period in samples to BPM
/// @param samples Period in samples
/// @param sample_rate Sample rate in Hz
/// @return Tempo in BPM
inline float samplesToBPM(int samples, float sample_rate) {
    return (60.0f * sample_rate) / static_cast<float>(samples);
}

/// @brief Convert frequency to FFT bin index
/// @param freq_hz Frequency in Hz
/// @param fft_size FFT size
/// @param sample_rate Sample rate in Hz
/// @return Bin index
inline int freqToBin(float freq_hz, int fft_size, float sample_rate) {
    return static_cast<int>((freq_hz * fft_size) / sample_rate);
}

/// @brief Convert FFT bin index to frequency
/// @param bin Bin index
/// @param fft_size FFT size
/// @param sample_rate Sample rate in Hz
/// @return Frequency in Hz
inline float binToFreq(int bin, int fft_size, float sample_rate) {
    return (static_cast<float>(bin) * sample_rate) / fft_size;
}

/// @brief Compute Rayleigh weighting function
/// @param lag Lag value
/// @param sigma Center parameter (typical tempo)
/// @return Weight value
inline float rayleighWeight(float lag, float sigma) {
    float x = lag / sigma;
    return x * fl::exp(-0.5f * x * x);
}

} // namespace fl

#endif // SKETCH_HAS_LOTS_OF_MEMORY
