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
    FxCompositingEngine(uint16_t numLeds) : mNumLeds(numLeds) {
        mLayers[0] = LayerPtr::FromHeap(new Layer());
        mLayers[1] = LayerPtr::FromHeap(new Layer());
        // TODO: When there is only Fx in the list then don't allocate memory for
        // the second layer
        mLayers[0]->surface.reset(new CRGB[numLeds]);
        mLayers[1]->surface.reset(new CRGB[numLeds]);
    }

    LayerPtr mLayers[2];
    const uint16_t mNumLeds;
};

FASTLED_NAMESPACE_END
