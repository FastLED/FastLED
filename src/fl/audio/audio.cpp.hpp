
#include "fl/audio/audio.h"
#include "fl/audio/fft/fft.h"
#include "fl/math/math.h"
#include "fl/stl/move.h"
#include "fl/stl/mutex.h"
#include "fl/stl/shared_ptr.h"
#include "fl/stl/singleton.h"
#include "fl/stl/span.h"
#include "fl/stl/vector.h"
#include "fl/stl/int.h"
#include "fl/stl/noexcept.h"

namespace fl {
namespace audio {

namespace {

struct GuardedFFT {
    void run(fl::span<const fl::i16> sample, fft::Bins *out,
             const fft::Args &args) {
        fl::unique_lock<fl::mutex> lock(mtx);
        fft.run(sample, out, args);
    }

  private:
    fl::mutex mtx;
    fft::FFT fft;
};

// Object pool implementation

struct AudioSamplePool {
    void put(SampleImplPtr&& impl) {
        if (impl.unique()) {
            // There is no more shared_ptr to this object, so we can recycle it.
            fl::unique_lock<fl::mutex> lock(mutex);
            if (impl && pool.size() < MAX_POOL_SIZE) {
                // Reset the impl for reuse (clear internal state)
                impl->reset();
                pool.push_back(impl);
                return;
            }
        }
        // Pool is full, discard the impl
        impl.reset();
    }
    SampleImplPtr getOrCreate() {
        {
            fl::unique_lock<fl::mutex> lock(mutex);
            if (!pool.empty()) {
                SampleImplPtr impl = pool.back();
                pool.pop_back();
                return impl;
            }
        }
        return fl::make_shared<SampleImpl>();
    }

    fl::vector<SampleImplPtr> pool;
    static constexpr fl::size MAX_POOL_SIZE = 8;
    fl::mutex mutex;
};

} // namespace

Sample::~Sample() FL_NOEXCEPT {
    if (mImpl) {
        fl::Singleton<AudioSamplePool>::instance().put(fl::move(mImpl));
    }
}

const Sample::VectorPCM &Sample::pcm() const {
    if (isValid()) {
        return mImpl->pcm();
    }
    static VectorPCM empty;
    return empty;
}

Sample &Sample::operator=(const Sample &other) FL_NOEXCEPT {
    mImpl = other.mImpl;
    return *this;
}

fl::size Sample::size() const {
    if (isValid()) {
        return mImpl->pcm().size();
    }
    return 0;
}

const fl::i16 &Sample::at(fl::size i) const {
    if (i < size()) {
        return pcm()[i];
    }
    return empty()[0];
}

const fl::i16 &Sample::operator[](fl::size i) const { return at(i); }

bool Sample::operator==(const Sample &other) const {
    if (mImpl == other.mImpl) {
        return true;
    }
    if (mImpl == nullptr || other.mImpl == nullptr) {
        return false;
    }
    if (mImpl->pcm().size() != other.mImpl->pcm().size()) {
        return false;
    }
    for (fl::size i = 0; i < mImpl->pcm().size(); ++i) {
        if (mImpl->pcm()[i] != other.mImpl->pcm()[i]) {
            return false;
        }
    }
    return true;
}

bool Sample::operator!=(const Sample &other) const {
    return !(*this == other);
}

const Sample::VectorPCM &Sample::empty() {
    static fl::i16 empty_data[1] = {0}; // okay static in header
    static VectorPCM empty(empty_data); // okay static in header
    return empty;
}

float Sample::zcf() const { return mImpl->zcf(); }

fl::u32 Sample::timestamp() const {
    if (isValid()) {
        return mImpl->timestamp();
    }
    return 0;
}

// O(1) - returns pre-computed cached value
float Sample::rms() const {
    if (!isValid()) {
        return 0.0f;
    }
    return mImpl->rms();
}

SoundLevelMeter::SoundLevelMeter(double spl_floor, double smoothing_alpha)
    : mSplFloor(spl_floor), mSmoothingAlpha(smoothing_alpha),
      mDbfsFloorGlobal(FL_INFINITY_DOUBLE), mOffset(0.0), mCurrentDbfs(0.0),
      mCurrentSpl(spl_floor) {}

void SoundLevelMeter::processBlock(const fl::i16 *samples, fl::size count) {
    // 1) compute block power → dBFS
    double sum_sq = 0.0;
    for (fl::size i = 0; i < count; ++i) {
        double s = samples[i] / 32768.0; // normalize to ±1
        sum_sq += s * s;
    }
    double p = sum_sq / count; // mean power
    double dbfs = 10.0 * log10(p + 1e-12);
    mCurrentDbfs = dbfs;

    // 2) update global floor (with optional smoothing)
    if (dbfs < mDbfsFloorGlobal) {
        if (mSmoothingAlpha <= 0.0) {
            mDbfsFloorGlobal = dbfs;
        } else {
            mDbfsFloorGlobal = mSmoothingAlpha * dbfs +
                                 (1.0 - mSmoothingAlpha) * mDbfsFloorGlobal;
        }
        mOffset = mSplFloor - mDbfsFloorGlobal;
    }

    // 3) estimate SPL
    mCurrentSpl = dbfs + mOffset;
}

void Sample::fft(fft::Bins *out) const {
    fl::span<const fl::i16> sample = pcm();
    fft::Args args;
    args.samples = sample.size();
    args.bands = out->bands();
    args.fmin = fft::Args::DefaultMinFrequency();
    args.fmax = fft::Args::DefaultMaxFrequency();
    args.sample_rate =
        fft::Args::DefaultSampleRate(); // TODO: get sample rate from Sample
    fl::Singleton<GuardedFFT>::instance().run(sample, out, args);
}


void Sample::applyGain(float gain) {
    if (!isValid() || gain == 1.0f) return;
    auto& samples = mImpl->pcm_mutable();
    for (fl::size i = 0; i < samples.size(); ++i) {
        fl::i32 val = static_cast<fl::i32>(static_cast<float>(samples[i]) * gain);
        if (val > 32767) val = 32767;
        if (val < -32768) val = -32768;
        samples[i] = static_cast<fl::i16>(val);
    }
}

Sample::Sample(fl::span<const fl::i16> span, fl::u32 timestamp) {
    mImpl = fl::Singleton<AudioSamplePool>::instance().getOrCreate();
    auto begin = span.data();
    auto end = begin + span.size();
    mImpl->assign(begin, end, timestamp);
}


} // namespace audio
} // namespace fl
