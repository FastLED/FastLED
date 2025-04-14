
/*
Experimental bilinearn downscaling algorithm. Not tested yet and completely
"vibe-coded" by ai.

If you use this and find an issue then please report it.
*/


#include "crgb.h"

namespace fl {

void downscaleBilinear(const CRGB *src, int srcWidth, int srcHeight,
                       CRGB *dst, int dstWidth, int dstHeight);

} // namespace fl