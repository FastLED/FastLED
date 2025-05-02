
#include "audio.h"

namespace fl {

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

size_t AudioSample::size() const {
    if (isValid()) {
        return mImpl->pcm().size();
    }
    return 0;
}

const int16_t &AudioSample::at(size_t i) const {
    if (i < size()) {
        return pcm()[i];
    }
    return empty()[0];
}

const int16_t &AudioSample::operator[](size_t i) const { return at(i); }

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
    for (size_t i = 0; i < mImpl->pcm().size(); ++i) {
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
    static int16_t empty_data[1] = {0};
    static VectorPCM empty(empty_data);
    return empty;
}

float AudioSample::zcr() const { return mImpl->zcf(); }

SoundLevelMeter::SoundLevelMeter(double spl_floor, double smoothing_alpha)
    : spl_floor_(spl_floor), smoothing_alpha_(smoothing_alpha),
      dbfs_floor_global_(INFINITY_DOUBLE), offset_(0.0), current_dbfs_(0.0),
      current_spl_(spl_floor) {}

void SoundLevelMeter::processBlock(const int16_t *samples, size_t count) {
    // 1) compute block power → dBFS
    double sum_sq = 0.0;
    for (size_t i = 0; i < count; ++i) {
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

} // namespace fl