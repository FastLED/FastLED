#include "fl/fx/audio/detectors/frequency_bands.h"
#include "fl/audio/audio_context.h"
#include "fl/stl/math.h"

namespace fl {

FrequencyBands::FrequencyBands()
    : mBass(0.0f)
    , mMid(0.0f)
    , mTreble(0.0f)
    , mBassMin(20.0f)
    , mBassMax(250.0f)
    , mMidMin(250.0f)
    , mMidMax(4000.0f)
    , mTrebleMin(4000.0f)
    , mTrebleMax(20000.0f)
    , mSmoothing(0.7f)
{}

FrequencyBands::~FrequencyBands() = default;

void FrequencyBands::update(shared_ptr<AudioContext> context) {
    const FFTBins& fft = context->getFFT(16);

    // Calculate energy for each band
    float bassEnergy = calculateBandEnergy(fft, mBassMin, mBassMax, 16);
    float midEnergy = calculateBandEnergy(fft, mMidMin, mMidMax, 16);
    float trebleEnergy = calculateBandEnergy(fft, mTrebleMin, mTrebleMax, 16);

    // Apply smoothing
    mBass = mSmoothing * mBass + (1.0f - mSmoothing) * bassEnergy;
    mMid = mSmoothing * mMid + (1.0f - mSmoothing) * midEnergy;
    mTreble = mSmoothing * mTreble + (1.0f - mSmoothing) * trebleEnergy;

    // Fire callbacks
    if (onLevelsUpdate) {
        onLevelsUpdate(mBass, mMid, mTreble);
    }
    if (onBassLevel) {
        onBassLevel(mBass);
    }
    if (onMidLevel) {
        onMidLevel(mMid);
    }
    if (onTrebleLevel) {
        onTrebleLevel(mTreble);
    }
}

void FrequencyBands::reset() {
    mBass = 0.0f;
    mMid = 0.0f;
    mTreble = 0.0f;
}

float FrequencyBands::calculateBandEnergy(const FFTBins& fft, float minFreq, float maxFreq, int numBands) {
    // Assume 44100 Hz sample rate and 16 bands covering 0-22050 Hz
    const float sampleRate = 44100.0f;
    const float nyquist = sampleRate / 2.0f;

    float energy = 0.0f;
    int count = 0;

    for (size i = 0; i < fft.bins_raw.size(); i++) {
        // Calculate the frequency range for this bin
        float binMinFreq = (i * nyquist) / numBands;
        float binMaxFreq = ((i + 1) * nyquist) / numBands;

        // Check if this bin overlaps with our target frequency range
        if (binMaxFreq >= minFreq && binMinFreq <= maxFreq) {
            energy += fft.bins_raw[i];
            count++;
        }
    }

    // Normalize by number of bins
    return (count > 0) ? energy / static_cast<float>(count) : 0.0f;
}

} // namespace fl
