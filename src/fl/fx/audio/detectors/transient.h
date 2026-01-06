#pragma once

#include "fl/audio/audio_detector.h"
#include "fl/audio/audio_context.h"
#include "fl/stl/function.h"
#include "fl/stl/vector.h"

namespace fl {

/**
 * TransientDetector - Detects sharp attack transients in audio
 *
 * Transients are rapid increases in energy that indicate the start of a sound,
 * such as drum hits, plucked strings, or any percussive element. This detector
 * uses spectral flux and energy envelope analysis to identify attack events.
 */
class TransientDetector : public AudioDetector {
public:
    TransientDetector();
    ~TransientDetector() override;

    void update(shared_ptr<AudioContext> context) override;
    bool needsFFT() const override { return true; }
    const char* getName() const override { return "TransientDetector"; }
    void reset() override;

    // Callbacks (multiple listeners supported)
    function_list<void()> onTransient;
    function_list<void(float strength)> onTransientWithStrength;
    function_list<void(float strength)> onAttack;

    // State access
    bool isTransient() const { return mTransientDetected; }
    float getStrength() const { return mStrength; }
    float getAttackTime() const { return mAttackTime; }

    // Configuration
    void setThreshold(float threshold) { mThreshold = threshold; }
    void setSensitivity(float sensitivity) { mSensitivity = sensitivity; }
    void setMinInterval(u32 intervalMs) { mMinIntervalMs = intervalMs; }

private:
    bool mTransientDetected;
    float mStrength;
    float mThreshold;
    float mSensitivity;
    u32 mMinIntervalMs;
    u32 mLastTransientTime;

    // Energy tracking
    float mPreviousEnergy;
    float mCurrentEnergy;
    float mAttackTime;  // Time in ms for attack phase

    // High-frequency emphasis for transient detection
    vector<float> mPreviousHighFreq;
    vector<float> mEnergyHistory;
    static constexpr size ENERGY_HISTORY_SIZE = 5;

    float calculateHighFreqEnergy(const FFTBins& fft);
    float calculateEnergyFlux(float currentEnergy);
    bool detectTransient(float flux, u32 timestamp);
    void updateAttackTime(float flux);
};

} // namespace fl
