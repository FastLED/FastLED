#pragma once

#include "fl/stdint.h"
#include <string.h>

#include "crgb.h"
#include "fl/namespace.h"
#include "fl/memory.h"
#include "fl/vector.h"
#include "fx/detail/fx_layer.h"
#include "fx/fx.h"

#ifndef FASTLED_FX_ENGINE_MAX_FX
#define FASTLED_FX_ENGINE_MAX_FX 64
#endif

namespace fl {

// Takes two fx layers and composites them together to a final output buffer.
class FxCompositor {
  public:
    FxCompositor(fl::u32 numLeds) : mNumLeds(numLeds) {
        mLayers[0] = fl::make_shared<FxLayer>();
        mLayers[1] = fl::make_shared<FxLayer>();
    }

    void startTransition(fl::u32 now, fl::u32 duration, fl::shared_ptr<Fx> nextFx) {
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

    void draw(fl::u32 now, fl::u32 warpedTime, CRGB *finalBuffer);

  private:
    void swapLayers() {
        FxLayerPtr tmp = mLayers[0];
        mLayers[0] = mLayers[1];
        mLayers[1] = tmp;
    }

    FxLayerPtr mLayers[2];
    const fl::u32 mNumLeds;
    Transition mTransition;
};

inline void FxCompositor::draw(fl::u32 now, fl::u32 warpedTime,
                               CRGB *finalBuffer) {
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
    const CRGB *surface0 = mLayers[0]->getSurface();
    const CRGB *surface1 = mLayers[1]->getSurface();

    for (fl::u32 i = 0; i < mNumLeds; i++) {
        const CRGB &p0 = surface0[i];
        const CRGB &p1 = surface1[i];
        CRGB out = CRGB::blend(p0, p1, progress);
        finalBuffer[i] = out;
    }
    if (progress == 255) {
        completeTransition();
    }
}

} // namespace fl
