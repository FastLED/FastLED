/// @file    FirstLight.ino
/// @brief   Animate a white dot moving along a strip of LEDs
/// @example FirstLight.ino

/*
 * BEGINNER'S GUIDE TO FASTLED - FIRST LIGHT EXAMPLE
 * 
 * This program creates a simple "scanning" effect - a single white LED moves
 * from one end of the strip to the other, like a bouncing ball or scanning laser.
 * This is often called a "larson scanner" effect.
 * 
 * What you'll learn:
 * - How to work with multiple LEDs using arrays
 * - How to use for loops to iterate through LEDs
 * - How array indexing works with LED strips
 * - The difference between setting colors and displaying them
 * - Basic animation timing with delay()
 * - How to turn LEDs on and off in sequence
 * 
 * HARDWARE NEEDED:
 * - Arduino or compatible microcontroller
 * - WS2812/NeoPixel LED strip (or similar)
 * - Power supply appropriate for your LED count
 * - Connecting wires
 */

// Use if you want to force the software SPI subsystem to be used for some reason (generally, you don't)
// #define FASTLED_FORCE_SOFTWARE_SPI
// Use if you want to force non-accelerated pin access (hint: you really don't, it breaks lots of things)
// #define FASTLED_FORCE_SOFTWARE_SPI
// #define FASTLED_FORCE_SOFTWARE_PINS
#include <FastLED.h>

///////////////////////////////////////////////////////////////////////////////////////////
//
// WHAT THIS PROGRAM DOES:
// This program moves a white dot along the strip of LEDs, from first to last.
// It demonstrates the basic concepts of LED array manipulation and simple animation.
// 

// Configuration constants - adjust these for your setup
#define NUM_LEDS 60         // How many LEDs are in your strip? Change this to match your hardware!

// Pin definitions - change these to match your wiring
#define DATA_PIN 3          // Which pin is connected to the data line of your LED strip?
#define CLOCK_PIN 13        // Only needed for SPI-based strips (4-wire)

/*
 * LED ARRAY EXPLANATION:
 * This creates an array of CRGB objects, one for each LED in your strip.
 * Think of it like a row of boxes, each box can hold one color.
 * The boxes are numbered from 0 to (NUM_LEDS - 1).
 * So if you have 60 LEDs, they're numbered 0, 1, 2, ... 58, 59.
 */
CRGB leds[NUM_LEDS];

/*
 * SETUP FUNCTION:
 * This runs once when the microcontroller starts up.
 * We use it to initialize FastLED and tell it about our LED strip.
 */
void setup() {
    /*
     * SAFETY DELAY:
     * This 2-second delay gives you time to unplug the system if something goes wrong.
     * It's especially important when working with lots of LEDs that could draw too much power.
     */
    delay(2000);

    /*
     * INITIALIZING FASTLED:
     * This tells FastLED what kind of LED strip you have and which pin it's connected to.
     * Uncomment the line that matches your LED type. WS2811 is used here as an example.
     * 
     * IMPORTANT: Choose the right LED type for your strip!
     * Using the wrong type can cause colors to appear wrong or LEDs not to work at all.
     */
    
    // ## Clockless types (most common) ##
    // FastLED.addLeds<NEOPIXEL, DATA_PIN>(leds, NUM_LEDS);         // Generic NeoPixel
    // FastLED.addLeds<WS2812, DATA_PIN, RGB>(leds, NUM_LEDS);      // WS2812 original
    // FastLED.addLeds<WS2812B, DATA_PIN, RGB>(leds, NUM_LEDS);     // WS2812B (most common)
    // FastLED.addLeds<SK6812, DATA_PIN, RGB>(leds, NUM_LEDS);      // SK6812 (RGBW capable)
    FastLED.addLeds<WS2811, DATA_PIN, RGB>(leds, NUM_LEDS);         // WS2811 (used in this example)
    
    // ## Clocked (SPI) types ##
    // FastLED.addLeds<APA102, DATA_PIN, CLOCK_PIN, RGB>(leds, NUM_LEDS);   // APA102/DotStar
    // FastLED.addLeds<WS2801, DATA_PIN, CLOCK_PIN, RGB>(leds, NUM_LEDS);   // WS2801
}

/*
 * LOOP FUNCTION:
 * This function runs continuously, over and over, creating our animation.
 * Each time through the loop, we move the white dot from start to finish.
 */
void loop() {
    /*
     * FOR LOOP EXPLANATION:
     * This for loop runs once for each LED in our strip.
     * - int whiteLed = 0: Start with the first LED (index 0)
     * - whiteLed < NUM_LEDS: Keep going while we haven't reached the end
     * - whiteLed = whiteLed + 1: After each iteration, move to the next LED
     * 
     * This is equivalent to: whiteLed++ (increment by 1)
     */
    for(int whiteLed = 0; whiteLed < NUM_LEDS; whiteLed = whiteLed + 1) {
        
        /*
         * SETTING THE LED COLOR:
         * Here we turn on the current LED by setting it to white.
         * CRGB::White is a predefined constant that represents full brightness
         * for all three color components (Red=255, Green=255, Blue=255).
         */
        leds[whiteLed] = CRGB::White;

        /*
         * DISPLAYING THE CHANGE:
         * FastLED.show() sends the color data to the actual LED strip.
         * This is when the LED physically lights up.
         * Remember: Setting the color in the array doesn't change the LED
         * until you call show()!
         */
        FastLED.show();

        /*
         * ANIMATION TIMING:
         * delay(100) pauses for 100 milliseconds (1/10th of a second).
         * This controls how fast the dot moves. Try different values:
         * - Smaller numbers = faster movement
         * - Larger numbers = slower movement
         * - delay(50) would be twice as fast
         * - delay(200) would be half as fast
         */
        delay(100);

        /*
         * TURNING OFF THE LED:
         * After showing the LED and waiting, we turn it off by setting it to black.
         * CRGB::Black means no color (Red=0, Green=0, Blue=0).
         * This creates the "moving" effect - only one LED is on at a time.
         * 
         * TRY THIS: Comment out this line to see what happens!
         * (Hint: you'll get a "comet trail" effect)
         */
        leds[whiteLed] = CRGB::Black;
    }
    
    /*
     * LOOP COMPLETION:
     * When the for loop finishes, we've moved the dot from the first LED
     * to the last LED. Then loop() automatically starts over from the beginning,
     * creating a continuous scanning effect.
     * 
     * EXPERIMENT IDEAS:
     * 1. Change CRGB::White to CRGB::Red or another color
     * 2. Try different delay values to change the speed
     * 3. Remove the "leds[whiteLed] = CRGB::Black;" line for a trail effect
     * 4. Add a second for loop to make the dot go backwards too
     */
}
