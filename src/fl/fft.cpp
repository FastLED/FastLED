
#include "fl/fft.h"
#include "fl/compiler_control.h"
#include "fl/fft_impl.h"
#include "fl/hash_map_lru.h"
#include "fl/int.h"
#include "fl/memory.h"

namespace fl {

template <> struct Hash<FFT_Args> {
    fl::u32 operator()(const FFT_Args &key) const noexcept {
        return MurmurHash3_x86_32(&key, sizeof(FFT_Args));
    }
};

struct FFT::HashMap : public HashMapLru<FFT_Args, fl::shared_ptr<FFTImpl>> {
    HashMap(fl::size max_size)
        : fl::HashMapLru<FFT_Args, fl::shared_ptr<FFTImpl>>(max_size) {}
};

FFT::FFT() { mMap.reset(new HashMap(8)); };

FFT::~FFT() = default;

FFT::FFT(const FFT &other) {
    // copy the map
    mMap.reset();
    mMap.reset(new HashMap(*other.mMap));
}

FFT &FFT::operator=(const FFT &other) {
    mMap.reset();
    mMap.reset(new HashMap(*other.mMap));
    return *this;
}

void FFT::run(const span<const fl::i16> &sample, FFTBins *out,
              const FFT_Args &args) {
    FFT_Args args2 = args;
    args2.samples = sample.size();
    get_or_create(args2).run(sample, out);
}

void FFT::clear() { mMap->clear(); }

fl::size FFT::size() const { return mMap->size(); }

void FFT::setFFTCacheSize(fl::size size) { mMap->setMaxSize(size); }

FFTImpl &FFT::get_or_create(const FFT_Args &args) {
    fl::shared_ptr<FFTImpl> *val = mMap->find_value(args);
    if (val) {
        // we have it.
        return **val;
    }
    // else we have to make a new one.
    fl::shared_ptr<FFTImpl> fft = fl::make_shared<FFTImpl>(args);
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
