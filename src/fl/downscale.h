
#pragma once

/*
Experimental bilinearn downscaling algorithm. Not tested yet and completely
"vibe-coded" by ai.

If you use this and find an issue then please report it.
*/

#include "fl/int.h"
#include "crgb.h"

namespace fl {

class XYMap;

void downscale(const CRGB *src, const XYMap &srcXY, CRGB *dst,
               const XYMap &dstXY);

// Optimized versions for downscaling by 50%. This is here for testing purposes
// mostly. You should prefer to use downscale(...) instead of calling these
// functions. It's important to note that downscale(...) will invoke
// downscaleHalf(...) automatically when the source and destination are half the
// size of each other.
void downscaleHalf(const CRGB *src, fl::u16 srcWidth, fl::u16 srcHeight,
                   CRGB *dst);
void downscaleHalf(const CRGB *src, const XYMap &srcXY, CRGB *dst,
                   const XYMap &dstXY);
void downscaleArbitrary(const CRGB *src, const XYMap &srcXY, CRGB *dst,
                        const XYMap &dstXY);

} // namespace fl
