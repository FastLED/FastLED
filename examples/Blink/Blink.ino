/// @file    Blink.ino
/// @brief   Blink the first LED of an LED strip
/// @example Blink.ino

/*
 * BEGINNER'S GUIDE TO FASTLED - BLINK EXAMPLE
 * 
 * This is the simplest possible FastLED program - it just blinks one LED on and off.
 * Think of it as the "Hello World" of LED programming!
 * 
 * What you'll learn:
 * - How to include the FastLED library
 * - How to define constants using #define
 * - How to create an array to represent your LEDs
 * - How to initialize FastLED in setup()
 * - How to control LED colors in loop()
 * - The difference between setting LED colors and showing them
 */

// This line includes the FastLED library, which gives us all the LED control functions
#include <FastLED.h>

// Constants: These are values that won't change during program execution
// #define creates a text replacement - wherever NUM_LEDS appears, it becomes 1
#define NUM_LEDS 1          // We're only using 1 LED for this simple example

// Pin definitions: These tell FastLED which pins on your microcontroller to use
#define DATA_PIN 3          // The pin connected to the data line of your LED strip
#define CLOCK_PIN 13        // Only needed for SPI-based LEDs (4-wire strips)

/*
 * LED ARRAY EXPLANATION:
 * This creates an array called 'leds' that can hold NUM_LEDS number of CRGB objects.
 * Think of CRGB as a container that holds one color (Red, Green, Blue values).
 * Each element in this array represents one physical LED in your strip.
 * Array indexing starts at 0, so the first LED is leds[0], second is leds[1], etc.
 */
CRGB leds[NUM_LEDS];

/*
 * SETUP FUNCTION:
 * This function runs ONCE when your microcontroller starts up.
 * Use it to initialize FastLED and configure your LED strip settings.
 */
void setup() { 
    /*
     * CHOOSING YOUR LED TYPE:
     * Uncomment (remove //) from the line that matches your LED strip type.
     * If you're not sure what type you have, NEOPIXEL is a safe default for WS2812-based strips.
     * 
     * The most common types are:
     * - NEOPIXEL: Generic term for WS2812/WS2812B strips (most common)
     * - WS2812B: Very common addressable LED strip
     * - APA102/DOTSTAR: Higher-end strips with separate clock and data lines
     * - SK6812: RGBW strips (Red, Green, Blue, White)
     * 
     * CLOCKLESS vs CLOCKED LEDs:
     * - Clockless (3-wire): Only need DATA_PIN (power, ground, data)
     * - Clocked (4-wire): Need both DATA_PIN and CLOCK_PIN (power, ground, data, clock)
     */
    
    // ## Clockless types (3-wire: power, ground, data) ##
    FastLED.addLeds<NEOPIXEL, DATA_PIN>(leds, NUM_LEDS);  // Most common - good default choice
    // FastLED.addLeds<WS2812, DATA_PIN, RGB>(leds, NUM_LEDS);      // Specific WS2812 chip
    // FastLED.addLeds<WS2812B, DATA_PIN, RGB>(leds, NUM_LEDS);     // Most common WS2812 variant
    // FastLED.addLeds<SK6812, DATA_PIN, RGB>(leds, NUM_LEDS);      // RGBW capable strip
    // FastLED.addLeds<WS2811, DATA_PIN, RGB>(leds, NUM_LEDS);      // Lower-level WS2811 chip
    
    // ## Clocked (SPI) types (4-wire: power, ground, data, clock) ##
    // FastLED.addLeds<APA102, DATA_PIN, CLOCK_PIN, RGB>(leds, NUM_LEDS);   // High-end APA102
    // FastLED.addLeds<DOTSTAR, DATA_PIN, CLOCK_PIN, RGB>(leds, NUM_LEDS);  // Adafruit's APA102
    // FastLED.addLeds<WS2801, DATA_PIN, CLOCK_PIN, RGB>(leds, NUM_LEDS);   // Older WS2801 strip
    
    // For a complete list of supported LED types, see the FastLED documentation
}

/*
 * LOOP FUNCTION:
 * This function runs continuously, over and over, forever.
 * Put your main LED animation code here.
 */
void loop() { 
    /*
     * SETTING LED COLORS:
     * We set the color of the first (and only) LED by assigning a color to leds[0].
     * CRGB::Red is a predefined constant for the color red.
     * Other predefined colors: CRGB::Green, CRGB::Blue, CRGB::White, CRGB::Black, etc.
     */
    leds[0] = CRGB::Red;        // Set the first LED to red
    
    /*
     * DISPLAYING THE COLORS:
     * FastLED.show() is crucial! This actually sends the color data to your LED strip.
     * Until you call show(), the LEDs won't change - the colors are just stored in memory.
     */
    FastLED.show();             // Send the colors to the actual LEDs
    
    /*
     * TIMING:
     * delay(500) pauses the program for 500 milliseconds (half a second).
     * This creates the "on" period of the blink.
     */
    delay(500);                 // Wait for 500 milliseconds
    
    // Turn the LED off by setting it to black (no color)
    leds[0] = CRGB::Black;      // Black means all color components (R,G,B) are 0
    FastLED.show();             // Send the "off" state to the LED
    delay(500);                 // Wait another 500 milliseconds
    
    // The loop() function automatically starts over, creating a continuous blink
}
