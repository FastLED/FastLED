#pragma once

#include "fl/audio/audio_detector.h"
#include "fl/audio/audio_context.h"
#include "fl/stl/function.h"

namespace fl {

class FrequencyBands : public AudioDetector {
public:
    FrequencyBands();
    ~FrequencyBands() override;

    void update(shared_ptr<AudioContext> context) override;
    bool needsFFT() const override { return true; }
    const char* getName() const override { return "FrequencyBands"; }
    void reset() override;

    // Callbacks (multiple listeners supported)
    function_list<void(float bass, float mid, float treble)> onLevelsUpdate;
    function_list<void(float level)> onBassLevel;
    function_list<void(float level)> onMidLevel;
    function_list<void(float level)> onTrebleLevel;

    // State access
    float getBass() const { return mBass; }
    float getMid() const { return mMid; }
    float getTreble() const { return mTreble; }

    // Configuration - set frequency ranges (in Hz)
    void setBassRange(float min, float max) { mBassMin = min; mBassMax = max; }
    void setMidRange(float min, float max) { mMidMin = min; mMidMax = max; }
    void setTrebleRange(float min, float max) { mTrebleMin = min; mTrebleMax = max; }

    // Smoothing factor (0.0 = no smoothing, 1.0 = maximum smoothing)
    void setSmoothing(float smoothing) { mSmoothing = smoothing; }

private:
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

    // Smoothing
    float mSmoothing;

    float calculateBandEnergy(const FFTBins& fft, float minFreq, float maxFreq, int numBands);
};

} // namespace fl
