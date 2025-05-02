#pragma once

#include "fl/scoped_ptr.h"
#include "fl/slice.h"
#include "fl/vector.h"

namespace fl {

// Proof of concept FFT using KISS FFT. Right now this is fixed sized blocks of
// 512. But this is intended to change with a C++ wrapper around ot/
typedef int16_t fft_audio_buffer_t[512];
// typedef float fft_output[16];

using fft_output_fixed = fl::vector_inlined<float, 16>;

class FFTContext;

class FFT {
  public:

    static int DefaultSamples() { return 512; }
    static int DefaultBands() { return 16; }
    static float DefaultMinFrequency() { return 174.6f; }
    static float DefaultMaxFrequency() { return 4698.3f; }
    static int DefaultSampleRate() { return 44100; }

    // FFT(int samples, int bands, float fmin, float fmax, int sample_rate);
    FFT(int samples = DefaultSamples(), int bands = DefaultBands(),
        float fmin = DefaultMinFrequency(), float fmax = DefaultMaxFrequency(),
        int sample_rate = DefaultSampleRate());
    ~FFT();

    FFT(const FFT &) = delete;
    FFT &operator=(const FFT &) = delete;
    FFT(FFT &&) = delete;
    FFT &operator=(FFT &&) = delete;

    void fft_unit_test(const fft_audio_buffer_t &buffer, fft_output_fixed *out);

    // Info on what the frequency the bins represent
    fl::Str info() const;

  private:
    fl::scoped_ptr<FFTContext> mContext;
};

void fft_init(); // Explicit initialization of FFT, otherwise it will be
                 // initialized on first run.
bool fft_is_initialized();
void fft_unit_test(const fft_audio_buffer_t &buffer, fft_output_fixed *out);

}; // namespace fl