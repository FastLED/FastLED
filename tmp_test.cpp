#include "FastLED.h"

void test() {
    CRGB leds[10];
    CRGBPalette16 palette;
    
    // Test fill_solid - should work with using declaration
    fill_solid(leds, 10, CRGB::Red);
    
    // Test ColorFromPalette - should work with using declaration  
    CRGB color = ColorFromPalette(palette, 0, 255, LINEARBLEND);
    
    // Test with explicit namespace - should always work
    fl::fill_solid(leds, 10, CRGB::Blue);
    CRGB color2 = fl::ColorFromPalette(palette, 0, 255, LINEARBLEND);
}