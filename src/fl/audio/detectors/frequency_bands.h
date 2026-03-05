#pragma once

#include "fl/audio/audio_detector.h"
#include "fl/fft.h"
#include "fl/filter.h"
#include "fl/stl/function.h"
#include "fl/stl/shared_ptr.h"

namespace fl {

class FrequencyBands : public AudioDetector {
public:
    FrequencyBands();
    ~FrequencyBands() override;

    void update(shared_ptr<AudioContext> context) override;
    void fireCallbacks() override;
    bool needsFFT() const override { return true; }
    const char* getName() const override { return "FrequencyBands"; }
    void reset() override;
    void setSampleRate(int sampleRate) override { mSampleRate = sampleRate; }

    // Callbacks (multiple listeners supported)
    function_list<void(float bass, float mid, float treble)> onLevelsUpdate;
    function_list<void(float level)> onBassLevel;
    function_list<void(float level)> onMidLevel;
    function_list<void(float level)> onTrebleLevel;

    // State access (raw unnormalized values)
    float getBass() const { return mBass; }
    float getMid() const { return mMid; }
    float getTreble() const { return mTreble; }

    // Per-band normalized values (0.0 - 1.0, self-referential via running max)
    float getBassNorm() const { return mBassNorm; }
    float getMidNorm() const { return mMidNorm; }
    float getTrebleNorm() const { return mTrebleNorm; }

    // Configuration - set frequency ranges (in Hz)
    void setBassRange(float min, float max) { mBassMin = min; mBassMax = max; }
    void setMidRange(float min, float max) { mMidMin = min; mMidMax = max; }
    void setTrebleRange(float min, float max) { mTrebleMin = min; mTrebleMax = max; }

    // Smoothing time constant in seconds (higher = smoother)
    void setSmoothing(float tau) { mBassSmoother.setTau(tau); mMidSmoother.setTau(tau); mTrebleSmoother.setTau(tau); }

    int getSampleRate() const { return mSampleRate; }

    // Diagnostic counters
    static int getPrivateFFTCount();
    static void resetPrivateFFTCount();

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
    shared_ptr<const FFTBins> mRetainedFFT;

    float calculateBandEnergy(const FFTBins& fft, float minFreq, float maxFreq,
                              float fftMinFreq, float fftMaxFreq);
};

} // namespace fl
