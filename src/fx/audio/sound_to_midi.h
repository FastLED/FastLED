/*  FastLED - Audio Sound → MIDI (Monophonic & Polyphonic)
    ---------------------------------------------------------
    Converts audio frames to MIDI Note On/Off events using YIN/MPM-like
    pitch detection (monophonic) or spectral peak analysis (polyphonic)
    with advanced features for noise rejection and stability.

    Overview:
      Real-time pitch detection and MIDI event generation from audio input.
      Supports both single-note (monophonic) and chord (polyphonic) detection
      with adaptive thresholds, sliding-window analysis, and multi-frame
      persistence filtering to eliminate spurious detections.

    Key Features:

      CORE PITCH DETECTION:
      • Monophonic: YIN/MPM autocorrelation-based fundamental frequency
      • Polyphonic: FFT spectral peak detection with harmonic filtering
      • Configurable frequency range (default: 40-1600 Hz)
      • RMS-based velocity calculation

      SLIDING WINDOW STFT (Integrated):
      • Internal ring buffer with configurable overlap (hop_size < frame_size)
      • Automatic Hann windowing when overlap enabled
      • Streaming API: feed arbitrary chunk sizes, analysis triggered at hop intervals
      • Zero API changes - backward compatible (hop_size = frame_size = legacy mode)
      • Eliminates edge effects and improves onset detection accuracy

      K-OF-M MULTI-FRAME ONSET DETECTION:
      • Require K detections in last M frames before triggering Note On
      • Per-note tracking for polyphonic mode (independent K-of-M per MIDI note)
      • Reduces spurious one-frame events from noise without adding latency
      • Configurable via enable_k_of_m, k_of_m_onset, k_of_m_window parameters
      • Disabled by default for backward compatibility

      PEAK CONTINUITY TRACKING (Polyphonic):
      • Per-note frequency and magnitude history
      • Tracks frames_absent for continuity detection across gaps
      • Foundation for advanced peak matching algorithms
      • Improves stability for sustained notes and dense chords

      AUTO-TUNING ADAPTIVE THRESHOLDS:
      • Noise floor estimation during silence periods
      • Adaptive RMS gate and peak thresholds based on environment
      • Confidence tracking and jitter monitoring
      • Event rate control to prevent detection floods
      • Optional callback for parameter change notifications
      • <1% CPU overhead, <1KB memory footprint

    Usage (Basic Monophonic):
      SoundToMIDI cfg;
      cfg.sample_rate_hz = 16000;
      cfg.frame_size = 512;
      cfg.hop_size = 256;              // 50% overlap
      cfg.confidence_threshold = 0.80f;

      SoundToMIDIMono eng(cfg);
      eng.onNoteOn  = [](uint8_t note, uint8_t vel){
        // Handle note on events
      };
      eng.onNoteOff = [](uint8_t note){
        // Handle note off events
      };

      // Stream audio - automatic buffering and overlap
      eng.processFrame(audioBuffer, bufferSize);

    Usage (Advanced with K-of-M and Auto-Tuning):
      SoundToMIDI cfg;
      cfg.sample_rate_hz = 16000;
      cfg.frame_size = 1024;
      cfg.hop_size = 256;              // 75% overlap
      cfg.enable_k_of_m = true;        // Multi-frame persistence
      cfg.k_of_m_onset = 2;            // Require 2 detections
      cfg.k_of_m_window = 3;           // In last 3 frames
      cfg.auto_tune_enable = true;     // Adaptive thresholds

      SoundToMIDIPoly eng(cfg);
      eng.setAutoTuneCallback([](const char* param, float old_val, float new_val) {
        printf("Auto-tune: %s %.3f → %.3f\n", param, old_val, new_val);
      });
      eng.processFrame(audioBuffer, bufferSize);

    License: MIT (same spirit as FastLED)
*/

#pragma once
#include "fl/stdint.h"
#include "fl/function.h"
#include "fl/vector.h"
#include "fl/map.h"

namespace fl {

// ---------- Enumerations for Polyphonic Spectral Processing ----------
/// @brief Window function types for FFT preprocessing
enum class WindowType : uint8_t {
  None = 0,    ///< No windowing (rectangular)
  Hann,        ///< Hann window (default, good general purpose)
  Hamming,     ///< Hamming window (better sidelobe suppression)
  Blackman     ///< Blackman window (best sidelobe suppression, wider main lobe)
};

/// @brief Smoothing modes for magnitude spectrum
enum class SmoothingMode : uint8_t {
  None = 0,    ///< No smoothing
  Box3,        ///< 3-point box filter
  Tri5,        ///< 5-point triangular filter
  AdjAvg       ///< Adjacent average (2-sided)
};

// ---------- Configuration ----------
/// @brief Configuration parameters for pitch detection and MIDI conversion
struct SoundToMIDI {
  // Audio Parameters
  float sample_rate_hz = 16000.0f;  ///< Input audio sample rate in Hz (typical: 16000-48000)
  int   frame_size     = 512;       ///< Analysis window size in samples (512 for 16kHz, 1024+ for 44.1kHz+)
  int   hop_size       = 512;       ///< Step size between frames (default: frame_size = no overlap, set < frame_size for sliding window)

  // Pitch Detection Range
  float fmin_hz        = 40.0f;     ///< Minimum detectable frequency in Hz (e.g., E1 ≈ 41.2 Hz)
  float fmax_hz        = 1600.0f;   ///< Maximum detectable frequency in Hz (e.g., G6 ≈ 1568 Hz)

  // Detection Thresholds
  float confidence_threshold = 0.80f; ///< Minimum confidence [0-1] to accept pitch (0.80 recommended for music)
  int   note_hold_frames     = 3;     ///< Consecutive frames required before Note On (debounce)
  int   silence_frames_off   = 3;     ///< Consecutive silent frames before Note Off (anti-flutter)
  float rms_gate             = 0.010f;///< RMS amplitude threshold below which signal is silent

  // Velocity Calculation
  float vel_gain             = 5.0f;  ///< Gain multiplier for RMS → velocity conversion
  uint8_t vel_floor          = 10;    ///< Minimum MIDI velocity (1-127, prevents zero velocity)

  // Stability/Anti-Jitter Controls (Monophonic only)
  int   note_change_semitone_threshold = 1;  ///< Semitones required to trigger note change (0=off)
  int   note_change_hold_frames = 3;         ///< Frames new note must persist before switching
  int   median_filter_size = 1;              ///< Median filter window size (1=off for monophonic, 3-5 for noisy input)

  // Multi-frame K-of-M Onset Detection (works with sliding window)
  bool  enable_k_of_m = false;               ///< Enable K-of-M onset filtering (reduces false triggers)
  uint8_t k_of_m_onset = 2;                  ///< Require K detections in last M frames for onset
  uint8_t k_of_m_window = 3;                 ///< Window size M for K-of-M detection

  // ---------- Polyphonic Spectral Processing Parameters ----------
  // FFT & Windowing
  WindowType window_type = WindowType::Hann;  ///< Window function for FFT preprocessing

  // Spectral Conditioning
  float spectral_tilt_db_per_decade = 0.0f;   ///< Spectral tilt in dB/decade (e.g., +3.0 boosts highs)
  SmoothingMode smoothing_mode = SmoothingMode::Box3;  ///< Magnitude spectrum smoothing

  // Peak Detection & Interpolation
  float peak_threshold_db = -40.0f;           ///< Magnitude threshold in dB for peak detection
  bool parabolic_interp = true;               ///< Enable parabolic interpolation for sub-bin accuracy

  // Harmonic Filtering
  bool harmonic_filter_enable = true;         ///< Enable harmonic filtering to suppress overtones
  float harmonic_tolerance_cents = 35.0f;     ///< Cents tolerance for harmonic detection (±35 cents)
  float harmonic_energy_ratio_max = 0.7f;     ///< Max energy ratio for harmonic vs fundamental

  // Octave Masking
  uint8_t octave_mask = 0xFF;                 ///< Bitmask for enabled octaves (bit 0-7 = octave 0-7)

  // PCP (Pitch Class Profile) Stabilizer
  bool pcp_enable = false;                    ///< Enable pitch class profile stabilizer
  uint8_t pcp_history_frames = 12;            ///< Number of frames for PCP history (EMA depth)
  float pcp_bias_weight = 0.1f;               ///< Weight for PCP bias in note acceptance [0-1]

  // Velocity from Peak Magnitude
  bool velocity_from_peak_mag = true;         ///< Use peak magnitude for velocity (else RMS)

  // ---------- Auto-Tuning Configuration ----------
  bool auto_tune_enable = false;              ///< Enable auto-tuning (default: off)

  // Auto-tuning adaptation margins
  float auto_tune_rms_margin = 1.8f;          ///< RMS gate margin multiplier (k_rms, 1.5-2.0)
  float auto_tune_peak_margin_db = 8.0f;      ///< Peak threshold margin above noise floor in dB (6-10)

  // Auto-tuning limits
  float auto_tune_rms_gate_min = 0.005f;      ///< Minimum RMS gate value
  float auto_tune_rms_gate_max = 0.100f;      ///< Maximum RMS gate value
  float auto_tune_confidence_min = 0.60f;     ///< Minimum confidence threshold
  float auto_tune_confidence_max = 0.95f;     ///< Maximum confidence threshold
  float auto_tune_peak_db_min = -60.0f;       ///< Minimum peak threshold in dB
  float auto_tune_peak_db_max = -20.0f;       ///< Maximum peak threshold in dB

  // Auto-tuning event rate targets
  float auto_tune_notes_per_sec_min = 1.0f;   ///< Minimum note events per second (monophonic)
  float auto_tune_notes_per_sec_max = 10.0f;  ///< Maximum note events per second (monophonic)
  float auto_tune_peaks_per_frame_min = 1.0f; ///< Minimum peaks per frame (polyphonic)
  float auto_tune_peaks_per_frame_max = 5.0f; ///< Maximum peaks per frame (polyphonic)

  // Auto-tuning adaptation speeds
  float auto_tune_update_rate_hz = 5.0f;      ///< Update frequency for adaptation (5-10 Hz)
  float auto_tune_param_smoothing = 0.95f;    ///< Smoothing factor for parameter updates (0.9-0.99)
  float auto_tune_threshold_step = 0.02f;     ///< Step size for threshold adjustments

  // Auto-tuning calibration
  float auto_tune_calibration_time_sec = 1.0f; ///< Initial calibration period in seconds
};

// ---------- Pitch Detection Result ----------
/// @brief Result structure from pitch detection algorithm
struct PitchResult {
  float freq_hz;      ///< Detected fundamental frequency in Hz (0 if no pitch detected)
  float confidence;   ///< Detection confidence level [0-1] (higher = more confident)
};

// ---------- Auto-Tuning State ----------
/// @brief Internal state for auto-tuning algorithm
struct AutoTuneState {
  // Noise floor estimation
  float noise_rms_est = 0.0f;           ///< EMA of RMS amplitude during silence
  float noise_mag_db_est = -80.0f;      ///< EMA of spectral median dB during silence

  // Tracking EMAs
  float confidence_ema = 0.0f;          ///< EMA of pitch confidence values
  float pitch_variance_ema = 0.0f;      ///< EMA of pitch variance (jitter)
  float event_rate_ema = 0.0f;          ///< EMA of event rate (notes/sec or peaks/frame)

  // Histogram tracking (simplified as EMAs)
  float note_duration_ema = 0.0f;       ///< EMA of note durations (in frames)
  float note_gap_ema = 0.0f;            ///< EMA of inter-note gaps (in frames)

  // Octave statistics (polyphonic)
  static constexpr int NUM_OCTAVES = 8;
  int octave_detections[NUM_OCTAVES] = {0}; ///< Detection counts per octave
  int octave_spurious[NUM_OCTAVES] = {0};   ///< Spurious detection counts per octave

  // Frame counting
  int frames_processed = 0;             ///< Total frames processed
  int frames_since_update = 0;          ///< Frames since last auto-tune update
  int calibration_frames = 0;           ///< Frames remaining in calibration
  bool in_calibration = true;           ///< Currently in calibration phase

  // Event tracking
  int note_events_count = 0;            ///< Note events in current update window
  int peaks_total = 0;                  ///< Total peaks detected in current window
  int peaks_count = 0;                  ///< Frames counted for peak average

  // Note duration tracking (monophonic)
  int current_note_start_frame = -1;    ///< Frame when current note started
  int last_note_off_frame = -1;         ///< Frame when last note ended

  // Previous pitch tracking (for jitter)
  float prev_pitch_hz = 0.0f;           ///< Previous detected pitch
  bool prev_pitch_valid = false;        ///< Whether prev_pitch_hz is valid
};

// ---------- Auto-Tuning Callback ----------
/// @brief Callback signature for auto-tuning parameter updates
/// @param param_name Name of the parameter that was adjusted
/// @param old_value Previous value
/// @param new_value New value
using AutoTuneCallback = function<void(const char* param_name, float old_value, float new_value)>;

// ---------- Pitch Detector ----------
/// @brief Low-level pitch detector using YIN/MPM-like autocorrelation algorithm
/// @details This class implements a modified YIN (Yet another INversion) algorithm
///          combined with MPM (McLeod Pitch Method) techniques for robust
///          fundamental frequency estimation from audio signals.
class PitchDetector {
public:
  /// @brief Construct a new pitch detector with default parameters
  PitchDetector();

  /// @brief Detect pitch from an audio frame
  /// @param x Pointer to audio samples (float array)
  /// @param N Number of samples in the frame
  /// @param sr Sample rate in Hz
  /// @param fmin Minimum frequency to detect in Hz
  /// @param fmax Maximum frequency to detect in Hz
  /// @return PitchResult containing detected frequency and confidence
  PitchResult detect(const float* x, int N, float sr, float fmin, float fmax);

private:
  static constexpr int MAX_TAU = 600;  ///< Maximum lag for autocorrelation
  float _d[MAX_TAU + 1];               ///< Difference function buffer
  float _cmnd[MAX_TAU + 1];            ///< Cumulative mean normalized difference buffer
};

// ---------- Abstract Base Class ----------
/// @brief Abstract base class for sound-to-MIDI conversion engines
/// @details Defines the common interface for both monophonic and polyphonic implementations
class SoundToMIDIBase {
public:
  virtual ~SoundToMIDIBase() = default;

  /// @brief Callback invoked when a new note starts
  /// @param note MIDI note number (0-127)
  /// @param velocity MIDI velocity (1-127)
  function<void(uint8_t note, uint8_t velocity)> onNoteOn;

  /// @brief Callback invoked when a note ends
  /// @param note MIDI note number (0-127)
  function<void(uint8_t note)> onNoteOff;

  /// @brief Process an audio frame and generate MIDI events
  /// @param frame Pointer to audio samples (normalized float array, typically -1.0 to +1.0)
  /// @param n Number of samples in the frame
  /// @details Call this repeatedly with consecutive audio chunks. The engine
  ///          will invoke onNoteOn/onNoteOff callbacks as needed.
  virtual void processFrame(const float* frame, int n) = 0;

  /// @brief Get current configuration
  /// @return Reference to active configuration
  virtual const SoundToMIDI& config() const = 0;

  /// @brief Update configuration (affects subsequent processFrame calls)
  /// @param c New configuration parameters
  virtual void setConfig(const SoundToMIDI& c) = 0;
};

// ---------- Monophonic MIDI Conversion Engine ----------
/// @brief Monophonic engine that converts audio frames to MIDI Note On/Off events
/// @details This class manages the complete pipeline from audio input to MIDI output:
///          1. Pitch detection via PitchDetector
///          2. Frequency → MIDI note conversion
///          3. Onset/offset detection with hysteresis
///          4. RMS-based velocity calculation
///          5. Anti-jitter filtering (semitone threshold, hold frames, median filter)
///          6. MIDI event callbacks
class SoundToMIDIMono : public SoundToMIDIBase {
public:
  /// @brief Construct engine with specified configuration
  /// @param cfg Configuration parameters (see SoundToMIDI struct)
  explicit SoundToMIDIMono(const SoundToMIDI& cfg);

  /// @brief Process an audio frame and generate MIDI events
  /// @param frame Pointer to audio samples (normalized float array, typically -1.0 to +1.0)
  /// @param n Number of samples in the frame
  void processFrame(const float* frame, int n) override;

  /// @brief Get current configuration
  /// @return Reference to active configuration
  const SoundToMIDI& config() const override;

  /// @brief Update configuration (affects subsequent processFrame calls)
  /// @param c New configuration parameters
  void setConfig(const SoundToMIDI& c) override;

  /// @brief Get auto-tuning state (for monitoring/debugging)
  /// @return Reference to auto-tuning state
  const AutoTuneState& getAutoTuneState() const { return _autoTuneState; }

  /// @brief Set auto-tuning callback for parameter updates
  /// @param cb Callback function
  void setAutoTuneCallback(AutoTuneCallback cb) { _autoTuneCallback = cb; }

private:
  SoundToMIDI _cfg;           ///< Active configuration
  PitchDetector _det;         ///< Pitch detection instance
  int _noteOnFrames;          ///< Counter for note onset hysteresis
  int _silenceFrames;         ///< Counter for silence/offset detection
  int _currentNote;           ///< Currently active MIDI note (or -1)

  // Debounce state for note_change_hold_frames
  int _candidateNote;         ///< Candidate note waiting for confirmation
  int _candidateHoldFrames;   ///< How long candidate has persisted

  // Median filter state for note stability
  static constexpr int MAX_MEDIAN_SIZE = 11;
  int _noteHistory[MAX_MEDIAN_SIZE];  ///< Circular buffer of recent note detections
  int _historyIndex;                  ///< Current write position in history
  int _historyCount;                  ///< Number of valid entries in history

  // Auto-tuning state
  AutoTuneState _autoTuneState;       ///< Auto-tuning state and statistics
  AutoTuneCallback _autoTuneCallback; ///< Optional callback for parameter updates

  // Sliding window internal buffers
  fl::vector<float> _sampleRing;      ///< Ring buffer for sample accumulation
  int _ringWriteIdx;                  ///< Current write position in ring buffer
  int _ringAccumulated;               ///< Samples accumulated since last analysis
  fl::vector<float> _analysisFrame;   ///< Temporary frame buffer for windowing
  fl::vector<float> _windowCoeffs;    ///< Precomputed window coefficients (Hann)
  bool _slidingEnabled;               ///< True if hop_size < frame_size (overlap mode)

  // K-of-M onset detection state
  fl::vector<bool> _onsetHistory;     ///< Circular buffer for onset detections
  int _onsetHistoryIdx;               ///< Current write position in onset history
  bool _lastFrameVoiced;              ///< Previous frame's voiced state for onset detection

  /// @brief Calculate median from note history buffer
  /// @return Median MIDI note number
  int getMedianNote();

  /// @brief Update auto-tuning parameters based on current statistics
  void autoTuneUpdate();

  /// @brief Update noise floor estimation
  /// @param rms Current frame RMS
  /// @param is_silent Whether frame is considered silent
  void updateNoiseFloor(float rms, bool is_silent);

  /// @brief Update confidence tracking
  /// @param confidence Current pitch confidence
  void updateConfidenceTracking(float confidence);

  /// @brief Update jitter tracking
  /// @param freq_hz Current detected pitch
  void updateJitterTracking(float freq_hz);

  /// @brief Update event rate tracking
  /// @param note_on True if a note-on event occurred
  void updateEventRate(bool note_on);

  /// @brief Update note duration tracking
  /// @param note_started True if note just started
  /// @param note_ended True if note just ended
  void updateNoteDuration(bool note_started, bool note_ended);

  /// @brief Notify parameter change via callback
  /// @param name Parameter name
  /// @param old_val Old value
  /// @param new_val New value
  void notifyParamChange(const char* name, float old_val, float new_val);

  /// @brief Extract frame from ring buffer, apply window, and analyze
  void extractAndAnalyzeFrame();

  /// @brief Process a single frame internally (core analysis logic)
  /// @param frame Windowed audio frame
  void processFrameInternal(const float* frame);

  /// @brief Check K-of-M onset condition
  /// @param current_onset True if current frame has onset/voiced
  /// @return True if K-of-M threshold met
  bool checkKofMOnset(bool current_onset);
};

// ---------- Polyphonic MIDI Conversion Engine ----------
/// @brief Polyphonic engine that detects multiple simultaneous notes
/// @details This class uses FFT-based pitch detection to identify multiple
///          fundamental frequencies and generate independent Note On/Off events
///          for each detected pitch. Supports advanced spectral processing features
///          including windowing, spectral tilt, smoothing, parabolic interpolation,
///          harmonic filtering, octave masking, and PCP stabilization.
class SoundToMIDIPoly : public SoundToMIDIBase {
public:
  /// @brief Construct engine with specified configuration
  /// @param cfg Configuration parameters (see SoundToMIDI struct)
  explicit SoundToMIDIPoly(const SoundToMIDI& cfg);

  /// @brief Process an audio frame and generate MIDI events
  /// @param frame Pointer to audio samples (normalized float array, typically -1.0 to +1.0)
  /// @param n Number of samples in the frame
  void processFrame(const float* frame, int n) override;

  /// @brief Get current configuration
  /// @return Reference to active configuration
  const SoundToMIDI& config() const override;

  /// @brief Update configuration (affects subsequent processFrame calls)
  /// @param c New configuration parameters
  void setConfig(const SoundToMIDI& c) override;

  /// @brief Set peak threshold at runtime
  /// @param db Threshold in dB
  void setPeakThresholdDb(float db);

  /// @brief Set octave mask at runtime
  /// @param mask Bitmask for enabled octaves
  void setOctaveMask(uint8_t mask);

  /// @brief Set spectral tilt at runtime
  /// @param db_per_decade Tilt in dB/decade
  void setSpectralTilt(float db_per_decade);

  /// @brief Set smoothing mode at runtime
  /// @param mode Smoothing mode
  void setSmoothingMode(SmoothingMode mode);

  /// @brief Get auto-tuning state (for monitoring/debugging)
  /// @return Reference to auto-tuning state
  const AutoTuneState& getAutoTuneState() const { return _autoTuneState; }

  /// @brief Set auto-tuning callback for parameter updates
  /// @param cb Callback function
  void setAutoTuneCallback(AutoTuneCallback cb) { _autoTuneCallback = cb; }

private:
  SoundToMIDI _cfg;           ///< Active configuration

  // Polyphonic tracking state (128 MIDI notes)
  bool _activeNotes[128];     ///< Which notes are currently on
  int _noteOnCount[128];      ///< Consecutive frames each note has been detected
  int _noteOffCount[128];     ///< Consecutive frames each note has been silent

  // PCP (Pitch Class Profile) state for stabilization
  static constexpr int NUM_PITCH_CLASSES = 12;
  float _pcpHistory[NUM_PITCH_CLASSES];  ///< EMA history for each pitch class (C, C#, D, ...)

  // Auto-tuning state
  AutoTuneState _autoTuneState;       ///< Auto-tuning state and statistics
  AutoTuneCallback _autoTuneCallback; ///< Optional callback for parameter updates

  // Sliding window internal buffers
  fl::vector<float> _sampleRing;      ///< Ring buffer for sample accumulation
  int _ringWriteIdx;                  ///< Current write position in ring buffer
  int _ringAccumulated;               ///< Samples accumulated since last analysis
  fl::vector<float> _analysisFrame;   ///< Temporary frame buffer for windowing
  fl::vector<float> _windowCoeffs;    ///< Precomputed window coefficients (Hann)
  bool _slidingEnabled;               ///< True if hop_size < frame_size (overlap mode)

  // K-of-M per-note tracking for polyphonic mode
  struct NoteKofM {
    int onsetCount = 0;   ///< Frames this note has been detected in last M frames
    int offsetCount = 0;  ///< Frames this note has been absent in last M frames
  };
  NoteKofM _noteKofM[128];  ///< K-of-M state for each MIDI note

  // Peak continuity tracking (polyphonic)
  struct PeakMemory {
    float freq_hz = 0.0f;     ///< Last detected frequency for this note
    float magnitude = 0.0f;   ///< Last detected magnitude
    int frames_absent = 0;    ///< Frames since last detection
  };
  PeakMemory _peakMemory[128];  ///< Peak memory for each MIDI note

  // Initialize internal state
  void initializeState();

  // Precompute window coefficients for current FFT size
  void precomputeWindow();

  // Check if a note passes the octave mask
  bool passesOctaveMask(int midiNote) const;

  // Update PCP history with detected notes
  void updatePCP(const fl::vector<int>& notes);

  // Get PCP bias for a given MIDI note
  float getPCPBias(int midiNote) const;

  // Auto-tuning methods
  void autoTuneUpdate();
  void updateNoiseFloor(float rms, int num_peaks, const fl::vector<float>& spectrum);
  void updatePeakTracking(int num_peaks);
  void updateOctaveStatistics(const fl::vector<int>& notes);
  void notifyParamChange(const char* name, float old_val, float new_val);

  // Sliding window methods
  void extractAndAnalyzeFrame();
  void processFrameInternal(const float* frame);
};

// ---------- Sliding Window STFT Configuration ----------
/// @brief Configuration for sliding-window (overlapped) analysis
struct SlidingCfg {
  // Streaming parameters
  uint16_t frame_size = 1024;           ///< Analysis window size in samples
  uint16_t hop_size = 512;              ///< Step size between frames (overlap = frame_size - hop_size)

  // Window function
  enum class Window : uint8_t {
    Hann = 0,
    Hamming,
    Blackman
  };
  Window window = Window::Hann;         ///< Window function type

  // Multi-frame logic for onset detection
  bool enable_k_of_m = false;           ///< Enable K-of-M onset filtering (disabled by default for compatibility)
  uint8_t k_of_m_onset = 2;             ///< Require ≥k detections in last m frames for onset
  uint8_t k_of_m_window = 3;            ///< Window size (m) for K-of-M onset detection

  // Peak continuity (polyphonic)
  bool enable_peak_continuity = true;   ///< Match peaks across frames for stability
  float continuity_cents = 35.0f;       ///< Max drift in cents to match a peak across frames

  // Adaptive thresholds
  bool adaptive_rms_gate = true;        ///< Enable adaptive RMS gate based on noise floor
  float rms_margin_db = 3.0f;           ///< Margin above noise floor in dB
  bool adaptive_peak_thresh = true;     ///< Enable adaptive peak threshold (polyphonic)
  float peak_margin_db = 6.0f;          ///< Margin above spectral median in dB

  // History buffers (bounded for MCU)
  uint8_t spectra_history = 6;          ///< Number of magnitude spectra to store (polyphonic)
  uint8_t pcp_history = 12;             ///< PCP history depth

  // CPU optimization
  bool magnitude_abs1 = true;           ///< Use |Re|+|Im| instead of sqrt(Re²+Im²)
};

// ---------- Sliding Window STFT Wrapper ----------
/// @brief Wrapper class providing sliding-window analysis for both mono and poly engines
/// @details This class maintains a ring buffer and schedules analysis at regular hop intervals,
///          enabling overlapped analysis for improved stability and onset recall
class SoundToMIDISliding {
public:
  /// @brief Construct with base configuration and sliding window parameters
  /// @param baseCfg Base SoundToMIDI configuration
  /// @param slideCfg Sliding window configuration
  /// @param use_poly Use polyphonic engine (default: false = monophonic)
  SoundToMIDISliding(const SoundToMIDI& baseCfg, const SlidingCfg& slideCfg, bool use_poly = false);

  /// @brief Stream audio samples - analyzer will trigger events when ready
  /// @param samples Pointer to audio samples (normalized float array)
  /// @param n Number of samples
  void processSamples(const float* samples, int n);

  /// @brief Update sliding window configuration at runtime
  /// @param cfg New sliding configuration
  void setSlidingCfg(const SlidingCfg& cfg);

  /// @brief Get current sliding configuration
  const SlidingCfg& slidingConfig() const { return mSlideCfg; }

  /// @brief Access underlying mono engine (only valid if constructed with use_poly=false)
  /// @return Reference to mono engine
  SoundToMIDIMono& mono();

  /// @brief Access underlying poly engine (only valid if constructed with use_poly=true)
  /// @return Reference to poly engine
  SoundToMIDIPoly& poly();

  /// @brief Check if using polyphonic mode
  bool isPolyphonic() const { return mUsePoly; }

  /// @brief Set NoteOn callback (replaces direct engine callback for K-of-M filtering)
  /// @param callback Function to call when a note turns on (after K-of-M filtering)
  void setNoteOnCallback(fl::function<void(uint8_t, uint8_t)> callback) {
    mUserNoteOn = callback;
  }

  /// @brief Set NoteOff callback (replaces direct engine callback for K-of-M filtering)
  /// @param callback Function to call when a note turns off (after K-of-M filtering)
  void setNoteOffCallback(fl::function<void(uint8_t)> callback) {
    mUserNoteOff = callback;
  }

private:
  SlidingCfg mSlideCfg;
  bool mUsePoly;

  // Ring buffer for PCM accumulation
  fl::vector<float> mPcmRing;
  int mWriteIdx = 0;
  int mAccumulated = 0;

  // Windowing coefficients
  fl::vector<float> mWindow;

  // Frame buffer
  fl::vector<float> mFrameBuffer;

  // K-of-M onset tracking for mono
  fl::vector<bool> mOnsetHistory;    // Circular buffer for onset detections
  int mOnsetHistoryIdx = 0;
  bool mLastVoiced = false;           // Track voiced state for onset detection

  // K-of-M note tracking for poly (per-MIDI-note persistence)
  struct NoteState {
    int onsetCount = 0;   // How many of last M frames detected this note
    int offCount = 0;     // How many of last M frames missed this note
    bool active = false;  // Currently emitting this note
    float lastFreqHz = 0.0f;  // Last detected frequency for continuity tracking
    uint8_t velocity = 64;    // Velocity for this note
  };
  fl::SortedHeapMap<uint8_t, NoteState> mNoteStates;  // Keyed by MIDI note number

  // Peak continuity tracking (polyphonic)
  struct PeakInfo {
    uint8_t note;
    float freq_hz;
    float magnitude;
  };
  fl::vector<PeakInfo> mLastPeaks;  // Peaks from previous frame for continuity matching

  // User callbacks (will forward filtered events here)
  fl::function<void(uint8_t, uint8_t)> mUserNoteOn;
  fl::function<void(uint8_t)> mUserNoteOff;

  // Underlying engines (only one will be used)
  SoundToMIDIMono* mMonoEngine = nullptr;
  SoundToMIDIPoly* mPolyEngine = nullptr;

  // Helper methods
  void initWindow();
  void makeFrame(float* outFrame);
  void applyWindow(float* frame);
  void runAnalysis(const float* frame);
  bool checkKofMOnset(bool current_onset);

  // K-of-M event filtering
  void handleMonoNoteOn(uint8_t note, uint8_t velocity);
  void handleMonoNoteOff(uint8_t note);
  void handlePolyNoteOn(uint8_t note, uint8_t velocity);
  void handlePolyNoteOff(uint8_t note);
};

// ---------- Legacy Alias (Deprecated) ----------
/// @brief Legacy alias for backward compatibility - use SoundToMIDIMono instead
/// @deprecated Use SoundToMIDIMono for monophonic or SoundToMIDIPoly for polyphonic
using SoundToMIDIEngine = SoundToMIDIMono;

} // namespace fl
