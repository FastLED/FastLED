#pragma once

#include "fl/unique_ptr.h"
#include "fl/span.h"
#include "fl/vector.h"
#include "fl/move.h"
#include "fl/memfill.h"

namespace fl {

class FFTImpl;
class AudioSample;

struct FFTBins {
  public:
    FFTBins(fl::size n) : mSize(n) {
        bins_raw.reserve(n);
        bins_db.reserve(n);
    }
    
    // Copy constructor and assignment
    FFTBins(const FFTBins &other) : bins_raw(other.bins_raw), bins_db(other.bins_db), mSize(other.mSize) {}
    FFTBins &operator=(const FFTBins &other) {
        if (this != &other) {
            mSize = other.mSize;
            bins_raw = other.bins_raw;
            bins_db = other.bins_db;
        }
        return *this;
    }
    
    // Move constructor and assignment
    FFTBins(FFTBins &&other) noexcept 
        : bins_raw(fl::move(other.bins_raw)), bins_db(fl::move(other.bins_db)), mSize(other.mSize) {}
    
    FFTBins &operator=(FFTBins &&other) noexcept {
        if (this != &other) {
            bins_raw = fl::move(other.bins_raw);
            bins_db = fl::move(other.bins_db);
            mSize = other.mSize;
        }
        return *this;
    }

    void clear() {
        bins_raw.clear();
        bins_db.clear();
    }

    fl::size size() const { return mSize; }

    // The bins are the output of the FFTImpl.
    fl::vector<float> bins_raw;
    // The frequency range of the bins.
    fl::vector<float> bins_db;

  private:
    fl::size mSize;
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
        fl::memfill(this, 0, sizeof(FFT_Args));
        this->samples = samples;
        this->bands = bands;
        this->fmin = fmin;
        this->fmax = fmax;
        this->sample_rate = sample_rate;
    }
    
    // Rule of 5 for POD data
    FFT_Args(const FFT_Args &other) = default;
    FFT_Args &operator=(const FFT_Args &other) = default;
    FFT_Args(FFT_Args &&other) noexcept = default;
    FFT_Args &operator=(FFT_Args &&other) noexcept = default;

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
