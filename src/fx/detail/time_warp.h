#pragma once

#include <stdint.h>

#include "namespace.h"
#include "callback.h"
#include "math_macros.h"

FASTLED_NAMESPACE_BEGIN

// This class keeps track of the current time and modifies it to allow for time warping effects.
// You will get the warped time value by calling getTime().
class TimeWarp {
public:
    enum class CallbackType {
        BOUNCE,
        EXACT
    };

    TimeWarp(uint32_t realTimeNow, float initialTimeScale = 1.0f) 
        : mRealTime(realTimeNow), mLastRealTime(realTimeNow), mTimeScale(initialTimeScale) {}

    void setTimeScale(float timeScale) { mTimeScale = timeScale; }
    float getTimeScale() const { return mTimeScale; }

    void update(uint32_t timeNow) {
        uint32_t elapsedRealTime = timeNow - mLastRealTime;
        if (mCurrentMode == CallbackType::EXACT) {
            mRealTime += elapsedRealTime;
        } else {
            mRealTime += static_cast<uint32_t>(elapsedRealTime * mTimeScale);
        }
        mLastRealTime = timeNow;

        if (mCallback) {
            mCallback();
        }
    }
    
    uint32_t getTime() const { return mRealTime; }

    uint32_t reset(uint32_t timeNow) {
        mRealTime = 0;
        mLastRealTime = timeNow;
        return mRealTime;
    }

    void setCallbackForType(CallbackType type, const Callback<>& callback) {
        mCurrentMode = type;
        switch (type) {
            case CallbackType::BOUNCE:
            case CallbackType::EXACT:
                setCallback(callback);
                break;
            // Add more cases here for future callback types
        }
    }

    CallbackType getCurrentMode() const {
        return mCurrentMode;
    }

    void bounce() {
        float diff = 1.0f - mTimeScale;
        mTimeScale += diff * 0.1f;  // Gradually move towards 1.0
        if (ABS(diff) < 0.001f) {
            mTimeScale = 1.0f;
            mCallback.clear();
        }
    }

private:
    void setCallback(const Callback<>& callback) {
        mCallback = callback;
    }

    uint32_t mRealTime = 0;
    uint32_t mLastRealTime = 0;
    float mTimeScale = 1.0f;
    Callback<> mCallback;
    CallbackType mCurrentMode = CallbackType::BOUNCE;
};

FASTLED_NAMESPACE_END
