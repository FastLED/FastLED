
#include "fl/fft.h"

namespace fl {


FFT::FFT() {
   mMap.reset(new HashMap(8));
};

FFT::~FFT() = default;

void FFT::run(const Slice<const int16_t> &sample, FFTBins *out,
              const FFT_Args &args) {
    FFT_Args args2 = args;
    args2.samples = sample.size();
    get_or_create(args2).run(sample, out);
}

void FFT::clear() { mMap->clear(); }

size_t FFT::size() const { return mMap->size(); }

} // namespace fl
