#include "fl/audio/auto_gain.h"
#include "fl/stl/algorithm.h"
#include "fl/stl/new.h"
#include "fl/stl/shared_ptr.h"
#include "fl/math/math.h"
#include "fl/stl/noexcept.h"

namespace fl {
namespace audio {

AutoGain::AutoGain() {
    configure(AutoGainConfig{});
}

AutoGain::AutoGain(const AutoGainConfig& config) {
    configure(config);
}

AutoGain::~AutoGain() FL_NOEXCEPT = default;

void AutoGain::configure(const AutoGainConfig& config) {
    mConfig = config;
    resolvePreset();
    // Initialize peak envelope to targetRMSLevel so initial gain is ~1.0
    mPeakEnvelope.reset(mConfig.targetRMSLevel);
    mStats.peakEnvelope = mConfig.targetRMSLevel;
}

void AutoGain::resolvePreset() {
    switch (mConfig.preset) {
    case AGCPreset::AGCPreset_Normal:
        mPeakDecayTau = 3.3f;
        mKp = 0.6f;
        mKi = 1.7f;
        mGainFollowSlowTau = 12.3f;
        mGainFollowFastTau = 0.38f;
        break;
    case AGCPreset::AGCPreset_Vivid:
        mPeakDecayTau = 1.3f;
        mKp = 1.5f;
        mKi = 1.85f;
        mGainFollowSlowTau = 8.2f;
        mGainFollowFastTau = 0.26f;
        break;
    case AGCPreset::AGCPreset_Lazy:
        mPeakDecayTau = 6.7f;
        mKp = 0.65f;
        mKi = 1.2f;
        mGainFollowSlowTau = 16.4f;
        mGainFollowFastTau = 0.51f;
        break;
    case AGCPreset::AGCPreset_Custom:
        mPeakDecayTau = mConfig.peakDecayTau;
        mKp = mConfig.kp;
        mKi = mConfig.ki;
        mGainFollowSlowTau = mConfig.gainFollowSlowTau;
        mGainFollowFastTau = mConfig.gainFollowFastTau;
        break;
    }
    // Reconfigure peak envelope filter with resolved decay tau
    // Attack is always fast (10ms), decay varies by preset
    mPeakEnvelope = AttackDecayFilter<float>(0.01f, mPeakDecayTau, mConfig.targetRMSLevel);
}

void AutoGain::reset() {
    mPeakEnvelope.reset(mConfig.targetRMSLevel);
    mIntegrator = 0.0f;
    mLastGain = 1.0f;
    mStats.currentGain = 1.0f;
    mStats.peakEnvelope = mConfig.targetRMSLevel;
    mStats.targetGain = 1.0f;
    mStats.integrator = 0.0f;
    mStats.inputRMS = 0.0f;
    mStats.outputRMS = 0.0f;
    mStats.samplesProcessed = 0;
}

Sample AutoGain::process(const Sample& sample) {
    // Pass through if disabled
    if (!mConfig.enabled) {
        return sample;
    }

    // Return empty sample if input is invalid or empty
    if (!sample.isValid() || sample.size() == 0) {
        return Sample();  // Return invalid sample
    }

    // Calculate input RMS
    const float inputRMS = sample.rms();
    mStats.inputRMS = inputRMS;

    // Compute dt from sample size and sample rate
    const float dt = (mSampleRate > 0 && sample.size() > 0)
        ? static_cast<float>(sample.size()) / static_cast<float>(mSampleRate)
        : 0.023f;

    // Silence detection: spin down integrator when input is essentially silent
    // (WLED: control_integrated *= 0.91 during silence)
    const float silenceThreshold = 10.0f;
    if (inputRMS < silenceThreshold) {
        mIntegrator *= 0.91f;
        if (fl::abs(mIntegrator) < 0.01f) mIntegrator = 0.0f;
    }

    // Step 1: Update peak envelope (fast attack, slow decay)
    mPeakEnvelope.update(inputRMS, dt);
    const float peakEnv = mPeakEnvelope.value();
    mStats.peakEnvelope = peakEnv;

    // Step 2: Compute target gain from peak envelope
    const float targetGain = computeTargetGain();
    mStats.targetGain = targetGain;

    // Step 3: PI controller smoothly drives gain toward target
    const float smoothedGain = updatePIController(targetGain, dt);

    // Step 4: Clamp to configured range
    const float clampedGain = fl::max(mConfig.minGain,
                                       fl::min(mConfig.maxGain, smoothedGain));
    mStats.currentGain = clampedGain;
    mLastGain = clampedGain;

    // Step 5: Apply gain to audio
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
    mStats.integrator = mIntegrator;

    // Create new Sample from amplified PCM
    SampleImplPtr impl = fl::make_shared<SampleImpl>();
    impl->assign(mOutputBuffer.begin(), mOutputBuffer.end(), sample.timestamp());
    return Sample(impl);
}

float AutoGain::computeTargetGain() {
    const float peakEnv = mPeakEnvelope.value();

    // Avoid division by very small numbers
    if (peakEnv < 1.0f) {
        return mConfig.maxGain;  // Signal is essentially silent
    }

    return mConfig.targetRMSLevel / peakEnv;
}

float AutoGain::updatePIController(float targetGain, float dt) {
    const float error = targetGain - mLastGain;

    // Bug 4 fix: Use absolute error threshold when gain is small to avoid
    // huge errorRatio from dividing by tiny mLastGain
    const bool largeError = (mLastGain > 0.1f)
        ? (fl::abs(error) / mLastGain > 0.2f)  // Relative: >20% of current gain
        : (fl::abs(error) > 0.1f);              // Absolute: >0.1 when gain is tiny
    const float tau = largeError ? mGainFollowFastTau : mGainFollowSlowTau;

    // Proportional term
    const float pTerm = mKp * error;

    // Bug 1 fix: PI target is where we WANT gain to be (not mLastGain + delta + delta)
    // targetGain + pTerm + integrator = the desired gain level
    const float piTarget = targetGain + pTerm + mIntegrator;

    // Smooth with exponential filter using adaptive tau
    float alpha = 1.0f;
    if (tau > 0.0f && dt > 0.0f) {
        alpha = 1.0f - fl::exp(-dt / tau);
    }

    const float unclamped = mLastGain + alpha * (piTarget - mLastGain);

    // Bug 2 fix: Back-calculation anti-windup — only accumulate integrator
    // when output is NOT clamped. Decay integrator when saturated (WLED: *= 0.91)
    const bool saturated = (unclamped > mConfig.maxGain) || (unclamped < mConfig.minGain);
    if (saturated) {
        mIntegrator *= 0.91f;
    } else {
        mIntegrator += mKi * error * dt;
    }
    // Clamp integrator magnitude to prevent runaway in pathological cases
    const float maxIntegral = 4.0f * mConfig.maxGain;
    mIntegrator = fl::max(-maxIntegral, fl::min(maxIntegral, mIntegrator));

    return unclamped;
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

} // namespace audio
} // namespace fl
