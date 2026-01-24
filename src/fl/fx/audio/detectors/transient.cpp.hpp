#include "fl/fx/audio/detectors/transient.h"
#include "fl/audio/audio_context.h"
#include "fl/stl/math.h"
#include "fl/stl/algorithm.h"

namespace fl {

TransientDetector::TransientDetector()
    : mTransientDetected(false)
    , mStrength(0.0f)
    , mThreshold(1.5f)
    , mSensitivity(1.0f)
    , mMinIntervalMs(30)  // Allow up to ~33 transients per second
    , mLastTransientTime(0)
    , mPreviousEnergy(0.0f)
    , mCurrentEnergy(0.0f)
    , mAttackTime(0.0f)
{
    mPreviousHighFreq.resize(16, 0.0f);
    mEnergyHistory.reserve(ENERGY_HISTORY_SIZE);
}

TransientDetector::~TransientDetector() = default;

void TransientDetector::update(shared_ptr<AudioContext> context) {
    const FFTBins& fft = context->getFFT(16);
    u32 timestamp = context->getTimestamp();

    // Calculate high-frequency energy (transients have strong high-freq components)
    mCurrentEnergy = calculateHighFreqEnergy(fft);

    // Calculate energy flux (rate of change)
    float flux = calculateEnergyFlux(mCurrentEnergy);

    // Detect transient
    mTransientDetected = detectTransient(flux, timestamp);

    if (mTransientDetected) {
        updateAttackTime(flux);

        if (onTransient) {
            onTransient();
        }
        if (onTransientWithStrength) {
            onTransientWithStrength(mStrength);
        }
        if (onAttack) {
            onAttack(mStrength);
        }

        mLastTransientTime = timestamp;
    }

    // Update previous energy for next frame
    mPreviousEnergy = mCurrentEnergy;

    // Update energy history
    if (mEnergyHistory.size() >= ENERGY_HISTORY_SIZE) {
        mEnergyHistory.erase(mEnergyHistory.begin());
    }
    mEnergyHistory.push_back(mCurrentEnergy);
}

void TransientDetector::reset() {
    mTransientDetected = false;
    mStrength = 0.0f;
    mLastTransientTime = 0;
    mPreviousEnergy = 0.0f;
    mCurrentEnergy = 0.0f;
    mAttackTime = 0.0f;
    fl::fill(mPreviousHighFreq.begin(), mPreviousHighFreq.end(), 0.0f);
    mEnergyHistory.clear();
}

float TransientDetector::calculateHighFreqEnergy(const FFTBins& fft) {
    // Focus on mid-high to high frequencies (bins 4-15) for transient detection
    // Low frequencies tend to have slower attack times
    float energy = 0.0f;
    size numBins = fft.bins_raw.size();

    // Weight higher frequencies more for transient detection
    for (size i = 4; i < numBins; i++) {
        float weight = static_cast<float>(i) / static_cast<float>(numBins);
        energy += fft.bins_raw[i] * (1.0f + weight);
    }

    size validBins = (numBins > 4) ? (numBins - 4) : 1;
    return energy / static_cast<float>(validBins);
}

float TransientDetector::calculateEnergyFlux(float currentEnergy) {
    // Calculate positive energy flux (increase in energy)
    float flux = fl::fl_max(0.0f, currentEnergy - mPreviousEnergy);

    // Normalize by previous energy to get relative change
    if (mPreviousEnergy > 1e-6f) {
        flux = flux / mPreviousEnergy;
    }

    return flux;
}

bool TransientDetector::detectTransient(float flux, u32 timestamp) {
    // Check cooldown period
    u32 timeSinceLastTransient = timestamp - mLastTransientTime;
    if (timeSinceLastTransient < mMinIntervalMs) {
        return false;
    }

    // Calculate adaptive threshold based on recent energy history
    float adaptiveThreshold = 0.0f;
    if (!mEnergyHistory.empty()) {
        float sum = 0.0f;
        for (size i = 0; i < mEnergyHistory.size(); i++) {
            sum += mEnergyHistory[i];
        }
        float meanEnergy = sum / static_cast<float>(mEnergyHistory.size());

        // Threshold is based on mean energy and configured threshold
        if (meanEnergy > 1e-6f) {
            adaptiveThreshold = mThreshold * mSensitivity;
        }
    }

    // Check if flux exceeds threshold
    if (flux <= adaptiveThreshold) {
        mStrength = 0.0f;
        return false;
    }

    // Calculate strength based on how much we exceeded threshold
    if (adaptiveThreshold > 0.0f) {
        mStrength = fl::fl_min(1.0f, (flux - adaptiveThreshold) / adaptiveThreshold);
    } else {
        mStrength = fl::fl_min(1.0f, flux);
    }

    return true;
}

void TransientDetector::updateAttackTime(float flux) {
    // Estimate attack time based on flux magnitude
    // Higher flux = faster attack = shorter attack time
    // Range: ~1-20ms for typical transients
    const float minAttackTime = 1.0f;  // ms
    const float maxAttackTime = 20.0f; // ms

    // Inverse relationship: stronger flux = shorter attack time
    float normalized = fl::fl_min(1.0f, flux / 10.0f);
    mAttackTime = maxAttackTime - (normalized * (maxAttackTime - minAttackTime));
}

} // namespace fl
