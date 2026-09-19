// ok no header - implementation for fl/channels/color_managed_source.h

#include "fl/channels/color_managed_source.h"
#include "fl/channels/dither_frame.h"

namespace fl {

namespace {

// One threshold per frame of the eight-frame cycle, in 1/256 of a code, in
// bit-reversed order (0, 4, 2, 6, 1, 5, 3, 7) so that however many frames of
// the eight carry the extra code, they are spread across the cycle rather than
// bunched. Each sits in the middle of its 1/8 band, so a fraction f of a code
// is rounded up on ceil((f - 16) / 32) of the eight frames: the cycle's mean
// is within 1/16 of a code of the exact value. Eight frames is the cycle
// BINARY_DITHER already runs, so this adds no new cadence (see
// docs/color-ws2812-shaping-paths.md).
constexpr u8 kTemporalDitherThresholds[8] = {16, 144, 80, 208, 48, 176, 112, 240};

}  // namespace

ColorManagedPixelSource::ColorManagedPixelSource(
    PixelController<RGB>& controller, EOrder order,
    const StreamingPipelineQ16& pipeline, u8 dither_phase) FL_NO_EXCEPT
    : mController(controller), mPipeline(pipeline),
      mSlot0(RGB_BYTE0(order)), mSlot1(RGB_BYTE1(order)),
      mSlot2(RGB_BYTE2(order)), mDitherPhase(dither_phase) {}

ColorManagedPixelSource::ColorManagedPixelSource(
    PixelController<RGB>& controller, EOrder order,
    const StreamingPipelineQ16& pipeline) FL_NO_EXCEPT
    : ColorManagedPixelSource(controller, order, pipeline,
                              detail::ditherFrame()) {}

void ColorManagedPixelSource::loadAndScaleRGB(u8* b0_out, u8* b1_out,
                                             u8* b2_out) FL_NO_EXCEPT {
        const u8* raw = mController.mData;
        i32 drives[3];
        processPixelQ16(mPipeline, raw[0], raw[1], raw[2], drives);
        u8 channels[3];
        if (temporalDitherEnabled()) {
            // The phase is the frame's, offset by the pixel's position, so
            // any eight neighbours cover the whole cycle in every frame: a
            // uniform strip's total light does not pulse. All three channels
            // share it, so a neutral pixel is neutral in every frame.
            const int position = mController.mLen - mController.mLenRemaining;
            const u8 phase = static_cast<u8>((mDitherPhase + (position & 7)) & 7);
            const u8 threshold = kTemporalDitherThresholds[phase];
            for (int i = 0; i < 3; ++i) {
                channels[i] = quantizeDithered(drives[i], threshold);
            }
        } else {
            for (int i = 0; i < 3; ++i) {
                channels[i] = quantize(drives[i]);
            }
        }
        *b0_out = channels[mSlot0];
        *b1_out = channels[mSlot1];
        *b2_out = channels[mSlot2];
}

#if !FL_PLATFORM_HAS_TINY_MEMORY
void ColorManagedPixelSource::loadAndScaleRGB16(u16* b0_out, u16* b1_out,
                                               u16* b2_out) FL_NO_EXCEPT {
        const u8* raw = mController.mData;
        i32 drives[3];
        processPixelQ16(mPipeline, raw[0], raw[1], raw[2], drives);
        u16 channels[3];
        for (int i = 0; i < 3; ++i) {
            channels[i] = quantize16(drives[i]);
        }
        *b0_out = channels[mSlot0];
        *b1_out = channels[mSlot1];
        *b2_out = channels[mSlot2];
}
#endif

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
    // Full scale: the drives already carry brightness as C4's flux scalar, so
    // the controller's brightness here would dim the strip a second time
    // through the 5-bit field. B1's conservative treatment -- SK9822, and any
    // chip without current-vs-chromaticity data -- holds the field fixed.
    *c0 = 255;
    *c1 = 255;
    *c2 = 255;
    *brightness = 255;
}
#endif

bool ColorManagedPixelSource::temporalDitherEnabled() const FL_NO_EXCEPT {
        return (mController.e[0] | mController.e[1] | mController.e[2]) != 0;
}

u8 ColorManagedPixelSource::quantizeDithered(i32 drive,
                                             u8 threshold) FL_NO_EXCEPT {
        if (drive <= 0) {
            return 0;
        }
        if (drive >= 65536) {
            return 255;
        }
        // drive * 255 < 2^24: the exact code in 16.16, split into its integer
        // part and its whole 16-bit fraction. Compared at full width, so the
        // cycle's mean is within 1/16 of a code exactly; truncating the
        // fraction to eight bits first would add up to 1/256 on top.
        const u32 exact = static_cast<u32>(drive) * 255u;
        const u32 base = exact >> 16;
        const u32 fraction = exact & 0xFFFFu;
        const u32 code =
            base + (fraction > (static_cast<u32>(threshold) << 8) ? 1u : 0u);
        return static_cast<u8>(code > 255u ? 255u : code);
}

u8 ColorManagedPixelSource::quantize(i32 drive) FL_NO_EXCEPT {
        if (drive <= 0) {
            return 0;
        }
        if (drive >= 65536) {
            return 255;
        }
        return static_cast<u8>((static_cast<i32>(drive) * 255 + 32768) >> 16);
}

#if !FL_PLATFORM_HAS_TINY_MEMORY
u16 ColorManagedPixelSource::quantize16(i32 drive) FL_NO_EXCEPT {
        if (drive <= 0) {
            return 0;
        }
        if (drive >= 65536) {
            return 65535;
        }
        // 64-bit intermediate on purpose: drive reaches 65535 and 65535*65535
        // leaves i32. The 8-bit form above multiplies by 255 and stays inside
        // it, which is why it does not need this.
        return static_cast<u16>(
            (static_cast<i64>(drive) * 65535 + 32768) >> 16);
}
#endif
}  // namespace fl
