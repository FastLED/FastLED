// ok no header - implementation for fl/channels/color_managed_source.h

#include "fl/channels/color_managed_source.h"

namespace fl {

void ColorManagedPixelSource::loadAndScaleRGB(u8* b0_out, u8* b1_out,
                                             u8* b2_out) FL_NO_EXCEPT {
        const u8* raw = mController.mData;
        i32 drives[3];
        processPixelQ16(mPipeline, raw[0], raw[1], raw[2], drives);
        u8 channels[3];
        for (int i = 0; i < 3; ++i) {
            channels[i] = quantize(drives[i]);
        }
        *b0_out = channels[mSlot0];
        *b1_out = channels[mSlot1];
        *b2_out = channels[mSlot2];
}

void ColorManagedPixelSource::loadAndScaleRGBW(const Rgbw& rgbw, u8* b0_out,
                                              u8* b1_out, u8* b2_out,
                                              u8* b3_out) FL_NO_EXCEPT {
        mController.loadAndScaleRGBW(rgbw, b0_out, b1_out, b2_out, b3_out);
}

void ColorManagedPixelSource::loadAndScaleRGBWW(Rgbww rgbww, u8* b0_out,
                                               u8* b1_out, u8* b2_out,
                                               u8* b3_out,
                                               u8* b4_out) FL_NO_EXCEPT {
        mController.loadAndScaleRGBWW(rgbww, b0_out, b1_out, b2_out, b3_out,
                                      b4_out);
}

#if FASTLED_HD_COLOR_MIXING
// Guarded to match the declaration. Without this the definition survives on a
// build with HD mixing off -- AVR, among others -- where the member does not
// exist, and the compile fails there and nowhere else.
void ColorManagedPixelSource::loadRGBScaleAndBrightness(
    u8* c0, u8* c1, u8* c2, u8* brightness) FL_NO_EXCEPT {
    mController.loadRGBScaleAndBrightness(c0, c1, c2, brightness);
}
#endif

u8 ColorManagedPixelSource::quantize(i32 drive) FL_NO_EXCEPT {
        if (drive <= 0) {
            return 0;
        }
        if (drive >= 65536) {
            return 255;
        }
        return static_cast<u8>((static_cast<i32>(drive) * 255 + 32768) >> 16);
}
}  // namespace fl
