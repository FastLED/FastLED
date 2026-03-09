#pragma once

#include "fl/stl/unique_ptr.h"
#include "fl/stl/span.h"
#include "fl/stl/vector.h"
#include "fl/stl/math.h"

namespace fl {

class FFTImpl;
class AudioSample;  // IWYU pragma: keep

// Controls which algorithm produces the log-spaced (CQ) output bins.
//   AUTO      — LOG_REBIN for <= 32 bins, CQ_OCTAVE for > 32 bins
//   LOG_REBIN — Single FFT + geometric bin grouping (~0.15ms on ESP32-S3)
//   CQ_OCTAVE — Octave-wise Constant-Q Transform (~1-5ms on ESP32-S3)
enum class FFTMode {
    AUTO,
    LOG_REBIN,
    CQ_OCTAVE
};

class FFTBins {
    friend class FFTContext;
    friend class AudioContext;

  public:
    FFTBins(fl::size n) : mBands(n) {
        mBinsRaw.reserve(n);
        mBinsDb.reserve(n);
        mBinsLinear.reserve(n);
    }

    FFTBins(const FFTBins &) = default;
    FFTBins &operator=(const FFTBins &) = default;
    FFTBins(FFTBins &&) noexcept = default;
    FFTBins &operator=(FFTBins &&) noexcept = default;

    void clear() {
        mBinsRaw.clear();
        mBinsDb.clear();
        mBinsLinear.clear();
    }

    // Configured band count (stable across clear/populate cycles)
    fl::size bands() const { return mBands; }

    // Read-only span accessors
    fl::span<const float> raw() const { return mBinsRaw; }
    fl::span<const float> db() const { return mBinsDb; }

    // Linear-spaced magnitude bins captured directly from raw FFT output.
    // Same count as CQ bins, evenly spaced from linearFmin() to linearFmax().
    fl::span<const float> linear() const { return mBinsLinear; }
    float linearFmin() const { return mLinearFmin; }
    float linearFmax() const { return mLinearFmax; }

    // CQ parameters (set by FFTImpl after populating bins)
    float fmin() const { return mFmin; }
    float fmax() const { return mFmax; }
    int sampleRate() const { return mSampleRate; }

    // Log-spaced center frequency for CQ bin i
    float binToFreq(int i) const {
        int nbands = static_cast<int>(mBinsRaw.size());
        if (nbands <= 1) return mFmin;
        float m = fl::logf(mFmax / mFmin);
        return mFmin * fl::expf(m * static_cast<float>(i) / static_cast<float>(nbands - 1));
    }

    // Find which CQ bin contains a given frequency (inverse of binToFreq)
    int freqToBin(float freq) const {
        int nbands = static_cast<int>(mBinsRaw.size());
        if (nbands <= 1) return 0;
        if (freq <= mFmin) return 0;
        if (freq >= mFmax) return nbands - 1;
        float m = fl::logf(mFmax / mFmin);
        float bin = fl::logf(freq / mFmin) / m * static_cast<float>(nbands - 1);
        int result = static_cast<int>(bin + 0.5f);
        if (result < 0) return 0;
        if (result >= nbands) return nbands - 1;
        return result;
    }

    // Frequency boundary between adjacent CQ bins i and i+1 (geometric mean)
    float binBoundary(int i) const {
        float f_i = binToFreq(i);
        float f_next = binToFreq(i + 1);
        return fl::sqrtf(f_i * f_next);
    }

  private:
    // Mutable accessors for FFTContext (the only writer)
    fl::vector<float>& raw_mut() { return mBinsRaw; }
    fl::vector<float>& db_mut() { return mBinsDb; }
    fl::vector<float>& linear_mut() { return mBinsLinear; }

    void setParams(float fmin, float fmax, int sampleRate) {
        mFmin = fmin;
        mFmax = fmax;
        mSampleRate = sampleRate;
    }

    void setLinearParams(float linearFmin, float linearFmax) {
        mLinearFmin = linearFmin;
        mLinearFmax = linearFmax;
    }

    fl::vector<float> mBinsRaw;
    fl::vector<float> mBinsDb;
    fl::vector<float> mBinsLinear;
    fl::size mBands;
    float mFmin = 174.6f;
    float mFmax = 4698.3f;
    int mSampleRate = 44100;
    float mLinearFmin = 0.0f;
    float mLinearFmax = 0.0f;
};

struct FFT_Args {
    static int DefaultSamples() { return 512; }
    static int DefaultBands() { return 16; }
    static float DefaultMinFrequency() { return 174.6f; }
    static float DefaultMaxFrequency() { return 4698.3f; }
    static int DefaultSampleRate() { return 44100; }

    int samples = DefaultSamples();
    int bands = DefaultBands();
    float fmin = DefaultMinFrequency();
    float fmax = DefaultMaxFrequency();
    int sample_rate = DefaultSampleRate();
    FFTMode mode = FFTMode::AUTO;

    FFT_Args(int samples = DefaultSamples(), int bands = DefaultBands(),
             float fmin = DefaultMinFrequency(),
             float fmax = DefaultMaxFrequency(),
             int sample_rate = DefaultSampleRate(),
             FFTMode mode = FFTMode::AUTO)
        : samples(samples), bands(bands), fmin(fmin), fmax(fmax),
          sample_rate(sample_rate), mode(mode) {}

    bool operator==(const FFT_Args &other) const ;
    bool operator!=(const FFT_Args &other) const { return !(*this == other); }
};

class FFT {
  public:
    FFT();
    ~FFT();

    FFT(FFT &&) = default;
    FFT &operator=(FFT &&) = default;
    FFT(const FFT & other);
    FFT &operator=(const FFT & other);

            void run(const span<const i16> &sample, FFTBins *out,
             const FFT_Args &args = FFT_Args());

    void clear();
    fl::size size() const;

    // FFT's are expensive to create, so we cache them. This sets the size of
    // the cache. The default is 8.
    void setFFTCacheSize(fl::size size);

  private:
    // Get the FFTImpl for the given arguments.
    FFTImpl &get_or_create(const FFT_Args &args);
    struct HashMap;
    scoped_ptr<HashMap> mMap;
};

}; // namespace fl
