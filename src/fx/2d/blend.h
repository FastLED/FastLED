#pragma once

#include <stdint.h>

#include "fl/xymap.h"
#include "fl/namespace.h"
#include "fl/ptr.h"
#include "fl/vector.h"
#include "fl/warn.h"
#include "fx/fx.h"
#include "fx/fx2d.h"
#include "fx/frame.h"

namespace fl {

class Fx2dBlend: public Fx2d {
  public:
    Fx2dBlend(uint16_t width, uint16_t height): Fx2d(XYMap::constructRectangularGrid(width, height)) {
        // Warning, the xyMap will be the final transrformation applied to the
        // frame. If the delegate Fx2d layers have their own transformation then
        // both will be applied.
        mFrame = FramePtr::New(mXyMap.getTotal());
        mFrameTransform = FramePtr::New(mXyMap.getTotal());
    }

    fl::Str fxName() const override {
        fl::Str out = "LayeredFx2d(";
        for (size_t i = 0; i < mLayers.size(); ++i) {
            out += mLayers[i]->fxName();
            if (i != mLayers.size() - 1) {
                out += ",";
            }
        }
        out += ")";
        return out;
    }

    void add(Fx2dPtr layer) {
        mLayers.push_back(layer);
    }

    void add(Fx2d& layer) {
        Fx2dPtr fx = Fx2dPtr::NoTracking(layer);
        this->add(fx);
    }

     virtual void draw(DrawContext context) override {
        mFrame->clear();
        mFrameTransform->clear();

        // Draw each layer in reverse order and applying the blending.
        for (auto it = mLayers.rbegin(); it != mLayers.rend(); ++it) {
            DrawContext tmp_ctx = context;
            tmp_ctx.leds = mFrame->rgb();
            (*it)->draw(tmp_ctx);
            mFrame->draw(mFrameTransform->rgb(), DrawMode::DRAW_MODE_BLEND_BY_BLACK);
        }

        // Copy the final result to the output
        // memcpy(mFrameTransform->rgb(), context.leds, sizeof(CRGB) * mXyMap.getTotal());
        mFrameTransform->draw(context.leds, DrawMode::DRAW_MODE_OVERWRITE);
     }

    void clear() {
        mLayers.clear();
    }
    
  protected:
    HeapVector<Fx2dPtr> mLayers;
    FramePtr mFrame;
    FramePtr mFrameTransform;
};

}
