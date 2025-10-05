/*  FastLED - Real-Time Beat Detection for EDM (Implementation)
    ---------------------------------------------------------
    Implements onset detection and beat tracking algorithms optimized
    for Electronic Dance Music (EDM) on embedded platforms (ESP32).

    License: MIT (same spirit as FastLED)
*/

#include "beat_detector.h"

#if SKETCH_HAS_LOTS_OF_MEMORY

#include "fl/math.h"
#include "fl/algorithm.h"
#include "fl/printf.h"
#include <math.h>

namespace fl {

// ---------- Lookup Tables for Optimization ----------

namespace {

// Lookup table for fast log10 approximation
constexpr int LOG10_LUT_SIZE = 256;
float g_log10_lut[LOG10_LUT_SIZE];
bool g_log10_lut_initialized = false;

// Lookup table for Rayleigh weighting
constexpr int RAYLEIGH_LUT_SIZE = 512;
float g_rayleigh_lut[RAYLEIGH_LUT_SIZE];
bool g_rayleigh_lut_initialized = false;

void initializeLookupTables() {
    if (!g_log10_lut_initialized) {
        for (int i = 0; i < LOG10_LUT_SIZE; ++i) {
            float x = static_cast<float>(i) / static_cast<float>(LOG10_LUT_SIZE - 1);
            g_log10_lut[i] = ::log10(x + 1e-10f);  // Avoid log(0)
        }
        g_log10_lut_initialized = true;
    }

    if (!g_rayleigh_lut_initialized) {
        for (int i = 0; i < RAYLEIGH_LUT_SIZE; ++i) {
            float x = static_cast<float>(i) / static_cast<float>(RAYLEIGH_LUT_SIZE / 4);
            g_rayleigh_lut[i] = x * fl::exp(-0.5f * x * x);
        }
        g_rayleigh_lut_initialized = true;
    }
}

// Fast log10 lookup
inline float fastLog10(float x) {
    if (x <= 0.0f) return -100.0f;
    if (x >= 1.0f) return ::log10(x);

    int idx = static_cast<int>(x * (LOG10_LUT_SIZE - 1));
    if (idx >= LOG10_LUT_SIZE) idx = LOG10_LUT_SIZE - 1;
    return g_log10_lut[idx];
}

// Convert magnitude to dB
inline float magnitudeToDB(float mag) {
    return 20.0f * fastLog10(mag);
}

// Fast Rayleigh weight lookup
inline float fastRayleighWeight(float x) {
    if (x < 0.0f) return 0.0f;
    int idx = static_cast<int>(x * (RAYLEIGH_LUT_SIZE / 4));
    if (idx >= RAYLEIGH_LUT_SIZE) {
        // Beyond LUT range, use exponential decay
        return x * fl::exp(-0.5f * x * x);
    }
    return g_rayleigh_lut[idx];
}

// Cooley-Tukey FFT (in-place, radix-2). Requires N to be a power of 2.
// Computes the FFT and returns magnitude spectrum.
inline void fft_magnitude(const float* input, int N, float* magnitude_out) {
    // Temporary arrays for real and imaginary parts
    static float temp_real[2048];
    static float temp_imag[2048];

    if (N > 2048) {
        // FFT size too large, zero output
        for (int i = 0; i < N / 2; ++i) {
            magnitude_out[i] = 0.0f;
        }
        return;
    }

    // Copy input into temp arrays
    for (int i = 0; i < N; ++i) {
        temp_real[i] = input[i];
        temp_imag[i] = 0.0f;
    }

    // Bit-reversal permutation
    int j = 0;
    for (int i = 0; i < N - 1; ++i) {
        if (i < j) {
            // Swap real parts
            float tmp = temp_real[i];
            temp_real[i] = temp_real[j];
            temp_real[j] = tmp;
            // Swap imaginary parts
            tmp = temp_imag[i];
            temp_imag[i] = temp_imag[j];
            temp_imag[j] = tmp;
        }
        int k = N >> 1;
        while (k <= j) {
            j -= k;
            k >>= 1;
        }
        j += k;
    }

    // FFT butterfly stages
    for (int len = 2; len <= N; len <<= 1) {
        float theta = -2.0f * M_PI / static_cast<float>(len);
        float wlen_re = ::cos(theta);
        float wlen_im = ::sin(theta);
        for (int i = 0; i < N; i += len) {
            float w_re = 1.0f;
            float w_im = 0.0f;
            for (int k = 0; k < len / 2; ++k) {
                int idx_even = i + k;
                int idx_odd = i + k + len / 2;

                float t_re = w_re * temp_real[idx_odd] - w_im * temp_imag[idx_odd];
                float t_im = w_re * temp_imag[idx_odd] + w_im * temp_real[idx_odd];

                temp_real[idx_odd] = temp_real[idx_even] - t_re;
                temp_imag[idx_odd] = temp_imag[idx_even] - t_im;
                temp_real[idx_even] = temp_real[idx_even] + t_re;
                temp_imag[idx_even] = temp_imag[idx_even] + t_im;

                float w_re_new = w_re * wlen_re - w_im * wlen_im;
                float w_im_new = w_re * wlen_im + w_im * wlen_re;
                w_re = w_re_new;
                w_im = w_im_new;
            }
        }
    }

    // Compute magnitude spectrum (only need first N/2 bins for real input)
    for (int i = 0; i < N / 2; ++i) {
        float real = temp_real[i];
        float imag = temp_imag[i];
        magnitude_out[i] = ::sqrt(real * real + imag * imag);
    }
}

} // anonymous namespace

// ---------- Mel Filterbank Computation ----------

/// @brief Compute mel filterbank center frequencies
/// @param num_bands Number of mel bands
/// @param fmin Minimum frequency in Hz
/// @param fmax Maximum frequency in Hz
/// @param sample_rate Sample rate in Hz
/// @param fft_size FFT size
/// @return Vector of bin ranges for each mel band [start_bin, end_bin]
fl::vector<fl::pair<int, int>> computeMelBands(int num_bands, float fmin, float fmax,
                                                float sample_rate, int fft_size) {
    fl::vector<fl::pair<int, int>> bands;
    bands.reserve(num_bands);

    // Convert Hz to Mel scale: mel = 2595 * log10(1 + f/700)
    auto hzToMel = [](float hz) -> float {
        return 2595.0f * ::log10(1.0f + hz / 700.0f);
    };

    // Convert Mel to Hz: hz = 700 * (10^(mel/2595) - 1)
    auto melToHz = [](float mel) -> float {
        return 700.0f * ::pow(10.0f, mel / 2595.0f) - 1.0f;
    };

    float mel_min = hzToMel(fmin);
    float mel_max = hzToMel(fmax);
    float mel_step = (mel_max - mel_min) / (num_bands + 1);

    // Compute bin ranges for each mel band
    for (int i = 0; i < num_bands; ++i) {
        float mel_center = mel_min + (i + 1) * mel_step;
        float mel_low = mel_center - mel_step;
        float mel_high = mel_center + mel_step;

        float hz_low = melToHz(mel_low);
        float hz_high = melToHz(mel_high);

        int bin_low = freqToBin(hz_low, fft_size, sample_rate);
        int bin_high = freqToBin(hz_high, fft_size, sample_rate);

        // Clamp to valid range
        bin_low = MAX(0, MIN(bin_low, fft_size / 2));
        bin_high = MAX(0, MIN(bin_high, fft_size / 2));

        if (bin_low < bin_high) {
            bands.push_back(fl::make_pair(bin_low, bin_high));
        }
    }

    return bands;
}

/// @brief Apply triangular mel filterbank to spectrum
/// @param spectrum Input magnitude spectrum
/// @param spectrum_size Size of spectrum
/// @param mel_bands Mel band bin ranges
/// @param out Output mel-filtered magnitudes
void applyMelFilterbank(const float* spectrum, int spectrum_size,
                        const fl::vector<fl::pair<int, int>>& mel_bands,
                        float* out) {
    for (size_t b = 0; b < mel_bands.size(); ++b) {
        int bin_low = mel_bands[b].first;
        int bin_high = mel_bands[b].second;
        int bin_center = (bin_low + bin_high) / 2;

        float sum = 0.0f;
        float weight_sum = 0.0f;

        // Triangular weighting
        for (int k = bin_low; k <= bin_high && k < spectrum_size; ++k) {
            float weight;
            if (k <= bin_center) {
                weight = static_cast<float>(k - bin_low) / static_cast<float>(bin_center - bin_low + 1);
            } else {
                weight = static_cast<float>(bin_high - k) / static_cast<float>(bin_high - bin_center + 1);
            }
            sum += spectrum[k] * weight;
            weight_sum += weight;
        }

        out[b] = (weight_sum > 0.0f) ? (sum / weight_sum) : 0.0f;
    }
}

// ---------- OnsetDetectionProcessor Implementation ----------

OnsetDetectionProcessor::OnsetDetectionProcessor(const BeatDetectorConfig& cfg)
    : _cfg(cfg)
    , _historyIndex(0)
    , _historyCount(0)
    , _lastEnergy(0.0f)
{
    reset();
    initializeLookupTables();
}

void OnsetDetectionProcessor::reset() {
    for (int i = 0; i < MAX_HISTORY; ++i) {
        for (int j = 0; j < MAX_SPECTRUM_SIZE; ++j) {
            _spectrumHistory[i][j] = 0.0f;
        }
    }
    for (int i = 0; i < MAX_SPECTRUM_SIZE; ++i) {
        _runningMax[i] = 0.0f;
    }
    _historyIndex = 0;
    _historyCount = 0;
    _lastEnergy = 0.0f;
}

void OnsetDetectionProcessor::setConfig(const BeatDetectorConfig& cfg) {
    _cfg = cfg;
}

float OnsetDetectionProcessor::processTimeDomain(const float* frame, int n) {
    // Simple energy-based ODF
    float energy = 0.0f;
    for (int i = 0; i < n; ++i) {
        energy += frame[i] * frame[i];
    }

    float novelty = MAX(0.0f, energy - _lastEnergy);
    _lastEnergy = energy;
    return novelty;
}

float OnsetDetectionProcessor::processSpectrum(const float* magnitude_spectrum, int spectrum_size) {
    if (spectrum_size > MAX_SPECTRUM_SIZE) {
        spectrum_size = MAX_SPECTRUM_SIZE;
    }

    // Copy spectrum to current history slot
    float* current = _spectrumHistory[_historyIndex];
    for (int i = 0; i < spectrum_size; ++i) {
        current[i] = magnitude_spectrum[i];
    }

    // Apply adaptive whitening if enabled
    if (_cfg.adaptive_whitening) {
        applyAdaptiveWhitening(current, spectrum_size);
    }

    // Apply log compression if enabled
    if (_cfg.log_compression) {
        applyLogCompression(current, spectrum_size);
    }

    float odf_value = 0.0f;

    // Compute ODF based on selected type
    switch (_cfg.odf_type) {
        case OnsetDetectionFunction::Energy:
            // Already handled in processTimeDomain
            break;

        case OnsetDetectionFunction::SpectralFlux:
            odf_value = computeSpectralFlux(current, spectrum_size);
            break;

        case OnsetDetectionFunction::SuperFlux:
            odf_value = computeSuperFlux(current, spectrum_size);
            break;

        case OnsetDetectionFunction::HFC:
            odf_value = computeHFC(current, spectrum_size);
            break;

        case OnsetDetectionFunction::ComplexDomain:
            // Complex domain requires phase info, fall back to spectral flux
            odf_value = computeSpectralFlux(current, spectrum_size);
            break;

        case OnsetDetectionFunction::MultiBand:
            odf_value = computeMultiBand(current, spectrum_size);
            break;
    }

    // Update history
    _historyIndex = (_historyIndex + 1) % MAX_HISTORY;
    if (_historyCount < MAX_HISTORY) {
        _historyCount++;
    }

    return odf_value;
}

float OnsetDetectionProcessor::computeSpectralFlux(const float* mag, int size) {
    if (_historyCount < 1) {
        return 0.0f;
    }

    // Get previous spectrum
    int prev_idx = (_historyIndex - 1 + MAX_HISTORY) % MAX_HISTORY;
    const float* prev = _spectrumHistory[prev_idx];

    // Compute positive difference
    float flux = 0.0f;
    for (int k = 0; k < size; ++k) {
        float diff = mag[k] - prev[k];
        flux += MAX(0.0f, diff);
    }

    return flux;
}

float OnsetDetectionProcessor::computeSuperFlux(const float* mag, int size) {
    int mu = _cfg.superflux_mu;
    if (_historyCount < mu) {
        return 0.0f;
    }

    // Get spectrum from mu frames ago
    int delayed_idx = (_historyIndex - mu + MAX_HISTORY) % MAX_HISTORY;
    float* delayed = _spectrumHistory[delayed_idx];

    // Apply maximum filter to delayed spectrum
    float filtered[MAX_SPECTRUM_SIZE];
    for (int i = 0; i < size; ++i) {
        filtered[i] = delayed[i];
    }
    applyMaximumFilter(filtered, size, _cfg.max_filter_radius);

    // Compute positive difference with filtered delayed spectrum
    float flux = 0.0f;
    for (int k = 0; k < size; ++k) {
        float diff = mag[k] - filtered[k];
        flux += MAX(0.0f, diff);
    }

    return flux;
}

float OnsetDetectionProcessor::computeHFC(const float* mag, int size) {
    float hfc = 0.0f;
    for (int k = 0; k < size; ++k) {
        hfc += k * mag[k];
    }
    return hfc;
}

float OnsetDetectionProcessor::computeMultiBand(const float* mag, int size) {
    if (_historyCount < 1) {
        return 0.0f;
    }

    // Get previous spectrum
    int prev_idx = (_historyIndex - 1 + MAX_HISTORY) % MAX_HISTORY;
    const float* prev = _spectrumHistory[prev_idx];

    float total_novelty = 0.0f;

    // Process each frequency band
    for (const auto& band : _cfg.bands) {
        int bin_low = freqToBin(band.low_hz, _cfg.fft_size, _cfg.sample_rate_hz);
        int bin_high = freqToBin(band.high_hz, _cfg.fft_size, _cfg.sample_rate_hz);

        bin_low = MAX(0, MIN(bin_low, size - 1));
        bin_high = MAX(0, MIN(bin_high, size));

        float band_flux = 0.0f;
        for (int k = bin_low; k < bin_high; ++k) {
            float diff = mag[k] - prev[k];
            band_flux += MAX(0.0f, diff);
        }

        total_novelty += band.weight * band_flux;
    }

    return total_novelty;
}

void OnsetDetectionProcessor::applyAdaptiveWhitening(float* mag, int size) {
    // Update running maximum for each bin
    for (int k = 0; k < size; ++k) {
        _runningMax[k] = MAX(mag[k], _cfg.whitening_alpha * _runningMax[k]);

        // Normalize by running maximum
        if (_runningMax[k] > 1e-6f) {
            mag[k] = mag[k] / _runningMax[k];
        }
    }
}

void OnsetDetectionProcessor::applyLogCompression(float* mag, int size) {
    for (int k = 0; k < size; ++k) {
        mag[k] = ::log(1.0f + mag[k]);
    }
}

void OnsetDetectionProcessor::applyMaximumFilter(float* mag, int size, int radius) {
    if (radius <= 0) return;

    float temp[MAX_SPECTRUM_SIZE];
    for (int i = 0; i < size; ++i) {
        temp[i] = mag[i];
    }

    // Apply maximum filter
    for (int i = 0; i < size; ++i) {
        float max_val = temp[i];
        for (int j = MAX(0, i - radius); j <= MIN(size - 1, i + radius); ++j) {
            max_val = MAX(max_val, temp[j]);
        }
        mag[i] = max_val;
    }
}

int OnsetDetectionProcessor::freqToBin(float freq_hz, int fft_size, float sample_rate) const {
    return static_cast<int>((freq_hz * fft_size) / sample_rate);
}

// ---------- PeakPicker Implementation ----------

PeakPicker::PeakPicker(const BeatDetectorConfig& cfg)
    : _cfg(cfg)
    , _bufferIndex(0)
    , _bufferCount(0)
    , _lastOnsetFrame(0)
{
    reset();
}

void PeakPicker::reset() {
    for (int i = 0; i < MAX_ODF_BUFFER; ++i) {
        _odfBuffer[i] = 0.0f;
        _frameBuffer[i] = 0;
        _timestampBuffer[i] = 0.0f;
    }
    _bufferIndex = 0;
    _bufferCount = 0;
    _lastOnsetFrame = 0;

    // Convert ms to frames
    _preMaxFrames = msToFrames(_cfg.peak_pre_max_ms);
    _postMaxFrames = msToFrames(_cfg.peak_post_max_ms);
    _preAvgFrames = msToFrames(_cfg.peak_pre_avg_ms);
    _postAvgFrames = msToFrames(_cfg.peak_post_avg_ms);
    _minInterOnsetFrames = msToFrames(_cfg.min_inter_onset_ms);
}

void PeakPicker::setConfig(const BeatDetectorConfig& cfg) {
    _cfg = cfg;
    reset();
}

fl::vector<OnsetEvent> PeakPicker::process(float odf_value, uint32_t frame_index, float timestamp_ms) {
    fl::vector<OnsetEvent> onsets;

    // Add to ring buffer
    _odfBuffer[_bufferIndex] = odf_value;
    _frameBuffer[_bufferIndex] = frame_index;
    _timestampBuffer[_bufferIndex] = timestamp_ms;
    _bufferIndex = (_bufferIndex + 1) % MAX_ODF_BUFFER;
    if (_bufferCount < MAX_ODF_BUFFER) {
        _bufferCount++;
    }

    // Need enough history for peak detection
    if (_bufferCount < _preMaxFrames + _postMaxFrames + 1) {
        return onsets;
    }

    // Check center position for peak (delay for post-window)
    int center_idx = wrapIndex(_bufferIndex - _postMaxFrames - 1);
    uint32_t center_frame = _frameBuffer[center_idx];
    float center_odf = _odfBuffer[center_idx];
    float center_timestamp = _timestampBuffer[center_idx];

    // Apply peak picking based on mode
    bool is_peak = false;

    switch (_cfg.peak_mode) {
        case PeakPickingMode::LocalMaximum:
            is_peak = isLocalMaximum(center_idx);
            break;

        case PeakPickingMode::AdaptiveThreshold: {
            float mean = computeLocalMean(center_idx);
            is_peak = isLocalMaximum(center_idx) &&
                      (center_odf >= mean + _cfg.peak_threshold_delta);
            break;
        }

        case PeakPickingMode::SuperFluxPeaks: {
            float mean = computeLocalMean(center_idx);
            is_peak = isLocalMaximum(center_idx) &&
                      (center_odf >= mean + _cfg.peak_threshold_delta) &&
                      meetsMinDistance(center_frame);
            break;
        }
    }

    if (is_peak) {
        OnsetEvent onset;
        onset.frame_index = center_frame;
        onset.timestamp_ms = center_timestamp;
        onset.confidence = center_odf;
        onsets.push_back(onset);
        _lastOnsetFrame = center_frame;
    }

    return onsets;
}

bool PeakPicker::isLocalMaximum(int center_idx) const {
    float center_val = _odfBuffer[center_idx];

    // Check pre-window
    for (int i = 1; i <= _preMaxFrames; ++i) {
        int idx = wrapIndex(center_idx - i);
        if (_odfBuffer[idx] >= center_val) {
            return false;
        }
    }

    // Check post-window
    for (int i = 1; i <= _postMaxFrames; ++i) {
        int idx = wrapIndex(center_idx + i);
        if (_odfBuffer[idx] > center_val) {
            return false;
        }
    }

    return true;
}

float PeakPicker::computeLocalMean(int center_idx) const {
    float sum = 0.0f;
    int count = 0;

    // Pre-window average
    for (int i = 1; i <= _preAvgFrames; ++i) {
        int idx = wrapIndex(center_idx - i);
        sum += _odfBuffer[idx];
        count++;
    }

    // Post-window average (for real-time, set post=0)
    for (int i = 1; i <= _postAvgFrames; ++i) {
        int idx = wrapIndex(center_idx + i);
        sum += _odfBuffer[idx];
        count++;
    }

    return (count > 0) ? (sum / count) : 0.0f;
}

bool PeakPicker::meetsMinDistance(uint32_t frame_index) const {
    return (frame_index - _lastOnsetFrame) >= static_cast<uint32_t>(_minInterOnsetFrames);
}

int PeakPicker::msToFrames(int ms) const {
    float frames_per_ms = _cfg.sample_rate_hz / (_cfg.hop_size * 1000.0f);
    return static_cast<int>(ms * frames_per_ms);
}

int PeakPicker::wrapIndex(int idx) const {
    while (idx < 0) idx += MAX_ODF_BUFFER;
    while (idx >= MAX_ODF_BUFFER) idx -= MAX_ODF_BUFFER;
    return idx;
}

// ---------- TempoTracker Implementation ----------

TempoTracker::TempoTracker(const BeatDetectorConfig& cfg)
    : _cfg(cfg)
    , _odfHistoryIndex(0)
    , _odfHistoryCount(0)
    , _currentBPM(120.0f)
    , _tempoConfidence(0.0f)
    , _periodSamples(0)
    , _beatPhase(0.0f)
    , _lastBeatTime(0.0f)
{
    reset();
    initializeLookupTables();
}

void TempoTracker::reset() {
    for (int i = 0; i < MAX_ODF_HISTORY; ++i) {
        _odfHistory[i] = 0.0f;
    }
    for (int i = 0; i < MAX_ACF_LAG; ++i) {
        _combFilter[i] = 0.0f;
    }
    _odfHistoryIndex = 0;
    _odfHistoryCount = 0;
    _currentBPM = 120.0f;
    _tempoConfidence = 0.0f;
    _periodSamples = 0;
    _beatPhase = 0.0f;
    _lastBeatTime = 0.0f;
}

void TempoTracker::setConfig(const BeatDetectorConfig& cfg) {
    _cfg = cfg;
}

void TempoTracker::addOnset(const OnsetEvent& onset) {
    // For tempo tracking from onsets (not currently used)
}

void TempoTracker::addODFValue(float odf_value, float timestamp_ms) {
    if (_cfg.tempo_tracker == TempoTrackerType::None) {
        return;
    }

    // Add to history
    _odfHistory[_odfHistoryIndex] = odf_value;
    _odfHistoryIndex = (_odfHistoryIndex + 1) % MAX_ODF_HISTORY;
    if (_odfHistoryCount < MAX_ODF_HISTORY) {
        _odfHistoryCount++;
    }

    // Update tempo estimate periodically (every ~0.5s)
    int update_interval = static_cast<int>(0.5f * _cfg.sample_rate_hz / _cfg.hop_size);
    if (_odfHistoryCount % update_interval == 0) {
        updateTempoEstimate();
    }
}

TempoEstimate TempoTracker::getTempo() const {
    TempoEstimate est;
    est.bpm = _currentBPM;
    est.confidence = _tempoConfidence;
    est.period_samples = _periodSamples;
    return est;
}

fl::vector<BeatEvent> TempoTracker::checkBeat(float timestamp_ms) {
    fl::vector<BeatEvent> beats;

    if (_periodSamples <= 0 || _cfg.tempo_tracker == TempoTrackerType::None) {
        return beats;
    }

    // Convert timestamp to samples
    float current_time_samples = (timestamp_ms / 1000.0f) * _cfg.sample_rate_hz;

    // Check if we've crossed a beat boundary
    float time_since_last_beat = current_time_samples - _lastBeatTime;
    if (time_since_last_beat >= _periodSamples) {
        BeatEvent beat;
        beat.frame_index = static_cast<uint32_t>(current_time_samples / _cfg.hop_size);
        beat.timestamp_ms = timestamp_ms;
        beat.bpm = _currentBPM;
        beat.confidence = _tempoConfidence;
        beat.phase = 0.0f;  // TODO: Implement phase tracking

        beats.push_back(beat);
        _lastBeatTime = current_time_samples;
    }

    return beats;
}

void TempoTracker::updateTempoEstimate() {
    if (_cfg.tempo_tracker == TempoTrackerType::None) {
        return;
    }

    // Compute autocorrelation of ODF
    float acf[MAX_ACF_LAG];
    int max_lag = MIN(MAX_ACF_LAG, _odfHistoryCount / 2);
    computeAutocorrelation(acf, max_lag);

    // Apply comb filter if enabled
    if (_cfg.tempo_tracker == TempoTrackerType::CombFilter) {
        applyCombFilter(acf, max_lag);
    }

    // Apply Rayleigh weighting to favor typical tempos
    applyRayleighWeighting(acf, max_lag);

    // Find peak lag
    int peak_lag = findPeakLag(acf, max_lag);
    if (peak_lag > 0 && peak_lag < max_lag) {
        _currentBPM = lagToBPM(peak_lag);
        _periodSamples = bpmToSamples(_currentBPM);
        _tempoConfidence = acf[peak_lag];
    }
}

void TempoTracker::computeAutocorrelation(float* acf, int max_lag) {
    // Compute autocorrelation using sliding window
    int window_size = MIN(_odfHistoryCount,
                              static_cast<int>(_cfg.tempo_acf_window_sec * _cfg.sample_rate_hz / _cfg.hop_size));

    for (int lag = 0; lag < max_lag; ++lag) {
        float sum = 0.0f;
        int count = 0;

        for (int i = 0; i < window_size - lag; ++i) {
            int idx1 = (_odfHistoryIndex - window_size + i + MAX_ODF_HISTORY) % MAX_ODF_HISTORY;
            int idx2 = (_odfHistoryIndex - window_size + i + lag + MAX_ODF_HISTORY) % MAX_ODF_HISTORY;
            sum += _odfHistory[idx1] * _odfHistory[idx2];
            count++;
        }

        acf[lag] = (count > 0) ? (sum / count) : 0.0f;
    }

    // Normalize
    if (acf[0] > 0.0f) {
        for (int lag = 0; lag < max_lag; ++lag) {
            acf[lag] /= acf[0];
        }
    }
}

void TempoTracker::applyCombFilter(float* acf, int max_lag) {
    // Apply comb filter to emphasize periodic structure
    float filtered[MAX_ACF_LAG];
    for (int i = 0; i < max_lag; ++i) {
        filtered[i] = acf[i];
    }

    for (int lag = 1; lag < max_lag; ++lag) {
        float sum = acf[lag];
        int count = 1;

        // Add harmonics (multiples of current lag)
        for (int mult = 2; mult * lag < max_lag; ++mult) {
            sum += acf[mult * lag];
            count++;
        }

        filtered[lag] = sum / count;
    }

    // Copy back
    for (int i = 0; i < max_lag; ++i) {
        acf[i] = filtered[i];
    }
}

void TempoTracker::applyRayleighWeighting(float* acf, int max_lag) {
    // Weight by Rayleigh distribution centered on target tempo
    for (int lag = 0; lag < max_lag; ++lag) {
        float bpm = lagToBPM(lag);
        float x = bpm / _cfg.tempo_rayleigh_sigma;
        acf[lag] *= fastRayleighWeight(x);
    }
}

int TempoTracker::findPeakLag(const float* acf, int max_lag) const {
    // Find lag within tempo range with maximum ACF value
    int min_lag = bpmToLag(_cfg.tempo_max_bpm);
    int max_lag_constrained = bpmToLag(_cfg.tempo_min_bpm);

    min_lag = MAX(1, MIN(min_lag, max_lag));
    max_lag_constrained = MIN(max_lag_constrained, max_lag);

    int peak_lag = min_lag;
    float peak_val = acf[min_lag];

    for (int lag = min_lag; lag < max_lag_constrained; ++lag) {
        if (acf[lag] > peak_val) {
            peak_val = acf[lag];
            peak_lag = lag;
        }
    }

    return peak_lag;
}

int TempoTracker::bpmToLag(float bpm) const {
    int period_samples = bpmToSamples(bpm);
    return period_samples / _cfg.hop_size;
}

float TempoTracker::lagToBPM(int lag) const {
    if (lag <= 0) {
        return 0.0f;  // Avoid division by zero
    }
    int period_samples = lag * _cfg.hop_size;
    if (period_samples <= 0) {
        return 0.0f;  // Avoid division by zero
    }
    return samplesToBPM(period_samples, _cfg.sample_rate_hz);
}

int TempoTracker::bpmToSamples(float bpm) const {
    return fl::bpmToSamples(bpm, _cfg.sample_rate_hz);
}

// ---------- BeatDetector Implementation ----------

BeatDetector::BeatDetector(const BeatDetectorConfig& cfg)
    : _cfg(cfg)
    , _odfProcessor(cfg)
    , _peakPicker(cfg)
    , _tempoTracker(cfg)
    , _frameCount(0)
    , _onsetCount(0)
    , _beatCount(0)
    , _currentODF(0.0f)
    , _lastTempoBPM(0.0f)
{
    initializeLookupTables();
}

void BeatDetector::reset() {
    _odfProcessor.reset();
    _peakPicker.reset();
    _tempoTracker.reset();
    _frameCount = 0;
    _onsetCount = 0;
    _beatCount = 0;
    _currentODF = 0.0f;
    _lastTempoBPM = 0.0f;
}

void BeatDetector::setConfig(const BeatDetectorConfig& cfg) {
    _cfg = cfg;
    _odfProcessor.setConfig(cfg);
    _peakPicker.setConfig(cfg);
    _tempoTracker.setConfig(cfg);
}

void BeatDetector::processFrame(const float* frame, int n) {
    float timestamp_ms = getTimestampMs();

    // For time-domain ODFs (Energy)
    if (_cfg.odf_type == OnsetDetectionFunction::Energy) {
        _currentODF = _odfProcessor.processTimeDomain(frame, n);
    } else {
        // Spectral methods require FFT
        static float padded_frame[2048];       // Padded input for FFT
        static float magnitude_spectrum[1024];  // Max FFT size / 2
        int spectrum_size = _cfg.fft_size / 2;

        if (_cfg.fft_size > 2048) {
            // FFT too large, skip this frame
            _frameCount++;
            return;
        }

        // Copy input frame and zero-pad if necessary
        int copy_size = MIN(n, _cfg.fft_size);
        for (int i = 0; i < copy_size; ++i) {
            padded_frame[i] = frame[i];
        }
        // Zero-pad the rest
        for (int i = copy_size; i < _cfg.fft_size; ++i) {
            padded_frame[i] = 0.0f;
        }

        // Compute FFT magnitude spectrum
        fft_magnitude(padded_frame, _cfg.fft_size, magnitude_spectrum);

        // Process via spectral method
        _currentODF = _odfProcessor.processSpectrum(magnitude_spectrum, spectrum_size);
    }


    // Peak picking
    auto onsets = _peakPicker.process(_currentODF, _frameCount, timestamp_ms);
    for (const auto& onset : onsets) {
        _onsetCount++;
        if (onOnset) {
            onOnset(onset.confidence, onset.timestamp_ms);
        }
    }

    // Tempo tracking
    _tempoTracker.addODFValue(_currentODF, timestamp_ms);
    auto beats = _tempoTracker.checkBeat(timestamp_ms);
    for (const auto& beat : beats) {
        _beatCount++;
        if (onBeat) {
            onBeat(beat.confidence, beat.bpm, beat.timestamp_ms);
        }

        // Notify tempo change
        if (ABS(beat.bpm - _lastTempoBPM) > 1.0f) {
            _lastTempoBPM = beat.bpm;
            if (onTempoChange) {
                onTempoChange(beat.bpm, beat.confidence);
            }
        }
    }

    _frameCount++;
}

void BeatDetector::processSpectrum(const float* magnitude_spectrum, int spectrum_size, float timestamp_ms) {
    _currentODF = _odfProcessor.processSpectrum(magnitude_spectrum, spectrum_size);

    // Peak picking
    auto onsets = _peakPicker.process(_currentODF, _frameCount, timestamp_ms);
    for (const auto& onset : onsets) {
        _onsetCount++;
        if (onOnset) {
            onOnset(onset.confidence, onset.timestamp_ms);
        }
    }

    // Tempo tracking
    _tempoTracker.addODFValue(_currentODF, timestamp_ms);
    auto beats = _tempoTracker.checkBeat(timestamp_ms);
    for (const auto& beat : beats) {
        _beatCount++;
        if (onBeat) {
            onBeat(beat.confidence, beat.bpm, beat.timestamp_ms);
        }

        // Notify tempo change
        if (ABS(beat.bpm - _lastTempoBPM) > 1.0f) {
            _lastTempoBPM = beat.bpm;
            if (onTempoChange) {
                onTempoChange(beat.bpm, beat.confidence);
            }
        }
    }

    _frameCount++;
}

TempoEstimate BeatDetector::getTempo() const {
    return _tempoTracker.getTempo();
}

float BeatDetector::getTimestampMs() const {
    return (_frameCount * _cfg.hop_size * 1000.0f) / _cfg.sample_rate_hz;
}

} // namespace fl

#endif // SKETCH_HAS_LOTS_OF_MEMORY
