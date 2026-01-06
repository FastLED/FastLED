// PercussionDetector - Drum-specific detection implementation
// Part of FastLED Audio System v2.0 - Phase 3 (Differentiators)

#include "fl/fx/audio/detectors/percussion.h"
#include "fl/audio/audio_context.h"
#include "fl/stl/math.h"

namespace fl {

PercussionDetector::PercussionDetector()
    : mKickThreshold(0.7f)
    , mSnareThreshold(0.6f)
    , mHiHatThreshold(0.5f)
    , mPrevBassEnergy(0.0f)
    , mPrevMidEnergy(0.0f)
    , mPrevTrebleEnergy(0.0f)
    , mLastKickTime(0)
    , mLastSnareTime(0)
    , mLastHiHatTime(0)
{}

PercussionDetector::~PercussionDetector() = default;

void PercussionDetector::update(shared_ptr<AudioContext> context) {
    const FFTBins& fft = context->getFFT(16);
    uint32_t timestamp = context->getTimestamp();

    // Calculate energy in each frequency band
    float bassEnergy = getBassEnergy(fft);
    float midEnergy = getMidEnergy(fft);
    float trebleEnergy = getTrebleEnergy(fft);

    // Calculate spectral flux (energy change) in each band
    float bassFlux = fl::fl_max(0.0f, bassEnergy - mPrevBassEnergy);
    float midFlux = fl::fl_max(0.0f, midEnergy - mPrevMidEnergy);
    float trebleFlux = fl::fl_max(0.0f, trebleEnergy - mPrevTrebleEnergy);

    // Detect individual percussion types
    bool kickDetected = detectKick(bassEnergy, bassFlux, timestamp);
    bool snareDetected = detectSnare(midEnergy, midFlux, timestamp);
    bool hihatDetected = detectHiHat(trebleEnergy, trebleFlux, timestamp);

    // Fire callbacks for detected percussion
    if (kickDetected) {
        if (onKick) onKick();
        if (onPercussionHit) onPercussionHit("kick");
        mLastKickTime = timestamp;
    }

    if (snareDetected) {
        if (onSnare) onSnare();
        if (onPercussionHit) onPercussionHit("snare");
        mLastSnareTime = timestamp;
    }

    if (hihatDetected) {
        if (onHiHat) onHiHat();
        if (onPercussionHit) onPercussionHit("hihat");
        mLastHiHatTime = timestamp;
    }

    // Update previous energy values for next frame
    mPrevBassEnergy = bassEnergy;
    mPrevMidEnergy = midEnergy;
    mPrevTrebleEnergy = trebleEnergy;
}

void PercussionDetector::reset() {
    mPrevBassEnergy = 0.0f;
    mPrevMidEnergy = 0.0f;
    mPrevTrebleEnergy = 0.0f;
    mLastKickTime = 0;
    mLastSnareTime = 0;
    mLastHiHatTime = 0;
}

float PercussionDetector::getBassEnergy(const FFTBins& fft) {
    // Bass range: bins 0-2 (typically 0-300 Hz)
    float energy = 0.0f;
    for (fl::size i = 0; i < 3 && i < fft.bins_raw.size(); i++) {
        energy += fft.bins_raw[i];
    }
    return energy / 3.0f;
}

float PercussionDetector::getMidEnergy(const FFTBins& fft) {
    // Mid range: bins 3-7 (typically 300-2000 Hz)
    float energy = 0.0f;
    for (fl::size i = 3; i < 8 && i < fft.bins_raw.size(); i++) {
        energy += fft.bins_raw[i];
    }
    return energy / 5.0f;
}

float PercussionDetector::getTrebleEnergy(const FFTBins& fft) {
    // Treble range: bins 8-15 (typically 2000+ Hz)
    float energy = 0.0f;
    for (fl::size i = 8; i < 16 && i < fft.bins_raw.size(); i++) {
        energy += fft.bins_raw[i];
    }
    return energy / 8.0f;
}

bool PercussionDetector::detectKick(float bassEnergy, float bassFlux, uint32_t timestamp) {
    // Check cooldown period to prevent double-triggering
    if (timestamp - mLastKickTime < KICK_COOLDOWN_MS) return false;

    // Kick drum characteristics:
    // - Strong bass energy
    // - Strong onset (flux)
    bool strongBass = (bassEnergy > mKickThreshold);
    bool strongOnset = (bassFlux > mKickThreshold * 0.5f);

    return strongBass && strongOnset;
}

bool PercussionDetector::detectSnare(float midEnergy, float midFlux, uint32_t timestamp) {
    // Check cooldown period to prevent double-triggering
    if (timestamp - mLastSnareTime < SNARE_COOLDOWN_MS) return false;

    // Snare drum characteristics:
    // - Strong mid-frequency energy
    // - Strong onset (flux)
    bool strongMid = (midEnergy > mSnareThreshold);
    bool strongOnset = (midFlux > mSnareThreshold * 0.5f);

    return strongMid && strongOnset;
}

bool PercussionDetector::detectHiHat(float trebleEnergy, float trebleFlux, uint32_t timestamp) {
    // Check cooldown period to prevent double-triggering
    if (timestamp - mLastHiHatTime < HIHAT_COOLDOWN_MS) return false;

    // Hi-hat characteristics:
    // - Strong treble energy
    // - Strong onset (flux)
    bool strongTreble = (trebleEnergy > mHiHatThreshold);
    bool strongOnset = (trebleFlux > mHiHatThreshold * 0.4f);

    return strongTreble && strongOnset;
}

} // namespace fl
