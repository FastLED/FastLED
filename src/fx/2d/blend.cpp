/*
Fx2d class that allows to blend multiple Fx2d layers together.
The bottom layer is always drawn at full capacity. Upper layers
are blended by the the max luminance of the components.
*/


#include <stdint.h>

#include "blend.h"

namespace fl {


Blend2d::Blend2d(const XYMap& xymap): Fx2d(xymap) {
    // Warning, the xyMap will be the final transrformation applied to the
    // frame. If the delegate Fx2d layers have their own transformation then
    // both will be applied.
    mFrame = FramePtr::New(mXyMap.getTotal());
    mFrameTransform = FramePtr::New(mXyMap.getTotal());
}

Str Blend2d::fxName() const {
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

void Blend2d::add(Fx2dPtr layer) {
    mLayers.push_back(layer);
}

void Blend2d::add(Fx2d& layer) {
    Fx2dPtr fx = Fx2dPtr::NoTracking(layer);
    this->add(fx);
}

void Blend2d::draw(DrawContext context) {
   mFrame->clear();
   mFrameTransform->clear();

   // Draw each layer in reverse order and applying the blending.
   bool first = true;
   for (auto it = mLayers.begin(); it != mLayers.end(); ++it) {
       DrawContext tmp_ctx = context;
       tmp_ctx.leds = mFrame->rgb();
       (*it)->draw(tmp_ctx);
       DrawMode mode = first ? DrawMode::DRAW_MODE_OVERWRITE : DrawMode::DRAW_MODE_BLEND_BY_BLACK;
       first = false;
       mFrame->draw(mFrameTransform->rgb(), mode);
   }

   // Copy the final result to the output
   // memcpy(mFrameTransform->rgb(), context.leds, sizeof(CRGB) * mXyMap.getTotal());
   mFrameTransform->drawXY(context.leds, mXyMap, DrawMode::DRAW_MODE_OVERWRITE);
}

void Blend2d::clear() {
    mLayers.clear();
}

}
