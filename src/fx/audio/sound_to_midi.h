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

namespace fl {

// ---------- Configuration ----------
/// @brief Configuration parameters for pitch detection and MIDI conversion
struct SoundToMIDI {
  // Audio Parameters
  float sample_rate_hz = 16000.0f;  ///< Input audio sample rate in Hz (typical: 16000-48000)
  int   frame_size     = 512;       ///< Analysis window size in samples (power of 2 recommended)
  int   hop_size       = 256;       ///< Step size between frames in samples (typically frame_size/2)

  // Pitch Detection Range
  float fmin_hz        = 40.0f;     ///< Minimum detectable frequency in Hz (e.g., E1 ≈ 41.2 Hz)
  float fmax_hz        = 1600.0f;   ///< Maximum detectable frequency in Hz (e.g., G6 ≈ 1568 Hz)

  // Detection Thresholds
  float confidence_threshold = 0.82f; ///< Minimum confidence [0-1] to accept pitch (higher = stricter)
  int   note_hold_frames     = 3;     ///< Consecutive frames required before Note On (debounce)
  int   silence_frames_off   = 3;     ///< Consecutive silent frames before Note Off (anti-flutter)
  float rms_gate             = 0.010f;///< RMS amplitude threshold below which signal is silent

  // Velocity Calculation
  float vel_gain             = 5.0f;  ///< Gain multiplier for RMS → velocity conversion
  uint8_t vel_floor          = 10;    ///< Minimum MIDI velocity (1-127, prevents zero velocity)

  // Stability/Anti-Jitter Controls
  int   note_change_semitone_threshold = 1;  ///< Semitones required to trigger note change (0=off)
  int   note_change_hold_frames = 5;         ///< Frames new note must persist before switching
  int   median_filter_size = 5;              ///< Median filter window size (1=off, max=11, odd recommended)

  // Polyphonic Mode
  bool  polyphonic = false;                  ///< Enable polyphonic detection (multiple simultaneous notes)
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

// ---------- MIDI Conversion Engine ----------
/// @brief Main engine that converts audio frames to MIDI Note On/Off events
/// @details This class manages the complete pipeline from audio input to MIDI output:
///          1. Pitch detection via PitchDetector
///          2. Frequency → MIDI note conversion
///          3. Onset/offset detection with hysteresis
///          4. RMS-based velocity calculation
///          5. Anti-jitter filtering (semitone threshold, hold frames, median filter)
///          6. MIDI event callbacks
class SoundToMIDIEngine {
public:
  /// @brief Construct engine with specified configuration
  /// @param cfg Configuration parameters (see SoundToMIDI struct)
  explicit SoundToMIDIEngine(const SoundToMIDI& cfg);

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
  void processFrame(const float* frame, int n);

  /// @brief Get current configuration
  /// @return Reference to active configuration
  const SoundToMIDI& config() const;

  /// @brief Update configuration (affects subsequent processFrame calls)
  /// @param c New configuration parameters
  void setConfig(const SoundToMIDI& c);

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

  // Polyphonic tracking state (128 MIDI notes)
  bool _activeNotes[128];             ///< Which notes are currently on
  int _noteOnCount[128];              ///< Consecutive frames each note has been detected
  int _noteOffCount[128];             ///< Consecutive frames each note has been silent

  /// @brief Calculate median from note history buffer
  /// @return Median MIDI note number
  int getMedianNote();
};

} // namespace fl
