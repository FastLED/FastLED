#pragma once

#include "fl/fft.h"
#include "fl/stl/math.h"
#include "fl/stl/shared_ptr.h"         // For FASTLED_SHARED_PTR macros
#include "fl/stl/span.h"
#include "fl/stl/vector.h"
#include "fl/int.h"
#include "fl/stl/stdint.h"

namespace fl {

class AudioSampleImpl;

FASTLED_SHARED_PTR(AudioSampleImpl);

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

    /// @returns most recent block's level in dBFS (≤ 0)
    double getDBFS() const { return mCurrentDbfs; }

    /// @returns calibrated estimate in dB SPL
    double getSPL() const { return mCurrentSpl; }

    /// change your known noise-floor SPL at runtime
    void setFloorSPL(double spl_floor) {
        mSplFloor = spl_floor;
        mOffset = mSplFloor - mDbfsFloorGlobal;
    }

    /// reset so the next quiet block will re-initialize your floor
    void resetFloor() {
        mDbfsFloorGlobal = FL_INFINITY_DOUBLE; // infinity<double>
        mOffset = 0.0;
    }

  private:
    double mSplFloor;         // e.g. 33.0 dB SPL
    double mSmoothingAlpha;   // 0 = pure min, >0 = slow adapt
    double mDbfsFloorGlobal; // lowest dBFS seen so far
    double mOffset;            // mSplFloor − mDbfsFloorGlobal
    double mCurrentDbfs;      // last block's dBFS
    double mCurrentSpl;       // last block's estimated SPL
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
        // Pre-compute zero crossings for O(1) access
        initZeroCrossings();
        // RMS is computed lazily on first access to avoid blocking
        mRmsComputed = false;
    }
    const VectorPCM &pcm() const { return mSignedPcm; }
    fl::u32 timestamp() const { return mTimestamp; }

    // For object pool - reset internal state for reuse
    void reset() {
        mSignedPcm.clear();
        mZeroCrossings = 0;
        mRms = 0.0f;
        mTimestamp = 0;
        mRmsComputed = false;
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
    // O(1) - pre-computed in constructor
    float zcf() const {
        const fl::size n = pcm().size();
        if (n < 2) {
            return 0.f;
        }
        return float(mZeroCrossings) / static_cast<float>(n - 1);
    }

    // Root mean square amplitude of the audio signal
    // Returns: RMS value (computed lazily on first access)
    float rms() const {
        if (!mRmsComputed) {
            const_cast<AudioSampleImpl*>(this)->initRms();
            const_cast<AudioSampleImpl*>(this)->mRmsComputed = true;
        }
        return mRms;
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

    void initRms() {
        if (mSignedPcm.empty()) {
            mRms = 0.0f;
            return;
        }
        fl::u64 sum_sq = 0;
        const int N = mSignedPcm.size();
        for (int i = 0; i < N; ++i) {
            fl::i32 x32 = fl::i32(mSignedPcm[i]);
            sum_sq += x32 * x32;
        }
        mRms = sqrtf(float(sum_sq) / N);
    }

    VectorPCM mSignedPcm;
    fl::i16 mZeroCrossings = 0;
    float mRms = 0.0f;  // Lazily computed RMS value
    fl::u32 mTimestamp = 0;
    mutable bool mRmsComputed = false;  // Track if RMS has been computed
};

} // namespace fl
