#pragma once

#include "fl/audio/audio_detector.h"
#include "fl/audio/fft/fft.h"
#include "fl/math/filter/filter.h"
#include "fl/stl/function.h"
#include "fl/stl/shared_ptr.h"
#include "fl/stl/noexcept.h"

namespace fl {
namespace audio {
namespace detector {

class FrequencyBands : public Detector {
public:
    FrequencyBands() FL_NO_EXCEPT;
    ~FrequencyBands() FL_NO_EXCEPT override;

    void update(shared_ptr<Context> context) FL_NO_EXCEPT override;
    void fireCallbacks() FL_NO_EXCEPT override;
    bool needsFFT() const FL_NO_EXCEPT override { return true; }
    const char* getName() const FL_NO_EXCEPT override { return "FrequencyBands"; }
    void reset() FL_NO_EXCEPT override;
    void setSampleRate(int sampleRate) FL_NO_EXCEPT override { mSampleRate = sampleRate; }

    // Callbacks (multiple listeners supported)
    function_list<void(float bass, float mid, float treble)> onLevelsUpdate;
    function_list<void(float level)> onBassLevel;
    function_list<void(float level)> onMidLevel;
    function_list<void(float level)> onTrebleLevel;

    // State access (raw unnormalized values)
    float getBass() const FL_NO_EXCEPT { return mBass; }
    float getMid() const FL_NO_EXCEPT { return mMid; }
    float getTreble() const FL_NO_EXCEPT { return mTreble; }

    // Per-band normalized values (0.0 - 1.0, self-referential via running max)
    float getBassNorm() const FL_NO_EXCEPT { return mBassNorm; }
    float getMidNorm() const FL_NO_EXCEPT { return mMidNorm; }
    float getTrebleNorm() const FL_NO_EXCEPT { return mTrebleNorm; }

    // Configuration - set frequency ranges (in Hz)
    void setBassRange(float min, float max) FL_NO_EXCEPT { mBassMin = min; mBassMax = max; }
    void setMidRange(float min, float max) FL_NO_EXCEPT { mMidMin = min; mMidMax = max; }
    void setTrebleRange(float min, float max) FL_NO_EXCEPT { mTrebleMin = min; mTrebleMax = max; }

    // Smoothing time constant in seconds (higher = smoother)
    void setSmoothing(float tau) FL_NO_EXCEPT { mBassSmoother.setTau(tau); mMidSmoother.setTau(tau); mTrebleSmoother.setTau(tau); }

    int getSampleRate() const FL_NO_EXCEPT { return mSampleRate; }

    // Diagnostic counters
    static int getPrivateFFTCount() FL_NO_EXCEPT;
    static void resetPrivateFFTCount() FL_NO_EXCEPT;

private:
    int mSampleRate = 44100;
    float mBass;
    float mMid;
    float mTreble;

    // Frequency ranges (Hz)
    float mBassMin;
    float mBassMax;
    float mMidMin;
    float mMidMax;
    float mTrebleMin;
    float mTrebleMax;

    // Smoothing (time-aware exponential smoothing)
    ExponentialSmoother<float> mBassSmoother{0.05f};
    ExponentialSmoother<float> mMidSmoother{0.05f};
    ExponentialSmoother<float> mTrebleSmoother{0.05f};

    // Per-band adaptive normalization (same pattern as EnergyAnalyzer)
    AttackDecayFilter<float> mBassMaxFilter{0.001f, 4.0f, 0.0f};
    AttackDecayFilter<float> mMidMaxFilter{0.001f, 4.0f, 0.0f};
    AttackDecayFilter<float> mTrebleMaxFilter{0.001f, 4.0f, 0.0f};
    float mBassNorm = 0.0f;
    float mMidNorm = 0.0f;
    float mTrebleNorm = 0.0f;

    // Retained FFT from context (keeps shared_ptr alive during update)
    shared_ptr<const fft::Bins> mRetainedFFT;

    float calculateBandEnergy(const fft::Bins& fft, float minFreq, float maxFreq,
                              float fftMinFreq, float fftMaxFreq) FL_NO_EXCEPT;
};

} // namespace detector
} // namespace audio
} // namespace fl
