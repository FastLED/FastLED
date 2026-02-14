#include "fl/audio/auto_gain.h"
#include "fl/stl/algorithm.h"
#include "fl/stl/new.h"
#include "fl/stl/shared_ptr.h"
#include "fl/stl/math.h"

namespace fl {

AutoGain::AutoGain() {
    configure(AutoGainConfig{});
}

AutoGain::AutoGain(const AutoGainConfig& config) {
    configure(config);
}

AutoGain::~AutoGain() = default;

void AutoGain::configure(const AutoGainConfig& config) {
    mConfig = config;
}

void AutoGain::reset() {
    mPercentileEstimate = 1000.0f;  // Reset to initial estimate
    mSmoothedGain = 1.0f;
    mStats.currentGain = 1.0f;
    mStats.percentileEstimate = 0.0f;
    mStats.inputRMS = 0.0f;
    mStats.outputRMS = 0.0f;
    mStats.samplesProcessed = 0;
}

AudioSample AutoGain::process(const AudioSample& sample) {
    // Pass through if disabled
    if (!mConfig.enabled) {
        return sample;
    }

    // Return empty sample if input is invalid or empty
    if (!sample.isValid() || sample.size() == 0) {
        return AudioSample();  // Return invalid sample
    }

    // Calculate input RMS
    const float inputRMS = sample.rms();
    mStats.inputRMS = inputRMS;

    // Update percentile estimate using Robbins-Monro algorithm
    updatePercentileEstimate(inputRMS);

    // Calculate gain from percentile estimate
    const float targetGain = calculateGain();

    // Apply smoothing to gain to prevent abrupt changes
    const float alpha = mConfig.gainSmoothing;
    mSmoothedGain = alpha * mSmoothedGain + (1.0f - alpha) * targetGain;

    // Clamp to configured range
    const float clampedGain = fl::max(mConfig.minGain,
                                       fl::min(mConfig.maxGain, mSmoothedGain));

    mStats.currentGain = clampedGain;

    // Apply gain to audio
    const auto& pcm = sample.pcm();
    mOutputBuffer.clear();
    mOutputBuffer.reserve(pcm.size());
    applyGain(pcm, clampedGain, mOutputBuffer);

    // Calculate output RMS for statistics
    i64 sumSq = 0;
    for (size i = 0; i < mOutputBuffer.size(); ++i) {
        i32 val = static_cast<i32>(mOutputBuffer[i]);
        sumSq += val * val;
    }
    mStats.outputRMS = sqrtf(static_cast<float>(sumSq) / static_cast<float>(mOutputBuffer.size()));

    // Update stats
    mStats.samplesProcessed += sample.size();

    // Create new AudioSample from amplified PCM
    AudioSampleImplPtr impl = fl::make_shared<AudioSampleImpl>();
    impl->assign(mOutputBuffer.begin(), mOutputBuffer.end(), sample.timestamp());
    return AudioSample(impl);
}

void AutoGain::updatePercentileEstimate(float observedRMS) {
    // Simplified percentile estimation using exponential moving average
    // with directional bias based on target percentile
    //
    // For P90 (targetPercentile = 0.9):
    // - We want the estimate to be at a level where 90% of samples are below it
    // - When we see samples above the estimate, increase it faster (we're below P90)
    // - When we see samples below the estimate, decrease it slower (we might be at or above P90)

    const float p = mConfig.targetPercentile;  // e.g., 0.9 for P90
    const float lr = mConfig.learningRate;     // Learning rate

    // Use an adaptive approach that moves toward observed values
    // with asymmetric rates to converge to the desired percentile
    if (observedRMS > mPercentileEstimate) {
        // Observed value is above estimate - we're likely below the target percentile
        // Increase estimate by moving toward the observed value
        // Use learning rate scaled by (1 - p) for the target percentile
        const float adaptiveRate = lr / (1.0f - p);  // Higher rate for high percentiles
        mPercentileEstimate += adaptiveRate * (observedRMS - mPercentileEstimate);
    } else {
        // Observed value is below estimate - we might be above the target percentile
        // Decrease estimate more slowly
        const float adaptiveRate = lr / p;  // Lower rate for high percentiles
        mPercentileEstimate += adaptiveRate * (observedRMS - mPercentileEstimate);
    }

    // Prevent estimate from going to zero or negative
    if (mPercentileEstimate < 1.0f) {
        mPercentileEstimate = 1.0f;
    }

    mStats.percentileEstimate = mPercentileEstimate;
}

float AutoGain::calculateGain() {
    // Calculate gain to bring percentile estimate to target RMS level
    // gain = targetRMS / percentileEstimate

    if (mPercentileEstimate < 1.0f) {
        return 1.0f;  // Avoid division by very small numbers
    }

    const float gain = mConfig.targetRMSLevel / mPercentileEstimate;

    return gain;
}

void AutoGain::applyGain(const vector<i16>& input, float gain, vector<i16>& output) {
    output.clear();
    output.reserve(input.size());

    for (size i = 0; i < input.size(); ++i) {
        // Multiply sample by gain
        float amplified = static_cast<float>(input[i]) * gain;

        // Clamp to int16 range to prevent overflow/clipping
        if (amplified > 32767.0f) amplified = 32767.0f;
        if (amplified < -32768.0f) amplified = -32768.0f;

        output.push_back(static_cast<i16>(amplified));
    }
}

} // namespace fl
