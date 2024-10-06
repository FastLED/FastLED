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

class FxCompositingEngine {
public:
    FxCompositingEngine(uint16_t numLeds) : mNumLeds(numLeds), mIsTransitioning(false) {
        mLayers[0] = LayerPtr::FromHeap(new Layer());
        mLayers[1] = LayerPtr::FromHeap(new Layer());
        // TODO: When there is only Fx in the list then don't allocate memory for
        // the second layer
        mLayers[0]->surface.reset(new CRGB[numLeds]);
        mLayers[1]->surface.reset(new CRGB[numLeds]);
    }

    void setLayerFx(RefPtr<Fx> fx0, RefPtr<Fx> fx1) {
        mLayers[0]->fx = fx0;
        mLayers[1]->fx = fx1;
    }

    void startTransition(uint32_t now, uint32_t duration) {
        mIsTransitioning = true;
        mTransition.start(now, duration);
    }

    void draw(uint32_t now, CRGB *finalBuffer);

    bool isTransitioning() const { return mIsTransitioning; }
    void completeTransition() { mIsTransitioning = false; }

    LayerPtr mLayers[2];
    const uint16_t mNumLeds;

private:
    bool mIsTransitioning;
    Transition mTransition;
};

inline void FxCompositingEngine::draw(uint32_t now, CRGB *finalBuffer) {
    Fx::DrawContext context = {now, mLayers[0]->surface.get()};
    mLayers[0]->fx->draw(context);

    if (!mIsTransitioning) {
        memcpy(finalBuffer, mLayers[0]->surface.get(), sizeof(CRGB) * mNumLeds);
        return;
    }

    context = {now, mLayers[1]->surface.get()};
    mLayers[1]->fx->draw(context);

    uint8_t progress = mTransition.getProgress(now);
    uint8_t inverse_progress = 255 - progress;

    const CRGB* surface0 = mLayers[0]->surface.get();
    const CRGB* surface1 = mLayers[1]->surface.get();

    for (uint16_t i = 0; i < mNumLeds; i++) {
        CRGB p0 = surface0[i];
        CRGB p1 = surface1[i];
        p0.nscale8(inverse_progress);
        p1.nscale8(progress);
        finalBuffer[i] = p0 + p1;
    }

    if (progress == 255) {
        mIsTransitioning = false;
    }
}

FASTLED_NAMESPACE_END
