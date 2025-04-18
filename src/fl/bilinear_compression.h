
/*
Experimental bilinearn downscaling algorithm. Not tested yet and completely
"vibe-coded" by ai.

If you use this and find an issue then please report it.
*/


#include "crgb.h"

namespace fl {

void downscaleBilinear(const CRGB *src, uint16_t srcWidth, uint16_t srcHeight,
                       CRGB *dst, uint16_t dstWidth, uint16_t dstHeight);

} // namespace fl