#pragma once

#include <stdint.h>
#include <string.h>

#include "crgb.h"
#include "fixed_vector.h"
#include "fx/fx.h"
#include "fx/detail/fx_layer.h"
#include "namespace.h"
#include "ref.h"



#ifndef FASTLED_FX_ENGINE_MAX_FX
#define FASTLED_FX_ENGINE_MAX_FX 64
#endif

FASTLED_NAMESPACE_BEGIN

// Takes two fx layers and composites them together to a final output buffer.
class FxCompositor {
public:
    FxCompositor(uint16_t numLeds) : mNumLeds(numLeds) {
        mLayers[0] = FxLayerRef::New();
        mLayers[1] = FxLayerRef::New();
    }

    void startTransition(uint32_t now, uint32_t duration, Ref<Fx> nextFx) {
        completeTransition();
        if (duration == 0) {
            mLayers[0]->setFx(nextFx);
            return;
        }
        mLayers[1]->setFx(nextFx);
        mTransition.start(now, duration);
    }

    void completeTransition() {
        if (mLayers[1]->getFx()) {
            swapLayers();
            mLayers[1]->release();
        }
        mTransition.end();
    }

    void draw(uint32_t now, uint32_t warpedTime, CRGB *finalBuffer);

private:
    void swapLayers() {
        FxLayerRef tmp = mLayers[0];
        mLayers[0] = mLayers[1];
        mLayers[1] = tmp;
    }

    FxLayerRef mLayers[2];
    const uint16_t mNumLeds;
    Transition mTransition;
};

inline void FxCompositor::draw(uint32_t now, uint32_t warpedTime, CRGB *finalBuffer) {
    if (!mLayers[0]->getFx()) {
        return;
    }
    mLayers[0]->draw(warpedTime);
    uint8_t progress = mTransition.getProgress(now);
    if (!progress) {
        memcpy(finalBuffer, mLayers[0]->getSurface(), sizeof(CRGB) * mNumLeds);
        return;
    }
    mLayers[1]->draw(warpedTime);
    const CRGB* surface0 = mLayers[0]->getSurface();
    const CRGB* surface1 = mLayers[1]->getSurface();

    for (uint16_t i = 0; i < mNumLeds; i++) {
        const CRGB& p0 = surface0[i];
        const CRGB& p1 = surface1[i];
        CRGB out = CRGB::blend(p0, p1, progress);
        finalBuffer[i] = out;
    }
    if (progress == 255) {
        completeTransition();
    }
}

FASTLED_NAMESPACE_END
