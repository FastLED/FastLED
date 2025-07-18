#include "fx_engine.h"
#include "video.h"

namespace fl {

FxEngine::FxEngine(uint16_t numLeds, bool interpolate)
    : mTimeFunction(0), mCompositor(numLeds), mCurrId(0),
      mInterpolate(interpolate) {}

FxEngine::~FxEngine() {}

int FxEngine::addFx(FxPtr effect) {
    float fps = 0;
    if (mInterpolate && effect->hasFixedFrameRate(&fps)) {
        // Wrap the effect in a VideoFxWrapper so that we can get
        // interpolation.
        VideoFxWrapperPtr vid_fx = fl::make_shared<VideoFxWrapper>(effect);
        vid_fx->setFade(0, 0); // No fade for interpolated effects
        effect = vid_fx;
    }
    bool auto_set = mEffects.empty();
    bool ok = mEffects.insert(mCounter, effect).first;
    if (!ok) {
        return -1;
    }
    if (auto_set) {
        mCurrId = mCounter;
        mCompositor.startTransition(0, 0, effect);
    }
    return mCounter++;
}

bool FxEngine::nextFx(uint16_t duration) {
    bool ok = mEffects.next(mCurrId, &mCurrId, true);
    if (!ok) {
        return false;
    }
    setNextFx(mCurrId, duration);
    return true;
}

bool FxEngine::setNextFx(int index, uint16_t duration) {
    if (!mEffects.has(index)) {
        return false;
    }
    mCurrId = index;
    mDuration = duration;
    mDurationSet = true;
    return true;
}

FxPtr FxEngine::removeFx(int index) {
    if (!mEffects.has(index)) {
        return FxPtr();
    }

    FxPtr removedFx;
    bool ok = mEffects.get(index, &removedFx);
    if (!ok) {
        return FxPtr();
    }

    if (mCurrId == index) {
        // If we're removing the current effect, switch to the next one
        mEffects.next(mCurrId, &mCurrId, true);
        mDurationSet = true;
        mDuration = 0; // Instant transition
    }

    return removedFx;
}

FxPtr FxEngine::getFx(int id) {
    if (mEffects.has(id)) {
        FxPtr fx;
        mEffects.get(id, &fx);
        return fx;
    }
    return FxPtr();
}

bool FxEngine::draw(fl::u32 now, CRGB *finalBuffer) {
    mTimeFunction.update(now);
    fl::u32 warpedTime = mTimeFunction.time();

    if (mEffects.empty()) {
        return false;
    }
    if (mDurationSet) {
        FxPtr fx;
        bool ok = mEffects.get(mCurrId, &fx);
        if (!ok) {
            // something went wrong.
            return false;
        }
        mCompositor.startTransition(now, mDuration, fx);
        mDurationSet = false;
    }
    if (!mEffects.empty()) {
        mCompositor.draw(now, warpedTime, finalBuffer);
    }
    return true;
}

} // namespace fl
