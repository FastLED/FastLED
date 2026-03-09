
#include "fl/audio/fft/fft.h"
#include "fl/compiler_control.h"
#include "fl/audio/fft/fft_impl.h"
#include "fl/hash_map_lru.h"
#include "fl/int.h"
#include "fl/stl/shared_ptr.h"  // For shared_ptr

namespace fl {

template <> struct Hash<FFT_Args> {
    fl::u32 operator()(const FFT_Args &key) const noexcept {
        // Hash fields individually to avoid padding-byte issues
        fl::u32 h = 0;
        h ^= MurmurHash3_x86_32(&key.samples, sizeof(key.samples));
        h ^= MurmurHash3_x86_32(&key.bands, sizeof(key.bands)) * 2654435761u;
        h ^= MurmurHash3_x86_32(&key.fmin, sizeof(key.fmin)) * 2246822519u;
        h ^= MurmurHash3_x86_32(&key.fmax, sizeof(key.fmax)) * 3266489917u;
        h ^= MurmurHash3_x86_32(&key.sample_rate, sizeof(key.sample_rate)) * 668265263u;
        int mode_int = static_cast<int>(key.mode);
        h ^= MurmurHash3_x86_32(&mode_int, sizeof(mode_int)) * 374761393u;
        return h;
    }
};

struct FFT::HashMap : public HashMapLru<FFT_Args, fl::shared_ptr<FFTImpl>> {
    HashMap(fl::size max_size)
        : fl::HashMapLru<FFT_Args, fl::shared_ptr<FFTImpl>>(max_size) {}
};

FFT::FFT() { mMap = fl::make_unique<HashMap>(8); };

FFT::~FFT() = default;

FFT::FFT(const FFT &other) {
    // copy the map
    mMap = fl::make_unique<HashMap>(*other.mMap);
}

FFT &FFT::operator=(const FFT &other) {
    mMap = fl::make_unique<HashMap>(*other.mMap);
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
           sample_rate == other.sample_rate && mode == other.mode;

    FL_DISABLE_WARNING_POP
}

} // namespace fl
