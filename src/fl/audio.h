#pragma once

#include "fl/math.h"
#include "fl/ptr.h"
#include "fl/slice.h"
#include "fl/vector.h"
#include <math.h>
#include <stdint.h>

namespace fl {

class AudioSampleImpl;

FASTLED_SMART_PTR(AudioSampleImpl);

// AudioSample is a wrapper around AudioSampleImpl, hiding the reference
// counting so that the api object can be simple and have standard object
// semantics.
class AudioSample {
  public:
    using VectorPCM = fl::vector<int16_t>;
    using const_iterator = VectorPCM::const_iterator;
    AudioSample() {}
    AudioSample(const AudioSample &other) : mImpl(other.mImpl) {}
    AudioSample(AudioSampleImplPtr impl) : mImpl(impl) {}
    AudioSample &operator=(const AudioSample &other);
    bool isValid() const { return mImpl != nullptr; }
    const VectorPCM &pcm() const;
    size_t size() const;
    const_iterator begin() const { return pcm().begin(); }
    const_iterator end() const { return pcm().end(); }
    const int16_t& at(size_t i) const;
    const int16_t& operator[](size_t i) const;
    operator bool() const { return isValid(); }
    bool operator==(const AudioSample &other) const;
    bool operator!=(const AudioSample &other) const;

  private:
    static const VectorPCM& empty() ;
    AudioSampleImplPtr mImpl;
};

// Sound level meter is a persistant measuring class that will auto-tune the
// microphone to real world SPL levels. It will adapt to the noise floor of the
// environment. Note that the microphone only ever outputs DBFS (dB Full Scale)
// values, which are collected over a stream of samples. The sound level meter
// will convert this to SPL (Sound Pressure Level) values, which are the real
// world values.
class SoundLevelMeter {
  public:
    /// @param spl_floor  The SPL (dB SPL) that corresponds to your true
    /// noise-floor.
    /// @param smoothing_alpha  [0…1] how quickly to adapt floor; 0=instant min.
    SoundLevelMeter(double spl_floor = 33.0f, double smoothing_alpha = 0.0);

    /// Process a block of int16 PCM samples.
    void processBlock(const int16_t *samples, size_t count);
    void processBlock(fl::Slice<const int16_t> samples) {
        processBlock(samples.data(), samples.size());
    }

    /// @returns most recent block’s level in dBFS (≤ 0)
    double getDBFS() const { return current_dbfs_; }

    /// @returns calibrated estimate in dB SPL
    double getSPL() const { return current_spl_; }

    /// change your known noise-floor SPL at runtime
    void setFloorSPL(double spl_floor) {
        spl_floor_ = spl_floor;
        offset_ = spl_floor_ - dbfs_floor_global_;
    }

    /// reset so the next quiet block will re-initialize your floor
    void resetFloor() {
        dbfs_floor_global_ = INFINITY_DOUBLE; // infinity<double>
        offset_ = 0.0;
    }

  private:
    double spl_floor_;         // e.g. 33.0 dB SPL
    double smoothing_alpha_;   // 0 = pure min, >0 = slow adapt
    double dbfs_floor_global_; // lowest dBFS seen so far
    double offset_;            // spl_floor_ − dbfs_floor_global_
    double current_dbfs_;      // last block’s dBFS
    double current_spl_;       // last block’s estimated SPL
};

// Implementation details.
class AudioSampleImpl : public fl::Referent {
  public:
    using VectorPCM = fl::vector<int16_t>;
    ~AudioSampleImpl() {}
    template <typename It> void assign(It begin, It end) {
        mSignedPcm.assign(begin, end);
    }
    const VectorPCM &pcm() const { return mSignedPcm; }

  private:
    VectorPCM mSignedPcm;
};

} // namespace fl