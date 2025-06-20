#pragma once

#include "fl/fft.h"
#include "fl/math.h"
#include "fl/ptr.h"
#include "fl/slice.h"
#include "fl/vector.h"
#include <math.h>
#include "fl/stdint.h"

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
    AudioSample();
    AudioSample(const AudioSample &other);
    AudioSample(AudioSampleImplPtr impl);
    AudioSample &operator=(const AudioSample &other);
    bool isValid() const;

    size_t size() const;
    // Raw pcm levels.
    const VectorPCM &pcm() const;
    // Zero crossing factor between 0.0f -> 1.0f, detects "hiss"
    // and sounds like cloths rubbing. Useful for sound analysis.
    float zcf() const;
    float rms() const;

    void fft(FFTBins *out);

    const_iterator begin() const;
    const_iterator end() const;
    const int16_t &at(size_t i) const;
    const int16_t &operator[](size_t i) const;
    operator bool() const;
    bool operator==(const AudioSample &other) const;
    bool operator!=(const AudioSample &other) const;

  private:
    static const VectorPCM &empty();
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
    void processBlock(fl::Slice<const int16_t> samples);

    /// @returns most recent block's level in dBFS (≤ 0)
    double getDBFS() const;

    /// @returns calibrated estimate in dB SPL
    double getSPL() const;

    /// change your known noise-floor SPL at runtime
    void setFloorSPL(double spl_floor);

    /// reset so the next quiet block will re-initialize your floor
    void resetFloor();

  private:
    double spl_floor_;         // e.g. 33.0 dB SPL
    double smoothing_alpha_;   // 0 = pure min, >0 = slow adapt
    double dbfs_floor_global_; // lowest dBFS seen so far
    double offset_;            // spl_floor_ − dbfs_floor_global_
    double current_dbfs_;      // last block's dBFS
    double current_spl_;       // last block's estimated SPL
};

// Implementation details.
class AudioSampleImpl : public fl::Referent {
  public:
    using VectorPCM = fl::vector<int16_t>;
    ~AudioSampleImpl();
    template <typename It> void assign(It begin, It end) {
      mSignedPcm.assign(begin, end);
      initZeroCrossings();
    }
    const VectorPCM &pcm() const;

    // "Zero crossing factor". High values > .4 indicate hissing
    // sounds. For example a microphone rubbing against a clothing.
    // These types of signals indicate the audio should be ignored.
    // Low zero crossing factors (with loud sound) indicate that there
    // is organized sound like that coming from music. This is so cheap
    // to calculate it's done automatically. It should be one of the first
    // signals to reject or accept a sound signal.
    //
    // Returns: a value -> [0.0f, 1.0f)
    float zcf() const;

  private:
    void initZeroCrossings();

    VectorPCM mSignedPcm;
    int16_t mZeroCrossings = 0;
};

} // namespace fl
