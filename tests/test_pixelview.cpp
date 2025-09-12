#include "doctest.h"
#include "FastLED.h"

TEST_CASE("CPixelView basic functionality") {
    // Setup test data
    CRGB leds[10];
    for(int i = 0; i < 10; i++) {
        leds[i] = CRGB(i * 25, i * 20, i * 15);
    }
    
    CRGBSet pixels(leds, 10);
    
    SUBCASE("Array-like access") {
        // Test reading
        CHECK(pixels[0].r == 0);
        CHECK(pixels[5].r == 125);
        CHECK(pixels[9].r == 225);
        
        // Test writing
        pixels[3] = CRGB::Red;
        CHECK(pixels[3] == CRGB::Red);
        CHECK(leds[3] == CRGB::Red);
    }
    
    SUBCASE("Subset creation") {
        // Create subset from indices 2-6
        CPixelView<CRGB> subset = pixels(2, 6);
        CHECK(subset.size() == 5);
        
        // Verify subset points to correct data
        CHECK(subset[0] == pixels[2]);
        CHECK(subset[4] == pixels[6]);
        
        // Modify through subset
        subset[1] = CRGB::Blue;
        CHECK(pixels[3] == CRGB::Blue);
        CHECK(leds[3] == CRGB::Blue);
    }
    
    SUBCASE("Reverse direction") {
        // Create reverse subset (6 to 2)  
        CPixelView<CRGB> reverse = pixels(6, 2);
        CHECK(reverse.size() == 5);
        CHECK(reverse.reversed() == true);
        
        // Verify reverse ordering
        CHECK(reverse[0] == pixels[6]);
        CHECK(reverse[1] == pixels[5]);
        CHECK(reverse[4] == pixels[2]);
        
        // Test reverse iteration
        int expected = 6;
        for(auto& pixel : reverse) {
            CHECK(pixel == pixels[expected]);
            expected--;
        }
    }
}

TEST_CASE("CPixelView color operations") {
    CRGB leds[5];
    CRGBSet pixels(leds, 5);
    
    SUBCASE("Fill operations") {
        pixels.fill_solid(CRGB::Green);
        for(int i = 0; i < 5; i++) {
            CHECK(pixels[i] == CRGB::Green);
        }
    }
    
    SUBCASE("Scaling operations") {
        pixels.fill_solid(CRGB(100, 100, 100));
        pixels.nscale8_video(128); // ~50% brightness
        CHECK(pixels[0].r == 51);  // 100 * 128 / 255 = 50.196... -> 51
        CHECK(pixels[0].g == 51);
        CHECK(pixels[0].b == 51);
    }
}

TEST_CASE("CPixelView equality and comparison") {
    CRGB leds1[3] = {CRGB::Red, CRGB::Green, CRGB::Blue};
    CRGB leds2[3] = {CRGB::Red, CRGB::Green, CRGB::Blue};
    
    CRGBSet pixels1(leds1, 3);
    CRGBSet pixels2(leds1, 3);  // Same data
    CRGBSet pixels3(leds2, 3);  // Different data, same values
    
    SUBCASE("Equality comparison") {
        CHECK(pixels1 == pixels2);  // Same data pointer
        CHECK_FALSE(pixels1 == pixels3);  // Different data pointer
    }
    
    SUBCASE("Boolean conversion") {
        pixels1.fill_solid(CRGB::Black);
        CHECK_FALSE(pixels1);  // All black = false
        
        pixels1[1] = CRGB::Red;
        CHECK(pixels1);  // Has non-zero pixel = true
    }
}
