#include "fl/audio/noise_floor_tracker.h"
#include "fl/stl/algorithm.h"
#include "fl/stl/math.h"

namespace fl {

NoiseFloorTracker::NoiseFloorTracker() {
    configure(NoiseFloorTrackerConfig{});
    mStats.currentFloor = mCurrentFloor;  // Initialize stats to match internal state
}

NoiseFloorTracker::NoiseFloorTracker(const NoiseFloorTrackerConfig& config) {
    configure(config);
    mStats.currentFloor = mCurrentFloor;  // Initialize stats to match internal state
}

NoiseFloorTracker::~NoiseFloorTracker() = default;

void NoiseFloorTracker::configure(const NoiseFloorTrackerConfig& config) {
    mConfig = config;
}

void NoiseFloorTracker::reset() {
    mCurrentFloor = 100.0f;
    mLastHysteresisFloor = 0.0f;  // Start at 0 to allow initial floor rise
    mBelowFloorCount = 0;
    mStats.currentFloor = 100.0f;  // Initialize to match mCurrentFloor
    mStats.minObserved = 0.0f;
    mStats.maxObserved = 0.0f;
    mStats.samplesProcessed = 0;
    mStats.inHysteresis = false;
}

void NoiseFloorTracker::update(float timedomainLevel, float frequencydomainLevel) {
    if (!mConfig.enabled) {
        return;
    }

    // Combine time and frequency domain metrics if both provided
    float combinedLevel = combineDomains(timedomainLevel, frequencydomainLevel);

    // Update statistics
    if (mStats.samplesProcessed == 0) {
        mStats.minObserved = combinedLevel;
        mStats.maxObserved = combinedLevel;

        // Initialize floor to first observed level
        mCurrentFloor = fl::max(mConfig.minFloor, fl::min(mConfig.maxFloor, combinedLevel));
        mLastHysteresisFloor = mCurrentFloor;
    } else {
        mStats.minObserved = fl::min(mStats.minObserved, combinedLevel);
        mStats.maxObserved = fl::max(mStats.maxObserved, combinedLevel);
    }
    mStats.samplesProcessed++;

    // Update floor based on current observation
    updateFloor(combinedLevel);

    // Update stats
    mStats.currentFloor = mCurrentFloor;
}

float NoiseFloorTracker::normalize(float level) const {
    // Remove noise floor from signal, clamped to non-negative
    const float normalized = level - mCurrentFloor;
    return fl::max(0.0f, normalized);
}

bool NoiseFloorTracker::isAboveFloor(float level) const {
    // Check if signal exceeds floor plus hysteresis margin
    return level > (mCurrentFloor + mConfig.hysteresisMargin);
}

void NoiseFloorTracker::updateFloor(float level) {
    // Exponential moving average tracking of noise floor
    // with asymmetric rates for rise (attack) and fall (decay)
    //
    // Hysteresis is achieved through the slow attack rate, not through
    // explicit blocking of updates

    if (level < mCurrentFloor) {
        // Signal is below floor - floor should decay (decrease) rapidly toward signal
        // Use decay rate (typically high, like 0.99) for downward movement
        const float decayAlpha = mConfig.decayRate;
        mCurrentFloor = decayAlpha * mCurrentFloor + (1.0f - decayAlpha) * level;

        mBelowFloorCount++;
        mStats.inHysteresis = false;

        // Track floor drops for statistics
        if (mLastHysteresisFloor > 0.0f && (mLastHysteresisFloor - mCurrentFloor) >= mConfig.hysteresisMargin) {
            mLastHysteresisFloor = mCurrentFloor;
        }
    } else {
        // Signal is above floor - floor should rise slowly toward signal
        // Use attack rate (typically low, like 0.01-0.05) for upward movement
        // This slow rise prevents chasing transient peaks
        const float attackAlpha = 1.0f - mConfig.attackRate;
        mCurrentFloor = attackAlpha * mCurrentFloor + mConfig.attackRate * level;

        mBelowFloorCount = 0;
        mStats.inHysteresis = false;

        // Update hysteresis reference when floor rises significantly
        if (mCurrentFloor - mLastHysteresisFloor >= mConfig.hysteresisMargin) {
            mLastHysteresisFloor = mCurrentFloor;
            mStats.inHysteresis = true;  // Indicate significant rise
        }
    }

    // Clamp to configured range
    mCurrentFloor = fl::max(mConfig.minFloor, fl::min(mConfig.maxFloor, mCurrentFloor));
}

float NoiseFloorTracker::combineDomains(float timeLevel, float freqLevel) const {
    // If no frequency-domain metric provided, use time-domain only
    if (freqLevel < 0.0f) {
        return timeLevel;
    }

    // Weighted average of time and frequency domain metrics
    const float w = mConfig.crossDomainWeight;
    return (1.0f - w) * timeLevel + w * freqLevel;
}

} // namespace fl
