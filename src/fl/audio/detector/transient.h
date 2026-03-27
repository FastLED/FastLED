#pragma once

#include "fl/audio/audio_detector.h"
#include "fl/math/filter/filter.h"
#include "fl/stl/function.h"
#include "fl/stl/vector.h"
#include "fl/stl/deque.h"
#include "fl/stl/noexcept.h"

namespace fl {
namespace audio {
namespace detector {

/**
 * Transient - Detects sharp attack transients in audio
 *
 * Transients are rapid increases in energy that indicate the start of a sound,
 * such as drum hits, plucked strings, or any percussive element. This detector
 * uses spectral flux and energy envelope analysis to identify attack events.
 */
class Transient : public Detector {
public:
    Transient() FL_NOEXCEPT;
    ~Transient() FL_NOEXCEPT override;

    void update(shared_ptr<Context> context) override;
    void fireCallbacks() override;
    bool needsFFT() const override { return true; }
    const char* getName() const override { return "Transient"; }
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
    deque<float> mEnergyHistory;
    static constexpr size ENERGY_HISTORY_SIZE = 5;

    // Adaptive outlier rejection for energy before history
    HampelFilter<float, 7> mEnergyOutlierFilter{2.5f};

    shared_ptr<const fft::Bins> mRetainedFFT;

    float calculateHighFreqEnergy(const fft::Bins& fft);
    float calculateEnergyFlux(float currentEnergy);
    bool detectTransient(float flux, u32 timestamp);
    void updateAttackTime(float flux);
};

} // namespace detector
} // namespace audio
} // namespace fl
