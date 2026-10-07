// IWYU pragma: private
// ok no header - public CRGB declarations remain in crgb.h.

/// @brief CRGB matrix scaling delegates, linked with graphics utilities.

#define FASTLED_INTERNAL
#include "crgb.h"
#include "fl/gfx/downscale.h"
#include "fl/gfx/upscale.h"
#include "fl/log/log.h"
#include "fl/math/xymap.h"
#include "fl/stl/noexcept.h"

void CRGB::downscale(const CRGB *src, const fl::XYMap &srcXY, CRGB *dst,
                     const fl::XYMap &dstXY) FL_NO_EXCEPT {
    fl::downscale(src, srcXY, dst, dstXY);
}

void CRGB::upscale(const CRGB *src, const fl::XYMap &srcXY, CRGB *dst,
                   const fl::XYMap &dstXY) FL_NO_EXCEPT {
    FL_WARN_IF(srcXY.getType() != fl::XYMap::kLineByLine, "Upscaling only works with a src matrix that is rectangular");
    fl::u16 w = srcXY.getWidth();
    fl::u16 h = srcXY.getHeight();
    fl::upscale(src, dst, w, h, dstXY);
}
