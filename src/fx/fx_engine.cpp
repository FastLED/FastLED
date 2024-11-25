#include "fx_engine.h"

namespace fl {


FxEngine::FxEngine(uint16_t numLeds):
      mTimeFunction(0), 
      mCompositor(numLeds), 
      mCurrId(0) {
}

FxEngine::~FxEngine() {}

int FxEngine::addFx(FxPtr effect) {
    bool auto_set = mEffects.empty();
    bool ok = mEffects.insert(mCounter, effect);
    if (!ok) {
        return -1;
    }
    if (auto_set) {
        mCurrId = mCounter;
        mCompositor.startTransition(0, 0, effect);
    }
    return mCounter++;
}

int FxEngine::addVideo(Video video) {
    auto vidfx = VideoFxPtr::New(video);
    int id = addFx(vidfx);
    return id;
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

bool FxEngine::draw(uint32_t now, CRGB *finalBuffer) {
    mTimeFunction.update(now);
    uint32_t warpedTime = mTimeFunction.time();

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

}  // namespace fl
