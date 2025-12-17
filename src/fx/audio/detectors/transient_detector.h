#pragma once

#include "fl/stl/vector.h"
#include "fl/audio/audio_context.h"

namespace fl {

class TransientDetector {
public:
    TransientDetector();
    ~TransientDetector();

    void update(shared_ptr<AudioContext> context);
    void reset();

    // Callback types
    using TransientCallback = void(*)();
    using TransientStrengthCallback = void(*)(float strength);

    // Callback functions
    TransientCallback onTransient = nullptr;           // Simple transient detection
    TransientStrengthCallback onTransientWithStrength = nullptr;  // With strength parameter
    TransientStrengthCallback onAttack = nullptr;      // Attack phase of transient

    // Configurable parameters
    void setThreshold(float threshold) { mThreshold = threshold; }
    void setSensitivity(float sensitivity) { mSensitivity = sensitivity; }

    // Getters
    bool isTransientDetected() const { return mTransientDetected; }
    float getStrength() const { return mStrength; }
    float getAttackTime() const { return mAttackTime; }

private:
    // Constants
    static constexpr size_t ENERGY_HISTORY_SIZE = 32;

    // Detection state
    bool mTransientDetected;
    float mStrength;
    float mThreshold;
    float mSensitivity;
    u32 mMinIntervalMs;
    u32 mLastTransientTime;

    // Energy tracking
    float mPreviousEnergy;
    float mCurrentEnergy;
    fl::vector<float> mPreviousHighFreq;
    fl::vector<float> mEnergyHistory;

    // Attack time estimation
    float mAttackTime;

    // Internal processing methods
    float calculateHighFreqEnergy(const FFTBins& fft);
    float calculateEnergyFlux(float currentEnergy);
    bool detectTransient(float flux, u32 timestamp);
    void updateAttackTime(float flux);
};

} // namespace fl
