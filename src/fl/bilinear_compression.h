
/*
Experimental bilinearn downscaling algorithm. Not tested yet and completely
"vibe-coded" by ai.

If you use this and find an issue then please report it.
*/


#include "crgb.h"

namespace fl {

#if 0
class XYMap;


void downscaleBilinear(const CRGB *src, uint16_t srcWidth, uint16_t srcHeight,
                       CRGB *dst, uint16_t dstWidth, uint16_t dstHeight);

void downscaleBilinearMapped(const CRGB* src, const XYMap& srcMap,
                             CRGB* dst, const XYMap& dstMap);
#endif

void downscaleHalf(const CRGB *src, uint16_t srcWidth, uint16_t srcHeight, CRGB *dst);

} // namespace fl