#pragma once

#include "fl/audio/audio_context.h"

namespace fl {

struct Drop {
    float impact = 0.0f;       // Impact strength of the drop (0-1)
    float bassEnergy = 0.0f;   // Bass energy at the time of drop
    float energyIncrease = 0.0f; // Relative energy increase
    u32 timestamp = 0;        // When the drop occurred

    // Default constructor
    Drop() = default;
};

class DropDetector {
public:
    DropDetector();
    ~DropDetector();

    void update(shared_ptr<AudioContext> context);
    void reset();

    // Callback types
    using VoidCallback = void(*)();
    using DropCallback = void(*)(const Drop&);
    using ImpactCallback = void(*)(float);

    // Callbacks for drop events
    VoidCallback onDrop = nullptr;               // Simplest drop event (just happened)
    DropCallback onDropEvent = nullptr;          // Detailed drop event information
    ImpactCallback onDropImpact = nullptr;       // Drop impact strength callback

    // Configuration methods
    void setImpactThreshold(float threshold) { mImpactThreshold = threshold; }
    void setMinTimeBetweenDrops(u32 ms) { mMinTimeBetweenDrops = ms; }
    void setBassThreshold(float threshold) { mBassThreshold = threshold; }
    void setEnergyFluxThreshold(float threshold) { mEnergyFluxThreshold = threshold; }

    // Getters
    const Drop& getLastDrop() const { return mLastDrop; }
    float getCurrentImpact(shared_ptr<AudioContext> context) const;

private:
    // Previous frame data for comparisons
    float mPrevRMS;
    float mPrevBassEnergy;
    float mPrevMidEnergy;
    float mPrevTrebleEnergy;

    // Baselines for energy
    float mEnergyBaseline;
    float mBassBaseline;

    // Detection thresholds
    float mImpactThreshold;       // Impact level required to trigger drop
    u32 mMinTimeBetweenDrops;     // Cooldown between detected drops
    float mBassThreshold;         // Bass energy threshold for drop
    float mEnergyFluxThreshold;   // Minimum energy change required

    // Last detected drop
    Drop mLastDrop;

    // Energy calculation helpers
    float getBassEnergy(const FFTBins& fft) const;
    float getMidEnergy(const FFTBins& fft) const;
    float getTrebleEnergy(const FFTBins& fft) const;

    // Drop detection calculations
    float calculateSpectralNovelty(float bass, float mid, float treble) const;
    float calculateEnergyFlux(float currentRMS) const;
    float calculateBassFlux(float currentBass) const;
    float calculateDropImpact(float energyFlux, float bassFlux, float spectralNovelty, float rms) const;
    bool shouldTriggerDrop(float impact, uint32_t timestamp) const;

    // Baseline update method for adaptive detection
    void updateBaselines(float rms, float bass);
};

} // namespace fl
