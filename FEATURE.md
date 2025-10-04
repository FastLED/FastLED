Polyphonic MIDI Detection: Research and Code Enhancements
Challenges of Polyphonic Pitch Detection

Detecting multiple simultaneous musical pitches (polyphonic pitch detection) is significantly more difficult than monophonic pitch detection. As one signal processing expert succinctly put it, "monophonic pitch detection is hard enough; polyphonic is a whole dimension harder"
dsp.stackexchange.com
. Unlike single-pitch (monophonic) detection where algorithms like YIN or autocorrelation can lock onto one fundamental frequency at a time, polyphonic detection must disentangle overlapping harmonics from several notes played together. There is no straightforward, standard algorithm that universally solves this problem
dsp.stackexchange.com
 – it remains an active research area
github.com
. Even commercial solutions like Melodyne’s Direct Note Access (a sophisticated polyphonic pitch editor) and real-time guitar synthesizers (e.g. JamOrigin’s MIDI Guitar) rely on complex, often proprietary methods, and still face challenges with speed and accuracy
forum.loopypro.com
. In summary, polyphonic pitch detection is challenging because multiple tones’ overtone series can mask or mimic each other, making it hard to reliably identify individual fundamentals.

Approaches to Polyphonic Detection

Researchers and developers have explored several approaches to detect multiple pitches simultaneously:

Spectral Peak Analysis: A classic approach is to apply a Fast Fourier Transform (FFT) to the audio frame and analyze the magnitude spectrum for peaks corresponding to musical tones. By “binning the frequencies according to the frequency ranges for all the standard musical pitches”, one can check for the presence of expected peaks and their harmonics
stackoverflow.com
. If a peak and its first few harmonics are all above a noise threshold, the corresponding note is assumed present
stackoverflow.com
. This approach is intuitive and fast, but choosing a proper detection threshold is tricky. Too high a threshold misses quiet notes; too low a threshold causes false positives
stackoverflow.com
. Moreover, different instruments have varying harmonic amplitude patterns (e.g. bass notes often have a weak fundamental and stronger overtones
stackoverflow.com
), which complicates simple peak-threshold methods.

Iterative Fundamental Extraction: Another strategy is iterative estimation and subtraction. The algorithm identifies the strongest pitch (e.g. via autocorrelation or finding the dominant spectral peak) and then “uses a comb filter to filter out that note” from the signal, thereby removing its harmonic structure
dsp.stackexchange.com
. After subtracting the first note, the algorithm repeats the process on the residual signal to find the next note, and so on. This peeling technique can isolate multiple fundamentals sequentially. It tends to work if one pitch is clearly dominant at a time, but it may struggle when several notes have comparable strength or when notes are close in frequency (and thus not cleanly removable). Additionally, comb-filter subtraction can introduce artifacts or fail if the identified fundamental was slightly off (potentially subtracting wrong frequencies).

Harmonic Grouping and Pattern Recognition: Instead of blindly picking peaks, more advanced algorithms group spectral components into clusters belonging to individual notes. For example, methods by Klapuri and others sum or correlate expected harmonic series patterns across the spectrum to decide the best set of fundamental frequencies that explain the observed peaks
dsp.stackexchange.com
. These methods often involve searching for combinations of fundamentals whose harmonics best match the spectrum, sometimes using machine learning or heuristic search. Techniques like Harmonic Product Spectrum (HPS) (which multiplies downsampled spectra to emphasize fundamentals) and parametric model-fitting (e.g. using Gaussian Mixture Models or MUSIC/ESPRIT frequency estimation
dsp.stackexchange.com
) have been investigated. Such methods can be more robust but are computationally heavier, sometimes unsuitable for real-time use without optimization
dsp.stackexchange.com
.

Machine Learning & Specialized Algorithms: Modern research has also introduced data-driven approaches (e.g. neural networks trained for multi-pitch or chord detection) and specialized signal processing techniques. For instance, one open-source project uses complex resonators in a combined time-frequency analysis for guitar chords
github.com
github.com
. It achieves impressive latency (~15–20ms) and accuracy by exploiting guitar string physics and detecting partial vibrations even before a full waveform cycle is complete
github.com
. However, such sophisticated solutions are complex to implement and often tailored to specific instruments or contexts. In practice, many real-time systems opt for simpler spectral or iterative methods and accept some trade-offs in accuracy. As noted by one developer, fully enabling polyphonic detection can make “note detection glitchy” in real-time transcription scenarios
forum.loopypro.com
, underscoring the practical challenges.

Implementing a Polyphonic Detector in Code

To enhance the provided pitch-to-MIDI code for polyphonic detection, we propose integrating an FFT-based multi-pitch detection mechanism and extending the logic to handle multiple simultaneous notes. The overall strategy is:

Frequency Spectrum Analysis: For each audio frame, compute its FFT to obtain the magnitude spectrum. Identify significant local maxima (peaks) in the spectrum within the expected pitch range (between configured fmin and fmax). We will ignore the DC component and any frequency bins outside the target range.

Adaptive Thresholding: Determine a threshold to separate real note peaks from noise. A robust choice is to use the median magnitude of the spectrum (or a multiple of it) as a baseline
stackoverflow.com
. Only peaks with magnitudes notably above this median (indicating they stand out from background noise/harmonics) are considered candidates. This helps adapt to different volume levels dynamically.

Harmonic Grouping: Filter out peaks that are likely harmonics of lower-frequency peaks. For each candidate peak (starting from the lowest frequency), check if higher-frequency peaks occur at integer multiples of that frequency. If so, those peaks are marked as belonging to the same note’s harmonic series rather than independent fundamentals. For example, if we detected a peak at 130 Hz and another at ~260 Hz (roughly 2×130), the 260 Hz peak is probably the second harmonic of the 130 Hz fundamental, not a new note. By grouping and eliminating harmonics, we obtain a set of distinct fundamental frequencies corresponding to separate notes. Note: This simple harmonic grouping may mistakenly merge notes that are exact octaves apart (since one’s fundamental can align with another’s harmonic), but in practice such scenarios are relatively rare or can be mitigated by context.

Convert to MIDI Notes: Convert each remaining fundamental frequency to the nearest MIDI note number using the provided hz_to_midi function. These MIDI notes represent the detected pitches for that frame. For each detected note, we can also carry along its amplitude (e.g. the magnitude of its fundamental frequency bin) to estimate a MIDI velocity.

Multiple Note Tracking: Extend the state logic to handle multiple concurrent notes. We maintain a boolean array (size 128 for all MIDI notes) to track which notes are currently “active” (sustained) and arrays of counters to implement hysteresis for note-on and note-off events. Similar to the original single-note logic, a note is only confirmed “ON” if it appears in several consecutive frames (determined by note_hold_frames) to avoid spurious one-frame detections. Likewise, a note turns “OFF” after it disappears (falls below detection threshold) for a number of frames (silence_frames_off). This prevents rapid flicker on/off if a note briefly dips below threshold. Each note has its own counters, so multiple notes can overlap in time with independent lifetimes.

Velocity Estimation: Compute a MIDI velocity for each note-on event based on the note’s amplitude. We can derive a note’s relative loudness from the FFT peak magnitude of its fundamental (normalized to the strongest note in the frame, and scaled by the overall frame energy). The original amp_to_velocity function mapped the frame’s RMS to velocity; we adapt this by weighting each note’s velocity according to its peak strength relative to the loudest note and the global gain setting. This ensures that louder notes produce higher velocities, while softer concurrently played notes get lower velocities. (Exact tuning of this scaling can be adjusted; the goal is to reflect dynamics for each note).

Using this approach, the code will detect chords or multiple simultaneous notes and emit corresponding MIDI note-on/off events. Below is the updated code snippet with the polyphonic detection integrated. We added inline documentation for clarity. This implementation assumes the frame size is a power of 2 (for FFT) and uses the existing configuration parameters (like frame_size, sample_rate_hz, fmin_hz, fmax_hz, note_hold_frames, silence_frames_off, vel_gain, vel_floor) from the PitchToMIDI config structure. A new boolean config field polyphonic is introduced to enable/disable polyphonic mode. When polyphonic is true, the code uses the new multi-note detection logic; otherwise it retains the original monophonic behavior for single-note sources.





SOURCE CODE SAMPLE





#include "pitch_to_midi.h"
#include "fl/math.h"
#include <vector>
#include <complex>
#include <cmath>

namespace fl {

// ---------- Helper structures and functions ----------
namespace {

// Structure to hold a detected note and its peak magnitude
struct NotePeak {
  int midi;
  float magnitude;
};

// Cooley-Tukey FFT (in-place, radix-2). Requires N to be a power of 2.
inline void fft(const float* input, int N, std::vector<std::complex<float>>& out) {
  out.resize(N);
  // Copy input into complex output array (imag = 0)
  for (int i = 0; i < N; ++i) {
    out[i] = std::complex<float>(input[i], 0.0f);
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
      std::swap(out[i], out[j]);
    }
  }
  // FFT butterfly stages
  for (int len = 2; len <= N; len <<= 1) {
    float theta = -2.0f * M_PI / (float)len;
    std::complex<float> wlen(std::cos(theta), std::sin(theta));
    for (int i = 0; i < N; i += len) {
      std::complex<float> w(1.0f, 0.0f);
      for (int j = 0; j < len/2; ++j) {
        std::complex<float> u = out[i+j];
        std::complex<float> v = out[i+j + len/2] * w;
        out[i+j]           = u + v;
        out[i+j + len/2]   = u - v;
        w *= wlen;
      }
    }
  }
}

// Detect multiple fundamental frequencies in the frame using FFT.
// Returns a list of NotePeak (MIDI note and its fundamental magnitude).
inline std::vector<NotePeak> detectPolyphonicNotes(const float* x, int N, float sr, float fmin, float fmax) {
  // Compute FFT of input frame (assume N is power of 2 for simplicity)
  std::vector<std::complex<float>> spectrum;
  fft(x, N, spectrum);
  int halfN = N / 2;
  // Magnitude spectrum (only need [0, N/2] for real input)
  std::vector<float> mag(halfN);
  for (int i = 0; i < halfN; ++i) {
    float re = spectrum[i].real();
    float im = spectrum[i].imag();
    mag[i] = std::sqrt(re*re + im*im);
  }

  // Determine frequency bin range for fmin to fmax
  int binMin = (int)std::floor(fmin * N / sr);
  int binMax = (int)std::ceil(fmax * N / sr);
  if (binMin < 1) binMin = 1;               // start from 1 to skip DC
  if (binMax > halfN - 1) binMax = halfN - 1;

  // Calculate median magnitude in [binMin, binMax] to use as noise threshold
  std::vector<float> magslice;
  magslice.reserve(binMax - binMin + 1);
  for (int i = binMin; i <= binMax; ++i) {
    magslice.push_back(mag[i]);
  }
  std::nth_element(magslice.begin(), magslice.begin() + magslice.size()/2, magslice.end());
  float medianMag = magslice[magslice.size()/2];
  float threshold = medianMag * 1.5f;  // threshold factor (1.5x median, adjust as needed)

  // Find local peaks above threshold in the specified range
  std::vector<int> peakIndices;
  peakIndices.reserve(16);
  for (int i = binMin + 1; i < binMax; ++i) {
    if (mag[i] > threshold && mag[i] >= mag[i-1] && mag[i] >= mag[i+1]) {
      peakIndices.push_back(i);
    }
  }
  if (peakIndices.empty()) {
    return {};  // no significant peaks found
  }
  // Sort detected peaks by frequency (ascending)
  std::sort(peakIndices.begin(), peakIndices.end());

  // Group peaks by fundamental-harmonic relationship
  std::vector<NotePeak> result;
  std::vector<bool> used(peakIndices.size(), false);
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
      int roundMult = (int)std::floor(ratio + 0.5f);
      if (roundMult >= 2 && std::fabs(ratio - roundMult) < 0.1f) {
        used[q] = true;
      }
    }
    // Store this fundamental note and its magnitude
    result.push_back({midiNote, peakMag});
  }
  return result;
}

inline int hz_to_midi(float f) {
  // Convert frequency (Hz) to MIDI note number (69 = A4 = 440 Hz).
  return (int)lroundf(69.0f + 12.0f * (std::logf(f / 440.0f) / std::logf(2.0f)));
}

inline float clamp01(float v) {
  return v < 0 ? 0.0f : (v > 1.0f ? 1.0f : v);
}

inline float compute_rms(const float* x, int n) {
  double acc = 0.0;
  for (int i = 0; i < n; ++i) {
    acc += (double)x[i] * x[i];
  }
  return std::sqrt(acc / (double)n);
}

inline uint8_t amp_to_velocity(float rms, float gain, uint8_t floorV) {
  // Map amplitude (RMS) to MIDI velocity using a simple linear scale.
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

} // namespace (anonymous)

// ---------- PitchDetector (monophonic) unchanged ----------
// ... (PitchDetector::PitchDetector constructor and PitchDetector::detect as in original code) ...

// ---------- PitchToMIDIEngine ----------
PitchToMIDIEngine::PitchToMIDIEngine(const PitchToMIDI& cfg)
    : _cfg(cfg), _noteOnFrames(0), _silenceFrames(0), _currentNote(-1),
      _candidateNote(-1), _candidateHoldFrames(0),
      _historyIndex(0), _historyCount(0) {
  // Initialize history buffer for monophonic median filter
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

void PitchToMIDIEngine::processFrame(const float* frame, int n) {
  if (!frame || n != _cfg.frame_size) return;
  // Compute loudness (RMS) of the frame
  const float rms = compute_rms(frame, n);
  if (_cfg.polyphonic) {
    // --- Polyphonic pitch detection branch ---
    bool voiced = (rms > _cfg.rms_gate);
    if (voiced) {
      // Get all detected note peaks in this frame
      std::vector<NotePeak> notes = detectPolyphonicNotes(frame, n, _cfg.sample_rate_hz,
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
      // Median filter for note stability
      _noteHistory[_historyIndex] = rawNote;
      _historyIndex = (_historyIndex + 1) % MAX_MEDIAN_SIZE;
      if (_historyCount < MAX_MEDIAN_SIZE) _historyCount++;
      const int note = getMedianNote();
      if (_currentNote < 0) {
        // Was silence, now a note candidate
        if (++_noteOnFrames >= _cfg.note_hold_frames) {
          uint8_t vel = amp_to_velocity(rms, _cfg.vel_gain, _cfg.vel_floor);
          if (onNoteOn) onNoteOn((uint8_t)note, vel);
          _currentNote = note;
          _noteOnFrames = 0;
          _silenceFrames = 0;
          _candidateNote = -1;
          _candidateHoldFrames = 0;
        }
      } else {
        // Already had a note active
        int dn = noteDelta(note, _currentNote);
        if (dn >= _cfg.note_change_semitone_threshold) {
          // Pitch changed significantly -> candidate for new note
          if (note == _candidateNote) {
            if (++_candidateHoldFrames >= _cfg.note_change_hold_frames) {
              // Confirm note change
              if (onNoteOff) onNoteOff((uint8_t)_currentNote);
              uint8_t vel = amp_to_velocity(rms, _cfg.vel_gain, _cfg.vel_floor);
              if (onNoteOn) onNoteOn((uint8_t)note, vel);
              _currentNote = note;
              _candidateNote = -1;
              _candidateHoldFrames = 0;
            }
          } else {
            // New candidate note (start counting)
            _candidateNote = note;
            _candidateHoldFrames = 1;
          }
        } else {
          // Note difference is small -> consider it the same note (no change)
          _candidateNote = -1;
          _candidateHoldFrames = 0;
        }
        _noteOnFrames = 0;
        _silenceFrames = 0;
      }
    } else {
      // No voiced pitch detected (noise/silence)
      _noteOnFrames = 0;
      _candidateNote = -1;
      _candidateHoldFrames = 0;
      if (_currentNote >= 0) {
        if (++_silenceFrames >= _cfg.silence_frames_off) {
          if (onNoteOff) onNoteOff((uint8_t)_currentNote);
          _currentNote = -1;
          _silenceFrames = 0;
          // Clear history on complete silence
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
