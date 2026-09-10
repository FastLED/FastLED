#pragma once

// Streaming colour-managed pixel source (P6, #4040).
//
// The last stage of the wiring: something a `PixelIterator` can be built
// over that runs the pipeline per pixel instead of the legacy scale.
//
// It is a source, not a buffer, and that is the point. C2 asks for the
// per-pixel work to happen inside the existing iteration with no RGB16
// framebuffer, and B3 forbids an intermediate RGB8 one. `showPixels`
// already has a place that rewrites pixels -- the XYMap path materializes a
// thread-local `CRGB` vector -- and putting the pipeline there would break
// both rules at once. This reads one pixel, transforms it, and yields the
// bytes; nothing is held between pixels.
//
// `PixelIterator`'s constructor is templated over its source, so this only
// has to provide the same members `PixelController` does. It is not itself
// templated on colour order: the order is applied to the three outputs at
// the end, which avoids six near-identical instantiations for something
// decided at bind time.

#include "fl/gfx/pipeline.h"
#include "fl/stl/int.h"
#include "fl/stl/noexcept.h"
#include "fl/gfx/eorder.h"
#include "fl/gfx/rgbw.h"
#include "fl/gfx/rgbww.h"
#include "pixel_controller.h"

namespace fl {

/// Runs `processPixelQ16` over a `PixelController`'s pixels as they are read.
///
/// Only the plain RGB path is colour-managed. `loadAndScaleRGBW`,
/// `loadAndScaleRGBWW` and the HD entry points delegate to the wrapped
/// controller, which is the legacy behaviour, and they say why at each site.
/// Those are not oversights: each is blocked on something this phase does
/// not own.
class ColorManagedPixelSource {
  public:
    ColorManagedPixelSource(PixelController<RGB>& controller, EOrder order,
                            const StreamingPipelineQ16& pipeline) FL_NO_EXCEPT
        : mController(controller), mPipeline(pipeline),
          mSlot0(RGB_BYTE0(order)), mSlot1(RGB_BYTE1(order)),
          mSlot2(RGB_BYTE2(order)) {}

    /// The colour-managed path.
    ///
    /// Reads the *raw* pixel rather than `loadAndScale0/1/2`, deliberately.
    /// Those fold in `mColorAdjustment`, whose `premixed` carries brightness
    /// -- and the pipeline carries brightness too, as C4's flux scalar, so
    /// going through them would apply it twice. Legacy correction and
    /// temperature are not a concern here: binding a profile clears them,
    /// which is what the `LegacyClearedByProfile` warning is for.
    void loadAndScaleRGB(u8* b0_out, u8* b1_out, u8* b2_out) FL_NO_EXCEPT;

    /// Legacy. An RGBW device's white emitter has no home in
    /// `EmitterProfile`, which carries three primaries and nothing else, so
    /// there is no profile for `allocateEmitterDrivesQ16` to be given. That
    /// is a schema question (P1/P3), not something to invent here.
    void loadAndScaleRGBW(const Rgbw& rgbw, u8* b0_out, u8* b1_out, u8* b2_out,
                          u8* b3_out) FL_NO_EXCEPT;

    /// Legacy, for the same reason, with two whites instead of one -- and
    /// the two-white allocation is itself unimplemented (#4198).
    void loadAndScaleRGBWW(Rgbww rgbww, u8* b0_out, u8* b1_out, u8* b2_out,
                           u8* b3_out, u8* b4_out) FL_NO_EXCEPT;

#if FASTLED_HD_COLOR_MIXING
    /// Legacy. This hands the encoder a colour triple *and* a separate 5-bit
    /// brightness for APA102-HD. What that should mean once the pipeline
    /// owns the amplitude stage is P8's question -- chipset-aware
    /// quantization and 5-bit semantics -- and answering it by whatever
    /// makes this compile would be the wrong way round.
    void loadRGBScaleAndBrightness(u8* c0, u8* c1, u8* c2,
                                   u8* brightness) FL_NO_EXCEPT;
#endif

    /// Dithering stays with the controller; P8 owns it.
    void stepDithering() FL_NO_EXCEPT { mController.stepDithering(); }

    void advanceData() FL_NO_EXCEPT { mController.advanceData(); }
    int size() const FL_NO_EXCEPT { return mController.size(); }
    bool has(int n) FL_NO_EXCEPT { return mController.has(n); }

  private:
    /// s16.16 drive to an 8-bit code, rounding to nearest.
    ///
    /// This is the single final quantization B3 asks for: nothing upstream
    /// of it is 8-bit, and nothing downstream re-quantizes.
    static u8 quantize(i32 drive) FL_NO_EXCEPT;

    PixelController<RGB>& mController;
    const StreamingPipelineQ16& mPipeline;
    int mSlot0;
    int mSlot1;
    int mSlot2;
};

}  // namespace fl
