/*  FastLED - Audio Pitch → MIDI (Monophonic)
    ---------------------------------------------------------
    Converts short audio frames to MIDI Note On/Off events using a
    YIN/MPM-like pitch detector plus simple onset/offset hysteresis
    and RMS-based velocity.

    Usage:
      PitchToMIDI cfg;
      cfg.sample_rate_hz = 16000;
      PitchToMIDIEngine eng(cfg);
      eng.onNoteOn  = [](uint8_t note, uint8_t vel){ ... };
      eng.onNoteOff = [](uint8_t note){ ... };
      eng.processFrame(frame, frameLen);

    License: MIT (same spirit as FastLED)
*/

#pragma once
#include "fl/stdint.h"
#include "fl/function.h"

namespace fl {

// ---------- Config ----------
struct PitchToMIDI {
  float sample_rate_hz = 16000.0f;
  int   frame_size     = 512;
  int   hop_size       = 256;
  float fmin_hz        = 40.0f;
  float fmax_hz        = 1600.0f;
  float confidence_threshold = 0.82f;
  int   note_hold_frames     = 3;
  int   silence_frames_off   = 3;
  float rms_gate             = 0.010f;
  float vel_gain             = 5.0f;
  uint8_t vel_floor          = 10;
};

// ---------- Pitch Result ----------
struct PitchResult {
  float freq_hz;
  float confidence;
};

// ---------- Pitch Detector ----------
class PitchDetector {
public:
  PitchDetector();
  PitchResult detect(const float* x, int N, float sr, float fmin, float fmax);

private:
  static constexpr int MAX_TAU = 600;
  float _d[MAX_TAU + 1];
  float _cmnd[MAX_TAU + 1];
};

// ---------- Engine ----------
class PitchToMIDIEngine {
public:
  explicit PitchToMIDIEngine(const PitchToMIDI& cfg);

  function<void(uint8_t note, uint8_t velocity)> onNoteOn;
  function<void(uint8_t note)> onNoteOff;

  void processFrame(const float* frame, int n);

  const PitchToMIDI& config() const;
  void setConfig(const PitchToMIDI& c);

private:
  PitchToMIDI _cfg;
  PitchDetector _det;
  int _noteOnFrames;
  int _silenceFrames;
  int _currentNote;
};

} // namespace fl
