#pragma once

#include "fl/fft.h"
#include "fl/math.h"
#include "fl/memory.h"
#include "fl/span.h"
#include "fl/vector.h"
#include "fl/int.h"
#include <math.h>
#include "fl/stdint.h"
#include "fl/span.h"

namespace fl {

class AudioSampleImpl;

FASTLED_SMART_PTR(AudioSampleImpl);

// AudioSample is a wrapper around AudioSampleImpl, hiding the reference
// counting so that the api object can be simple and have standard object
// semantics.
class AudioSample {
  public:
    using VectorPCM = fl::vector<fl::i16>;
    using const_iterator = VectorPCM::const_iterator;
    AudioSample() {}
    AudioSample(const AudioSample &other) : mImpl(other.mImpl) {}
    AudioSample(AudioSampleImplPtr impl) : mImpl(impl) {}
    ~AudioSample();
    
    // Constructor that takes raw audio data and handles pooling internally
    AudioSample(fl::span<const fl::i16> span, fl::u32 timestamp = 0);


    AudioSample &operator=(const AudioSample &other);
    bool isValid() const { return mImpl != nullptr; }

    fl::size size() const;
    // Raw pcm levels.
    const VectorPCM &pcm() const;
    // Zero crossing factor between 0.0f -> 1.0f, detects "hiss"
    // and sounds like cloths rubbing. Useful for sound analysis.
    float zcf() const;
    float rms() const;
    fl::u32 timestamp() const;  // Timestamp when sample became valid (millis)

    void fft(FFTBins *out) const;

    const_iterator begin() const { return pcm().begin(); }
    const_iterator end() const { return pcm().end(); }
    const fl::i16 &at(fl::size i) const;
    const fl::i16 &operator[](fl::size i) const;
    operator bool() const { return isValid(); }
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
    SoundLevelMeter(double spl_floor = 33.0, double smoothing_alpha = 0.0);

    /// Process a block of int16 PCM samples.
    void processBlock(const fl::i16 *samples, fl::size count);
            void processBlock(fl::span<const fl::i16> samples) {
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
class AudioSampleImpl {
  public:
    using VectorPCM = fl::vector<fl::i16>;
    ~AudioSampleImpl() {}
    // template <typename It> void assign(It begin, It end) {
    //     assign(begin, end, 0);  // Default timestamp to 0
    // }
    template <typename It> void assign(It begin, It end, fl::u32 timestamp) {
        mSignedPcm.assign(begin, end);
        mTimestamp = timestamp;
        // calculate zero crossings
        initZeroCrossings();
    }
    const VectorPCM &pcm() const { return mSignedPcm; }
    fl::u32 timestamp() const { return mTimestamp; }
    
    // For object pool - reset internal state for reuse
    void reset() {
        mSignedPcm.clear();
        mZeroCrossings = 0;
        mTimestamp = 0;
    }

    // "Zero crossing factor". High values > .4 indicate hissing
    // sounds. For example a microphone rubbing against a clothing.
    // These types of signals indicate the audio should be ignored.
    // Low zero crossing factors (with loud sound) indicate that there
    // is organized sound like that coming from music. This is so cheap
    // to calculate it's done automatically. It should be one of the first
    // signals to reject or accept a sound signal.
    //
    // Returns: a value -> [0.0f, 1.0f)
    float zcf() const {
        const fl::size n = pcm().size();
        if (n < 2) {
            return 0.f;
        }
        return float(mZeroCrossings) / static_cast<float>(n - 1);
    }

  private:
    void initZeroCrossings() {
        mZeroCrossings = 0;
        if (mSignedPcm.size() > 1) {
            for (fl::size i = 1; i < mSignedPcm.size(); ++i) {
                const bool crossed =
                    (mSignedPcm[i - 1] < 0 && mSignedPcm[i] >= 0) ||
                    (mSignedPcm[i - 1] >= 0 && mSignedPcm[i] < 0);
                if (crossed) {
                    ++mZeroCrossings;
                }
            }
        }
    }

    VectorPCM mSignedPcm;
    fl::i16 mZeroCrossings = 0;
    fl::u32 mTimestamp = 0;
};

} // namespace fl
