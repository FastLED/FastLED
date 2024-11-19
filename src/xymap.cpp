
#include <stdint.h>
#include <string.h>

#include "force_inline.h"
#include "namespace.h"
#include "xymap.h"
#include "screenmap.h"

FASTLED_NAMESPACE_BEGIN


ScreenMap XYMap::toScreenMap() const {
    const uint16_t length = width * height;
    ScreenMap out(length);
    for (uint16_t w = 0; w < width; w++) {
        for (uint16_t h = 0; h < height; h++) {
            uint16_t index = mapToIndex(w, h);
            pair_xy_float p = {
                static_cast<float>(w),
                static_cast<float>(h)
            };
            out.set(index, p);
        }
    }
    return out;
}

FASTLED_NAMESPACE_END