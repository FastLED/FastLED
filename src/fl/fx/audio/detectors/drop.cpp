// DropDetector.cpp - Implementation of EDM drop detection

#include "fl/fx/audio/detectors/drop.h"
#include "fl/audio/audio_context.h"
#include "fl/stl/math.h"
#include "fl/dbg.h"

namespace fl {

DropDetector::DropDetector()
    : mPrevRMS(0.0f)
    , mPrevBassEnergy(0.0f)
    , mPrevMidEnergy(0.0f)
    , mPrevTrebleEnergy(0.0f)
    , mEnergyBaseline(0.0f)
    , mBassBaseline(0.0f)
    , mImpactThreshold(0.75f)       // 75% impact to trigger
    , mMinTimeBetweenDrops(2000)    // 2 seconds between drops
    , mBassThreshold(0.6f)          // 60% bass energy required
    , mEnergyFluxThreshold(0.5f)    // 50% energy increase required
{
    mLastDrop.timestamp = 0;  // Initialize to allow immediate first drop
}

DropDetector::~DropDetector() = default;

void DropDetector::update(shared_ptr<AudioContext> context) {
    if (!context) {
        // Null context - nothing to process
        return;
    }

    const FFTBins& fft = context->getFFT(32);
    float rms = context->getRMS();
    uint32_t timestamp = context->getTimestamp();

    // Calculate frequency band energies
    float bassEnergy = getBassEnergy(fft);
    float midEnergy = getMidEnergy(fft);
    float trebleEnergy = getTrebleEnergy(fft);

    // Calculate flux values (rate of change)
    float energyFlux = calculateEnergyFlux(rms);
    float bassFlux = calculateBassFlux(bassEnergy);

    // Calculate spectral novelty (how much spectrum changed)
    float spectralNovelty = calculateSpectralNovelty(bassEnergy, midEnergy, trebleEnergy);

    // Calculate drop impact
    float impact = calculateDropImpact(energyFlux, bassFlux, spectralNovelty, rms);

    // Check if we should trigger a drop
    if (shouldTriggerDrop(impact, timestamp)) {
        // Create drop event
        Drop dropEvent;
        dropEvent.impact = impact;
        dropEvent.bassEnergy = bassEnergy;
        dropEvent.energyIncrease = energyFlux;
        dropEvent.timestamp = timestamp;

        mLastDrop = dropEvent;

        FL_DBG("DropDetector: Drop detected! Impact=" << impact
               << ", Bass=" << bassEnergy
               << ", Energy flux=" << energyFlux);

        // Fire callbacks
        if (onDrop) {
            onDrop();
        }
        if (onDropEvent) {
            onDropEvent(dropEvent);
        }
        if (onDropImpact) {
            onDropImpact(impact);
        }
    }

    // Update baselines (exponential moving average)
    updateBaselines(rms, bassEnergy);

    // Store current values for next frame
    mPrevRMS = rms;
    mPrevBassEnergy = bassEnergy;
    mPrevMidEnergy = midEnergy;
    mPrevTrebleEnergy = trebleEnergy;
}

void DropDetector::reset() {
    mLastDrop = Drop();
    mPrevRMS = 0.0f;
    mPrevBassEnergy = 0.0f;
    mPrevMidEnergy = 0.0f;
    mPrevTrebleEnergy = 0.0f;
    mEnergyBaseline = 0.0f;
    mBassBaseline = 0.0f;
}

float DropDetector::getBassEnergy(const FFTBins& fft) const {
    // Bass = first 25% of bins (sub-bass and bass)
    int endBin = fl::fl_max(1, static_cast<int>(fft.bins_raw.size() / 4));
    float energy = 0.0f;

    for (int i = 0; i < endBin; i++) {
        energy += fft.bins_raw[i];
    }

    return energy / static_cast<float>(endBin);
}

float DropDetector::getMidEnergy(const FFTBins& fft) const {
    // Mid = middle 50% of bins (midrange frequencies)
    int startBin = static_cast<int>(fft.bins_raw.size() / 4);
    int endBin = static_cast<int>(fft.bins_raw.size() * 3 / 4);
    float energy = 0.0f;
    int count = 0;

    for (int i = startBin; i < endBin; i++) {
        energy += fft.bins_raw[i];
        count++;
    }

    return (count > 0) ? energy / static_cast<float>(count) : 0.0f;
}

float DropDetector::getTrebleEnergy(const FFTBins& fft) const {
    // Treble = top 25% of bins (high frequencies)
    int startBin = static_cast<int>(fft.bins_raw.size() * 3 / 4);
    float energy = 0.0f;
    int count = 0;

    for (fl::size i = startBin; i < fft.bins_raw.size(); i++) {
        energy += fft.bins_raw[i];
        count++;
    }

    return (count > 0) ? energy / static_cast<float>(count) : 0.0f;
}

float DropDetector::calculateSpectralNovelty(float bass, float mid, float treble) const {
    // Calculate how much the spectrum changed from previous frame
    float bassChange = fl::fl_abs(bass - mPrevBassEnergy);
    float midChange = fl::fl_abs(mid - mPrevMidEnergy);
    float trebleChange = fl::fl_abs(treble - mPrevTrebleEnergy);

    // Weight bass changes more heavily (drops often emphasize bass)
    float novelty = bassChange * 0.5f + midChange * 0.3f + trebleChange * 0.2f;

    // Normalize (typical max change is ~2.0)
    return fl::fl_min(1.0f, novelty / 2.0f);
}

float DropDetector::calculateEnergyFlux(float currentRMS) const {
    // Calculate how much energy increased from baseline
    if (mEnergyBaseline < 1e-6f) {
        return 0.0f;  // No baseline yet
    }

    float increase = currentRMS - mEnergyBaseline;
    float ratio = increase / mEnergyBaseline;

    // Normalize to [0, 1] (2x increase = 1.0)
    return fl::fl_max(0.0f, fl::fl_min(1.0f, ratio / 2.0f));
}

float DropDetector::calculateBassFlux(float currentBass) const {
    // Calculate how much bass increased from baseline
    if (mBassBaseline < 1e-6f) {
        return 0.0f;  // No baseline yet
    }

    float increase = currentBass - mBassBaseline;
    float ratio = increase / mBassBaseline;

    // Normalize to [0, 1] (2x increase = 1.0)
    return fl::fl_max(0.0f, fl::fl_min(1.0f, ratio / 2.0f));
}

float DropDetector::calculateDropImpact(float energyFlux, float bassFlux, float spectralNovelty, float rms) const {
    // Drop impact is a weighted combination of:
    // - Energy flux (40%) - sudden energy burst
    // - Bass flux (35%) - bass impact
    // - Spectral novelty (15%) - dramatic change
    // - Overall energy (10%) - absolute energy level

    float normalizedRMS = fl::fl_min(1.0f, rms);

    float impact = energyFlux * 0.4f +
                   bassFlux * 0.35f +
                   spectralNovelty * 0.15f +
                   normalizedRMS * 0.1f;

    return fl::fl_max(0.0f, fl::fl_min(1.0f, impact));
}

bool DropDetector::shouldTriggerDrop(float impact, uint32_t timestamp) const {
    // Don't trigger if:
    // 1. Impact below threshold
    if (impact < mImpactThreshold) {
        return false;
    }

    // 2. Too soon after last drop (cooldown)
    uint32_t timeSinceLast = timestamp - mLastDrop.timestamp;
    if (timeSinceLast < mMinTimeBetweenDrops) {
        return false;
    }

    // 3. Not enough energy flux (not a real burst)
    float energyFlux = calculateEnergyFlux(mPrevRMS);
    if (energyFlux < mEnergyFluxThreshold) {
        return false;
    }

    // 4. Not enough bass (drops should have strong bass)
    float bassFlux = calculateBassFlux(mPrevBassEnergy);
    if (bassFlux < mBassThreshold * 0.5f) {  // At least 50% of bass threshold
        return false;
    }

    return true;
}

void DropDetector::updateBaselines(float rms, float bass) {
    // Exponential moving average with alpha = 0.9 (slow adaptation)
    // This creates a "rolling baseline" of recent energy levels
    const float alpha = 0.9f;

    if (mEnergyBaseline < 1e-6f) {
        // Initialize baseline
        mEnergyBaseline = rms;
        mBassBaseline = bass;
    } else {
        // Update baseline
        mEnergyBaseline = alpha * mEnergyBaseline + (1.0f - alpha) * rms;
        mBassBaseline = alpha * mBassBaseline + (1.0f - alpha) * bass;
    }
}

} // namespace fl
