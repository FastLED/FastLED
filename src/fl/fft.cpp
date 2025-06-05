
#include "fl/fft.h"
#include "fl/fft_impl.h"
#include "fl/hash_map_lru.h"
#include "fl/compiler_control.h"

namespace fl {

template <> struct Hash<FFT_Args> {
    uint32_t operator()(const FFT_Args &key) const noexcept {
        return MurmurHash3_x86_32(&key, sizeof(FFT_Args));
    }
};

struct FFT::HashMap : public fl::HashMapLru<FFT_Args, Ptr<FFTImpl>> {
    HashMap(size_t max_size)
        : fl::HashMapLru<FFT_Args, Ptr<FFTImpl>>(max_size) {}
};

FFT::FFT() { mMap.reset(new HashMap(8)); };

FFT::~FFT() = default;

void FFT::run(const Slice<const int16_t> &sample, FFTBins *out,
              const FFT_Args &args) {
    FFT_Args args2 = args;
    args2.samples = sample.size();
    get_or_create(args2).run(sample, out);
}

void FFT::clear() { mMap->clear(); }

size_t FFT::size() const { return mMap->size(); }

void FFT::setFFTCacheSize(size_t size) { mMap->setMaxSize(size); }

FFTImpl &FFT::get_or_create(const FFT_Args &args) {
    Ptr<FFTImpl> *val = mMap->find_value(args);
    if (val) {
        // we have it.
        return **val;
    }
    // else we have to make a new one.
    Ptr<FFTImpl> fft = NewPtr<FFTImpl>(args);
    (*mMap)[args] = fft;
    return *fft;
}



bool FFT_Args::operator==(const FFT_Args &other) const {
    FL_DISABLE_WARNING_PUSH
    FL_DISABLE_WARNING(float-equal);

    return samples == other.samples && bands == other.bands &&
           fmin == other.fmin && fmax == other.fmax &&
           sample_rate == other.sample_rate;

    FL_DISABLE_WARNING_POP
}

} // namespace fl
