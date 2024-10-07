#pragma once

#include "crgb.h"
#include "fixed_vector.h"
#include "fx/fx.h"
#include "fx/util/fx_compositor.h"
#include "fx/util/fx_layer.h"
#include "namespace.h"
#include "ptr.h"
#include <stdint.h>
#include <string.h>

#ifndef FASTLED_FX_ENGINE_MAX_FX
#define FASTLED_FX_ENGINE_MAX_FX 64
#endif

FASTLED_NAMESPACE_BEGIN

class FxEngine {
  public:
    FxEngine(uint16_t numLeds);
    ~FxEngine();
    bool addFx(RefPtr<Fx> effect);
    void draw(uint32_t now, CRGB *outputBuffer);
    bool nextFx(uint16_t duration);
    bool setNextFx(uint16_t index, uint16_t duration);

  private:
    uint16_t mNumLeds;
    FixedVector<RefPtr<Fx>, FASTLED_FX_ENGINE_MAX_FX> mEffects;
    FxCompositor mCompositor;
    uint16_t mCurrentIndex;
    uint16_t mDuration = 0;
    bool mDurationSet = false;
};

inline FxEngine::FxEngine(uint16_t numLeds)
    : mNumLeds(numLeds), mCompositor(numLeds), mCurrentIndex(0) {
}

inline FxEngine::~FxEngine() {}

inline bool FxEngine::addFx(RefPtr<Fx> effect) {
    if (mEffects.size() >= FASTLED_FX_ENGINE_MAX_FX) {
        return false;
    }
    mEffects.push_back(effect);
    if (mEffects.size() == 1) {
        mCompositor.startTransition(0, 0, mEffects[0]);
    }
    return true;
}

inline bool FxEngine::nextFx(uint16_t duration) {
    uint16_t next_index = (mCurrentIndex + 1) % mEffects.size();
    return setNextFx(next_index, duration);
}

inline bool FxEngine::setNextFx(uint16_t index, uint16_t duration) {
    if (index >= mEffects.size() || index == mCurrentIndex) {
        return false;
    }
    mCurrentIndex = index;
    mDuration = duration;
    mDurationSet = true;
    return true;
}

inline void FxEngine::draw(uint32_t now, CRGB *finalBuffer) {
    if (mDurationSet) {
        mCompositor.startTransition(now, mDuration, mEffects[mCurrentIndex]);
        mDurationSet = false;
    }
    if (!mEffects.empty()) {
        mCompositor.draw(now, finalBuffer);
    }
}

FASTLED_NAMESPACE_END
