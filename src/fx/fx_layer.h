#pragma once

#include "crgb.h"
#include "fixed_vector.h"
#include "fx/fx.h"
#include "namespace.h"
#include "ptr.h"
#include <stdint.h>
#include <string.h>


FASTLED_NAMESPACE_BEGIN

struct Layer;
typedef RefPtr<Layer> LayerPtr;
struct Layer : public Referent {
    scoped_array<CRGB> surface;
    scoped_array<uint8_t> surface_alpha;
    RefPtr<Fx> fx;
};

FASTLED_NAMESPACE_END
