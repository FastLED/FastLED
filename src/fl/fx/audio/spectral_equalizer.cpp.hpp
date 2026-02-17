/// @file spectral_equalizer.cpp.hpp
/// @brief Implementation of SpectralEqualizer for frequency-dependent gain correction

#include "fl/fx/audio/spectral_equalizer.h"
#include "fl/stl/math.h"
#include "fl/math_macros.h"
#include "fl/compiler_control.h"

namespace fl {

// Define static constexpr arrays
constexpr float SpectralEqualizer::A_WEIGHTING_16BAND[16];
constexpr float SpectralEqualizer::A_WEIGHTING_32BAND[32];

SpectralEqualizer::SpectralEqualizer() {
    configure(SpectralEqualizerConfig());
}

SpectralEqualizer::SpectralEqualizer(const SpectralEqualizerConfig& config) {
    configure(config);
}

SpectralEqualizer::~SpectralEqualizer() = default;

void SpectralEqualizer::configure(const SpectralEqualizerConfig& config) {
    mConfig = config;

    // Reset stats
    mStats = Stats();

    // Allocate gain array
    mGains.clear();
    mGains.resize(mConfig.numBands, 1.0f);

    // Calculate gains based on curve type
    calculateGains();
}

void SpectralEqualizer::calculateGains() {
    switch (mConfig.curve) {
        case EqualizationCurve::Flat:
            calculateFlatGains();
            break;

        case EqualizationCurve::AWeighting:
            calculateAWeightingGains();
            break;

        case EqualizationCurve::Custom:
            // Use custom gains if provided
            if (mConfig.customGains.size() == mConfig.numBands) {
                for (size i = 0; i < mConfig.numBands; ++i) {
                    mGains[i] = mConfig.customGains[i];
                }
            } else {
                FL_WARN("SpectralEqualizer: custom gains size mismatch ("
                        << mConfig.customGains.size() << " != " << mConfig.numBands
                        << "), using flat gains");
                calculateFlatGains();
            }
            break;

        default:
            calculateFlatGains();
            break;
    }
}

void SpectralEqualizer::calculateFlatGains() {
    // All gains = 1.0 (no equalization)
    for (size i = 0; i < mConfig.numBands; ++i) {
        mGains[i] = 1.0f;
    }
}

void SpectralEqualizer::calculateAWeightingGains() {
    // Use appropriate A-weighting curve based on number of bands
    const float* curve = nullptr;
    size curveSize = 0;

    if (mConfig.numBands == 16) {
        curve = A_WEIGHTING_16BAND;
        curveSize = 16;
    } else if (mConfig.numBands == 32) {
        curve = A_WEIGHTING_32BAND;
        curveSize = 32;
    } else {
        // Unsupported band count - use flat gains
        FL_WARN("SpectralEqualizer: A-weighting not defined for " << mConfig.numBands
                << " bands, using flat gains");
        calculateFlatGains();
        return;
    }

    // Copy A-weighting coefficients to gains
    for (size i = 0; i < mConfig.numBands && i < curveSize; ++i) {
        mGains[i] = curve[i];
    }
}

void SpectralEqualizer::setCustomGains(span<const float> gains) {
    if (gains.size() != mConfig.numBands) {
        FL_WARN("SpectralEqualizer: custom gains size mismatch ("
                << gains.size() << " != " << mConfig.numBands << ")");
        return;
    }

    // Copy custom gains
    mConfig.customGains.clear();
    mConfig.customGains.reserve(gains.size());
    for (size i = 0; i < gains.size(); ++i) {
        mConfig.customGains.push_back(gains[i]);
        mGains[i] = gains[i];
    }

    // Switch to custom curve
    mConfig.curve = EqualizationCurve::Custom;
}

void SpectralEqualizer::apply(span<const float> inputBins, span<float> outputBins) const {
    if (inputBins.size() != mConfig.numBands) {
        FL_WARN("SpectralEqualizer: input size mismatch ("
                << inputBins.size() << " != " << mConfig.numBands << ")");
        return;
    }

    if (outputBins.size() < mConfig.numBands) {
        FL_WARN("SpectralEqualizer: output buffer too small ("
                << outputBins.size() << " < " << mConfig.numBands << ")");
        return;
    }

    // Track input/output levels for stats
    float inputPeak = 0.0f;
    float outputPeak = 0.0f;
    float inputSum = 0.0f;
    float outputSum = 0.0f;

    // Apply per-band gains
    for (size i = 0; i < mConfig.numBands; ++i) {
        float inputValue = inputBins[i];
        float gain = mGains[i];
        float outputValue = inputValue * gain;

        // Apply compression if enabled
        if (mConfig.enableCompression) {
            outputValue = applyCompression(outputValue);
        }

        outputBins[i] = outputValue;

        // Track levels
        if (inputValue > inputPeak) {
            inputPeak = inputValue;
        }
        if (outputValue > outputPeak) {
            outputPeak = outputValue;
        }
        inputSum += inputValue;
        outputSum += outputValue;
    }

    // Calculate makeup gain if enabled
    float makeupGain = 1.0f;
    if (mConfig.applyMakeupGain) {
        makeupGain = calculateMakeupGain(inputBins, outputBins);

        // Apply makeup gain to all output bins
        for (size i = 0; i < mConfig.numBands; ++i) {
            outputBins[i] *= makeupGain;
        }

        // Adjust output stats
        outputPeak *= makeupGain;
        outputSum *= makeupGain;
    }

    // Update stats (mutable in const method for statistics)
    const_cast<SpectralEqualizer*>(this)->mStats.applicationsCount++;
    const_cast<SpectralEqualizer*>(this)->mStats.lastInputPeak = inputPeak;
    const_cast<SpectralEqualizer*>(this)->mStats.lastOutputPeak = outputPeak;
    const_cast<SpectralEqualizer*>(this)->mStats.lastMakeupGain = makeupGain;
    const_cast<SpectralEqualizer*>(this)->mStats.avgInputLevel = inputSum / static_cast<float>(mConfig.numBands);
    const_cast<SpectralEqualizer*>(this)->mStats.avgOutputLevel = outputSum / static_cast<float>(mConfig.numBands);
}

float SpectralEqualizer::calculateMakeupGain(span<const float> inputBins, span<const float> outputBins) const {
    // Calculate average levels
    float inputAvg = 0.0f;
    float outputAvg = 0.0f;

    for (size i = 0; i < mConfig.numBands; ++i) {
        inputAvg += inputBins[i];
        outputAvg += outputBins[i];
    }

    inputAvg /= static_cast<float>(mConfig.numBands);
    outputAvg /= static_cast<float>(mConfig.numBands);

    // Avoid division by zero
    if (outputAvg < 0.001f) {
        return 1.0f;
    }

    // Calculate makeup gain to bring output to target level
    // Target is relative to input average
    float targetLevel = inputAvg * mConfig.makeupGainTarget;
    float makeupGain = targetLevel / outputAvg;

    // Clamp to reasonable range (0.1 to 10.0)
    if (makeupGain < 0.1f) {
        makeupGain = 0.1f;
    }
    if (makeupGain > 10.0f) {
        makeupGain = 10.0f;
    }

    return makeupGain;
}

float SpectralEqualizer::applyCompression(float value) const {
    // Simple soft-knee compression
    if (value <= mConfig.compressionThreshold) {
        // Below threshold - no compression
        return value;
    }

    // Above threshold - apply compression ratio
    float excess = value - mConfig.compressionThreshold;
    float compressed = excess / mConfig.compressionRatio;
    return mConfig.compressionThreshold + compressed;
}

void SpectralEqualizer::resetStats() {
    mStats = Stats();
}

} // namespace fl
