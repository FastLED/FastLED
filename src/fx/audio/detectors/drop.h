// DropDetector.h - EDM drop detection for FastLED audio system
// Detects sudden energy bursts and dramatic changes typical of EDM drops

#pragma once

#include "fl/audio/audio_detector.h"
#include "fl/audio/audio_context.h"
#include "fl/stl/function.h"

namespace fl {

// Forward declarations
class AudioContext;

// Drop event structure
struct Drop {
    float impact;           // Impact strength (0.0 to 1.0)
    float bassEnergy;       // Bass energy at drop
    float energyIncrease;   // Energy increase from previous state
    uint32_t timestamp;     // When drop occurred

    Drop()
        : impact(0.0f)
        , bassEnergy(0.0f)
        , energyIncrease(0.0f)
        , timestamp(0) {}
};

// DropDetector - Detects sudden energy bursts and drops in EDM music
//
// EDM drops are characterized by:
// 1. Sudden, dramatic energy increase (energy burst)
// 2. Strong bass impact (sub-bass and bass frequencies)
// 3. High spectral change (new elements introduced)
// 4. Often follows a buildup or break
//
// The detector uses multiple indicators to identify drops with high confidence:
// - Energy flux (rate of energy change)
// - Bass energy surge
// - Spectral novelty (dramatic frequency content change)
// - Temporal context (time since last drop)
//
// Optimized for EDM, trap, dubstep, future bass, and similar genres.
//
// Usage:
//   DropDetector drop;
//   drop.onDrop([](const Drop& dropEvent) { /* React to drop */ });
//   drop.onDropImpact([](float impact) { /* Scale effect by impact */ });
//
class DropDetector : public AudioDetector {
public:
    DropDetector();
    ~DropDetector() override;

    // AudioDetector interface
    void update(shared_ptr<AudioContext> context) override;
    bool needsFFT() const override { return true; }
    bool needsFFTHistory() const override { return false; }
    const char* getName() const override { return "DropDetector"; }
    void reset() override;

    // Event callbacks (multiple listeners supported)
    function_list<void()> onDrop;                          // Fired when drop detected
    function_list<void(const Drop&)> onDropEvent;          // Fired with drop details
    function_list<void(float impact)> onDropImpact;               // Fired with impact strength

    // State access
    const Drop& getLastDrop() const { return mLastDrop; }
    uint32_t getTimeSinceLastDrop(uint32_t currentTime) const {
        return currentTime - mLastDrop.timestamp;
    }

    // Configuration
    void setImpactThreshold(float threshold) { mImpactThreshold = threshold; }
    void setMinTimeBetweenDrops(uint32_t ms) { mMinTimeBetweenDrops = ms; }
    void setBassThreshold(float threshold) { mBassThreshold = threshold; }
    void setEnergyFluxThreshold(float threshold) { mEnergyFluxThreshold = threshold; }

private:
    // Last drop event
    Drop mLastDrop;

    // Previous frame state for flux calculation
    float mPrevRMS;
    float mPrevBassEnergy;
    float mPrevMidEnergy;
    float mPrevTrebleEnergy;

    // Short-term energy tracking (for baseline)
    float mEnergyBaseline;      // Rolling average of recent energy
    float mBassBaseline;        // Rolling average of recent bass

    // Configuration
    float mImpactThreshold;         // Minimum impact to trigger drop
    uint32_t mMinTimeBetweenDrops;  // Cooldown period between drops (ms)
    float mBassThreshold;           // Minimum bass energy ratio
    float mEnergyFluxThreshold;     // Minimum energy increase ratio

    // Analysis methods
    float getBassEnergy(const FFTBins& fft) const;
    float getMidEnergy(const FFTBins& fft) const;
    float getTrebleEnergy(const FFTBins& fft) const;
    float calculateSpectralNovelty(float bass, float mid, float treble) const;
    float calculateEnergyFlux(float currentRMS) const;
    float calculateBassFlux(float currentBass) const;
    float calculateDropImpact(float energyFlux, float bassFlux, float spectralNovelty, float rms) const;
    bool shouldTriggerDrop(float impact, uint32_t timestamp) const;
    void updateBaselines(float rms, float bass);
};

} // namespace fl
