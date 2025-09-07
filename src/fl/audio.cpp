
#include "audio.h"
#include "fl/thread_local.h"
#include "fl/int.h"
#include "fl/mutex.h"

namespace fl {

namespace {

FFT &get_flex_fft() {
    static ThreadLocal<FFT> gFlexFFT;
    return gFlexFFT.access();
}

// Object pool implementation

struct AudioSamplePool {
    static AudioSamplePool& instance() {
        static AudioSamplePool s_pool;
        return s_pool;
    }
    void put(AudioSampleImplPtr&& impl) {
        if (impl.unique()) {
            // There is no more shared_ptr to this object, so we can recycle it.
            fl::lock_guard<fl::mutex> lock(mutex);
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
    AudioSampleImplPtr getOrCreate() {
        {
            fl::lock_guard<fl::mutex> lock(mutex);
            if (!pool.empty()) {
                AudioSampleImplPtr impl = pool.back();
                pool.pop_back();
                return impl;
            }
        }
        return fl::make_shared<AudioSampleImpl>();
    }

    fl::vector<AudioSampleImplPtr> pool;
    static constexpr fl::size MAX_POOL_SIZE = 8;
    fl::mutex mutex;
};

} // namespace

AudioSample::~AudioSample() {
    if (mImpl) {
        AudioSamplePool::instance().put(fl::move(mImpl));
    }
}

const AudioSample::VectorPCM &AudioSample::pcm() const {
    if (isValid()) {
        return mImpl->pcm();
    }
    static VectorPCM empty;
    return empty;
}

AudioSample &AudioSample::operator=(const AudioSample &other) {
    mImpl = other.mImpl;
    return *this;
}

fl::size AudioSample::size() const {
    if (isValid()) {
        return mImpl->pcm().size();
    }
    return 0;
}

const fl::i16 &AudioSample::at(fl::size i) const {
    if (i < size()) {
        return pcm()[i];
    }
    return empty()[0];
}

const fl::i16 &AudioSample::operator[](fl::size i) const { return at(i); }

bool AudioSample::operator==(const AudioSample &other) const {
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

bool AudioSample::operator!=(const AudioSample &other) const {
    return !(*this == other);
}

const AudioSample::VectorPCM &AudioSample::empty() {
    static fl::i16 empty_data[1] = {0};
    static VectorPCM empty(empty_data);
    return empty;
}

float AudioSample::zcf() const { return mImpl->zcf(); }

fl::u32 AudioSample::timestamp() const {
    if (isValid()) {
        return mImpl->timestamp();
    }
    return 0;
}

float AudioSample::rms() const {
    if (!isValid()) {
        return 0.0f;
    }
    fl::u64 sum_sq = 0;
    const int N = size();
    for (int i = 0; i < N; ++i) {
        fl::i32 x32 = fl::i32(pcm()[i]);
        sum_sq += x32 * x32;
    }
    float rms = sqrtf(float(sum_sq) / N);
    return rms;
}

SoundLevelMeter::SoundLevelMeter(double spl_floor, double smoothing_alpha)
    : spl_floor_(spl_floor), smoothing_alpha_(smoothing_alpha),
      dbfs_floor_global_(INFINITY_DOUBLE), offset_(0.0), current_dbfs_(0.0),
      current_spl_(spl_floor) {}

void SoundLevelMeter::processBlock(const fl::i16 *samples, fl::size count) {
    // 1) compute block power → dBFS
    double sum_sq = 0.0;
    for (fl::size i = 0; i < count; ++i) {
        double s = samples[i] / 32768.0; // normalize to ±1
        sum_sq += s * s;
    }
    double p = sum_sq / count; // mean power
    double dbfs = 10.0 * log10(p + 1e-12);
    current_dbfs_ = dbfs;

    // 2) update global floor (with optional smoothing)
    if (dbfs < dbfs_floor_global_) {
        if (smoothing_alpha_ <= 0.0) {
            dbfs_floor_global_ = dbfs;
        } else {
            dbfs_floor_global_ = smoothing_alpha_ * dbfs +
                                 (1.0 - smoothing_alpha_) * dbfs_floor_global_;
        }
        offset_ = spl_floor_ - dbfs_floor_global_;
    }

    // 3) estimate SPL
    current_spl_ = dbfs + offset_;
}

void AudioSample::fft(FFTBins *out) const {
            fl::span<const fl::i16> sample = pcm();
    FFT_Args args;
    args.samples = sample.size();
    args.bands = out->size();
    args.fmin = FFT_Args::DefaultMinFrequency();
    args.fmax = FFT_Args::DefaultMaxFrequency();
    args.sample_rate =
        FFT_Args::DefaultSampleRate(); // TODO: get sample rate from AudioSample
    get_flex_fft().run(sample, out, args);
}


AudioSample::AudioSample(fl::span<const fl::i16> span, fl::u32 timestamp) {
    mImpl = AudioSamplePool::instance().getOrCreate();
    auto begin = span.data();
    auto end = begin + span.size();
    mImpl->assign(begin, end, timestamp);
}


} // namespace fl
