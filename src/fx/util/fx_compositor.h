#pragma once

#include "crgb.h"
#include "fixed_vector.h"
#include "fx/fx.h"
#include "fx/util/fx_layer.h"
#include "namespace.h"
#include "ptr.h"
#include <stdint.h>
#include <string.h>
// #include <iostream>


#ifndef FASTLED_FX_ENGINE_MAX_FX
#define FASTLED_FX_ENGINE_MAX_FX 64
#endif

FASTLED_NAMESPACE_BEGIN

// Takes two fx layers and composites them together to a final output buffer.
class FxCompositor {
public:
    FxCompositor(uint16_t numLeds) : mNumLeds(numLeds) {
        mLayers[0] = FxLayerPtr::FromHeap(new FxLayer());
        mLayers[1] = FxLayerPtr::FromHeap(new FxLayer());
    }

    void startTransition(uint32_t now, uint32_t duration, RefPtr<Fx> nextFx) {
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

    void draw(uint32_t now, CRGB *finalBuffer);

private:
    void swapLayers() {
        FxLayerPtr tmp = mLayers[0];
        mLayers[0] = mLayers[1];
        mLayers[1] = tmp;
    }

    FxLayerPtr mLayers[2];
    const uint16_t mNumLeds;
    Transition mTransition;
};

inline void FxCompositor::draw(uint32_t now, CRGB *finalBuffer) {
    if (!mLayers[0]->getFx()) {
        return;
    }
    mLayers[0]->draw(now);
    uint8_t progress = mTransition.getProgress(now);
    if (!progress) {
        //std::cout << "Not transitioning: " << mLayers[0]->getFx()->fxName(0) << std::endl;
        memcpy(finalBuffer, mLayers[0]->getSurface(), sizeof(CRGB) * mNumLeds);
        return;
    }
    mLayers[1]->draw(now);
    //std::cout << "Progress: " << int(progress) << " on " << mLayers[0]->getFx()->fxName(0) << " and " << mLayers[1]->getFx()->fxName(0) << std::endl;
    uint8_t inverse_progress = 255 - progress;
    const CRGB* surface0 = mLayers[0]->getSurface();
    const CRGB* surface1 = mLayers[1]->getSurface();

    for (uint16_t i = 0; i < mNumLeds; i++) {
        CRGB p0 = surface0[i];
        CRGB p1 = surface1[i];
        p0.nscale8(inverse_progress);
        p1.nscale8(progress);
        finalBuffer[i] = p0 + p1;
    }
    if (progress == 255) {
        completeTransition();
    }
}

FASTLED_NAMESPACE_END
