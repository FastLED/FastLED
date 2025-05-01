/// #include <Arduino.h>
// #include <iostream>
// #include "audio_types.h"
// // #include "defs.h"
// #include "thirdparty/cq_kernel/cq_kernel.h"
// #include "thirdparty/cq_kernel/kiss_fftr.h"
// #include "util.h"

#define FASTLED_INTERNAL 1

#include "FastLED.h"

#include "third_party/cq_kernel/cq_kernel.h"
#include "third_party/cq_kernel/kiss_fftr.h"

#include "fl/array.h"
#include "fl/fft.h"
#include "fl/str.h"
#include "fl/unused.h"
#include "fl/vector.h"
#include "fl/warn.h"

// #define SAMPLES IS2_AUDIO_BUFFER_LEN
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
        memset(&m_cq_cfg, 0, sizeof(m_cq_cfg));
        m_cq_cfg.samples = samples;
        m_cq_cfg.bands = bands;
        m_cq_cfg.fmin = fmin;
        m_cq_cfg.fmax = fmax;
        m_cq_cfg.fs = sample_rate;
        m_cq_cfg.min_val = MIN_VAL;
        m_fftr_cfg = kiss_fftr_alloc(samples, 0, NULL, NULL);
        if (!m_fftr_cfg) {
            FASTLED_WARN("Failed to allocate FFT context");
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

    void fft_unit_test(const fft_audio_buffer_t &buffer,
                       fft_output_fixed *out) {
        // uint32_t start = millis();
        out->clear();
        // kiss_fft_cpx fft[SAMPLES] = {};
        FASTLED_STACK_ARRAY(kiss_fft_cpx, fft, m_cq_cfg.samples);
        // memset(fft, 0, sizeof(fft));
        kiss_fftr(m_fftr_cfg, buffer, fft);
        // kiss_fft_cpx cq[BANDS] = {};
        FASTLED_STACK_ARRAY(kiss_fft_cpx, cq, m_cq_cfg.bands);
        // memset(cq, 0, sizeof(cq));
        apply_kernels(fft, cq, m_kernels, m_cq_cfg);
        // uint32_t diff = millis() - start;
        //  FASTLED_UNUSED(diff);
        const float maxf = m_cq_cfg.fmax;
        const float minf = m_cq_cfg.fmin;
        const float delta_f = (maxf - minf) / m_cq_cfg.bands;
        // char output_str[2048]; // Assuming this size is sufficient. Adjust as
        // necessary.
        // int offset = 0;
        // offset += snprintf(output_str + offset, sizeof(output_str) - offset,
        // "FFT took %u ms. FFT output: ", diff);
        // process output here
        for (int i = 0; i < m_cq_cfg.bands; ++i) {
            int32_t real = cq[i].r;
            int32_t imag = cq[i].i;
            float r2 = float(real * real);
            float i2 = float(imag * imag);
            float magnitude = sqrt(r2 + i2);
            float magnitude_db = 20 * log10(magnitude);
            float f_start = minf + i * delta_f;
            float f_end = f_start + delta_f;
            FASTLED_UNUSED(f_start);
            FASTLED_UNUSED(f_end);
            FASTLED_UNUSED(magnitude_db);

            FASTLED_WARN("magnitude: " << magnitude);

            out->push_back(magnitude);

            if (magnitude <= 0.0f) {
                magnitude_db = 0.0f;
            }
        }
    }

    fl::Str generateHeaderInfo() const {
        // Calculate frequency delta
        float delta_f = (m_cq_cfg.fmax - m_cq_cfg.fmin) / m_cq_cfg.bands;

        // // Print header with frequency bands
        // char output_str[2048] = {0}; // Buffer for header text
        // int offset = 0;

        // offset += snprintf(output_str + offset, sizeof(output_str) - offset,
        //                    "FFT Frequency Bands: ");

        fl::StrStream ss;
        ss << "FFT Frequency Bands: ";

        for (int i = 0; i < m_cq_cfg.bands; ++i) {
            float f_start = m_cq_cfg.fmin + i * delta_f;
            float f_end = f_start + delta_f;
            // offset += snprintf(output_str + offset, sizeof(output_str) - offset,
            //                    "%.2fHz-%.2fHz, ", static_cast<double>(f_start),
            //                    static_cast<double>(f_end));
            ss << f_start << "Hz-" << f_end << "Hz, ";
        }

        return ss.str();
    }

  private:
    kiss_fftr_cfg m_fftr_cfg;
    cq_kernels_t m_kernels;
    cq_kernel_cfg m_cq_cfg;
};

FFT::FFT(int samples, int bands, float fmin, float fmax, int sample_rate)
    : mContext(new FFTContext(samples, bands, fmin, fmax, sample_rate)) {
    if (!mContext) {
        FASTLED_WARN("Failed to allocate FFT context");
    }
}

FFT::~FFT() { mContext.reset(); }

void FFT::fft_unit_test(const fft_audio_buffer_t &buffer,
                        fft_output_fixed *out) {

    if (mContext) {
        mContext->fft_unit_test(buffer, out);
    } else {
        FASTLED_WARN("FFT context is not initialized");
    }
}

fl::Str FFT::generateHeaderInfo() const {
    if (mContext) {
        return mContext->generateHeaderInfo();
    } else {
        FASTLED_WARN("FFT context is not initialized");
        return fl::Str();
    }
}

} // namespace fl
