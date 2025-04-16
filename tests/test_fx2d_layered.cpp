
// g++ --std=c++11 test.cpp

#include "test.h"

#include "test.h"
#include "fx/time.h"
#include "fx/2d/blend.h"
#include "fx/fx.h"

#include "fl/namespace.h"

using namespace fl;

// Simple test effect that fills with a solid color
class SolidColorFx2d : public fl::Fx2d {
public:
    SolidColorFx2d(uint16_t width, uint16_t height, CRGB color)
        : fl::Fx2d(fl::XYMap::constructRectangularGrid(width, height)), mColor(color) {}

    fl::Str fxName() const override { return "SolidColorFx2d"; }

    void draw(Fx::DrawContext context) override {
        for (uint16_t i = 0; i < mXyMap.getTotal(); i++) {
            context.leds[i] = mColor;
        }
    }

private:
    CRGB mColor;
};

TEST_CASE("Test FX2d Layered Blending") {
    const uint16_t width = 1;
    const uint16_t height = 1;
    
    // Create a red layer
    SolidColorFx2d redLayer(width, height, CRGB(255, 0, 0));
    
    // Create a layered effect with just the red layer
    fl::Blend2d blendFx(width, height);
    blendFx.add(redLayer);
    
    // Create a buffer for the output
    CRGB led;
    
    // Draw the layered effect
    Fx::DrawContext context(0, &led);
    context.now = 0;
    blendFx.draw(context);

    MESSAGE("Layered Effect Output: %s", led.toString().c_str());
    
    // Verify the result - should be red
    CHECK(led.r == 255);
    CHECK(led.g == 0);
    CHECK(led.b == 0);
}
