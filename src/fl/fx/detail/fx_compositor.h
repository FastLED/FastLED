#pragma once

#include "fl/stl/stdint.h"
#include "fl/stl/cstring.h"
#include "fl/stl/span.h"

#include "crgb.h"  // IWYU pragma: keep
#include "fl/stl/shared_ptr.h"  // For shared_ptr
#include "fl/stl/vector.h"  // IWYU pragma: keep
#include "fl/fx/detail/fx_layer.h"
#include "fl/fx/fx.h"  // IWYU pragma: keep
#include "fl/stl/noexcept.h"

#ifndef FASTLED_FX_ENGINE_MAX_FX
#define FASTLED_FX_ENGINE_MAX_FX 64
#endif

namespace fl {

// Takes two fx layers and composites them together to a final output buffer.
class FxCompositor {
  public:
    FxCompositor(fl::u32 numLeds) FL_NO_EXCEPT : mNumLeds(numLeds) {
        mLayers[0] = fl::make_shared<FxLayer>();
        mLayers[1] = fl::make_shared<FxLayer>();
    }

    void startTransition(fl::u32 now, fl::u32 duration, fl::shared_ptr<Fx> nextFx) FL_NO_EXCEPT {
        completeTransition();
        if (duration == 0) {
            mLayers[0]->setFx(nextFx);
            return;
        }
        mLayers[1]->setFx(nextFx);
        mTransition.start(now, duration);
    }

    void completeTransition() FL_NO_EXCEPT {
        if (mLayers[1]->getFx()) {
            swapLayers();
            mLayers[1]->release();
        }
        mTransition.end();
    }

    void draw(fl::u32 now, fl::u32 warpedTime, fl::span<CRGB> finalBuffer,
              float speed = 1.0f, const AudioBatch *audio = nullptr) FL_NO_EXCEPT;

  private:
    void swapLayers() FL_NO_EXCEPT {
        FxLayerPtr tmp = mLayers[0];
        mLayers[0] = mLayers[1];
        mLayers[1] = tmp;
    }

    FxLayerPtr mLayers[2];
    const fl::u32 mNumLeds;
    Transition mTransition;
};

inline void FxCompositor::draw(fl::u32 now, fl::u32 warpedTime,
                               fl::span<CRGB> finalBuffer,
                               float speed, const AudioBatch *audio) FL_NO_EXCEPT {
    if (!mLayers[0]->getFx()) {
        return;
    }
    mLayers[0]->draw(warpedTime, speed, audio);
    u8 progress = mTransition.getProgress(now);
    if (!progress) {
        fl::span<CRGB> surface0 = mLayers[0]->getSurface();
        fl::memcpy(finalBuffer.data(), surface0.data(), sizeof(CRGB) * mNumLeds);
        return;
    }
    mLayers[1]->draw(warpedTime, speed, audio);
    fl::span<CRGB> surface0 = mLayers[0]->getSurface();
    fl::span<CRGB> surface1 = mLayers[1]->getSurface();

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
