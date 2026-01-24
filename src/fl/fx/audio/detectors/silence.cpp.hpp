#include "fl/fx/audio/detectors/silence.h"
#include "fl/audio/audio_context.h"
#include "fl/stl/math.h"

namespace fl {

SilenceDetector::SilenceDetector()
    : mIsSilent(false)
    , mPreviousSilent(false)
    , mCurrentRMS(0.0f)
    , mSilenceThreshold(DEFAULT_SILENCE_THRESHOLD)
    , mHysteresis(DEFAULT_HYSTERESIS)
    , mSilenceStartTime(0)
    , mSilenceEndTime(0)
    , mMinSilenceDuration(DEFAULT_MIN_SILENCE_MS)
    , mMaxSilenceDuration(DEFAULT_MAX_SILENCE_MS)
    , mLastUpdateTime(0)
    , mHistorySize(DEFAULT_HISTORY_SIZE)
    , mHistoryIndex(0)
{
    mRMSHistory.reserve(mHistorySize);
}

SilenceDetector::~SilenceDetector() = default;

void SilenceDetector::update(shared_ptr<AudioContext> context) {
    mCurrentRMS = context->getRMS();
    u32 timestamp = context->getTimestamp();
    mLastUpdateTime = timestamp;

    // Build history
    if (mRMSHistory.size() < static_cast<fl::size>(mHistorySize)) {
        mRMSHistory.push_back(mCurrentRMS);
    } else {
        mRMSHistory[mHistoryIndex] = mCurrentRMS;
        mHistoryIndex = (mHistoryIndex + 1) % mHistorySize;
    }

    // Get smoothed RMS
    float smoothedRMS = getSmoothedRMS();

    // Check silence condition with hysteresis
    bool nowSilent = checkSilenceCondition(smoothedRMS);

    // State transition logic
    if (nowSilent && !mPreviousSilent) {
        // Entering silence
        mSilenceStartTime = timestamp;
        mPreviousSilent = true;
    } else if (!nowSilent && mPreviousSilent) {
        // Exiting silence
        mSilenceEndTime = timestamp;
        u32 duration = mSilenceEndTime - mSilenceStartTime;

        // Only fire callbacks if minimum duration was met
        if (mIsSilent && duration >= mMinSilenceDuration) {
            if (onSilenceEnd) onSilenceEnd();
            if (onSilenceChange) onSilenceChange(false);
            if (onSilenceDuration) onSilenceDuration(duration);
        }

        mIsSilent = false;
        mPreviousSilent = false;
        mSilenceStartTime = 0;
    } else if (nowSilent && mPreviousSilent) {
        // Continuing silence
        u32 duration = timestamp - mSilenceStartTime;

        // Check if we've reached minimum duration and should fire start event
        if (!mIsSilent && duration >= mMinSilenceDuration) {
            mIsSilent = true;
            if (onSilenceStart) onSilenceStart();
            if (onSilenceChange) onSilenceChange(true);
        }

        // Check if we've exceeded maximum duration
        if (mIsSilent && mMaxSilenceDuration > 0 && duration >= mMaxSilenceDuration) {
            // Periodic duration callback at max duration
            if (onSilenceDuration) onSilenceDuration(duration);
        }
    }
}

void SilenceDetector::reset() {
    mIsSilent = false;
    mPreviousSilent = false;
    mCurrentRMS = 0.0f;
    mSilenceStartTime = 0;
    mSilenceEndTime = 0;
    mLastUpdateTime = 0;
    mRMSHistory.clear();
    mHistoryIndex = 0;
}

float SilenceDetector::getSmoothedRMS() {
    if (mRMSHistory.empty()) {
        return 0.0f;
    }

    float sum = 0.0f;
    for (fl::size i = 0; i < mRMSHistory.size(); i++) {
        sum += mRMSHistory[i];
    }
    return sum / static_cast<float>(mRMSHistory.size());
}

bool SilenceDetector::checkSilenceCondition(float smoothedRMS) {
    // Use hysteresis to prevent rapid toggling
    // When currently silent, use a higher threshold to exit
    // When not silent, use a lower threshold to enter
    if (mPreviousSilent) {
        // Currently in potential silence - need louder signal to exit
        float exitThreshold = mSilenceThreshold * (1.0f + mHysteresis);
        return smoothedRMS <= exitThreshold;
    } else {
        // Currently not silent - need quiet signal to enter
        float enterThreshold = mSilenceThreshold * (1.0f - mHysteresis);
        return smoothedRMS <= enterThreshold;
    }
}

u32 SilenceDetector::getSilenceDuration() const {
    if (!mIsSilent || mSilenceStartTime == 0) {
        return 0;
    }
    return mLastUpdateTime - mSilenceStartTime;
}

} // namespace fl
