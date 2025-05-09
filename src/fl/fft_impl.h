#pragma once

#include "fl/hash_map_lru.h"
#include "fl/pair.h"
#include "fl/scoped_ptr.h"
#include "fl/slice.h"
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
class FFTImpl : public fl::Referent {
  public:
    // Result indicating success or failure of the FFTImpl run (in which case
    // there will be an error message).
    struct Result {
        Result(bool ok, const Str &error) : ok(ok), error(error) {}
        bool ok = false;
        fl::Str error;
    };
    // Default values for the FFTImpl.
    FFTImpl(const FFT_Args &args);
    ~FFTImpl();

    size_t sampleSize() const;
    // Note that the sample sizes MUST match the samples size passed into the
    // constructor.
    Result run(const AudioSample &sample, FFTBins *out);
    Result run(Slice<const int16_t> sample, FFTBins *out);
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

}; // namespace fl