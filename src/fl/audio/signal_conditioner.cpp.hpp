#include "fl/audio/signal_conditioner.h"
#include "fl/stl/algorithm.h"
#include "fl/stl/new.h"
#include "fl/stl/shared_ptr.h"
#include "fl/math/math.h"
#include "fl/stl/noexcept.h"

namespace fl {
namespace audio {

SignalConditioner::SignalConditioner() {
    configure(SignalConditionerConfig{});
}

SignalConditioner::SignalConditioner(const SignalConditionerConfig& config) {
    configure(config);
}

SignalConditioner::~SignalConditioner() FL_NOEXCEPT = default;

void SignalConditioner::configure(const SignalConditionerConfig& config) {
    mConfig = config;
}

void SignalConditioner::reset() {
    mNoiseGateOpen = false;
    mStats.dcOffset = 0;
    mStats.noiseGateOpen = false;
    mStats.spikesRejected = 0;
    mStats.samplesProcessed = 0;
}

Sample SignalConditioner::processSample(const Sample& sample) {
    if (!sample.isValid() || sample.size() == 0) {
        return Sample();  // Return empty sample
    }

    const auto pcm = sample.pcm();
    const size sampleCount = pcm.size();

    // Reserve buffers to avoid repeated allocations
    mValidMask.clear();
    mValidMask.reserve(sampleCount);
    mTempBuffer.clear();
    mTempBuffer.reserve(sampleCount);
    mOutputBuffer.clear();
    mOutputBuffer.reserve(sampleCount);

    // Stage 1: Spike filtering (if enabled)
    if (mConfig.enableSpikeFilter) {
        filterSpikes(pcm, mValidMask);
    } else {
        // All samples valid if spike filtering disabled
        mValidMask.assign(sampleCount, true);
    }

    // Stage 2: DC offset removal (if enabled)
    // Uses per-buffer instantaneous DC calculation for accuracy,
    // plus per-sample DCBlocker for cross-buffer continuity.
    i32 dcOffset = 0;
    if (mConfig.enableDCRemoval) {
        dcOffset = calculateDCOffset(pcm, mValidMask);
        removeDCOffset(pcm, dcOffset, mTempBuffer);
    } else {
        // Copy samples to temp buffer without DC removal
        mTempBuffer.assign(pcm.begin(), pcm.end());
    }

    // Stage 3: Noise gate (if enabled)
    if (mConfig.enableNoiseGate) {
        applyNoiseGate(mTempBuffer, mOutputBuffer);
    } else {
        // Copy temp buffer to output without gating
        mOutputBuffer = mTempBuffer;
    }

    // Update stats
    mStats.dcOffset = dcOffset;
    mStats.noiseGateOpen = mNoiseGateOpen;
    mStats.samplesProcessed += sampleCount;

    // Create new Sample from cleaned PCM
    SampleImplPtr impl = fl::make_shared<SampleImpl>();
    impl->assign(mOutputBuffer.begin(), mOutputBuffer.end(), sample.timestamp());
    return Sample(impl);
}

size SignalConditioner::filterSpikes(span<const i16> pcm, vector<bool>& validMask) {
    const size count = pcm.size();
    validMask.clear();
    validMask.reserve(count);

    size validCount = 0;
    const i16 threshold = mConfig.spikeThreshold;

    for (size i = 0; i < count; ++i) {
        const i16 sample = pcm[i];
        const bool isValid = (sample > -threshold) && (sample < threshold);
        validMask.push_back(isValid);
        if (isValid) {
            validCount++;
        }
    }

    // Update spike rejection count
    const size spikesInSample = count - validCount;
    mStats.spikesRejected += spikesInSample;

    return validCount;
}

i32 SignalConditioner::calculateDCOffset(span<const i16> pcm, const vector<bool>& validMask) {
    const size count = pcm.size();
    i64 sum = 0;
    size validCount = 0;

    // Calculate average of valid samples only
    for (size i = 0; i < count; ++i) {
        if (validMask[i]) {
            sum += pcm[i];
            validCount++;
        }
    }

    if (validCount == 0) {
        // No valid samples - return zero offset
        return 0;
    }

    // Calculate instantaneous DC offset for this buffer
    const i32 instantDC = static_cast<i32>(sum / static_cast<i64>(validCount));
    return instantDC;
}

void SignalConditioner::removeDCOffset(span<const i16> pcm, i32 dcOffset, vector<i16>& output) {
    const size count = pcm.size();
    output.clear();
    output.reserve(count);

    for (size i = 0; i < count; ++i) {
        // Zero out samples that were marked as spikes
        if (i < mValidMask.size() && !mValidMask[i]) {
            output.push_back(0);
            continue;
        }

        i32 sample32 = static_cast<i32>(pcm[i]) - dcOffset;

        // Clamp to int16 range to prevent overflow
        if (sample32 > 32767) sample32 = 32767;
        if (sample32 < -32768) sample32 = -32768;

        output.push_back(static_cast<i16>(sample32));
    }
}

void SignalConditioner::applyNoiseGate(span<const i16> pcm, vector<i16>& output) {
    const size count = pcm.size();
    output.clear();
    output.reserve(count);

    const i16 openThreshold = mConfig.noiseGateOpenThreshold;
    const i16 closeThreshold = mConfig.noiseGateCloseThreshold;

    for (size i = 0; i < count; ++i) {
        const i16 sample = pcm[i];
        const i16 absSample = (sample < 0) ? -sample : sample;

        // Hysteresis logic: Different thresholds for opening vs closing
        if (!mNoiseGateOpen) {
            // Gate is closed - check if signal exceeds open threshold
            if (absSample >= openThreshold) {
                mNoiseGateOpen = true;
            }
        } else {
            // Gate is open - check if signal falls below close threshold
            if (absSample < closeThreshold) {
                mNoiseGateOpen = false;
            }
        }

        // Output sample if gate is open, else zero
        output.push_back(mNoiseGateOpen ? sample : 0);
    }
}

} // namespace audio
} // namespace fl
