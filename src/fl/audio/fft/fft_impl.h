#pragma once

#include "fl/stl/span.h"
#include "fl/stl/string.h"
#include "fl/stl/unique_ptr.h"
#include "fl/stl/noexcept.h"

namespace fl {
namespace audio {

class Sample;  // Forward declare in fl::audio (correct namespace)

namespace fft {

class Context;
struct Args;

// Example:
//   Impl fft(512, 16);
//   auto sample = SINE WAVE OF 512 SAMPLES
//   fft.run(buffer, &out);
//   FL_WARN("Impl output: " << out);  // 16 bands of output.
class Impl {
  public:
    // Result indicating success or failure of the Impl run (in which case
    // there will be an error message).
    struct Result {
        Result(bool ok, const string &error) : ok(ok), error(error) {}
        bool ok = false;
        fl::string error;
    };
    // Default values for the Impl.
    Impl(const Args &args);
    ~Impl() FL_NOEXCEPT;

    fl::size sampleSize() const;
    // Note that the sample sizes MUST match the samples size passed into the
    // constructor.
    Result run(const Sample &sample, Bins *out);
    Result run(span<const i16> sample, Bins *out);
    // Info on what the frequency the bins represent
    fl::string info() const;

    // Disable copy and move constructors and assignment operators
    Impl(const Impl &) FL_NOEXCEPT = delete;
    Impl &operator=(const Impl &) FL_NOEXCEPT = delete;
    Impl(Impl &&) FL_NOEXCEPT = delete;
    Impl &operator=(Impl &&) FL_NOEXCEPT = delete;

  private:
    fl::unique_ptr<Context> mContext;
};

} // namespace fft
} // namespace audio
}; // namespace fl
