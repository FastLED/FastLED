
/*
Experimental bilinearn downscaling algorithm. Not tested yet and completely
"vibe-coded" by ai.

If you use this and find an issue then please report it.
*/


#include "crgb.h"

namespace fl {

class XYMap;

void downscale(const CRGB* src, const XYMap& srcXY, CRGB* dst, const XYMap& dstXY);

#ifdef FASTLED_TESTING
void downscaleHalf(const CRGB *src, uint16_t srcWidth, uint16_t srcHeight, CRGB *dst);
void downscaleHalf(const CRGB* src, const XYMap& srcXY, CRGB* dst, const XYMap& dstXY);
#endif  // FASTLED_TESTING

} // namespace fl