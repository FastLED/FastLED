// BuildupDetector.h - EDM buildup detection for FastLED audio system
// Detects rising energy and tension patterns typical of EDM buildups

#pragma once

#include "fl/audio/audio_detector.h"
#include "fl/audio/audio_context.h"
#include "fl/stl/function.h"

namespace fl {

// Forward declarations
class AudioContext;

// Buildup event structure
struct Buildup {
    float intensity;        // Buildup intensity (0.0 to 1.0)
    float progress;         // Progress through buildup (0.0 to 1.0)
    u32 duration;      // Duration in milliseconds
    u32 timestamp;     // When buildup started
    bool active;            // Whether buildup is currently active

    Buildup()
        : intensity(0.0f)
        , progress(0.0f)
        , duration(0)
        , timestamp(0)
        , active(false) {}
};

// BuildupDetector - Detects rising energy and tension patterns in EDM music
//
// EDM buildups are characterized by:
// 1. Rising energy over time (energy ramp)
// 2. Increasing high-frequency content (filter sweeps)
// 3. Increasing spectral complexity (layering)
// 4. Sustained duration (typically 4-16 seconds)
//
// The detector tracks multiple indicators and combines them to detect buildups
// with high confidence. It's optimized for EDM, trap, dubstep, and similar genres.
//
// Usage:
//   BuildupDetector buildup;
//   buildup.onBuildupStart([]() { /* React to buildup start */ });
//   buildup.onBuildupProgress([](float progress) { /* Update UI */ });
//   buildup.onBuildupPeak([]() { /* Trigger drop effect */ });
//
class BuildupDetector : public AudioDetector {
public:
    BuildupDetector();
    ~BuildupDetector() override;

    // AudioDetector interface
    void update(shared_ptr<AudioContext> context) override;
    void fireCallbacks() override;
    bool needsFFT() const override { return true; }
    bool needsFFTHistory() const override { return false; }
    const char* getName() const override { return "BuildupDetector"; }
    void reset() override;

    // Event callbacks (multiple listeners supported)
    function_list<void()> onBuildupStart;                      // Fired when buildup starts
    function_list<void(float progress)> onBuildupProgress;              // Fired during buildup (0.0-1.0)
    function_list<void()> onBuildupPeak;                       // Fired at peak (just before drop)
    function_list<void()> onBuildupEnd;                        // Fired when buildup ends (cancelled)
    function_list<void(const Buildup&)> onBuildup;             // Fired every frame during buildup

    // State access
    bool isBuilding() const { return mBuildupActive; }
    float getIntensity() const { return mCurrentBuildup.intensity; }
    float getProgress() const { return mCurrentBuildup.progress; }
    const Buildup& getBuildup() const { return mCurrentBuildup; }

    // Configuration
    void setMinDuration(u32 ms) { mMinDuration = ms; }
    void setMaxDuration(u32 ms) { mMaxDuration = ms; }
    void setIntensityThreshold(float threshold) { mIntensityThreshold = threshold; }
    void setEnergyRiseThreshold(float threshold) { mEnergyRiseThreshold = threshold; }

private:
    // Current buildup state
    Buildup mCurrentBuildup;
    bool mBuildupActive;
    bool mPeakFired;

    // Energy tracking for rise detection
    float mEnergyHistory[32];  // Last 32 frames (~0.7s at 44.1kHz, 512 samples)
    int mEnergyHistoryIndex;
    int mEnergyHistorySize;

    // High-frequency content tracking
    float mTrebleHistory[16];  // Last 16 frames (~0.35s)
    int mTrebleHistoryIndex;
    int mTrebleHistorySize;

    // Previous frame state
    float mPrevEnergy;
    float mPrevTreble;
    u32 mLastUpdateTime;

    // Configuration
    u32 mMinDuration;          // Minimum buildup duration (ms)
    u32 mMaxDuration;          // Maximum buildup duration (ms)
    float mIntensityThreshold;      // Minimum intensity to start buildup
    float mEnergyRiseThreshold;     // Minimum energy rise rate

    // Analysis methods
    float calculateEnergyTrend() const;      // Calculate energy rise trend
    float calculateTrebleTrend() const;      // Calculate treble rise trend
    float calculateBuildupIntensity(float energyTrend, float trebleTrend, float rms) const;
    bool shouldStartBuildup(float intensity) const;
    bool shouldEndBuildup() const;
    bool shouldPeak() const;
    void updateEnergyHistory(float energy);
    void updateTrebleHistory(float treble);
    float getTrebleEnergy(const FFTBins& fft) const;

    // Pending callback flags
    bool mFireBuildupStart = false;
    bool mFireBuildupPeak = false;
    bool mFireBuildupEnd = false;
    bool mFireBuildupProgress = false;
    bool mFireBuildup = false;
};

} // namespace fl
