#pragma once

#include "fl/hash_map_lru.h"
#include "fl/pair.h"
#include "fl/unique_ptr.h"
#include "fl/span.h"
#include "fl/vector.h"

namespace fl {

class AudioSample;
class FFTContext;
struct FFT_Args;

// Example:
//   FFTImpl fft(512, 16);
//   auto sample = SINE WAVE OF 512 SAMPLES
//   fft.run(buffer, &out);
//   FASTLED_WARN("FFTImpl output: " << out);  // 16 bands of output.
class FFTImpl {
  public:
    // Result indicating success or failure of the FFTImpl run (in which case
    // there will be an error message).
    struct Result {
        Result(bool ok, const string &error) : ok(ok), error(error) {}
        bool ok = false;
        fl::string error;
    };
    // Default values for the FFTImpl.
    FFTImpl(const FFT_Args &args);
    ~FFTImpl();

    fl::size sampleSize() const;
    // Note that the sample sizes MUST match the samples size passed into the
    // constructor.
    Result run(const AudioSample &sample, FFTBins *out);
    Result run(span<const i16> sample, FFTBins *out);
    // Info on what the frequency the bins represent
    fl::string info() const;

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
    fl::unique_ptr<FFTContext> mContext;
};

}; // namespace fl
