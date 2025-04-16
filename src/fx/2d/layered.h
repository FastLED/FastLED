#pragma once

#include <stdint.h>

#include "fl/xymap.h"
#include "fl/namespace.h"
#include "fl/ptr.h"
#include "fl/vector.h"
#include "fx/fx.h"
#include "fx/fx2d.h"
#include "fx/frame.h"

namespace fl {

class Fx2dLayered: public Fx2d {
  public:
    Fx2dLayered(const XYMap& xyMap): Fx2d(xyMap) {
        mFrame = FramePtr::New(xyMap.getTotal());
    }

    void addLayer(Fx2dPtr layer) {
        mLayers.push_back(layer);
    }

     virtual void draw(DrawContext context) override {
        mFrame->clear();
        DrawContext tmp = context;
        tmp.leds = mFrame->rgb();

        for (auto it = mLayers.rbegin(); it != mLayers.rend(); ++it) {
            (*it)->draw(tmp);
            mFrame->draw(context.leds, DrawMode::DRAW_MODE_BLEND_BY_BLACK);
        }
     }

    void clear() {
        mLayers.clear();
    }
    
  protected:
    HeapVector<Fx2dPtr> mLayers;
    FramePtr mFrame;
};

}