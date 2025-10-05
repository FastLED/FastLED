#include "sound_to_midi.h"
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

// Apply window function to signal
inline void applyWindow(float* signal, int N, WindowType windowType) {
  if (windowType == WindowType::None) return;

  for (int i = 0; i < N; ++i) {
    float w = 1.0f;
    float t = (float)i / (float)(N - 1);

    switch (windowType) {
      case WindowType::None:
        break;
      case WindowType::Hann:
        w = 0.5f * (1.0f - cosf(2.0f * M_PI * t));
        break;
      case WindowType::Hamming:
        w = 0.54f - 0.46f * cosf(2.0f * M_PI * t);
        break;
      case WindowType::Blackman:
        w = 0.42f - 0.5f * cosf(2.0f * M_PI * t) + 0.08f * cosf(4.0f * M_PI * t);
        break;
    }
    signal[i] *= w;
  }
}

// Apply spectral tilt (linear EQ) to magnitude spectrum
inline void applySpectralTilt(fl::vector<float>& mag, float db_per_decade, float sr, int N) {
  if (fabsf(db_per_decade) < 1e-6f) return;

  // Convert dB/decade to linear slope per bin
  // f_nyquist = sr/2, number of decades from DC to Nyquist
  float f_nyquist = sr / 2.0f;
  float decades = log10f(f_nyquist / 1.0f); // Decades from 1 Hz to Nyquist

  for (size_t i = 1; i < mag.size(); ++i) {
    float freq = (float)i * sr / (float)N;
    if (freq < 1.0f) freq = 1.0f;
    float decades_from_ref = log10f(freq / 1.0f);
    float gain_db = db_per_decade * decades_from_ref / decades;
    float gain_linear = powf(10.0f, gain_db / 20.0f);
    mag[i] *= gain_linear;
  }
}

// Apply smoothing to magnitude spectrum
inline void applySmoothing(fl::vector<float>& mag, SmoothingMode mode) {
  if (mode == SmoothingMode::None || mag.size() < 3) return;

  fl::vector<float> smoothed = mag;

  switch (mode) {
    case SmoothingMode::None:
      break;

    case SmoothingMode::Box3:
      // 3-point box filter
      for (size_t i = 1; i < mag.size() - 1; ++i) {
        smoothed[i] = (mag[i-1] + mag[i] + mag[i+1]) / 3.0f;
      }
      break;

    case SmoothingMode::Tri5:
      // 5-point triangular filter [1, 2, 3, 2, 1] / 9
      if (mag.size() >= 5) {
        for (size_t i = 2; i < mag.size() - 2; ++i) {
          smoothed[i] = (mag[i-2] + 2.0f*mag[i-1] + 3.0f*mag[i] + 2.0f*mag[i+1] + mag[i+2]) / 9.0f;
        }
      }
      break;

    case SmoothingMode::AdjAvg:
      // Adjacent average (2-sided)
      for (size_t i = 1; i < mag.size() - 1; ++i) {
        smoothed[i] = (mag[i-1] + mag[i+1]) / 2.0f;
      }
      break;
  }

  mag = smoothed;
}

// Parabolic interpolation for sub-bin accuracy
inline float parabolicInterp(float y_minus1, float y0, float y_plus1, int bin0) {
  float denom = (y_minus1 - 2.0f*y0 + y_plus1);
  if (fabsf(denom) < 1e-12f) return (float)bin0;

  float delta = 0.5f * (y_minus1 - y_plus1) / denom;
  // Clamp delta to reasonable range
  if (delta > 0.5f) delta = 0.5f;
  if (delta < -0.5f) delta = -0.5f;

  return (float)bin0 + delta;
}

// Detect multiple fundamental frequencies in the frame using FFT with enhanced spectral processing.
// Returns a list of NotePeak (MIDI note and its fundamental magnitude).
inline fl::vector<NotePeak> detectPolyphonicNotes(
    const float* x, int N, float sr, float fmin, float fmax,
    const SoundToMIDI& cfg) {

  // Apply windowing
  fl::vector<float> windowed(N);
  for (int i = 0; i < N; ++i) {
    windowed[i] = x[i];
  }
  applyWindow(windowed.data(), N, cfg.window_type);

  // Compute FFT of windowed input frame
  fl::vector<fl::pair<float, float>> spectrum;
  fft(windowed.data(), N, spectrum);
  int halfN = N / 2;

  // Magnitude spectrum (only need [0, N/2] for real input)
  fl::vector<float> mag(halfN);
  for (int i = 0; i < halfN; ++i) {
    float re = spectrum[i].first;
    float im = spectrum[i].second;
    mag[i] = sqrtf(re*re + im*im);
  }

  // Apply spectral tilt
  applySpectralTilt(mag, cfg.spectral_tilt_db_per_decade, sr, N);

  // Apply smoothing
  applySmoothing(mag, cfg.smoothing_mode);

  // Determine frequency bin range for fmin to fmax
  int binMin = (int)floorf(fmin * N / sr);
  int binMax = (int)ceilf(fmax * N / sr);
  if (binMin < 1) binMin = 1;               // start from 1 to skip DC
  if (binMax > halfN - 1) binMax = halfN - 1;

  // Convert threshold from dB to linear
  float threshold_linear = powf(10.0f, cfg.peak_threshold_db / 20.0f);

  // Find local peaks above threshold in the specified range
  struct PeakInfo {
    float bin;     // Possibly fractional bin (after parabolic interp)
    float mag;     // Magnitude
    float freq;    // Frequency in Hz
    int midiNote;  // MIDI note number
  };

  fl::vector<PeakInfo> peaks;
  peaks.reserve(16);

  for (int i = binMin + 1; i < binMax; ++i) {
    if (mag[i] > threshold_linear && mag[i] >= mag[i-1] && mag[i] >= mag[i+1]) {
      float binFractional = (float)i;

      // Apply parabolic interpolation if enabled
      if (cfg.parabolic_interp && i > 0 && i < halfN - 1) {
        binFractional = parabolicInterp(mag[i-1], mag[i], mag[i+1], i);
      }

      float freq = binFractional * sr / (float)N;
      int midiNote = clampMidi(hz_to_midi(freq));

      peaks.push_back({binFractional, mag[i], freq, midiNote});
    }
  }

  if (peaks.empty()) {
    return fl::vector<NotePeak>();  // no significant peaks found
  }

  // Sort peaks by magnitude descending (strongest first)
  for (size_t i = 0; i < peaks.size() - 1; ++i) {
    for (size_t j = 0; j < peaks.size() - i - 1; ++j) {
      if (peaks[j].mag < peaks[j + 1].mag) {
        PeakInfo tmp = peaks[j];
        peaks[j] = peaks[j + 1];
        peaks[j + 1] = tmp;
      }
    }
  }

  // Harmonic filtering: veto peaks that are harmonics of stronger fundamentals
  fl::vector<bool> vetoed(peaks.size(), false);

  if (cfg.harmonic_filter_enable) {
    for (size_t p = 0; p < peaks.size(); ++p) {
      if (vetoed[p]) continue;

      float f0 = peaks[p].freq;

      // Check all other peaks to see if they are harmonics of this fundamental
      for (size_t q = 0; q < peaks.size(); ++q) {
        if (p == q || vetoed[q]) continue;

        float fq = peaks[q].freq;

        // Check if fq is approximately an integer multiple of f0
        float ratio = fq / f0;
        int harmonic_num = (int)lroundf(ratio);

        if (harmonic_num >= 2 && harmonic_num <= 8) {
          // Calculate frequency difference in cents
          float expected_freq = f0 * harmonic_num;
          float cents_diff = 1200.0f * (logf(fq / expected_freq) / logf(2.0f));

          if (fabsf(cents_diff) < cfg.harmonic_tolerance_cents) {
            // fq is likely a harmonic of f0
            // Check energy ratio: harmonic should be weaker than fundamental (or similar)
            if (peaks[q].mag < peaks[p].mag * cfg.harmonic_energy_ratio_max) {
              vetoed[q] = true;
            }
          }
        }
      }
    }
  }

  // Build result from non-vetoed peaks that pass octave mask
  fl::vector<NotePeak> result;
  for (size_t p = 0; p < peaks.size(); ++p) {
    if (vetoed[p]) continue;

    int midiNote = peaks[p].midiNote;

    // Check octave mask
    int octave = midiNote / 12;
    if (octave > 7) octave = 7;
    if ((cfg.octave_mask & (1 << octave)) == 0) {
      continue; // This octave is masked out
    }

    result.push_back({midiNote, peaks[p].mag});
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
  const float thresh = 0.10f; // ITER7: Stricter threshold for better pitch accuracy
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

// ---------- SoundToMIDIMono ----------
SoundToMIDIMono::SoundToMIDIMono(const SoundToMIDI& cfg)
    : _cfg(cfg), _noteOnFrames(0), _silenceFrames(0), _currentNote(-1),
      _candidateNote(-1), _candidateHoldFrames(0),
      _historyIndex(0), _historyCount(0),
      _ringWriteIdx(0), _ringAccumulated(0), _slidingEnabled(false),
      _onsetHistoryIdx(0), _lastFrameVoiced(false) {
  for (int i = 0; i < MAX_MEDIAN_SIZE; ++i) {
    _noteHistory[i] = -1;
  }

  // Initialize K-of-M onset history
  if (_cfg.enable_k_of_m) {
    _onsetHistory.resize(_cfg.k_of_m_window, false);
  }

  // Ensure hop_size doesn't exceed frame_size
  if (_cfg.hop_size > _cfg.frame_size) {
    _cfg.hop_size = _cfg.frame_size;
  }

  // Initialize sliding window if hop_size < frame_size
  _slidingEnabled = (_cfg.hop_size < _cfg.frame_size);

  if (_slidingEnabled) {
    // Allocate ring buffer (size = frame_size + hop_size to avoid wrap branch)
    _sampleRing.resize(_cfg.frame_size + _cfg.hop_size);
    _analysisFrame.resize(_cfg.frame_size);

    // Precompute Hann window coefficients
    _windowCoeffs.resize(_cfg.frame_size);
    for (int i = 0; i < _cfg.frame_size; ++i) {
      float t = (float)i / (float)(_cfg.frame_size - 1);
      _windowCoeffs[i] = 0.5f * (1.0f - cosf(2.0f * M_PI * t));
    }
  }

  // Initialize auto-tuning state
  if (_cfg.auto_tune_enable) {
    // Calculate calibration frames needed
    float frames_per_sec = _cfg.sample_rate_hz / (float)_cfg.hop_size;
    _autoTuneState.calibration_frames = (int)(_cfg.auto_tune_calibration_time_sec * frames_per_sec);
    _autoTuneState.in_calibration = true;
  }
}

int SoundToMIDIMono::getMedianNote() {
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

void SoundToMIDIMono::processFrame(const float* samples, int n) {
  if (!samples || n <= 0) return;

  if (!_slidingEnabled) {
    // Legacy mode: require exact frame_size, no windowing
    if (n != _cfg.frame_size) return;
    processFrameInternal(samples);
    return;
  }

  // Sliding window mode: accumulate and trigger analysis
  for (int i = 0; i < n; ++i) {
    _sampleRing[_ringWriteIdx++] = samples[i];
    if (_ringWriteIdx >= (int)_sampleRing.size()) {
      _ringWriteIdx = 0;
    }
    _ringAccumulated++;

    // Trigger analysis every hop_size samples
    if (_ringAccumulated >= _cfg.hop_size) {
      extractAndAnalyzeFrame();
      _ringAccumulated -= _cfg.hop_size;
    }
  }
}

void SoundToMIDIMono::extractAndAnalyzeFrame() {
  // Extract last frame_size samples from ring buffer
  int readIdx = _ringWriteIdx - _cfg.frame_size;
  if (readIdx < 0) readIdx += _sampleRing.size();

  // Apply window and copy to analysis frame
  for (int i = 0; i < _cfg.frame_size; ++i) {
    _analysisFrame[i] = _sampleRing[readIdx] * _windowCoeffs[i];
    readIdx++;
    if (readIdx >= (int)_sampleRing.size()) readIdx = 0;
  }

  // Run analysis on windowed frame
  processFrameInternal(_analysisFrame.data());
}

void SoundToMIDIMono::processFrameInternal(const float* frame) {
  // Compute loudness (RMS) of the frame
  const float rms = compute_rms(frame, _cfg.frame_size);

  // --- Monophonic pitch detection logic ---
  const PitchResult pr = _det.detect(frame, _cfg.frame_size, _cfg.sample_rate_hz, _cfg.fmin_hz, _cfg.fmax_hz);
  const bool voiced = (rms > _cfg.rms_gate) &&
                      (pr.confidence > _cfg.confidence_threshold) &&
                      (pr.freq_hz > 0.0f);

  // Auto-tuning: Update tracking statistics
  if (_cfg.auto_tune_enable) {
    _autoTuneState.frames_processed++;

    // Handle calibration phase
    if (_autoTuneState.in_calibration) {
      _autoTuneState.calibration_frames--;
      if (_autoTuneState.calibration_frames <= 0) {
        _autoTuneState.in_calibration = false;
      }
      // During calibration, update noise floor but don't emit events
      updateNoiseFloor(rms, !voiced);
      return; // Skip MIDI output during calibration
    }

    // Update noise floor estimation
    updateNoiseFloor(rms, _currentNote < 0);

    // Update confidence tracking
    if (voiced) {
      updateConfidenceTracking(pr.confidence);
      updateJitterTracking(pr.freq_hz);
    }

    // Determine if auto-tune update is needed
    _autoTuneState.frames_since_update++;
    float frames_per_sec = _cfg.sample_rate_hz / (float)_cfg.hop_size;
    int frames_per_update = (int)(frames_per_sec / _cfg.auto_tune_update_rate_hz);
    if (_autoTuneState.frames_since_update >= frames_per_update) {
      autoTuneUpdate();
      _autoTuneState.frames_since_update = 0;
    }
  }

  // K-of-M onset detection: check if we should treat this frame as voiced
  bool k_of_m_voiced = checkKofMOnset(voiced);

  if (k_of_m_voiced) {
    int rawNote = clampMidi(hz_to_midi(pr.freq_hz));

    // Add to median filter history
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

        // Auto-tuning: Track note start
        updateEventRate(true);
        updateNoteDuration(true, false);
      }
    } else {
      // Check semitone threshold
      const int dn = noteDelta(note, _currentNote);
      if (dn >= _cfg.note_change_semitone_threshold) {
        // Check if candidate note persists
        if (note == _candidateNote) {
          _candidateHoldFrames++;
          if (_candidateHoldFrames >= _cfg.note_change_hold_frames) {
            // Retrigger confirmed
            if (onNoteOff) onNoteOff((uint8_t)_currentNote);
            const uint8_t vel = amp_to_velocity(rms, _cfg.vel_gain, _cfg.vel_floor);
            if (onNoteOn) onNoteOn((uint8_t)note, vel);

            // Auto-tuning: Track note change
            updateNoteDuration(false, true);
            updateEventRate(true);
            updateNoteDuration(true, false);

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

        // Auto-tuning: Track note end
        updateNoteDuration(false, true);

        _currentNote = -1;
        _silenceFrames = 0;
        // Clear median filter history on silence
        _historyCount = 0;
        _historyIndex = 0;
      }
    }
  }
}

const SoundToMIDI& SoundToMIDIMono::config() const {
  return _cfg;
}

void SoundToMIDIMono::setConfig(const SoundToMIDI& c) {
  _cfg = c;
}

bool SoundToMIDIMono::checkKofMOnset(bool current_onset) {
  if (!_cfg.enable_k_of_m || _onsetHistory.empty()) {
    return current_onset;  // K-of-M disabled, pass through
  }

  // Add current onset to circular buffer
  _onsetHistory[_onsetHistoryIdx] = current_onset;
  _onsetHistoryIdx = (_onsetHistoryIdx + 1) % _onsetHistory.size();

  // Count how many of last M frames were onsets
  int onset_count = 0;
  for (bool onset : _onsetHistory) {
    if (onset) onset_count++;
  }

  // Require at least K onsets in last M frames
  return onset_count >= (int)_cfg.k_of_m_onset;
}

// ---------- SoundToMIDIPoly ----------
SoundToMIDIPoly::SoundToMIDIPoly(const SoundToMIDI& cfg)
    : _cfg(cfg), _ringWriteIdx(0), _ringAccumulated(0), _slidingEnabled(false) {
  initializeState();

  // Ensure hop_size doesn't exceed frame_size
  if (_cfg.hop_size > _cfg.frame_size) {
    _cfg.hop_size = _cfg.frame_size;
  }

  // Initialize sliding window if hop_size < frame_size
  _slidingEnabled = (_cfg.hop_size < _cfg.frame_size);

  if (_slidingEnabled) {
    // Allocate ring buffer (size = frame_size + hop_size to avoid wrap branch)
    _sampleRing.resize(_cfg.frame_size + _cfg.hop_size);
    _analysisFrame.resize(_cfg.frame_size);

    // Precompute Hann window coefficients
    _windowCoeffs.resize(_cfg.frame_size);
    for (int i = 0; i < _cfg.frame_size; ++i) {
      float t = (float)i / (float)(_cfg.frame_size - 1);
      _windowCoeffs[i] = 0.5f * (1.0f - cosf(2.0f * M_PI * t));
    }
  }

  // Initialize auto-tuning state
  if (_cfg.auto_tune_enable) {
    // Calculate calibration frames needed
    float frames_per_sec = _cfg.sample_rate_hz / (float)_cfg.hop_size;
    _autoTuneState.calibration_frames = (int)(_cfg.auto_tune_calibration_time_sec * frames_per_sec);
    _autoTuneState.in_calibration = true;
  }
}

void SoundToMIDIPoly::initializeState() {
  // Initialize polyphonic tracking arrays
  for (int note = 0; note < 128; ++note) {
    _activeNotes[note] = false;
    _noteOnCount[note] = 0;
    _noteOffCount[note] = 0;
    // Initialize K-of-M state
    _noteKofM[note].onsetCount = 0;
    _noteKofM[note].offsetCount = 0;
    // Initialize peak continuity memory
    _peakMemory[note].freq_hz = 0.0f;
    _peakMemory[note].magnitude = 0.0f;
    _peakMemory[note].frames_absent = 0;
  }
  // Initialize PCP history
  for (int i = 0; i < NUM_PITCH_CLASSES; ++i) {
    _pcpHistory[i] = 0.0f;
  }
}

void SoundToMIDIPoly::processFrame(const float* samples, int n) {
  if (!samples || n <= 0) return;

  if (!_slidingEnabled) {
    // Legacy mode: require exact frame_size, no windowing
    if (n != _cfg.frame_size) return;
    processFrameInternal(samples);
    return;
  }

  // Sliding window mode: accumulate and trigger analysis
  for (int i = 0; i < n; ++i) {
    _sampleRing[_ringWriteIdx++] = samples[i];
    if (_ringWriteIdx >= (int)_sampleRing.size()) {
      _ringWriteIdx = 0;
    }
    _ringAccumulated++;

    // Trigger analysis every hop_size samples
    if (_ringAccumulated >= _cfg.hop_size) {
      extractAndAnalyzeFrame();
      _ringAccumulated -= _cfg.hop_size;
    }
  }
}

void SoundToMIDIPoly::extractAndAnalyzeFrame() {
  // Extract last frame_size samples from ring buffer
  int readIdx = _ringWriteIdx - _cfg.frame_size;
  if (readIdx < 0) readIdx += _sampleRing.size();

  // Apply window and copy to analysis frame
  for (int i = 0; i < _cfg.frame_size; ++i) {
    _analysisFrame[i] = _sampleRing[readIdx] * _windowCoeffs[i];
    readIdx++;
    if (readIdx >= (int)_sampleRing.size()) readIdx = 0;
  }

  // Run analysis on windowed frame
  processFrameInternal(_analysisFrame.data());
}

void SoundToMIDIPoly::processFrameInternal(const float* frame) {
  // Compute loudness (RMS) of the frame
  const float rms = compute_rms(frame, _cfg.frame_size);

  // Auto-tuning: Update frame counter and calibration
  if (_cfg.auto_tune_enable) {
    _autoTuneState.frames_processed++;

    // Handle calibration phase
    if (_autoTuneState.in_calibration) {
      _autoTuneState.calibration_frames--;
      if (_autoTuneState.calibration_frames <= 0) {
        _autoTuneState.in_calibration = false;
      }
      // During calibration, skip MIDI output
      return;
    }
  }

  // --- Polyphonic pitch detection logic ---
  bool voiced = (rms > _cfg.rms_gate);
  if (voiced) {
    // Get all detected note peaks in this frame using enhanced spectral processing
    fl::vector<NotePeak> notes = detectPolyphonicNotes(frame, _cfg.frame_size, _cfg.sample_rate_hz,
                                                       _cfg.fmin_hz, _cfg.fmax_hz, _cfg);

    // Auto-tuning: Track peaks and update statistics
    if (_cfg.auto_tune_enable) {
      updatePeakTracking((int)notes.size());

      fl::vector<int> midi_notes;
      midi_notes.reserve(notes.size());
      for (const NotePeak& np : notes) {
        midi_notes.push_back(np.midi);
      }
      updateOctaveStatistics(midi_notes);

      // Note: We don't have easy access to the spectrum here, so we'll use a simplified approach
      // for noise floor estimation based on RMS when no peaks are detected
      fl::vector<float> empty_spectrum;
      updateNoiseFloor(rms, (int)notes.size(), empty_spectrum);

      // Determine if auto-tune update is needed
      _autoTuneState.frames_since_update++;
      float frames_per_sec = _cfg.sample_rate_hz / (float)_cfg.hop_size;
      int frames_per_update = (int)(frames_per_sec / _cfg.auto_tune_update_rate_hz);
      if (_autoTuneState.frames_since_update >= frames_per_update) {
        autoTuneUpdate();
        _autoTuneState.frames_since_update = 0;
      }
    }

    // Update PCP (Pitch Class Profile) if enabled
    if (_cfg.pcp_enable) {
      fl::vector<int> detected_midi_notes;
      detected_midi_notes.reserve(notes.size());
      for (const NotePeak& np : notes) {
        detected_midi_notes.push_back(np.midi);
      }
      updatePCP(detected_midi_notes);
    }

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

    // Update note states with K-of-M filtering
    for (int note = 0; note < 128; ++note) {
      // Update K-of-M counters
      if (_cfg.enable_k_of_m) {
        if (present[note]) {
          _noteKofM[note].onsetCount++;
          _noteKofM[note].offsetCount = 0;
          // Clamp to window size
          if (_noteKofM[note].onsetCount > (int)_cfg.k_of_m_window) {
            _noteKofM[note].onsetCount = _cfg.k_of_m_window;
          }
        } else {
          _noteKofM[note].offsetCount++;
          _noteKofM[note].onsetCount = 0;
          // Clamp to window size
          if (_noteKofM[note].offsetCount > (int)_cfg.k_of_m_window) {
            _noteKofM[note].offsetCount = _cfg.k_of_m_window;
          }
        }
      }

      // Determine if note should be considered present after K-of-M filtering
      bool k_of_m_present = present[note];
      if (_cfg.enable_k_of_m) {
        k_of_m_present = (_noteKofM[note].onsetCount >= (int)_cfg.k_of_m_onset);
      }

      if (k_of_m_present) {
        // Note is detected (after K-of-M filtering)

        // Update peak memory for continuity tracking
        if (present[note]) {  // Only update if actually detected this frame
          for (const NotePeak& np : notes) {
            if (np.midi == note) {
              // Convert MIDI back to Hz for tracking (MIDI 69 = A4 = 440Hz)
              _peakMemory[note].freq_hz = 440.0f * powf(2.0f, (note - 69) / 12.0f);
              _peakMemory[note].magnitude = np.magnitude;
              _peakMemory[note].frames_absent = 0;
              break;
            }
          }
        }

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
        // Note is NOT present (after K-of-M filtering)
        _noteOnCount[note] = 0;

        // Increment frames absent for peak memory
        _peakMemory[note].frames_absent++;

        if (_activeNotes[note]) {
          // An active note might be turning off
          _noteOffCount[note] += 1;
          if (_noteOffCount[note] >= _cfg.silence_frames_off) {
            // Confirmed note-off after sustained silence
            if (onNoteOff) onNoteOff((uint8_t)note);
            _activeNotes[note] = false;
            _noteOffCount[note] = 0;

            // Clear peak memory on note off
            _peakMemory[note].freq_hz = 0.0f;
            _peakMemory[note].magnitude = 0.0f;
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
}

const SoundToMIDI& SoundToMIDIPoly::config() const {
  return _cfg;
}

void SoundToMIDIPoly::setConfig(const SoundToMIDI& c) {
  _cfg = c;
}

void SoundToMIDIPoly::setPeakThresholdDb(float db) {
  _cfg.peak_threshold_db = db;
}

void SoundToMIDIPoly::setOctaveMask(uint8_t mask) {
  _cfg.octave_mask = mask;
}

void SoundToMIDIPoly::setSpectralTilt(float db_per_decade) {
  _cfg.spectral_tilt_db_per_decade = db_per_decade;
}

void SoundToMIDIPoly::setSmoothingMode(SmoothingMode mode) {
  _cfg.smoothing_mode = mode;
}

void SoundToMIDIPoly::precomputeWindow() {
  // Window precomputation will be added when we implement windowing
  // For now this is a placeholder
}

bool SoundToMIDIPoly::passesOctaveMask(int midiNote) const {
  if (midiNote < 0 || midiNote > 127) return false;
  int octave = midiNote / 12;
  if (octave > 7) octave = 7; // Cap at octave 7
  return (_cfg.octave_mask & (1 << octave)) != 0;
}

void SoundToMIDIPoly::updatePCP(const fl::vector<int>& notes) {
  if (!_cfg.pcp_enable) return;

  // Decay factor for EMA
  const float decay = 1.0f - (1.0f / _cfg.pcp_history_frames);

  // Decay all pitch classes
  for (int i = 0; i < NUM_PITCH_CLASSES; ++i) {
    _pcpHistory[i] *= decay;
  }

  // Increment detected pitch classes
  for (int note : notes) {
    if (note >= 0 && note < 128) {
      int pc = note % 12;
      _pcpHistory[pc] += (1.0f - decay);
      if (_pcpHistory[pc] > 1.0f) _pcpHistory[pc] = 1.0f;
    }
  }
}

float SoundToMIDIPoly::getPCPBias(int midiNote) const {
  if (!_cfg.pcp_enable || midiNote < 0 || midiNote > 127) {
    return 0.0f;
  }
  int pc = midiNote % 12;
  return _pcpHistory[pc] * _cfg.pcp_bias_weight;
}

// ---------- Auto-Tuning Methods (Monophonic) ----------

void SoundToMIDIMono::updateNoiseFloor(float rms, bool is_silent) {
  if (!_cfg.auto_tune_enable) return;

  // Only update noise floor during silence (no active note)
  if (is_silent) {
    const float alpha = 0.05f; // Slow EMA for noise floor (time constant ~0.5s at 5Hz update)
    _autoTuneState.noise_rms_est = _autoTuneState.noise_rms_est * (1.0f - alpha) + rms * alpha;
  }
}

void SoundToMIDIMono::updateConfidenceTracking(float confidence) {
  if (!_cfg.auto_tune_enable) return;

  const float alpha = 0.1f; // EMA smoothing factor
  _autoTuneState.confidence_ema = _autoTuneState.confidence_ema * (1.0f - alpha) + confidence * alpha;
}

void SoundToMIDIMono::updateJitterTracking(float freq_hz) {
  if (!_cfg.auto_tune_enable) return;

  if (_autoTuneState.prev_pitch_valid && freq_hz > 0) {
    // Calculate pitch variance (semitone difference)
    float semitone_diff = 12.0f * (logf(freq_hz / _autoTuneState.prev_pitch_hz) / logf(2.0f));
    float variance = semitone_diff * semitone_diff;

    const float alpha = 0.1f;
    _autoTuneState.pitch_variance_ema = _autoTuneState.pitch_variance_ema * (1.0f - alpha) + variance * alpha;
  }

  _autoTuneState.prev_pitch_hz = freq_hz;
  _autoTuneState.prev_pitch_valid = (freq_hz > 0);
}

void SoundToMIDIMono::updateEventRate(bool note_on) {
  if (!_cfg.auto_tune_enable) return;

  if (note_on) {
    _autoTuneState.note_events_count++;
  }
}

void SoundToMIDIMono::updateNoteDuration(bool note_started, bool note_ended) {
  if (!_cfg.auto_tune_enable) return;

  if (note_started) {
    _autoTuneState.current_note_start_frame = _autoTuneState.frames_processed;
  }

  if (note_ended && _autoTuneState.current_note_start_frame >= 0) {
    // Calculate note duration
    int duration = _autoTuneState.frames_processed - _autoTuneState.current_note_start_frame;

    // Calculate gap since last note
    int gap = 0;
    if (_autoTuneState.last_note_off_frame >= 0) {
      gap = _autoTuneState.current_note_start_frame - _autoTuneState.last_note_off_frame;
    }

    // Update EMAs
    const float alpha = 0.15f;
    if (_autoTuneState.note_duration_ema > 0) {
      _autoTuneState.note_duration_ema = _autoTuneState.note_duration_ema * (1.0f - alpha) + duration * alpha;
    } else {
      _autoTuneState.note_duration_ema = (float)duration;
    }

    if (gap > 0) {
      if (_autoTuneState.note_gap_ema > 0) {
        _autoTuneState.note_gap_ema = _autoTuneState.note_gap_ema * (1.0f - alpha) + gap * alpha;
      } else {
        _autoTuneState.note_gap_ema = (float)gap;
      }
    }

    _autoTuneState.last_note_off_frame = _autoTuneState.frames_processed;
    _autoTuneState.current_note_start_frame = -1;
  }
}

void SoundToMIDIMono::notifyParamChange(const char* name, float old_val, float new_val) {
  if (_autoTuneCallback && fabsf(new_val - old_val) > 1e-6f) {
    _autoTuneCallback(name, old_val, new_val);
  }
}

void SoundToMIDIMono::autoTuneUpdate() {
  if (!_cfg.auto_tune_enable || _autoTuneState.in_calibration) return;

  const float smoothing = _cfg.auto_tune_param_smoothing;
  const float step = _cfg.auto_tune_threshold_step;

  // 1. Adaptive RMS gate
  float target_rms_gate = _autoTuneState.noise_rms_est * _cfg.auto_tune_rms_margin;
  // Clamp to configured limits
  if (target_rms_gate < _cfg.auto_tune_rms_gate_min) target_rms_gate = _cfg.auto_tune_rms_gate_min;
  if (target_rms_gate > _cfg.auto_tune_rms_gate_max) target_rms_gate = _cfg.auto_tune_rms_gate_max;

  // Smooth update
  float old_rms = _cfg.rms_gate;
  _cfg.rms_gate = _cfg.rms_gate * smoothing + target_rms_gate * (1.0f - smoothing);
  notifyParamChange("rms_gate", old_rms, _cfg.rms_gate);

  // 2. Adaptive confidence threshold
  // If average confidence is low, raise threshold; if missing notes, lower it
  if (_autoTuneState.confidence_ema > 0) {
    float old_conf = _cfg.confidence_threshold;

    if (_autoTuneState.confidence_ema < _cfg.confidence_threshold - 0.1f) {
      // Many low-confidence detections, raise threshold
      _cfg.confidence_threshold += step;
    } else if (_autoTuneState.event_rate_ema < _cfg.auto_tune_notes_per_sec_min * 0.8f) {
      // Too few events, lower threshold
      _cfg.confidence_threshold -= step;
    }

    // Clamp
    if (_cfg.confidence_threshold < _cfg.auto_tune_confidence_min) {
      _cfg.confidence_threshold = _cfg.auto_tune_confidence_min;
    }
    if (_cfg.confidence_threshold > _cfg.auto_tune_confidence_max) {
      _cfg.confidence_threshold = _cfg.auto_tune_confidence_max;
    }

    notifyParamChange("confidence_threshold", old_conf, _cfg.confidence_threshold);
  }

  // 3. Jitter-based median filter adjustment
  if (_autoTuneState.pitch_variance_ema > 0) {
    int old_median = _cfg.median_filter_size;

    if (_autoTuneState.pitch_variance_ema > 1.0f) {
      // High jitter, increase median filter
      if (_cfg.median_filter_size < 5) _cfg.median_filter_size = 3;
      if (_autoTuneState.pitch_variance_ema > 4.0f && _cfg.median_filter_size < 5) {
        _cfg.median_filter_size = 5;
      }
    } else if (_autoTuneState.pitch_variance_ema < 0.25f) {
      // Low jitter, reduce latency
      _cfg.median_filter_size = 1;
    }

    if (old_median != _cfg.median_filter_size) {
      notifyParamChange("median_filter_size", (float)old_median, (float)_cfg.median_filter_size);
    }
  }

  // 4. Event rate control
  // Calculate frames per update window
  float frames_per_sec = _cfg.sample_rate_hz / (float)_cfg.hop_size;
  float update_window_sec = 1.0f / _cfg.auto_tune_update_rate_hz;
  float frames_per_window = frames_per_sec * update_window_sec;

  // Calculate event rate (events per second)
  float event_rate = _autoTuneState.note_events_count / update_window_sec;

  // Update EMA
  const float alpha_rate = 0.2f;
  _autoTuneState.event_rate_ema = _autoTuneState.event_rate_ema * (1.0f - alpha_rate) + event_rate * alpha_rate;

  // Adjust confidence threshold based on event rate
  if (_autoTuneState.event_rate_ema > _cfg.auto_tune_notes_per_sec_max) {
    float old_conf = _cfg.confidence_threshold;
    _cfg.confidence_threshold += step * 0.5f;
    if (_cfg.confidence_threshold > _cfg.auto_tune_confidence_max) {
      _cfg.confidence_threshold = _cfg.auto_tune_confidence_max;
    }
    notifyParamChange("confidence_threshold", old_conf, _cfg.confidence_threshold);
  } else if (_autoTuneState.event_rate_ema < _cfg.auto_tune_notes_per_sec_min) {
    float old_conf = _cfg.confidence_threshold;
    _cfg.confidence_threshold -= step * 0.5f;
    if (_cfg.confidence_threshold < _cfg.auto_tune_confidence_min) {
      _cfg.confidence_threshold = _cfg.auto_tune_confidence_min;
    }
    notifyParamChange("confidence_threshold", old_conf, _cfg.confidence_threshold);
  }

  // 5. Hold-time optimization (based on note duration/gap EMAs)
  if (_autoTuneState.note_duration_ema > 0) {
    int old_hold = _cfg.note_hold_frames;

    // Set hold frames to ~75% of average note duration
    float target_hold = _autoTuneState.note_duration_ema * 0.75f;
    int new_hold = (int)(target_hold + 0.5f);
    if (new_hold < 1) new_hold = 1;
    if (new_hold > 10) new_hold = 10;

    _cfg.note_hold_frames = new_hold;

    if (old_hold != _cfg.note_hold_frames) {
      notifyParamChange("note_hold_frames", (float)old_hold, (float)_cfg.note_hold_frames);
    }
  }

  if (_autoTuneState.note_gap_ema > 0) {
    int old_silence = _cfg.silence_frames_off;

    // Set silence frames to ~50% of average gap
    float target_silence = _autoTuneState.note_gap_ema * 0.5f;
    int new_silence = (int)(target_silence + 0.5f);
    if (new_silence < 1) new_silence = 1;
    if (new_silence > 10) new_silence = 10;

    _cfg.silence_frames_off = new_silence;

    if (old_silence != _cfg.silence_frames_off) {
      notifyParamChange("silence_frames_off", (float)old_silence, (float)_cfg.silence_frames_off);
    }
  }

  // Reset event counter for next window
  _autoTuneState.note_events_count = 0;
}

// ---------- Auto-Tuning Methods (Polyphonic) ----------

void SoundToMIDIPoly::updateNoiseFloor(float rms, int num_peaks, const fl::vector<float>& spectrum) {
  if (!_cfg.auto_tune_enable) return;

  // Update noise floor when no significant peaks detected
  if (num_peaks == 0) {
    const float alpha = 0.05f;
    _autoTuneState.noise_rms_est = _autoTuneState.noise_rms_est * (1.0f - alpha) + rms * alpha;

    // Calculate spectral median for noise mag estimation
    if (!spectrum.empty()) {
      fl::vector<float> sorted = spectrum;
      // Simple bubble sort for median (spectrum is small)
      for (size_t i = 0; i < sorted.size() - 1; ++i) {
        for (size_t j = 0; j < sorted.size() - i - 1; ++j) {
          if (sorted[j] > sorted[j + 1]) {
            float tmp = sorted[j];
            sorted[j] = sorted[j + 1];
            sorted[j + 1] = tmp;
          }
        }
      }
      float median_mag = sorted[sorted.size() / 2];
      float median_db = 20.0f * log10f(median_mag + 1e-12f);

      _autoTuneState.noise_mag_db_est = _autoTuneState.noise_mag_db_est * (1.0f - alpha) + median_db * alpha;
    }
  }
}

void SoundToMIDIPoly::updatePeakTracking(int num_peaks) {
  if (!_cfg.auto_tune_enable) return;

  _autoTuneState.peaks_total += num_peaks;
  _autoTuneState.peaks_count++;
}

void SoundToMIDIPoly::updateOctaveStatistics(const fl::vector<int>& notes) {
  if (!_cfg.auto_tune_enable) return;

  for (int note : notes) {
    int octave = note / 12;
    if (octave >= 0 && octave < AutoTuneState::NUM_OCTAVES) {
      _autoTuneState.octave_detections[octave]++;
    }
  }
}

void SoundToMIDIPoly::notifyParamChange(const char* name, float old_val, float new_val) {
  if (_autoTuneCallback && fabsf(new_val - old_val) > 1e-6f) {
    _autoTuneCallback(name, old_val, new_val);
  }
}

void SoundToMIDIPoly::autoTuneUpdate() {
  if (!_cfg.auto_tune_enable || _autoTuneState.in_calibration) return;

  const float smoothing = _cfg.auto_tune_param_smoothing;
  const float step = _cfg.auto_tune_threshold_step;

  // 1. Adaptive peak threshold based on noise floor
  float target_peak_db = _autoTuneState.noise_mag_db_est + _cfg.auto_tune_peak_margin_db;

  // Clamp to configured limits
  if (target_peak_db < _cfg.auto_tune_peak_db_min) target_peak_db = _cfg.auto_tune_peak_db_min;
  if (target_peak_db > _cfg.auto_tune_peak_db_max) target_peak_db = _cfg.auto_tune_peak_db_max;

  // Smooth update
  float old_peak = _cfg.peak_threshold_db;
  _cfg.peak_threshold_db = _cfg.peak_threshold_db * smoothing + target_peak_db * (1.0f - smoothing);
  notifyParamChange("peak_threshold_db", old_peak, _cfg.peak_threshold_db);

  // 2. Event rate control (peaks per frame)
  if (_autoTuneState.peaks_count > 0) {
    float avg_peaks = (float)_autoTuneState.peaks_total / (float)_autoTuneState.peaks_count;

    // Update EMA
    const float alpha_rate = 0.2f;
    _autoTuneState.event_rate_ema = _autoTuneState.event_rate_ema * (1.0f - alpha_rate) + avg_peaks * alpha_rate;

    // Adjust peak threshold based on event rate
    if (_autoTuneState.event_rate_ema > _cfg.auto_tune_peaks_per_frame_max) {
      float old_db = _cfg.peak_threshold_db;
      _cfg.peak_threshold_db += step * 2.0f; // Larger step for dB
      if (_cfg.peak_threshold_db > _cfg.auto_tune_peak_db_max) {
        _cfg.peak_threshold_db = _cfg.auto_tune_peak_db_max;
      }
      notifyParamChange("peak_threshold_db", old_db, _cfg.peak_threshold_db);
    } else if (_autoTuneState.event_rate_ema < _cfg.auto_tune_peaks_per_frame_min) {
      float old_db = _cfg.peak_threshold_db;
      _cfg.peak_threshold_db -= step * 2.0f;
      if (_cfg.peak_threshold_db < _cfg.auto_tune_peak_db_min) {
        _cfg.peak_threshold_db = _cfg.auto_tune_peak_db_min;
      }
      notifyParamChange("peak_threshold_db", old_db, _cfg.peak_threshold_db);
    }

    // Reset counters
    _autoTuneState.peaks_total = 0;
    _autoTuneState.peaks_count = 0;
  }

  // 3. Octave mask adjustment (disable noisy octaves)
  int total_detections = 0;
  for (int i = 0; i < AutoTuneState::NUM_OCTAVES; ++i) {
    total_detections += _autoTuneState.octave_detections[i];
  }

  if (total_detections > 100) { // Need enough data
    for (int i = 0; i < AutoTuneState::NUM_OCTAVES; ++i) {
      float ratio = (float)_autoTuneState.octave_detections[i] / (float)total_detections;

      // If an octave has very few detections (<1%) and it's enabled, consider disabling
      if (ratio < 0.01f && (_cfg.octave_mask & (1 << i))) {
        // Don't disable automatically - just track as potentially spurious
        _autoTuneState.octave_spurious[i]++;
      }

      // Reset detection counts periodically
      _autoTuneState.octave_detections[i] = 0;
    }
  }

  // 4. Harmonic filter adjustment
  // If event rate is still high after threshold increase, tighten harmonic filtering
  if (_autoTuneState.event_rate_ema > _cfg.auto_tune_peaks_per_frame_max * 1.2f) {
    if (_cfg.harmonic_energy_ratio_max > 0.5f) {
      float old_ratio = _cfg.harmonic_energy_ratio_max;
      _cfg.harmonic_energy_ratio_max -= 0.05f;
      if (_cfg.harmonic_energy_ratio_max < 0.5f) _cfg.harmonic_energy_ratio_max = 0.5f;
      notifyParamChange("harmonic_energy_ratio_max", old_ratio, _cfg.harmonic_energy_ratio_max);
    }
  }

  // 5. PCP stabilizer adjustment
  // Enable PCP if we detect oscillation between pitch classes (heuristic: moderate event rate with variance)
  // This is a simplified heuristic - full implementation would track pitch class transitions
  if (!_cfg.pcp_enable && _autoTuneState.event_rate_ema > 3.0f) {
    // Could enable PCP here, but let's be conservative
    // _cfg.pcp_enable = true;
  }
}

// ========== Sliding Window STFT Implementation ==========

SoundToMIDISliding::SoundToMIDISliding(const SoundToMIDI& baseCfg, const SlidingCfg& slideCfg, bool use_poly)
  : mSlideCfg(slideCfg), mUsePoly(use_poly) {

  // Allocate ring buffer (capacity = frame_size + hop_size to avoid wrap branch)
  mPcmRing.resize(mSlideCfg.frame_size + mSlideCfg.hop_size);

  // Allocate frame buffer
  mFrameBuffer.resize(mSlideCfg.frame_size);

  // Initialize window coefficients
  initWindow();

  // Initialize K-of-M onset history
  mOnsetHistory.resize(mSlideCfg.k_of_m_window);

  // Create appropriate engine
  if (mUsePoly) {
    mPolyEngine = new SoundToMIDIPoly(baseCfg);

    // Only intercept callbacks if K-of-M is enabled
    if (mSlideCfg.enable_k_of_m) {
      mPolyEngine->onNoteOn = [this](uint8_t note, uint8_t velocity) {
        this->handlePolyNoteOn(note, velocity);
      };
      mPolyEngine->onNoteOff = [this](uint8_t note) {
        this->handlePolyNoteOff(note);
      };
    }
  } else {
    mMonoEngine = new SoundToMIDIMono(baseCfg);

    // Only intercept callbacks if K-of-M is enabled
    if (mSlideCfg.enable_k_of_m) {
      mMonoEngine->onNoteOn = [this](uint8_t note, uint8_t velocity) {
        this->handleMonoNoteOn(note, velocity);
      };
      mMonoEngine->onNoteOff = [this](uint8_t note) {
        this->handleMonoNoteOff(note);
      };
    }
  }
}

void SoundToMIDISliding::initWindow() {
  mWindow.resize(mSlideCfg.frame_size);

  for (uint16_t i = 0; i < mSlideCfg.frame_size; ++i) {
    float t = static_cast<float>(i) / static_cast<float>(mSlideCfg.frame_size - 1);

    switch (mSlideCfg.window) {
      case SlidingCfg::Window::Hann:
        mWindow[i] = 0.5f * (1.0f - cosf(2.0f * M_PI * t));
        break;

      case SlidingCfg::Window::Hamming:
        mWindow[i] = 0.54f - 0.46f * cosf(2.0f * M_PI * t);
        break;

      case SlidingCfg::Window::Blackman:
        mWindow[i] = 0.42f - 0.5f * cosf(2.0f * M_PI * t) + 0.08f * cosf(4.0f * M_PI * t);
        break;
    }
  }
}

void SoundToMIDISliding::processSamples(const float* samples, int n) {
  for (int i = 0; i < n; ++i) {
    // Add sample to ring buffer
    mPcmRing[mWriteIdx++] = samples[i];
    if (mWriteIdx >= static_cast<int>(mPcmRing.size())) {
      mWriteIdx = 0;
    }
    mAccumulated++;

    // When we have enough samples, process a frame
    if (mAccumulated >= static_cast<int>(mSlideCfg.hop_size)) {
      makeFrame(mFrameBuffer.data());
      applyWindow(mFrameBuffer.data());
      runAnalysis(mFrameBuffer.data());
      mAccumulated -= static_cast<int>(mSlideCfg.hop_size);
    }
  }
}

void SoundToMIDISliding::makeFrame(float* outFrame) {
  // Extract last frame_size samples from ring buffer into contiguous frame
  int readIdx = mWriteIdx - mSlideCfg.frame_size;
  if (readIdx < 0) {
    readIdx += mPcmRing.size();
  }

  for (uint16_t i = 0; i < mSlideCfg.frame_size; ++i) {
    outFrame[i] = mPcmRing[readIdx++];
    if (readIdx >= static_cast<int>(mPcmRing.size())) {
      readIdx = 0;
    }
  }
}

void SoundToMIDISliding::applyWindow(float* frame) {
  for (uint16_t i = 0; i < mSlideCfg.frame_size; ++i) {
    frame[i] *= mWindow[i];
  }
}

void SoundToMIDISliding::runAnalysis(const float* frame) {
  if (mUsePoly) {
    mPolyEngine->processFrame(frame, mSlideCfg.frame_size);
  } else {
    mMonoEngine->processFrame(frame, mSlideCfg.frame_size);
  }
}

bool SoundToMIDISliding::checkKofMOnset(bool current_onset) {
  // Add current onset to history
  mOnsetHistory[mOnsetHistoryIdx] = current_onset;
  mOnsetHistoryIdx = (mOnsetHistoryIdx + 1) % mSlideCfg.k_of_m_window;

  // Count how many of the last m frames had onsets
  int onset_count = 0;
  for (uint8_t i = 0; i < mSlideCfg.k_of_m_window; ++i) {
    if (mOnsetHistory[i]) {
      onset_count++;
    }
  }

  // Return true if we have at least k onsets in the last m frames
  return onset_count >= mSlideCfg.k_of_m_onset;
}

void SoundToMIDISliding::setSlidingCfg(const SlidingCfg& cfg) {
  // Update configuration
  bool needResize = (cfg.frame_size != mSlideCfg.frame_size ||
                     cfg.hop_size != mSlideCfg.hop_size ||
                     cfg.k_of_m_window != mSlideCfg.k_of_m_window);

  mSlideCfg = cfg;

  if (needResize) {
    // Reallocate buffers
    mPcmRing.resize(mSlideCfg.frame_size + mSlideCfg.hop_size);
    mFrameBuffer.resize(mSlideCfg.frame_size);
    mOnsetHistory.resize(mSlideCfg.k_of_m_window);

    // Reset state
    mWriteIdx = 0;
    mAccumulated = 0;
    mOnsetHistoryIdx = 0;

    // Reinitialize window
    initWindow();
  } else if (cfg.window != mSlideCfg.window) {
    // Just update window coefficients
    initWindow();
  }
}

SoundToMIDIMono& SoundToMIDISliding::mono() {
  if (!mMonoEngine) {
    // Engine is polyphonic - this is a usage error
    static SoundToMIDI dummyCfg;
    static SoundToMIDIMono dummyEngine(dummyCfg);
    return dummyEngine;
  }
  return *mMonoEngine;
}

SoundToMIDIPoly& SoundToMIDISliding::poly() {
  if (!mPolyEngine) {
    // Engine is monophonic - this is a usage error
    static SoundToMIDI dummyCfg;
    static SoundToMIDIPoly dummyEngine(dummyCfg);
    return dummyEngine;
  }
  return *mPolyEngine;
}

// K-of-M filtering for monophonic mode
void SoundToMIDISliding::handleMonoNoteOn(uint8_t note, uint8_t velocity) {
  if (!mSlideCfg.enable_k_of_m) {
    // K-of-M disabled - pass through directly
    if (mUserNoteOn) {
      mUserNoteOn(note, velocity);
    }
    return;
  }

  // For mono, track onset across frames using K-of-M
  bool onset_detected = checkKofMOnset(true);

  if (onset_detected && mUserNoteOn) {
    mUserNoteOn(note, velocity);
  }
}

void SoundToMIDISliding::handleMonoNoteOff(uint8_t note) {
  if (!mSlideCfg.enable_k_of_m) {
    // K-of-M disabled - pass through directly
    if (mUserNoteOff) {
      mUserNoteOff(note);
    }
    return;
  }

  // For offs, also check K-of-M but inverted (require K silent frames)
  bool offset_detected = checkKofMOnset(false);

  if (!offset_detected && mUserNoteOff) {
    mUserNoteOff(note);
  }
}

// K-of-M filtering for polyphonic mode
void SoundToMIDISliding::handlePolyNoteOn(uint8_t note, uint8_t velocity) {
  if (!mSlideCfg.enable_k_of_m) {
    // K-of-M disabled - pass through directly
    if (mUserNoteOn) {
      mUserNoteOn(note, velocity);
    }
    return;
  }

  auto& state = mNoteStates[note];

  // Increment onset count for this note
  state.onsetCount++;
  state.offCount = 0;  // Reset off count

  // Check if we've seen this note in K of the last M frames
  if (state.onsetCount >= (int)mSlideCfg.k_of_m_onset && !state.active) {
    // Emit NoteOn
    if (mUserNoteOn) {
      mUserNoteOn(note, velocity);
    }
    state.active = true;
  }

  // Decay onset counts for all notes (simulating sliding window)
  // Keep counts within reasonable bounds
  if (state.onsetCount > (int)mSlideCfg.k_of_m_window) {
    state.onsetCount = mSlideCfg.k_of_m_window;
  }
}

void SoundToMIDISliding::handlePolyNoteOff(uint8_t note) {
  if (!mSlideCfg.enable_k_of_m) {
    // K-of-M disabled - pass through directly
    if (mUserNoteOff) {
      mUserNoteOff(note);
    }
    return;
  }

  auto it = mNoteStates.find(note);
  if (it == mNoteStates.end()) {
    return;  // Note wasn't tracked
  }

  auto& state = it->second;

  // Increment off count for this note
  state.offCount++;
  state.onsetCount = 0;  // Reset onset count

  // Check if we've NOT seen this note in K of the last M frames
  if (state.offCount >= (int)mSlideCfg.k_of_m_onset && state.active) {
    // Emit NoteOff
    if (mUserNoteOff) {
      mUserNoteOff(note);
    }
    state.active = false;

    // Clean up state for this note
    mNoteStates.erase(it);
  }

  // Decay off counts (keep within bounds)
  if (state.offCount > (int)mSlideCfg.k_of_m_window) {
    state.offCount = mSlideCfg.k_of_m_window;
  }
}

} // namespace fl
