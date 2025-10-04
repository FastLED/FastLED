#include "pitch_to_midi.h"
#include "fl/math.h"

namespace fl {

// ---------- Helper functions ----------
namespace {

inline int hz_to_midi(float f) {
  // log2(x) = log(x) / log(2)
  return (int)lroundf(69.0f + 12.0f * (logf(f / 440.0f) / logf(2.0f)));
}

inline float clamp01(float v) {
  return v < 0 ? 0 : (v > 1 ? 1 : v);
}

inline float compute_rms(const float* x, int n) {
  double acc = 0.0;
  for (int i = 0; i < n; ++i) {
    acc += (double)x[i] * (double)x[i];
  }
  return sqrtf((float)(acc / (double)n));
}

inline uint8_t amp_to_velocity(float rms, float gain, uint8_t floorV) {
  float v = clamp01(rms * gain);
  int vel = (int)lroundf(floorV + v * (127 - floorV));
  if (vel < 1) vel = 1;
  if (vel > 127) vel = 127;
  return (uint8_t)vel;
}

inline int clampMidi(int n) {
  if (n < 0) return 0;
  if (n > 127) return 127;
  return n;
}

inline int noteDelta(int a, int b) {
  int d = a - b;
  return d >= 0 ? d : -d;
}

} // namespace

// ---------- PitchDetector ----------
PitchDetector::PitchDetector() {
  for (int i = 0; i <= MAX_TAU; ++i) {
    _d[i] = 0.0f;
    _cmnd[i] = 0.0f;
  }
}

PitchResult PitchDetector::detect(const float* x, int N, float sr, float fmin, float fmax) {
  int tauMin = (int)floorf(sr / fmax);
  int tauMax = (int)ceilf(sr / fmin);
  if (tauMin < 2) tauMin = 2;
  if (tauMax >= N - 1) tauMax = N - 2;
  if (tauMax > MAX_TAU) tauMax = MAX_TAU;

  const float eps = 1e-12f;

  // Difference function d(tau)
  for (int tau = 0; tau <= tauMax; ++tau) {
    _d[tau] = 0.0f;
  }
  for (int tau = 1; tau <= tauMax; ++tau) {
    double sum = 0.0;
    const int limit = N - tau;
    for (int i = 0; i < limit; ++i) {
      float diff = x[i] - x[i + tau];
      sum += (double)diff * (double)diff;
    }
    _d[tau] = (float)sum;
  }

  // CMND
  _cmnd[0] = 1.0f;
  double cum = 0.0;
  for (int tau = 1; tau <= tauMax; ++tau) {
    cum += _d[tau];
    _cmnd[tau] = (float)(_d[tau] * tau / (cum + eps));
  }

  // First crossing under classic threshold; else global min
  const float thresh = 0.12f;
  int tauEst = -1;
  for (int tau = tauMin; tau <= tauMax; ++tau) {
    if (_cmnd[tau] < thresh) {
      tauEst = tau;
      break;
    }
  }
  if (tauEst < 0) {
    float minv = 1e9f;
    int idx = -1;
    for (int tau = tauMin; tau <= tauMax; ++tau) {
      if (_cmnd[tau] < minv) {
        minv = _cmnd[tau];
        idx = tau;
      }
    }
    tauEst = idx;
    if (tauEst < 0) return {0.0f, 0.0f};
  }

  // Parabolic interpolation (sub-sample)
  int t0 = tauEst;
  if (t0 > 1 && t0 < tauMax) {
    float a = _cmnd[t0 - 1], b = _cmnd[t0], c = _cmnd[t0 + 1];
    float denom = (a - 2 * b + c);
    if (fabsf(denom) > 1e-12f) {
      float delta = 0.5f * (a - c) / denom;
      float t = (float)t0 + delta;
      if (t >= 2 && t <= tauMax - 1) {
        tauEst = (int)lroundf(t);
      }
    }
  }

  float freq = sr / (float)tauEst;
  float conf = 1.0f - ((_cmnd[tauEst] < 1.0f) ? _cmnd[tauEst] : 1.0f);
  if (freq < fmin || freq > fmax) return {0.0f, 0.0f};
  return {freq, conf};
}

// ---------- PitchToMIDIEngine ----------
PitchToMIDIEngine::PitchToMIDIEngine(const PitchToMIDI& cfg)
    : _cfg(cfg), _noteOnFrames(0), _silenceFrames(0), _currentNote(-1),
      _candidateNote(-1), _candidateHoldFrames(0),
      _historyIndex(0), _historyCount(0) {
  for (int i = 0; i < MAX_MEDIAN_SIZE; ++i) {
    _noteHistory[i] = -1;
  }
}

int PitchToMIDIEngine::getMedianNote() {
  if (_historyCount == 0) return -1;

  // Clamp filter size to valid range
  int filterSize = _cfg.median_filter_size;
  if (filterSize < 1) filterSize = 1;
  if (filterSize > MAX_MEDIAN_SIZE) filterSize = MAX_MEDIAN_SIZE;
  if (filterSize > _historyCount) filterSize = _historyCount;

  // If filter size is 1, just return the most recent note
  if (filterSize == 1) {
    int idx = (_historyIndex - 1 + MAX_MEDIAN_SIZE) % MAX_MEDIAN_SIZE;
    return _noteHistory[idx];
  }

  // Copy last N notes for sorting
  int temp[MAX_MEDIAN_SIZE];
  for (int i = 0; i < filterSize; ++i) {
    int idx = (_historyIndex - filterSize + i + MAX_MEDIAN_SIZE) % MAX_MEDIAN_SIZE;
    temp[i] = _noteHistory[idx];
  }

  // Simple bubble sort (small array, fine for embedded)
  for (int i = 0; i < filterSize - 1; ++i) {
    for (int j = 0; j < filterSize - i - 1; ++j) {
      if (temp[j] > temp[j + 1]) {
        int swap = temp[j];
        temp[j] = temp[j + 1];
        temp[j + 1] = swap;
      }
    }
  }

  // Return median
  return temp[filterSize / 2];
}

void PitchToMIDIEngine::processFrame(const float* frame, int n) {
  if (!frame || n != _cfg.frame_size) return;

  // Pitch & loudness
  const PitchResult pr = _det.detect(frame, n, _cfg.sample_rate_hz, _cfg.fmin_hz, _cfg.fmax_hz);
  const float rms = compute_rms(frame, n);
  const bool voiced = (rms > _cfg.rms_gate) &&
                      (pr.confidence > _cfg.confidence_threshold) &&
                      (pr.freq_hz > 0.0f);

  if (voiced) {
    int rawNote = clampMidi(hz_to_midi(pr.freq_hz));

    // Option 3: Add to median filter history
    _noteHistory[_historyIndex] = rawNote;
    _historyIndex = (_historyIndex + 1) % MAX_MEDIAN_SIZE;
    if (_historyCount < MAX_MEDIAN_SIZE) _historyCount++;

    // Get filtered note
    const int note = getMedianNote();

    if (_currentNote < 0) {
      // confirm new note-on
      if (++_noteOnFrames >= _cfg.note_hold_frames) {
        const uint8_t vel = amp_to_velocity(rms, _cfg.vel_gain, _cfg.vel_floor);
        if (onNoteOn) onNoteOn((uint8_t)note, vel);
        _currentNote = note;
        _noteOnFrames = 0;
        _silenceFrames = 0;
        _candidateNote = -1;
        _candidateHoldFrames = 0;
      }
    } else {
      // Option 1: Check semitone threshold
      const int dn = noteDelta(note, _currentNote);
      if (dn >= _cfg.note_change_semitone_threshold) {
        // Option 2: Check if candidate note persists
        if (note == _candidateNote) {
          _candidateHoldFrames++;
          if (_candidateHoldFrames >= _cfg.note_change_hold_frames) {
            // Retrigger confirmed
            if (onNoteOff) onNoteOff((uint8_t)_currentNote);
            const uint8_t vel = amp_to_velocity(rms, _cfg.vel_gain, _cfg.vel_floor);
            if (onNoteOn) onNoteOn((uint8_t)note, vel);
            _currentNote = note;
            _candidateNote = -1;
            _candidateHoldFrames = 0;
          }
        } else {
          // New candidate note
          _candidateNote = note;
          _candidateHoldFrames = 1;
        }
      } else {
        // Note within threshold, reset candidate
        _candidateNote = -1;
        _candidateHoldFrames = 0;
      }
      _noteOnFrames = 0;
      _silenceFrames = 0;
    }
  } else {
    _noteOnFrames = 0;
    _candidateNote = -1;
    _candidateHoldFrames = 0;
    if (_currentNote >= 0) {
      if (++_silenceFrames >= _cfg.silence_frames_off) {
        if (onNoteOff) onNoteOff((uint8_t)_currentNote);
        _currentNote = -1;
        _silenceFrames = 0;
        // Clear median filter history on silence
        _historyCount = 0;
        _historyIndex = 0;
      }
    }
  }
}

const PitchToMIDI& PitchToMIDIEngine::config() const {
  return _cfg;
}

void PitchToMIDIEngine::setConfig(const PitchToMIDI& c) {
  _cfg = c;
}

} // namespace fl
