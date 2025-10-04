#include "pitch_to_midi.h"
#include "fl/math.h"
#include "fl/vector.h"
#include "fl/pair.h"

namespace fl {

// ---------- Helper functions ----------
namespace {

// Structure to hold a detected note and its peak magnitude
struct NotePeak {
  int midi;
  float magnitude;
};

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

// Cooley-Tukey FFT (in-place, radix-2). Requires N to be a power of 2.
inline void fft(const float* input, int N, fl::vector<fl::pair<float, float>>& out) {
  out.resize(N);
  // Copy input into output array (real, imag)
  for (int i = 0; i < N; ++i) {
    out[i].first = input[i];
    out[i].second = 0.0f;
  }

  // Bit-reversal permutation
  int log2N = 0;
  while ((1 << log2N) < N) ++log2N;
  for (int i = 1, j = 0; i < N; ++i) {
    int bit = N >> 1;
    for (; j & bit; bit >>= 1) {
      j ^= bit;
    }
    j ^= bit;
    if (i < j) {
      fl::pair<float, float> tmp = out[i];
      out[i] = out[j];
      out[j] = tmp;
    }
  }

  // FFT butterfly stages
  for (int len = 2; len <= N; len <<= 1) {
    float theta = -2.0f * M_PI / (float)len;
    float wlen_re = cosf(theta);
    float wlen_im = sinf(theta);
    for (int i = 0; i < N; i += len) {
      float w_re = 1.0f;
      float w_im = 0.0f;
      for (int j = 0; j < len/2; ++j) {
        float u_re = out[i+j].first;
        float u_im = out[i+j].second;
        float v_re = out[i+j+len/2].first * w_re - out[i+j+len/2].second * w_im;
        float v_im = out[i+j+len/2].first * w_im + out[i+j+len/2].second * w_re;

        out[i+j].first = u_re + v_re;
        out[i+j].second = u_im + v_im;
        out[i+j+len/2].first = u_re - v_re;
        out[i+j+len/2].second = u_im - v_im;

        float w_tmp = w_re * wlen_re - w_im * wlen_im;
        w_im = w_re * wlen_im + w_im * wlen_re;
        w_re = w_tmp;
      }
    }
  }
}

// Detect multiple fundamental frequencies in the frame using FFT.
// Returns a list of NotePeak (MIDI note and its fundamental magnitude).
inline fl::vector<NotePeak> detectPolyphonicNotes(const float* x, int N, float sr, float fmin, float fmax) {
  // Compute FFT of input frame (assume N is power of 2 for simplicity)
  fl::vector<fl::pair<float, float>> spectrum;
  fft(x, N, spectrum);
  int halfN = N / 2;

  // Magnitude spectrum (only need [0, N/2] for real input)
  fl::vector<float> mag(halfN);
  for (int i = 0; i < halfN; ++i) {
    float re = spectrum[i].first;
    float im = spectrum[i].second;
    mag[i] = sqrtf(re*re + im*im);
  }

  // Determine frequency bin range for fmin to fmax
  int binMin = (int)floorf(fmin * N / sr);
  int binMax = (int)ceilf(fmax * N / sr);
  if (binMin < 1) binMin = 1;               // start from 1 to skip DC
  if (binMax > halfN - 1) binMax = halfN - 1;

  // Calculate median magnitude in [binMin, binMax] to use as noise threshold
  fl::vector<float> magslice;
  magslice.reserve(binMax - binMin + 1);
  for (int i = binMin; i <= binMax; ++i) {
    magslice.push_back(mag[i]);
  }

  // Simple median calculation (sort and take middle)
  for (size_t i = 0; i < magslice.size() - 1; ++i) {
    for (size_t j = 0; j < magslice.size() - i - 1; ++j) {
      if (magslice[j] > magslice[j + 1]) {
        float tmp = magslice[j];
        magslice[j] = magslice[j + 1];
        magslice[j + 1] = tmp;
      }
    }
  }
  float medianMag = magslice[magslice.size() / 2];
  float threshold = medianMag * 1.5f;  // threshold factor (1.5x median, adjust as needed)

  // Find local peaks above threshold in the specified range
  fl::vector<int> peakIndices;
  peakIndices.reserve(16);
  for (int i = binMin + 1; i < binMax; ++i) {
    if (mag[i] > threshold && mag[i] >= mag[i-1] && mag[i] >= mag[i+1]) {
      peakIndices.push_back(i);
    }
  }
  if (peakIndices.empty()) {
    return fl::vector<NotePeak>();  // no significant peaks found
  }

  // Sort detected peaks by frequency (ascending) - already in order from loop above

  // Group peaks by fundamental-harmonic relationship
  fl::vector<NotePeak> result;
  fl::vector<bool> used(peakIndices.size(), false);
  for (size_t p = 0; p < peakIndices.size(); ++p) {
    if (used[p]) continue;
    int idx = peakIndices[p];
    // Consider this peak a fundamental frequency
    float freq = sr * (float)idx / (float)N;
    int midiNote = clampMidi(hz_to_midi(freq));
    float peakMag = mag[idx];

    // Mark harmonics of this fundamental as used
    for (size_t q = p + 1; q < peakIndices.size(); ++q) {
      int idx2 = peakIndices[q];
      if (used[q]) continue;
      // Check if idx2 is ~ an integer multiple of idx
      float ratio = (float)idx2 / idx;
      int roundMult = (int)floorf(ratio + 0.5f);
      if (roundMult >= 2 && fabsf(ratio - roundMult) < 0.1f) {
        used[q] = true;
      }
    }
    // Store this fundamental note and its magnitude
    result.push_back({midiNote, peakMag});
  }
  return result;
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
  // Initialize polyphonic tracking arrays
  for (int note = 0; note < 128; ++note) {
    _activeNotes[note] = false;
    _noteOnCount[note] = 0;
    _noteOffCount[note] = 0;
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

  // Compute loudness (RMS) of the frame
  const float rms = compute_rms(frame, n);

  if (_cfg.polyphonic) {
    // --- Polyphonic pitch detection branch ---
    bool voiced = (rms > _cfg.rms_gate);
    if (voiced) {
      // Get all detected note peaks in this frame
      fl::vector<NotePeak> notes = detectPolyphonicNotes(frame, n, _cfg.sample_rate_hz,
                                                         _cfg.fmin_hz, _cfg.fmax_hz);
      // Prepare a lookup for which MIDI notes are present in this frame
      bool present[128] = {false};
      for (const NotePeak& np : notes) {
        if (np.midi >= 0 && np.midi < 128) {
          present[np.midi] = true;
        }
      }
      // Compute global amplitude normalization for velocity
      float globalNorm = clamp01(rms * _cfg.vel_gain);
      // Determine the maximum peak magnitude among detected notes (for relative loudness)
      float maxMag = 0.0f;
      for (const NotePeak& np : notes) {
        if (np.magnitude > maxMag) maxMag = np.magnitude;
      }
      if (maxMag < 1e-6f) maxMag = 1e-6f;  // avoid division by zero

      // Update note states
      for (int note = 0; note < 128; ++note) {
        if (present[note]) {
          // Note is detected in current frame
          if (!_activeNotes[note]) {
            // Note currently off -> potential note-on
            _noteOnCount[note] += 1;
            _noteOffCount[note] = 0;
            if (_noteOnCount[note] >= _cfg.note_hold_frames) {
              // Confirmed new Note On
              // Calculate velocity based on note's relative magnitude
              float relAmp = 0.0f;
              // Find this note's magnitude in the notes vector
              for (const NotePeak& np : notes) {
                if (np.midi == note) {
                  relAmp = np.magnitude / maxMag;
                  break;
                }
              }
              float velNorm = clamp01(globalNorm * relAmp);
              int vel = (int)lroundf(_cfg.vel_floor + velNorm * (127 - _cfg.vel_floor));
              if (vel < 1) vel = 1;
              if (vel > 127) vel = 127;
              if (onNoteOn) onNoteOn((uint8_t)note, (uint8_t)vel);
              _activeNotes[note] = true;
              _noteOnCount[note] = 0;
            }
          } else {
            // Note is already active and continues
            _noteOnCount[note] = 0;  // reset any pending on counter (already on)
            _noteOffCount[note] = 0; // reset off counter since still sounding
          }
        } else {
          // Note is NOT present in current frame
          _noteOnCount[note] = 0;
          if (_activeNotes[note]) {
            // An active note might be turning off
            _noteOffCount[note] += 1;
            if (_noteOffCount[note] >= _cfg.silence_frames_off) {
              // Confirmed note-off after sustained silence
              if (onNoteOff) onNoteOff((uint8_t)note);
              _activeNotes[note] = false;
              _noteOffCount[note] = 0;
            }
          } else {
            _noteOffCount[note] = 0;
          }
        }
      }
    } else {
      // Not voiced (below RMS gate): treat as silence, increment off counters for all active notes
      for (int note = 0; note < 128; ++note) {
        _noteOnCount[note] = 0;
        if (_activeNotes[note]) {
          _noteOffCount[note] += 1;
          if (_noteOffCount[note] >= _cfg.silence_frames_off) {
            if (onNoteOff) onNoteOff((uint8_t)note);
            _activeNotes[note] = false;
            _noteOffCount[note] = 0;
          }
        }
      }
    }
  } else {
    // --- Monophonic pitch detection branch (original logic) ---
    const PitchResult pr = _det.detect(frame, n, _cfg.sample_rate_hz, _cfg.fmin_hz, _cfg.fmax_hz);
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
}

const PitchToMIDI& PitchToMIDIEngine::config() const {
  return _cfg;
}

void PitchToMIDIEngine::setConfig(const PitchToMIDI& c) {
  _cfg = c;
}

} // namespace fl
