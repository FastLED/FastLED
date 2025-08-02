
// g++ --std=c++11 test.cpp

#include <iostream>

#include "test.h"

#include "fx/2d/blend.h"
#include "fx/fx.h"
#include "fx/time.h"
#include "test.h"

#include "fl/namespace.h"
#include "fl/scoped_array.h"
#include "fl/ostream.h"

using namespace fl;

// Simple test effect that fills with a solid color
class SolidColorFx2d : public fl::Fx2d {
  public:
    SolidColorFx2d(uint16_t width, uint16_t height, CRGB color)
        : fl::Fx2d(fl::XYMap::constructRectangularGrid(width, height)),
          mColor(color) {}

    fl::string fxName() const override { return "SolidColorFx2d"; }

    void draw(Fx::DrawContext context) override {
        for (uint16_t i = 0; i < mXyMap.getTotal(); i++) {
            context.leds[i] = mColor;
        }
    }

  private:
    CRGB mColor;
};

class TestFx2D : public fl::Fx2d {
  public:
    TestFx2D(uint16_t width, uint16_t height)
        : fl::Fx2d(fl::XYMap::constructRectangularGrid(width, height)) {
        mLeds.reset(new CRGB[width * height]);
    }

    void set(uint16_t x, uint16_t y, CRGB color) {
        if (x < mXyMap.getWidth() && y < mXyMap.getHeight()) {
            uint16_t index = mXyMap(x, y);
            if (index < mXyMap.getTotal()) {
                mLeds[index] = color;
            }
        }
    }

    fl::string fxName() const override { return "TestFx2D"; }

    void draw(Fx::DrawContext context) override {
        for (uint16_t i = 0; i < mXyMap.getTotal(); i++) {
            context.leds[i] = mLeds[i];
        }
    }

    scoped_array<CRGB> mLeds;
};

TEST_CASE("Test FX2d Layered Blending") {
    const uint16_t width = 1;
    const uint16_t height = 1;
    XYMap xyMap = XYMap::constructRectangularGrid(width, height);

    // Create a red layer
    SolidColorFx2d redLayer(width, height, CRGB(255, 0, 0));

    // Create a layered effect with just the red layer
    fl::Blend2d blendFx(xyMap);
    blendFx.add(redLayer);

    // Create a buffer for the output
    CRGB led;

    // Draw the layered effect
    Fx::DrawContext context(0, &led);
    context.now = 0;
    blendFx.draw(context);

    // Verify the result - should be red
    CHECK(led.r == 255);
    CHECK(led.g == 0);
    CHECK(led.b == 0);
}

TEST_CASE("Test FX2d Layered with XYMap") {
    enum {
        width = 2,
        height = 2,
    };

    XYMap xyMapSerp = XYMap::constructSerpentine(width, height);
    XYMap xyRect = XYMap::constructRectangularGrid(width, height);

    SUBCASE("Rectangular Grid") {
        // Create a blue layer
        // SolidColorFx2d blueLayer(width, height, CRGB(0, 0, 255));
        TestFx2D testFx(width, height);
        testFx.set(0, 0, CRGB(0, 0, 255)); // Set the first pixel to blue
        testFx.set(1, 0, CRGB(255, 0, 0)); // Set the second pixel to red
        testFx.set(0, 1, CRGB(0, 255, 0)); // Set the third pixel to gree
        testFx.set(1, 1, CRGB(0, 0, 0)); // Set the fourth pixel to black

        // Create a layered effect with just the blue layer
        fl::Blend2d blendFx(xyRect);
        blendFx.add(testFx);

        // Create a buffer for the output
        CRGB led[width * height] = {};

        // Draw the layered effect
        Fx::DrawContext context(0, led);
        context.now = 0;
        blendFx.draw(context);

        cout << "Layered Effect Output: " << led[0].toString().c_str() << endl;
        cout << "Layered Effect Output: " << led[1].toString().c_str() << endl;
        cout << "Layered Effect Output: " << led[2].toString().c_str() << endl;
        cout << "Layered Effect Output: " << led[3].toString().c_str() << endl;

        // Verify the result - should be blue
        CHECK(led[0].r == 0);
        CHECK(led[0].g == 0);
        CHECK(led[0].b == 255);

        CHECK(led[1].r == 255);
        CHECK(led[1].g == 0);
        CHECK(led[1].b == 0);

        CHECK(led[2].r == 0);
        CHECK(led[2].g == 255);
        CHECK(led[2].b == 0);

        CHECK(led[3].r == 0);
        CHECK(led[3].g == 0);
        CHECK(led[3].b == 0);
    }

    SUBCASE("Serpentine") {
        // Create a blue layer
        TestFx2D testFx(width, height);
        testFx.set(0, 0, CRGB(0, 0, 255)); // Set the first pixel to blue
        testFx.set(1, 0, CRGB(255, 0, 0)); // Set the second pixel to red
        testFx.set(0, 1, CRGB(0, 255, 0)); // Set the third pixel to gree
        testFx.set(1, 1, CRGB(0, 0, 0)); // Set the fourth pixel to black

        // Create a layered effect with just the blue layer
        fl::Blend2d blendFx(xyMapSerp);
        blendFx.add(testFx);

        // Create a buffer for the output
        CRGB led[width * height] = {};

        // Draw the layered effect
        Fx::DrawContext context(0, led);
        context.now = 0;
        blendFx.draw(context);

        cout << "Layered Effect Output: " << led[0].toString().c_str() << endl;
        cout << "Layered Effect Output: " << led[1].toString().c_str() << endl;
        cout << "Layered Effect Output: " << led[2].toString().c_str() << endl;
        cout << "Layered Effect Output: " << led[3].toString().c_str() << endl;

        // Verify the result - should be blue
        CHECK(led[0].r == 0);
        CHECK(led[0].g == 0);
        CHECK(led[0].b == 255);

        CHECK(led[1].r == 255);
        CHECK(led[1].g == 0);
        CHECK(led[1].b == 0);

        // Now it's supposed to go up to the next line at the same column.
        CHECK(led[2].r == 0);
        CHECK(led[2].g == 0);
        CHECK(led[2].b == 0);

        CHECK(led[3].r == 0);
        CHECK(led[3].g == 255);
        CHECK(led[3].b == 0);
    }
}
