

#include "test.h"

#include "fx/fx.h"
#include "fx/fx2d.h"
#include "fx/video.h"
#include "fl/vector.h"
#include "FastLED.h"


FASTLED_SMART_PTR(Fake2d);

// Simple Fx2d object which writes a single red pixel to the first LED
// with the red component being the intensity of the frame counter.
class Fake2d : public Fx2d {
  public:
    Fake2d() : Fx2d(XYMap::constructRectangularGrid(1,1)) {}

    void draw(DrawContext context) override {
        CRGB c = mColors[mFrameCounter % mColors.size()];
        context.leds[0] = c;
        mFrameCounter++;
    }

    bool hasFixedFrameRate(float *fps) const override {
        *fps = 1;
        return true;
    }

    Str fxName() const override { return "Fake2d"; }
    uint8_t mFrameCounter = 0;
    FixedVector<CRGB, 5> mColors;
};



TEST_CASE("test_fixed_fps") {
    Fake2dPtr fake = fl::make_shared<Fake2d>();
    fake->mColors.push_back(CRGB(0, 0, 0));
    fake->mColors.push_back(CRGB(255, 0, 0));
    VideoFxWrapper wrapper(fake);
    wrapper.setFade(0, 0);
    CRGB leds[1];
    Fx::DrawContext context(0, leds);
    wrapper.draw(context);
    CHECK_EQ(1, fake->mFrameCounter);
    CHECK_EQ(leds[0], CRGB(0, 0, 0));
    context.now = 500;
    wrapper.draw(context);
    CHECK_EQ(2, fake->mFrameCounter);
    CHECK_EQ(leds[0], CRGB(127, 0, 0));
}
