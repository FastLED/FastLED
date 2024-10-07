#pragma once

#include "crgb.h"
#include "fixed_vector.h"
#include "fx/fx.h"
#include "fx/fx_layer.h"
#include "namespace.h"
#include "ptr.h"
#include <stdint.h>
#include <string.h>

#ifndef FASTLED_FX_ENGINE_MAX_FX
#define FASTLED_FX_ENGINE_MAX_FX 64
#endif

FASTLED_NAMESPACE_BEGIN

class FxCompositor {
public:
    FxCompositor(uint16_t numLeds) : mNumLeds(numLeds), mIsTransitioning(false) {
        mLayers[0] = FxLayerPtr::FromHeap(new FxLayer());
        mLayers[1] = FxLayerPtr::FromHeap(new FxLayer());
    }

    void startTransition(uint32_t now, uint32_t duration, RefPtr<Fx> nextFx) {
        completeTransition();
        if (duration == 0) {
            setLayerFx(nextFx, RefPtr<Fx>());
            return;
        }
        setLayerFx(mLayers[0]->getFx(), nextFx);
        mLayers[1]->setFx(nextFx);
        mIsTransitioning = true;
        mTransition.start(now, duration);
    }

    void completeTransition() {
        mIsTransitioning = false;
        if (mLayers[1]->getFx()) {
            swapLayers();
            mLayers[1]->release();
        }
    }

    void draw(uint32_t now, CRGB *finalBuffer);
    bool isTransitioning() const { return mIsTransitioning; }

private:
    void swapLayers() {
        FxLayerPtr tmp = mLayers[0];
        mLayers[0] = mLayers[1];
        mLayers[1] = tmp;
    }
    void setLayerFx(RefPtr<Fx> fx0, RefPtr<Fx> fx1) {
        if (fx0 == mLayers[1]->getFx()) {
            // Recycle the layer because the new fx needs
            // to keep it's state.
            FxLayerPtr tmp = mLayers[0];
            mLayers[0] = mLayers[1];
            mLayers[1] = tmp;
            // Setting the fx will pause the layer and memclear the framebuffer.
            mLayers[1]->setFx(fx1);
        } else {
            mLayers[0]->setFx(fx0);
            mLayers[1]->setFx(fx1);
        }
        mIsTransitioning = false;
    }

    FxLayerPtr mLayers[2];
    const uint16_t mNumLeds;
    bool mIsTransitioning;
    Transition mTransition;
};

inline void FxCompositor::draw(uint32_t now, CRGB *finalBuffer) {
    mLayers[0]->draw(now);
    if (!mIsTransitioning) {
        if (mLayers[0]->getSurface()) {
            memcpy(finalBuffer, mLayers[0]->getSurface(), sizeof(CRGB) * mNumLeds);
        }
        return;
    }
    mLayers[1]->draw(now);

    uint8_t progress = mTransition.getProgress(now);
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
