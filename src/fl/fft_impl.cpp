/// #include <Arduino.h>
// #include <iostream>
// #include "audio_types.h"
// // #include "defs.h"
// #include "thirdparty/cq_kernel/cq_kernel.h"
// #include "thirdparty/cq_kernel/kiss_fftr.h"
// #include "util.h"

#ifndef FASTLED_INTERNAL
#define FASTLED_INTERNAL 1
#endif

#include "FastLED.h"

#include "third_party/cq_kernel/cq_kernel.h"
#include "third_party/cq_kernel/kiss_fftr.h"

#include "fl/array.h"
#include "fl/audio.h"
#include "fl/fft.h"
#include "fl/fft_impl.h"
#include "fl/str.h"
#include "fl/unused.h"
#include "fl/vector.h"
#include "fl/warn.h"

#include "fl/memfill.h"

#define AUDIO_SAMPLE_RATE 44100
#define SAMPLES 512
#define BANDS 16
#define SAMPLING_FREQUENCY AUDIO_SAMPLE_RATE
#define MAX_FREQUENCY 4698.3
#define MIN_FREQUENCY 174.6
#define MIN_VAL 5000 // Equivalent to 0.15 in Q15

#define PRINT_HEADER 1

namespace fl {

class FFTContext {
  public:
    FFTContext(int samples, int bands, float fmin, float fmax, int sample_rate)
        : m_fftr_cfg(nullptr), m_kernels(nullptr) {
        fl::memfill(&m_cq_cfg, 0, sizeof(m_cq_cfg));
        m_cq_cfg.samples = samples;
        m_cq_cfg.bands = bands;
        m_cq_cfg.fmin = fmin;
        m_cq_cfg.fmax = fmax;
        m_cq_cfg.fs = sample_rate;
        m_cq_cfg.min_val = MIN_VAL;
        m_fftr_cfg = kiss_fftr_alloc(samples, 0, NULL, NULL);
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

    void fft_unit_test(span<const i16> buffer, FFTBins *out) {

        // FASTLED_ASSERT(512 == m_cq_cfg.samples, "FFTImpl samples mismatch and
        // are still hardcoded to 512");
        out->clear();
        // allocate
        FASTLED_STACK_ARRAY(kiss_fft_cpx, fft, m_cq_cfg.samples);
        FASTLED_STACK_ARRAY(kiss_fft_cpx, cq, m_cq_cfg.bands);
        // initialize
        kiss_fftr(m_fftr_cfg, buffer.data(), fft);
        apply_kernels(fft, cq, m_kernels, m_cq_cfg);
        const float maxf = m_cq_cfg.fmax;
        const float minf = m_cq_cfg.fmin;
        const float delta_f = (maxf - minf) / m_cq_cfg.bands;
        // begin transform
        for (int i = 0; i < m_cq_cfg.bands; ++i) {
            i32 real = cq[i].r;
            i32 imag = cq[i].i;
            float r2 = float(real * real);
            float i2 = float(imag * imag);
            float magnitude = sqrt(r2 + i2);
            float magnitude_db = 20 * log10(magnitude);
            float f_start = minf + i * delta_f;
            float f_end = f_start + delta_f;
            FASTLED_UNUSED(f_start);
            FASTLED_UNUSED(f_end);

            if (magnitude <= 0.0f) {
                magnitude_db = 0.0f;
            }

            // FASTLED_UNUSED(magnitude_db);
            // FASTLED_WARN("magnitude_db: " << magnitude_db);
            // out->push_back(magnitude_db);
            out->bins_raw.push_back(magnitude);
            out->bins_db.push_back(magnitude_db);
        }
    }

    fl::string info() const {
        // Calculate frequency delta
        float delta_f = (m_cq_cfg.fmax - m_cq_cfg.fmin) / m_cq_cfg.bands;
        fl::StrStream ss;
        ss << "FFTImpl Frequency Bands: ";

        for (int i = 0; i < m_cq_cfg.bands; ++i) {
            float f_start = m_cq_cfg.fmin + i * delta_f;
            float f_end = f_start + delta_f;
            ss << f_start << "Hz-" << f_end << "Hz, ";
        }

        return ss.str();
    }

  private:
    kiss_fftr_cfg m_fftr_cfg;
    cq_kernels_t m_kernels;
    cq_kernel_cfg m_cq_cfg;
};

FFTImpl::FFTImpl(const FFT_Args &args) {
    mContext.reset(new FFTContext(args.samples, args.bands, args.fmin,
                                  args.fmax, args.sample_rate));
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
    mContext->fft_unit_test(sample, out);
    return FFTImpl::Result(true, "");
}

} // namespace fl
