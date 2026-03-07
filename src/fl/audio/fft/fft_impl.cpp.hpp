#ifndef FASTLED_INTERNAL
#define FASTLED_INTERNAL
#endif

#include "fl/fastled.h"

// IWYU pragma: begin_keep
#include "third_party/cq_kernel/cq_kernel.h"
#include "third_party/cq_kernel/kiss_fftr.h"

// IWYU pragma: end_keep
#include "fl/alloca.h"
#include "fl/stl/array.h"
#include "fl/audio.h"
#include "fl/audio/fft/fft.h"
#include "fl/audio/fft/fft_impl.h"
#include "fl/stl/string.h"
#include "fl/unused.h"
#include "fl/stl/vector.h"
#include "fl/warn.h"

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

class FFTContext {
  public:
    FFTContext(int samples, int bands, float fmin, float fmax, int sample_rate)
        : m_fftr_cfg(nullptr), m_kernels(nullptr) {
        fl::memset(&m_cq_cfg, 0, sizeof(m_cq_cfg));
        m_cq_cfg.samples = samples;
        m_cq_cfg.bands = bands;
        m_cq_cfg.fmin = fmin;
        m_cq_cfg.fmax = fmax;
        m_cq_cfg.fs = sample_rate;
        m_cq_cfg.min_val = FL_FFT_MIN_VAL;
        m_fftr_cfg = kiss_fftr_alloc(samples, 0, nullptr, nullptr);
        if (!m_fftr_cfg) {
            FASTLED_WARN("Failed to allocate FFTImpl context");
            return;
        }
        m_kernels = generate_kernels(m_cq_cfg);
    }
    ~FFTContext() {
        if (m_fftr_cfg) {
            kiss_fftr_free(m_fftr_cfg);
        }
        if (m_kernels) {
            free_kernels(m_kernels, m_cq_cfg);
        }
    }

    fl::size sampleSize() const { return m_cq_cfg.samples; }

    void run(span<const i16> buffer, FFTBins *out) {
        out->setParams(m_cq_cfg.fmin, m_cq_cfg.fmax, m_cq_cfg.fs);
        FASTLED_STACK_ARRAY(kiss_fft_cpx, fft, m_cq_cfg.samples);
        FASTLED_STACK_ARRAY(kiss_fft_cpx, cq, m_cq_cfg.bands);
        kiss_fftr(m_fftr_cfg, buffer.data(), fft);

        // Capture linear-spaced magnitude bins directly from raw FFT output.
        // kiss_fftr produces nfft/2+1 complex bins spanning 0 to Nyquist.
        // We rebin these into m_cq_cfg.bands linear bins spanning fmin to fmax.
        // This is O(nfft/2) — a single pass with no transcendentals.
        {
            const int nfft = m_cq_cfg.samples;
            const int numRawBins = nfft / 2 + 1;
            const int numLinearBins = m_cq_cfg.bands;
            const float fs = static_cast<float>(m_cq_cfg.fs);
            const float fmin = m_cq_cfg.fmin;
            const float fmax = m_cq_cfg.fmax;
            const float rawBinHz = fs / static_cast<float>(nfft);
            const float linearBinHz = (fmax - fmin) / static_cast<float>(numLinearBins);

            // resize reuses capacity when recycled — zero allocs after warmup
            fl::vector<float>& linBins = out->linear_mut();
            linBins.resize(numLinearBins);
            for (int i = 0; i < numLinearBins; ++i) {
                linBins[i] = 0.0f;
            }
            out->setLinearParams(fmin, fmax);

            for (int k = 0; k < numRawBins; ++k) {
                float freq = static_cast<float>(k) * rawBinHz;
                if (freq < fmin || freq >= fmax) continue;

                float re = static_cast<float>(fft[k].r);
                float im = static_cast<float>(fft[k].i);
                float mag = sqrt(re * re + im * im);

                int linIdx = static_cast<int>((freq - fmin) / linearBinHz);
                if (linIdx >= numLinearBins) linIdx = numLinearBins - 1;
                linBins[linIdx] += mag;
            }
        }

        apply_kernels(fft, cq, m_kernels, m_cq_cfg);
        // Use resize + indexed assignment instead of clear + push_back
        // to reuse vector capacity when recycling — zero allocs after warmup
        const int bands = m_cq_cfg.bands;
        fl::vector<float>& rawBins = out->raw_mut();
        fl::vector<float>& dbBins = out->db_mut();
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

    fl::string info() const {
        // Build a temporary FFTBins to use its binBoundary/binToFreq methods
        FFTBins tmp(m_cq_cfg.bands);
        tmp.setParams(m_cq_cfg.fmin, m_cq_cfg.fmax, m_cq_cfg.fs);
        // Populate raw bins so binToFreq uses correct size
        for (int i = 0; i < m_cq_cfg.bands; ++i) {
            tmp.raw_mut().push_back(0.0f);
        }

        fl::sstream ss;
        ss << "FFTImpl Frequency Bands (CQ log-spaced): ";
        for (int i = 0; i < m_cq_cfg.bands; ++i) {
            float f_low = (i == 0) ? m_cq_cfg.fmin : tmp.binBoundary(i - 1);
            float f_high = (i == m_cq_cfg.bands - 1) ? m_cq_cfg.fmax : tmp.binBoundary(i);
            ss << f_low << "Hz-" << f_high << "Hz, ";
        }
        return ss.str();
    }

  private:
    kiss_fftr_cfg m_fftr_cfg;
    cq_kernels_t m_kernels;
    cq_kernel_cfg m_cq_cfg;
};

FFTImpl::FFTImpl(const FFT_Args &args) {
    mContext = fl::make_unique<FFTContext>(args.samples, args.bands, args.fmin,
                                          args.fmax, args.sample_rate);
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
