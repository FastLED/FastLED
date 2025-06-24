
#include "gfx.h"
#include "FastLED.h"

// the rendering buffer (16*16)
// do not touch
CRGB leds[NUM_LEDS];
XYMap xyMap = XYMap::constructRectangularGrid(CUSTOM_WIDTH, CUSTOM_HEIGHT);

void InitGraphics() {
    // set the brightness
    auto* controller = &FastLED.addLeds<CHIPSET, LED_PIN, COLOR_ORDER>(leds + 5, NUM_LEDS - 5);
    fl::ScreenMap screenMap = xyMap.toScreenMap();
    controller->setScreenMap(screenMap);
    // use this line only when using a custom size matrix
    // FastLED.addLeds<CHIPSET, LED_PIN, COLOR_ORDER>(leds2, CUSTOM_HEIGHT *
    // CUSTOM_WIDTH);
    FastLED.setBrightness(BRIGHTNESS);
    FastLED.setDither(0);
}

void GraphicsShow() {
    // when using a matrix different than 16*16 use
    // RenderCustomMatrix();
    FastLED.show();
}