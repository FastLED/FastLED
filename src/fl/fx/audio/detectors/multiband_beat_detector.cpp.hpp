#include "fl/fx/audio/detectors/multiband_beat_detector.h"
#include "fl/stl/math.h"

namespace fl {

MultiBandBeatDetector::MultiBandBeatDetector() = default;

MultiBandBeatDetector::MultiBandBeatDetector(const MultiBandBeatDetectorConfig& config) {
    configure(config);
}

MultiBandBeatDetector::~MultiBandBeatDetector() = default;

void MultiBandBeatDetector::configure(const MultiBandBeatDetectorConfig& config) {
    mConfig = config;
    reset();
}

void MultiBandBeatDetector::detectBeats(span<const float> frequencyBins) {
    // Verify input size
    if (frequencyBins.size() < 16) {
        // Invalid input - reset beat flags
        mBassBeat = false;
        mMidBeat = false;
        mTrebleBeat = false;
        return;
    }

    // Calculate current energies
    float bassEnergy = calculateBassEnergy(frequencyBins);
    float midEnergy = calculateMidEnergy(frequencyBins);
    float trebleEnergy = calculateTrebleEnergy(frequencyBins);

    // Decrement cooldown counters
    if (mBassCooldown > 0) mBassCooldown--;
    if (mMidCooldown > 0) mMidCooldown--;
    if (mTrebleCooldown > 0) mTrebleCooldown--;

    // Detect beats in each band
    mBassBeat = detectBandBeat(bassEnergy, mPreviousBassEnergy,
                               mConfig.bassThreshold, mBassCooldown);
    mMidBeat = detectBandBeat(midEnergy, mPreviousMidEnergy,
                              mConfig.midThreshold, mMidCooldown);
    mTrebleBeat = detectBandBeat(trebleEnergy, mPreviousTrebleEnergy,
                                 mConfig.trebleThreshold, mTrebleCooldown);

    // Cross-band correlation: if multiple bands trigger, boost confidence
    if (mConfig.enableCrossBandCorrelation) {
        u32 bandCount = 0;
        if (mBassBeat) bandCount++;
        if (mMidBeat) bandCount++;
        if (mTrebleBeat) bandCount++;

        // Multi-band beat detected
        if (bandCount >= 2) {
            mStats.multiBandBeats++;
        }
    }

    // Update statistics
    if (mBassBeat) mStats.bassBeats++;
    if (mMidBeat) mStats.midBeats++;
    if (mTrebleBeat) mStats.trebleBeats++;

    // Store current energies for next frame
    mPreviousBassEnergy = bassEnergy;
    mPreviousMidEnergy = midEnergy;
    mPreviousTrebleEnergy = trebleEnergy;

    // Update current energies in stats
    mBassEnergy = bassEnergy;
    mMidEnergy = midEnergy;
    mTrebleEnergy = trebleEnergy;

    mStats.bassEnergy = bassEnergy;
    mStats.midEnergy = midEnergy;
    mStats.trebleEnergy = trebleEnergy;

    mCurrentFrame++;
}

bool MultiBandBeatDetector::isBassBeat() const {
    return mBassBeat;
}

bool MultiBandBeatDetector::isMidBeat() const {
    return mMidBeat;
}

bool MultiBandBeatDetector::isTrebleBeat() const {
    return mTrebleBeat;
}

float MultiBandBeatDetector::getBassEnergy() const {
    return mBassEnergy;
}

float MultiBandBeatDetector::getMidEnergy() const {
    return mMidEnergy;
}

float MultiBandBeatDetector::getTrebleEnergy() const {
    return mTrebleEnergy;
}

bool MultiBandBeatDetector::isMultiBandBeat() const {
    u32 bandCount = 0;
    if (mBassBeat) bandCount++;
    if (mMidBeat) bandCount++;
    if (mTrebleBeat) bandCount++;
    return bandCount >= 2;
}

void MultiBandBeatDetector::reset() {
    mBassBeat = false;
    mMidBeat = false;
    mTrebleBeat = false;

    mBassEnergy = 0.0f;
    mMidEnergy = 0.0f;
    mTrebleEnergy = 0.0f;

    mPreviousBassEnergy = 0.0f;
    mPreviousMidEnergy = 0.0f;
    mPreviousTrebleEnergy = 0.0f;

    mBassCooldown = 0;
    mMidCooldown = 0;
    mTrebleCooldown = 0;

    mCurrentFrame = 0;

    mStats.bassBeats = 0;
    mStats.midBeats = 0;
    mStats.trebleBeats = 0;
    mStats.multiBandBeats = 0;
    mStats.bassEnergy = 0.0f;
    mStats.midEnergy = 0.0f;
    mStats.trebleEnergy = 0.0f;
}

bool MultiBandBeatDetector::detectBandBeat(float currentEnergy, float previousEnergy,
                                           float threshold, u32& cooldownCounter) {
    // Check if cooldown is active
    if (cooldownCounter > 0) {
        return false;
    }

    // Need previous energy to calculate relative increase
    // Skip beat detection on first frame (no baseline)
    if (previousEnergy <= 0.0001f) {
        return false;
    }

    // Calculate energy increase
    float energyIncrease = currentEnergy - previousEnergy;

    // Must be a significant increase
    if (energyIncrease <= 0.0f) {
        return false;
    }

    // Calculate relative increase (percentage)
    float relativeIncrease = energyIncrease / previousEnergy;

    // Detect beat if relative increase exceeds threshold
    if (relativeIncrease > threshold) {
        // Activate cooldown
        cooldownCounter = mConfig.beatCooldownFrames;
        return true;
    }

    return false;
}

float MultiBandBeatDetector::calculateBassEnergy(span<const float> frequencyBins) const {
    // Average bins 0-1 (20-80 Hz)
    float sum = 0.0f;
    for (size i = BASS_BIN_START; i < BASS_BIN_END; ++i) {
        sum += frequencyBins[i];
    }
    return sum / static_cast<float>(BASS_BIN_END - BASS_BIN_START);
}

float MultiBandBeatDetector::calculateMidEnergy(span<const float> frequencyBins) const {
    // Average bins 6-7 (320-640 Hz)
    float sum = 0.0f;
    for (size i = MID_BIN_START; i < MID_BIN_END; ++i) {
        sum += frequencyBins[i];
    }
    return sum / static_cast<float>(MID_BIN_END - MID_BIN_START);
}

float MultiBandBeatDetector::calculateTrebleEnergy(span<const float> frequencyBins) const {
    // Average bins 14-15 (5120-16000 Hz)
    float sum = 0.0f;
    for (size i = TREBLE_BIN_START; i < TREBLE_BIN_END; ++i) {
        sum += frequencyBins[i];
    }
    return sum / static_cast<float>(TREBLE_BIN_END - TREBLE_BIN_START);
}

} // namespace fl
