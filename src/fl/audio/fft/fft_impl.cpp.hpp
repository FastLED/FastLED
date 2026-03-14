#ifndef FASTLED_INTERNAL
#define FASTLED_INTERNAL
#endif

#include "fl/fastled.h"

// IWYU pragma: begin_keep
#include "third_party/cq_kernel/cq_kernel.h"
#include "third_party/cq_kernel/kiss_fftr.h"

// IWYU pragma: end_keep
#include "fl/stl/alloca.h"
#include "fl/stl/array.h"
#include "fl/audio/audio.h"
#include "fl/audio/fft/fft.h"
#include "fl/audio/fft/fft_impl.h"
#include "fl/stl/string.h"
#include "fl/stl/compiler_control.h"
#include "fl/stl/vector.h"
#include "fl/system/log.h"

#include "fl/stl/cstring.h"

#ifndef FL_AUDIO_SAMPLE_RATE
#define FL_AUDIO_SAMPLE_RATE 44100
#endif

#ifndef FL_FFT_SAMPLE_RATE
#define FL_FFT_SAMPLE_RATE FL_AUDIO_SAMPLE_RATE
#endif

#ifndef FL_FFT_SAMPLES
#define FL_FFT_SAMPLES 512
#endif

#ifndef FL_FFT_BANDS
#define FL_FFT_BANDS 16
#endif

#ifndef FL_FFT_MIN_VAL
#define FL_FFT_MIN_VAL 5000 // Equivalent to 0.15 in Q15
#endif

#ifndef FL_FFT_PRINT_HEADER
#define FL_FFT_PRINT_HEADER 1
#endif

namespace fl {

    // Resolve FFTMode::AUTO based on bin count.
    // <= 32 bins → LOG_REBIN (fast, no quality difference at this resolution)
    // > 32 bins  → CQ_OCTAVE (accurate per-bin frequency discrimination)
    static FFTMode resolveMode(FFTMode mode, int /*samples*/, float /*fmin*/,
                               float /*fmax*/, int bands) {
        if (mode != FFTMode::AUTO) return mode;
        return (bands <= 32) ? FFTMode::LOG_REBIN : FFTMode::CQ_OCTAVE;
    }

class FFTContext {
  public:
    FFTContext(int samples, int bands, float fmin, float fmax, int sample_rate,
              FFTMode mode)
        : m_fftr_cfg(nullptr), m_input_samples(samples),
          m_kernels(nullptr), m_octaveWise(false),
          m_mode(resolveMode(mode, samples, fmin, fmax, bands)),
          m_totalBands(bands), m_fmin(fmin), m_fmax(fmax),
          m_sampleRate(sample_rate) {
        fl::memset(&m_cq_cfg, 0, sizeof(m_cq_cfg));

        m_fftr_cfg = kiss_fftr_alloc(samples, 0, nullptr, nullptr);
        if (!m_fftr_cfg) {
            FASTLED_WARN("Failed to allocate FFTImpl context");
            return;
        }

        if (m_mode == FFTMode::LOG_REBIN) {
            // No CQ kernels needed — just FFT + geometric bin grouping.
            // Pre-compute log-spaced bin edges for runLogRebin().
            initLogRebin();
        } else {
            // CQ mode: pick naive or octave-wise based on kernel conditioning.
            // N_window = N * fmin / fmax. When < 2, kernels are degenerate.
            int winMin = static_cast<int>(
                static_cast<float>(samples) * fmin / fmax);
            if (winMin >= 2) {
                initNaive(samples, bands, fmin, fmax, sample_rate);
            } else {
                initOctaveWise(samples, bands, fmin, fmax, sample_rate);
            }
        }
    }

    ~FFTContext() {
        if (m_fftr_cfg) {
            kiss_fftr_free(m_fftr_cfg);
        }
        if (m_kernels) {
            free_kernels(m_kernels, m_cq_cfg);
        }
        for (int i = 0; i < static_cast<int>(m_octaves.size()); i++) {
            if (m_octaves[i].kernels) {
                free_kernels(m_octaves[i].kernels, m_octaves[i].cfg);
            }
        }
    }

    fl::size sampleSize() const { return m_input_samples; }

    void run(span<const i16> buffer, FFTBins *out) {
        if (m_mode == FFTMode::LOG_REBIN) {
            runLogRebin(buffer, out);
        } else if (m_octaveWise) {
            runOctaveWise(buffer, out);
        } else {
            runNaive(buffer, out);
        }
    }

    fl::string info() const {
        FFTBins tmp(m_totalBands);
        tmp.setParams(m_fmin, m_fmax, m_sampleRate);
        for (int i = 0; i < m_totalBands; ++i) {
            tmp.raw_mut().push_back(0.0f);
        }

        fl::sstream ss;
        ss << "FFTImpl Frequency Bands (CQ log-spaced): ";
        for (int i = 0; i < m_totalBands; ++i) {
            float f_low = (i == 0) ? m_fmin : tmp.binBoundary(i - 1);
            float f_high =
                (i == m_totalBands - 1) ? m_fmax : tmp.binBoundary(i);
            ss << f_low << "Hz-" << f_high << "Hz, ";
        }
        return ss.str();
    }

  private:
    // ---- Log-rebin path (fast, no CQ kernels) ----
    //
    // Single 512-point FFT, then group the linear FFT bins into
    // geometrically-spaced output bins. Same approach as WLED.
    // Cost: ~0.15ms on ESP32-S3. No kernel memory.

    void initLogRebin() {
        // Pre-compute bin edges aligned with CQ center frequencies.
        // CQ center[i] = fmin * exp(logRatio * i / (bands-1)), so
        // binToFreq(i) returns these centers for both modes.
        //
        // Edges are placed at the geometric mean of adjacent centers:
        //   edge[i] = sqrt(center[i-1] * center[i])
        //           = fmin * exp(logRatio * (2*i - 1) / (2*(bands-1)))
        //
        // edge[0] and edge[bands] extend half a bin beyond fmin/fmax.
        const int bands = m_totalBands;
        m_logBinEdges.resize(bands + 1);
        float logRatio = logf(m_fmax / m_fmin);
        if (bands <= 1) {
            m_logBinEdges[0] = m_fmin;
            m_logBinEdges[1] = m_fmax;
        } else {
            float denom = 2.0f * static_cast<float>(bands - 1);
            // Edge below first center (half-bin below fmin)
            m_logBinEdges[0] =
                m_fmin * expf(-logRatio / denom);
            // Intermediate edges: geometric mean of adjacent CQ centers
            for (int i = 1; i < bands; i++) {
                m_logBinEdges[i] =
                    m_fmin *
                    expf(logRatio * (2.0f * static_cast<float>(i) - 1.0f) /
                         denom);
            }
            // Edge above last center (half-bin above fmax)
            m_logBinEdges[bands] =
                m_fmax * expf(logRatio / denom);
        }

        // Pre-compute Hanning window to reduce spectral leakage
        const int N = m_input_samples;
        m_hanningWindow.resize(N);
        const float invNm1 = 1.0f / static_cast<float>(N - 1);
        for (int n = 0; n < N; ++n) {
            float phase = 2.0f * static_cast<float>(FL_M_PI) *
                          static_cast<float>(n) * invNm1;
            m_hanningWindow[n] = 0.5f * (1.0f - fl::cosf(phase));
        }
    }

    void runLogRebin(span<const i16> buffer, FFTBins *out) {
        out->setParams(m_fmin, m_fmax, m_sampleRate);
        const int N = m_input_samples;
        const int bands = m_totalBands;

        // Apply Hanning window to reduce spectral leakage
        FASTLED_STACK_ARRAY(kiss_fft_scalar, windowed, N);
        for (int i = 0; i < N; ++i) {
            windowed[i] = static_cast<kiss_fft_scalar>(
                static_cast<float>(buffer[i]) * m_hanningWindow[i]);
        }

        FASTLED_STACK_ARRAY(kiss_fft_cpx, fft, N);
        kiss_fftr(m_fftr_cfg, windowed, fft);

        // Linear bins (same as other paths)
        computeLinearBins(fft, N, out);

        // Group FFT bins into log-spaced output bins
        fl::vector<float> &rawBins = out->raw_mut();
        fl::vector<float> &dbBins = out->db_mut();
        rawBins.resize(bands);
        dbBins.resize(bands);
        for (int i = 0; i < bands; ++i) {
            rawBins[i] = 0.0f;
        }

        const float fs = static_cast<float>(m_sampleRate);
        const float rawBinHz = fs / static_cast<float>(N);
        const int numRawBins = N / 2 + 1;
        const float halfBin = rawBinHz * 0.5f;

        for (int k = 0; k < numRawBins; ++k) {
            float freq = static_cast<float>(k) * rawBinHz;
            // Each FFT bin covers [freq - halfBin, freq + halfBin].
            // Include it if any part of that range overlaps [fmin, fmax].
            if (freq + halfBin < m_fmin || freq - halfBin >= m_fmax)
                continue;

            float re = static_cast<float>(fft[k].r);
            float im = static_cast<float>(fft[k].i);
            float mag = sqrt(re * re + im * im);

            // Binary search for the output bin this frequency falls into.
            // m_logBinEdges is sorted, so find largest i where edge[i] <= freq.
            // Clamp: FFT bins below fmin map to bin 0, above fmax to last bin.
            int lo = 0, hi = bands - 1;
            while (lo < hi) {
                int mid = (lo + hi + 1) / 2;
                if (m_logBinEdges[mid] <= freq)
                    lo = mid;
                else
                    hi = mid - 1;
            }
            rawBins[lo] += mag;
        }

        for (int i = 0; i < bands; ++i) {
            dbBins[i] = (rawBins[i] > 0.0f) ? 20.0f * log10f(rawBins[i])
                                              : 0.0f;
        }
    }

    // ---- Naive single-FFT path (narrow frequency ranges) ----

    void initNaive(int samples, int bands, float fmin, float fmax, int sr) {
        m_cq_cfg.samples = samples;
        m_cq_cfg.bands = bands;
        m_cq_cfg.fmin = fmin;
        m_cq_cfg.fmax = fmax;
        m_cq_cfg.fs = sr;
        m_cq_cfg.min_val = FL_FFT_MIN_VAL;
        m_kernels = generate_kernels(m_cq_cfg);
    }

    void runNaive(span<const i16> buffer, FFTBins *out) {
        out->setParams(m_fmin, m_fmax, m_sampleRate);
        const int fftSize = m_input_samples;

        FASTLED_STACK_ARRAY(kiss_fft_cpx, fft, fftSize);
        kiss_fftr(m_fftr_cfg, buffer.data(), fft);

        computeLinearBins(fft, fftSize, out);

        FASTLED_STACK_ARRAY(kiss_fft_cpx, cq, m_cq_cfg.bands);
        apply_kernels(fft, cq, m_kernels, m_cq_cfg);

        const int bands = m_cq_cfg.bands;
        fl::vector<float> &rawBins = out->raw_mut();
        fl::vector<float> &dbBins = out->db_mut();
        rawBins.resize(bands);
        dbBins.resize(bands);
        for (int i = 0; i < bands; ++i) {
            i32 real = cq[i].r;
            i32 imag = cq[i].i;
            float r2 = float(real * real);
            float i2 = float(imag * imag);
            float magnitude = sqrt(r2 + i2);

            float magnitude_db = 20 * log10(magnitude);

            if (magnitude <= 0.0f) {
                magnitude_db = 0.0f;
            }

            rawBins[i] = magnitude;
            dbBins[i] = magnitude_db;
        }
    }

    // ---- Octave-wise CQT path (wide frequency ranges) ----
    //
    // Instead of one massive FFT + degenerate kernels, split the frequency
    // range into octaves. Each octave uses the same N-point FFT (512) with
    // well-conditioned CQ kernels (N_window >= N/2). The signal is decimated
    // by 2x between octaves via a halfband filter.
    //
    // Memory: ~25KB total vs ~830KB for zero-padding approach.

    struct OctaveInfo {
        int firstBin;
        int numBins;
        cq_kernel_cfg cfg;
        cq_kernels_t kernels;
    };

    void initOctaveWise(int samples, int bands, float fmin, float fmax,
                        int sr) {
        m_octaveWise = true;

        // Use floor so the top octave covers the remaining frequency range
        // rather than creating a tiny sliver octave with 1 bin.
        int numOctaves = static_cast<int>(floorf(log2f(fmax / fmin)));
        if (numOctaves < 1)
            numOctaves = 1;

        // Log-spaced center frequencies for all bins
        float logRatio = logf(fmax / fmin);
        fl::vector<float> centerFreqs(bands);
        for (int i = 0; i < bands; i++) {
            centerFreqs[i] =
                fmin *
                expf(logRatio * static_cast<float>(i) /
                     static_cast<float>(bands - 1));
        }

        // Assign each bin to an octave: oct j spans [fmin*2^j, fmin*2^(j+1))
        fl::vector<int> binOctave(bands);
        for (int i = 0; i < bands; i++) {
            int oct =
                static_cast<int>(floorf(log2f(centerFreqs[i] / fmin)));
            if (oct < 0)
                oct = 0;
            if (oct >= numOctaves)
                oct = numOctaves - 1;
            binOctave[i] = oct;
        }

        // Build per-octave CQ kernel sets
        m_octaves.resize(numOctaves);
        m_maxBinsPerOctave = 0;
        for (int oct = 0; oct < numOctaves; oct++) {
            int first = -1, last = -1;
            for (int i = 0; i < bands; i++) {
                if (binOctave[i] == oct) {
                    if (first < 0)
                        first = i;
                    last = i;
                }
            }

            OctaveInfo &oi = m_octaves[oct];
            fl::memset(&oi.cfg, 0, sizeof(oi.cfg));
            oi.kernels = nullptr;
            if (first < 0) {
                oi.firstBin = 0;
                oi.numBins = 0;
                continue;
            }

            oi.firstBin = first;
            oi.numBins = last - first + 1;
            if (oi.numBins > m_maxBinsPerOctave) {
                m_maxBinsPerOctave = oi.numBins;
            }

            // Decimation: top octave (numOctaves-1) uses original sample rate.
            // Each lower octave halves the effective sample rate.
            int decimExp = numOctaves - 1 - oct;
            float effectiveFs =
                static_cast<float>(sr) /
                static_cast<float>(1 << decimExp);

            oi.cfg.samples = samples;
            oi.cfg.bands = oi.numBins;
            oi.cfg.fmin = centerFreqs[first];
            oi.cfg.fmax = (oi.numBins > 1) ? centerFreqs[last]
                                            : centerFreqs[first] * 2.0f;
            oi.cfg.fs = effectiveFs;
            oi.cfg.min_val = FL_FFT_MIN_VAL;

            oi.kernels = generate_kernels(oi.cfg);
        }

        // Pre-allocate reusable buffers
        m_workBuf.resize(samples);
        m_fftOut.resize(samples);
    }

    void runOctaveWise(span<const i16> buffer, FFTBins *out) {
        const int N = m_input_samples;
        const int numOctaves = static_cast<int>(m_octaves.size());

        out->setParams(m_fmin, m_fmax, m_sampleRate);

        // Copy input to working buffer
        int workLen = N;
        for (int i = 0; i < N; i++) {
            m_workBuf[i] =
                (i < static_cast<int>(buffer.size())) ? buffer[i] : 0;
        }

        // FFT at full sample rate (for linear bins + top octave CQ)
        kiss_fftr(m_fftr_cfg, m_workBuf.data(), m_fftOut.data());
        computeLinearBins(m_fftOut.data(), N, out);

        // Prepare CQ output bins
        fl::vector<float> &rawBins = out->raw_mut();
        fl::vector<float> &dbBins = out->db_mut();
        rawBins.resize(m_totalBands);
        dbBins.resize(m_totalBands);
        for (int i = 0; i < m_totalBands; i++) {
            rawBins[i] = 0.0f;
            dbBins[i] = 0.0f;
        }

        // Pre-allocate CQ accumulator once (avoids alloca in loop)
        FASTLED_STACK_ARRAY(kiss_fft_cpx, cq, m_maxBinsPerOctave);

        // Process octaves from top (highest freq) to bottom (lowest freq).
        // Top octave uses the FFT already computed above.
        // Each lower octave: decimate signal by 2x, then FFT + CQ.
        for (int oct = numOctaves - 1; oct >= 0; oct--) {
            const OctaveInfo &oi = m_octaves[oct];
            if (oi.numBins <= 0 || !oi.kernels)
                continue;

            if (oct != numOctaves - 1) {
                decimateBy2(m_workBuf.data(), workLen);
                workLen = workLen / 2;
                // Zero-pad remainder so FFT sees clean input
                for (int i = workLen; i < N; i++)
                    m_workBuf[i] = 0;
                kiss_fftr(m_fftr_cfg, m_workBuf.data(), m_fftOut.data());
            }

            // Zero the CQ accumulator and apply kernels
            fl::memset(cq, 0, sizeof(kiss_fft_cpx) * oi.numBins);
            apply_kernels(m_fftOut.data(), cq, oi.kernels, oi.cfg);

            for (int i = 0; i < oi.numBins; i++) {
                int binIdx = oi.firstBin + i;
                i32 real = cq[i].r;
                i32 imag = cq[i].i;
                float r2 = float(real * real);
                float i2 = float(imag * imag);
                float mag = sqrt(r2 + i2);
                float db = (mag > 0.0f) ? 20.0f * log10f(mag) : 0.0f;
                rawBins[binIdx] = mag;
                dbBins[binIdx] = db;
            }
        }
    }

    // ---- Shared utilities ----

    void computeLinearBins(const kiss_fft_cpx *fft, int nfft,
                           FFTBins *out) {
        const int numRawBins = nfft / 2 + 1;
        const int numLinearBins = m_totalBands;
        const float fs = static_cast<float>(m_sampleRate);
        const float rawBinHz = fs / static_cast<float>(nfft);
        const float linearBinHz =
            (m_fmax - m_fmin) / static_cast<float>(numLinearBins);

        fl::vector<float> &linBins = out->linear_mut();
        linBins.resize(numLinearBins);
        for (int i = 0; i < numLinearBins; ++i) {
            linBins[i] = 0.0f;
        }
        out->setLinearParams(m_fmin, m_fmax);

        const float halfBin = rawBinHz * 0.5f;
        for (int k = 0; k < numRawBins; ++k) {
            float freq = static_cast<float>(k) * rawBinHz;
            // Each FFT bin covers [freq - halfBin, freq + halfBin].
            // Include it if any part of that range overlaps [fmin, fmax].
            if (freq + halfBin < m_fmin || freq - halfBin >= m_fmax)
                continue;

            float re = static_cast<float>(fft[k].r);
            float im = static_cast<float>(fft[k].i);
            float mag = sqrt(re * re + im * im);

            int linIdx = static_cast<int>((freq - m_fmin) / linearBinHz);
            if (linIdx < 0)
                linIdx = 0;
            if (linIdx >= numLinearBins)
                linIdx = numLinearBins - 1;
            linBins[linIdx] += mag;
        }
    }

    // 3-tap halfband filter [0.25, 0.5, 0.25] + decimate by 2.
    // -13dB stopband rejection — adequate for CQ magnitude analysis.
    static void decimateBy2(kiss_fft_scalar *buf, int len) {
        int outLen = len / 2;
        for (int i = 0; i < outLen; i++) {
            int idx = i * 2;
            i32 prev = (idx > 0) ? buf[idx - 1] : buf[idx];
            i32 curr = buf[idx];
            i32 next = (idx + 1 < len) ? buf[idx + 1] : buf[idx];
            buf[i] =
                static_cast<kiss_fft_scalar>((prev + 2 * curr + next) / 4);
        }
    }

    // ---- Member variables ----

    // Shared
    kiss_fftr_cfg m_fftr_cfg;
    int m_input_samples;

    // Naive CQ path only
    cq_kernels_t m_kernels;
    cq_kernel_cfg m_cq_cfg;

    // Log-rebin path only
    fl::vector<float> m_logBinEdges; // bands+1 geometric bin edges
    fl::vector<float> m_hanningWindow; // N pre-computed Hanning coefficients

    // Octave-wise CQ path
    bool m_octaveWise;
    FFTMode m_mode;
    fl::vector<OctaveInfo> m_octaves;
    fl::vector<kiss_fft_scalar> m_workBuf; // reusable decimation buffer
    fl::vector<kiss_fft_cpx> m_fftOut;     // reusable FFT output buffer
    int m_maxBinsPerOctave = 0;

    // Used by both paths
    int m_totalBands;
    float m_fmin, m_fmax;
    int m_sampleRate;
};

FFTImpl::FFTImpl(const FFT_Args &args) {
    mContext = fl::make_unique<FFTContext>(args.samples, args.bands, args.fmin,
                                          args.fmax, args.sample_rate,
                                          args.mode);
}

FFTImpl::~FFTImpl() { mContext.reset(); }

fl::string FFTImpl::info() const {
    if (mContext) {
        return mContext->info();
    } else {
        FASTLED_WARN("FFTImpl context is not initialized");
        return fl::string();
    }
}

fl::size FFTImpl::sampleSize() const {
    if (mContext) {
        return mContext->sampleSize();
    }
    return 0;
}

FFTImpl::Result FFTImpl::run(const AudioSample &sample, FFTBins *out) {
    auto &audio_sample = sample.pcm();
    span<const i16> slice(audio_sample);
    return run(slice, out);
}

FFTImpl::Result FFTImpl::run(span<const i16> sample, FFTBins *out) {
    if (!mContext) {
        return FFTImpl::Result(false, "FFTImpl context is not initialized");
    }
    if (sample.size() != mContext->sampleSize()) {
        FASTLED_WARN("FFTImpl sample size mismatch");
        return FFTImpl::Result(false, "FFTImpl sample size mismatch");
    }
    mContext->run(sample, out);
    return FFTImpl::Result(true, "");
}

} // namespace fl
