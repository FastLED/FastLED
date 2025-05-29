#pragma once

#include "fl/scoped_ptr.h"
#include "fl/slice.h"
#include "fl/vector.h"

namespace fl {

class FFTImpl;
class AudioSample;

struct FFTBins {
  public:
    FFTBins(size_t n) : mSize(n) {
        bins_raw.reserve(n);
        bins_db.reserve(n);
    }

    void clear() {
        bins_raw.clear();
        bins_db.clear();
    }

    size_t size() const { return mSize; }

    // The bins are the output of the FFTImpl.
    fl::vector<float> bins_raw;
    // The frequency range of the bins.
    fl::vector<float> bins_db;

  private:
    size_t mSize;
};

struct FFT_Args {
    static int DefaultSamples() { return 512; }
    static int DefaultBands() { return 16; }
    static float DefaultMinFrequency() { return 174.6f; }
    static float DefaultMaxFrequency() { return 4698.3f; }
    static int DefaultSampleRate() { return 44100; }

    int samples;
    int bands;
    float fmin;
    float fmax;
    int sample_rate;

    FFT_Args(int samples = DefaultSamples(), int bands = DefaultBands(),
             float fmin = DefaultMinFrequency(),
             float fmax = DefaultMaxFrequency(),
             int sample_rate = DefaultSampleRate()) {
        // Memset so that this object can be hashed without garbage from packed
        // in data.
        memset(this, 0, sizeof(FFT_Args));
        this->samples = samples;
        this->bands = bands;
        this->fmin = fmin;
        this->fmax = fmax;
        this->sample_rate = sample_rate;
    }

    bool operator==(const FFT_Args &other) const ;
    bool operator!=(const FFT_Args &other) const { return !(*this == other); }
};

class FFT {
  public:
    FFT();
    ~FFT();

    void run(const Slice<const int16_t> &sample, FFTBins *out,
             const FFT_Args &args = FFT_Args());

    void clear();
    size_t size() const;

    // FFT's are expensive to create, so we cache them. This sets the size of
    // the cache. The default is 8.
    void setFFTCacheSize(size_t size);

  private:
    // Get the FFTImpl for the given arguments.
    FFTImpl &get_or_create(const FFT_Args &args);
    struct HashMap;
    scoped_ptr<HashMap> mMap;
};

}; // namespace fl