#pragma once

#include "fl/scoped_ptr.h"
#include "fl/slice.h"
#include "fl/vector.h"
#include "fl/pair.h"


namespace fl {


class AudioSample;
class FFTContext;

struct FFTBins {
  public:
    FFTBins() = default;
    // The bins are the output of the FFT.
    fl::vector<float> bins_raw;
    // The frequency range of the bins.
    fl::vector<float> bins_db;
    void clear() {
      bins_raw.clear();
      bins_db.clear();
    }
};

// Example:
//   FFT fft(512, 16);
//   auto sample = SINE WAVE OF 512 SAMPLES
//   fft.run(buffer, &out);
//   FASTLED_WARN("FFT output: " << out);  // 16 bands of output.
class FFT {
  public:
    // Output bins for FFT. This is the output when the fft is run.
    // using OutputBins = fl::vector_inlined<float, 16>;

    using OutputBins = FFTBins;

    // Result indicating success or failure of the FFT run (in which case there
    // will be an error message).
    struct Result {
      Result(bool ok, const Str& error) : ok(ok), error(error) {}
      bool ok = false;
      fl::Str error;
    };
    // Default values for the FFT.
    FFT(int samples = DefaultSamples(), int bands = DefaultBands(),
        float fmin = DefaultMinFrequency(), float fmax = DefaultMaxFrequency(),
        int sample_rate = DefaultSampleRate());
    ~FFT();

    size_t sampleSize() const;
    // Note that the sample sizes MUST match the samples size passed into the constructor.
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
    FFT(const FFT &) = delete;
    FFT &operator=(const FFT &) = delete;
    FFT(FFT &&) = delete;
    FFT &operator=(FFT &&) = delete;

  private:
    fl::scoped_ptr<FFTContext> mContext;
};

}; // namespace fl