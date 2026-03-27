
#include "fl/audio/fft/fft.h"
#include "fl/stl/compiler_control.h"
#include "fl/audio/fft/fft_impl.h"
#include "fl/stl/unordered_map_lru.h"
#include "fl/stl/int.h"
#include "fl/stl/shared_ptr.h"  // For shared_ptr
#include "fl/stl/singleton.h"
#include "fl/stl/mutex.h"
#include "fl/stl/noexcept.h"

namespace fl {
namespace audio {
namespace fft {

// Recycles fl::vector<float> buffers to avoid repeated allocation.
// Vectors returned to the pool retain their capacity for reuse.
class FloatVectorPool {
  public:
    fl::vector<float> acquire(fl::size capacity) {
        for (fl::size i = 0; i < mPool.size(); ++i) {
            if (mPool[i].capacity() >= capacity) {
                fl::vector<float> v = fl::move(mPool[i]);
                mPool[i] = fl::move(mPool.back());
                mPool.pop_back();
                v.clear();
                return v;
            }
        }
        fl::vector<float> v;
        v.reserve(capacity);
        return v;
    }

    void release(fl::vector<float>&& v) {
        if (v.capacity() > 0 && mPool.size() < kMaxPoolSize) {
            v.clear();
            mPool.push_back(fl::move(v));
        }
    }

    void releaseIfNotEmpty(fl::vector<float>&& v) {
        if (!v.empty()) {
            release(fl::move(v));
        }
    }

  private:
    static const fl::size kMaxPoolSize = 32;
    fl::vector<fl::vector<float>> mPool;
};

void Bins::clear() {
    mBinsRaw.clear();
    mBinsLinear.clear();
    mNormFactors.clear();
    mNormalizedDirty = true;
    mDbDirty = true;
}

fl::size Bins::bands() const { return mBands; }

fl::span<const float> Bins::raw() const { return mBinsRaw; }

fl::span<const float> Bins::db() const {
    if (!mDbDirty) {
        return mBinsDb;
    }
    if (mBinsDb.capacity() == 0) {
        mBinsDb = pool().acquire(mBands);
    }
    mBinsDb.resize(mBinsRaw.size());
    for (fl::size i = 0; i < mBinsRaw.size(); ++i) {
        float x = mBinsRaw[i];
        mBinsDb[i] = (x > 0.0f) ? 20.0f * fl::log10f(x) : 0.0f;
    }
    mDbDirty = false;
    return mBinsDb;
}

fl::span<const float> Bins::rawNormalized() const {
    if (!mNormalizedDirty) {
        return mBinsRawNormalized;
    }
    if (mBinsRawNormalized.capacity() == 0) {
        mBinsRawNormalized = pool().acquire(mBands);
    }
    mBinsRawNormalized.resize(mBinsRaw.size());
    for (fl::size i = 0; i < mBinsRaw.size(); ++i) {
        float norm = (i < mNormFactors.size()) ? mNormFactors[i] : 1.0f;
        mBinsRawNormalized[i] = mBinsRaw[i] * norm;
    }
    mNormalizedDirty = false;
    return mBinsRawNormalized;
}

fl::span<const float> Bins::linear() const { return mBinsLinear; }
float Bins::linearFmin() const { return mLinearFmin; }
float Bins::linearFmax() const { return mLinearFmax; }
float Bins::fmin() const { return mFmin; }
float Bins::fmax() const { return mFmax; }
int Bins::sampleRate() const { return mSampleRate; }

float Bins::binToFreq(int i) const {
    int nbands = static_cast<int>(mBinsRaw.size());
    if (nbands <= 1) return mFmin;
    float m = fl::logf(mFmax / mFmin);
    return mFmin * fl::expf(m * static_cast<float>(i) / static_cast<float>(nbands - 1));
}

int Bins::freqToBin(float freq) const {
    int nbands = static_cast<int>(mBinsRaw.size());
    if (nbands <= 1) return 0;
    if (freq <= mFmin) return 0;
    if (freq >= mFmax) return nbands - 1;
    float m = fl::logf(mFmax / mFmin);
    float bin = fl::logf(freq / mFmin) / m * static_cast<float>(nbands - 1);
    int result = static_cast<int>(bin + 0.5f);
    if (result < 0) return 0;
    if (result >= nbands) return nbands - 1;
    return result;
}

float Bins::binBoundary(int i) const {
    float f_i = binToFreq(i);
    float f_next = binToFreq(i + 1);
    return fl::sqrtf(f_i * f_next);
}

fl::vector<float>& Bins::raw_mut() {
    if (mBinsRaw.capacity() == 0) {
        mBinsRaw = pool().acquire(mBands);
    }
    mNormalizedDirty = true;
    mDbDirty = true;
    return mBinsRaw;
}

fl::vector<float>& Bins::linear_mut() {
    if (mBinsLinear.capacity() == 0) {
        mBinsLinear = pool().acquire(mBands);
    }
    return mBinsLinear;
}

void Bins::setParams(float fmin, float fmax, int sampleRate) {
    mFmin = fmin;
    mFmax = fmax;
    mSampleRate = sampleRate;
}

void Bins::setLinearParams(float linearFmin, float linearFmax) {
    mLinearFmin = linearFmin;
    mLinearFmax = linearFmax;
}

void Bins::setNormFactors(const fl::vector<float>& factors) {
    if (mNormFactors.capacity() == 0) {
        mNormFactors = pool().acquire(mBands);
    }
    mNormFactors.resize(factors.size());
    for (fl::size i = 0; i < factors.size(); ++i) {
        mNormFactors[i] = factors[i];
    }
    mNormalizedDirty = true;
}

FloatVectorPool& Bins::pool() {
    return Singleton<FloatVectorPool>::instance();
}

Bins::Bins(fl::size n)
    : mBands(n) {}

Bins::~Bins() FL_NOEXCEPT {
    auto& p = pool();
    p.releaseIfNotEmpty(fl::move(mBinsRaw));
    p.releaseIfNotEmpty(fl::move(mBinsLinear));
    p.releaseIfNotEmpty(fl::move(mNormFactors));
    p.releaseIfNotEmpty(fl::move(mBinsDb));
    p.releaseIfNotEmpty(fl::move(mBinsRawNormalized));
}

} // namespace fft
} // namespace audio

// Hash specialization must be in fl:: namespace where Hash is defined
template <> struct Hash<audio::fft::Args> {
    fl::u32 operator()(const audio::fft::Args &key) const FL_NOEXCEPT {
        // Hash fields individually to avoid padding-byte issues
        fl::u32 h = 0;
        h ^= MurmurHash3_x86_32(&key.samples, sizeof(key.samples));
        h ^= MurmurHash3_x86_32(&key.bands, sizeof(key.bands)) * 2654435761u;
        h ^= MurmurHash3_x86_32(&key.fmin, sizeof(key.fmin)) * 2246822519u;
        h ^= MurmurHash3_x86_32(&key.fmax, sizeof(key.fmax)) * 3266489917u;
        h ^= MurmurHash3_x86_32(&key.sample_rate, sizeof(key.sample_rate)) * 668265263u;
        int mode_int = static_cast<int>(key.mode);
        h ^= MurmurHash3_x86_32(&mode_int, sizeof(mode_int)) * 374761393u;
        int window_int = static_cast<int>(key.window);
        h ^= MurmurHash3_x86_32(&window_int, sizeof(window_int)) * 2246822519u;
        return h;
    }
};

namespace audio {
namespace fft {

struct FFT::ImplCache {
    static constexpr fl::size kDefaultMaxSize = 10;
    using LruMap = HashMapLru<Args, fl::shared_ptr<Impl>>;

    ImplCache() : mMap(kDefaultMaxSize) {}

    fl::shared_ptr<Impl> get_or_create(const Args &args) {
        fl::lock_guard<fl::mutex> lock(mMutex);
        fl::shared_ptr<Impl> *val = mMap.find_value(args);
        if (val) {
            return *val;
        }
        fl::shared_ptr<Impl> fft = fl::make_shared<Impl>(args);
        mMap[args] = fft;
        return fft;
    }

    void clear() {
        fl::lock_guard<fl::mutex> lock(mMutex);
        mMap.clear();
    }

    fl::size size() {
        fl::lock_guard<fl::mutex> lock(mMutex);
        return mMap.size();
    }

    void setMaxSize(fl::size max_size) {
        fl::lock_guard<fl::mutex> lock(mMutex);
        mMap.setMaxSize(max_size);
    }

private:
    fl::mutex mMutex;
    LruMap mMap;
};

FFT::ImplCache &FFT::globalCache() {
    // Global LRU cache with max 10 entries — shared by all FFT instances.
    // This avoids regenerating expensive CQ kernels when AudioContext is
    // recreated (each Impl with 128 CQ bins takes ~850ms to initialize).
    return fl::Singleton<ImplCache>::instance();
}

void FFT::run(const span<const fl::i16> &sample, Bins *out,
              const Args &args) {
    Args args2 = args;
    args2.samples = sample.size();
    // Fetch cached impl (thread-safe), then run FFT outside the lock.
    fl::shared_ptr<Impl> impl = globalCache().get_or_create(args2);
    impl->run(sample, out);
}

void FFT::clear() { globalCache().clear(); }

fl::size FFT::size() const { return globalCache().size(); }

void FFT::setFFTCacheSize(fl::size size) { globalCache().setMaxSize(size); }

void Args::resolveModeEnums(Mode &mode, Window &window, int bands,
                                int samples, float fmin, float fmax) {
    // Resolve mode first
    if (mode == Mode::AUTO) {
        if (bands <= 32) {
            mode = Mode::LOG_REBIN;
        } else {
            // Check kernel conditioning: N_window = N * fmin / fmax.
            // When >= 2, CQ_NAIVE (single FFT + kernels) works well.
            // When < 2, kernels degenerate and we need octave-wise CQT.
            int winMin = static_cast<int>(
                static_cast<float>(samples) * fmin / fmax);
            mode = (winMin >= 2) ? Mode::CQ_NAIVE : Mode::CQ_OCTAVE;
        }
    }

    // Resolve window based on resolved mode
    if (window == Window::AUTO) {
        switch (mode) {
        case Mode::LOG_REBIN:
        case Mode::CQ_HYBRID:
            // These paths apply time-domain windowing before fft::FFT.
            // BLACKMAN_HARRIS: -92 dB sidelobe rejection (vs -31 dB HANNING).
            // Sidelobes produce only mag=0-1 quantization noise, ensuring
            // distant output bins have near-zero energy (≤2) for pure tones.
            // Main lobe is 8 FFT bins wide (~690 Hz at 512/44100), so
            // output bins narrower than ~130 Hz may see main lobe leakage
            // at distance 3+ — a fundamental resolution limit.
            window = Window::BLACKMAN_HARRIS;
            break;
        case Mode::CQ_NAIVE:
        case Mode::CQ_OCTAVE:
            // CQ kernels already apply Hamming windowing in frequency domain.
            // No time-domain window is applied on these paths, so this is
            // cosmetic. HANNING is the lighter/cheaper default.
            window = Window::HANNING;
            break;
        default:
            window = Window::BLACKMAN_HARRIS;
            break;
        }
    }
}

bool Args::operator==(const Args &other) const {
    FL_DISABLE_WARNING_PUSH
    FL_DISABLE_WARNING(float-equal);

    return samples == other.samples && bands == other.bands &&
           fmin == other.fmin && fmax == other.fmax &&
           sample_rate == other.sample_rate && mode == other.mode &&
           window == other.window;

    FL_DISABLE_WARNING_POP
}

} // namespace fft
} // namespace audio
} // namespace fl
