/*  FastLED - Audio Sound → MIDI (Monophonic)
    ---------------------------------------------------------
    Converts short audio frames to MIDI Note On/Off events using a
    YIN/MPM-like pitch detector plus simple onset/offset hysteresis
    and RMS-based velocity.

    Overview:
      This module provides real-time pitch detection and MIDI event
      generation from audio input. It uses a YIN/MPM-based algorithm
      to detect fundamental frequency, then converts that to MIDI
      note numbers with onset/offset detection and velocity estimation.

    Key Features:
      • Monophonic pitch detection (single note at a time)
      • Configurable frequency range (default: 40-1600 Hz)
      • RMS-based velocity calculation
      • Anti-jitter filtering (semitone threshold, hold frames, median filter)
      • Onset/offset hysteresis to prevent rapid note toggling
      • Callback-based event system

    Usage:
      SoundToMIDI cfg;
      cfg.sample_rate_hz = 16000;
      cfg.frame_size = 512;
      cfg.confidence_threshold = 0.82f;

      SoundToMIDIEngine eng(cfg);
      eng.onNoteOn  = [](uint8_t note, uint8_t vel){
        // Handle note on events
      };
      eng.onNoteOff = [](uint8_t note){
        // Handle note off events
      };

      // Process audio in chunks
      eng.processFrame(audioBuffer, bufferSize);

    License: MIT (same spirit as FastLED)
*/

#pragma once
#include "fl/stdint.h"
#include "fl/function.h"
#include "fl/vector.h"

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
  int   hop_size       = 256;       ///< Step size between frames in samples (typically frame_size/2)

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
};

// ---------- Pitch Detection Result ----------
/// @brief Result structure from pitch detection algorithm
struct PitchResult {
  float freq_hz;      ///< Detected fundamental frequency in Hz (0 if no pitch detected)
  float confidence;   ///< Detection confidence level [0-1] (higher = more confident)
};

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

  /// @brief Calculate median from note history buffer
  /// @return Median MIDI note number
  int getMedianNote();
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

private:
  SoundToMIDI _cfg;           ///< Active configuration

  // Polyphonic tracking state (128 MIDI notes)
  bool _activeNotes[128];     ///< Which notes are currently on
  int _noteOnCount[128];      ///< Consecutive frames each note has been detected
  int _noteOffCount[128];     ///< Consecutive frames each note has been silent

  // PCP (Pitch Class Profile) state for stabilization
  static constexpr int NUM_PITCH_CLASSES = 12;
  float _pcpHistory[NUM_PITCH_CLASSES];  ///< EMA history for each pitch class (C, C#, D, ...)

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
};

// ---------- Legacy Alias (Deprecated) ----------
/// @brief Legacy alias for backward compatibility - use SoundToMIDIMono instead
/// @deprecated Use SoundToMIDIMono for monophonic or SoundToMIDIPoly for polyphonic
using SoundToMIDIEngine = SoundToMIDIMono;

} // namespace fl
