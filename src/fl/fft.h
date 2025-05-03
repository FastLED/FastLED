#pragma once

#include "fl/hash_map_lru.h"
#include "fl/pair.h"
#include "fl/scoped_ptr.h"
#include "fl/slice.h"
#include "fl/vector.h"

namespace fl {

class AudioSample;
class FFTContext;

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

    bool operator==(const FFT_Args &other) const {
        return samples == other.samples && bands == other.bands &&
               fmin == other.fmin && fmax == other.fmax &&
               sample_rate == other.sample_rate;
    }
    bool operator!=(const FFT_Args &other) const { return !(*this == other); }
};

template <> struct Hash<FFT_Args> {
    uint32_t operator()(const FFT_Args &key) const noexcept {
        return MurmurHash3_x86_32(&key, sizeof(FFT_Args));
    }
};

// Example:
//   FFTImpl fft(512, 16);
//   auto sample = SINE WAVE OF 512 SAMPLES
//   fft.run(buffer, &out);
//   FASTLED_WARN("FFTImpl output: " << out);  // 16 bands of output.
class FFTImpl : public fl::Referent {
  public:
    // Output bins for FFTImpl. This is the output when the fft is run.
    // using OutputBins = fl::vector_inlined<float, 16>;

    using OutputBins = FFTBins;

    // Result indicating success or failure of the FFTImpl run (in which case there
    // will be an error message).
    struct Result {
        Result(bool ok, const Str &error) : ok(ok), error(error) {}
        bool ok = false;
        fl::Str error;
    };
    // Default values for the FFTImpl.
    FFTImpl(FFT_Args args = FFT_Args());
    ~FFTImpl();

    size_t sampleSize() const;
    // Note that the sample sizes MUST match the samples size passed into the
    // constructor.
    Result run(const AudioSample &sample, OutputBins *out);
    Result run(Slice<const int16_t> sample, OutputBins *out);
    // Info on what the frequency the bins represent
    fl::Str info() const;

    // Detail.
    static int DefaultSamples() { return 512; }
    static int DefaultBands() { return 16; }
    static float DefaultMinFrequency() { return 174.6f; }
    static float DefaultMaxFrequency() { return 4698.3f; }
    static int DefaultSampleRate() { return 44100; }

    // Disable copy and move constructors and assignment operators
    FFTImpl(const FFTImpl &) = delete;
    FFTImpl &operator=(const FFTImpl &) = delete;
    FFTImpl(FFTImpl &&) = delete;
    FFTImpl &operator=(FFTImpl &&) = delete;

  private:
    fl::scoped_ptr<FFTContext> mContext;
};

class FlexFFT {
  public:
    FlexFFT() = default;
    ~FlexFFT() = default;

    void run(const Slice<const int16_t> &sample, FFTImpl::OutputBins *out,
             const FFT_Args &args = FFT_Args()) {
        FFT_Args args2 = args;
        args2.samples = sample.size();
        get_or_create(args2).run(sample, out);
    }

    void clear() { mMap.clear(); }

    size_t size() const { return mMap.size(); }

  private:
    // Get the FFTImpl for the given arguments.
    FFTImpl &get_or_create(const FFT_Args &args) {
        Ptr<FFTImpl> *val = mMap.find_value(args);
        if (val) {
            // we have it.
            return **val;
        }
        // else we have to make a new one.
        Ptr<FFTImpl> fft = NewPtr<FFTImpl>(args);
        mMap[args] = fft;
        return *fft;
    }

    using HashMap = fl::HashMapLru<FFT_Args, Ptr<FFTImpl>>;
    HashMap mMap = HashMap(8);
};

}; // namespace fl