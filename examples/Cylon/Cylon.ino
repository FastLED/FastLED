/// @file    Cylon.ino
/// @brief   An animation that moves a single LED back and forth as the entire strip changes.
///          (Larson Scanner effect)
/// @example Cylon.ino

/*
 * BEGINNER'S GUIDE TO FASTLED - CYLON EXAMPLE
 * 
 * This program creates the classic "Cylon eye" effect - a bright LED moves back and forth
 * along the strip, leaving fading trails behind it. The color also changes over time,
 * cycling through the rainbow spectrum.
 * 
 * This effect is named after the scanning red eye of the Cylons from Battlestar Galactica.
 * It's also known as a "Larson Scanner" after Glen Larson, the show's creator.
 * 
 * What you'll learn:
 * - HSV color space (Hue, Saturation, Value/Brightness)
 * - Static variables (variables that keep their value between function calls)
 * - Using namespaces to access FastLED features
 * - Creating fading/trailing effects
 * - Bidirectional animation (going forward AND backward)
 * - Serial output for debugging
 * - The nscale8() function for dimming colors
 * 
 * CONCEPTS INTRODUCED:
 * - HSV vs RGB color models
 * - Incremental color changes
 * - Memory persistence in functions
 * - Brightness scaling
 */

#include <FastLED.h>

/*
 * NAMESPACE EXPLANATION:
 * "using namespace fl;" tells the compiler to look in the FastLED (fl) namespace
 * for functions and constants. This is like saying "when I use a FastLED function,
 * you know I mean the one from FastLED library."
 * 
 * Think of namespaces like last names - if you're in a room with multiple people
 * named "John", you might need to say "John Smith" vs "John Doe" to be clear.
 * Namespaces do the same thing for code.
 */
using namespace fl;

// Configuration constants
#define NUM_LEDS 64         // How many LEDs in your strip? Adjust for your hardware
#define DATA_PIN 2          // Which pin is connected to your LED strip's data line
#define CLOCK_PIN 13        // Only needed for SPI-based LED strips (4-wire)

// Create the array to represent our LED strip
CRGB leds[NUM_LEDS];

void setup() { 
    /*
     * SERIAL COMMUNICATION SETUP:
     * Serial.begin() sets up communication between your Arduino and your computer.
     * 57600 is the "baud rate" - how fast data is sent. Think of it like the speed limit
     * for data transmission. This is useful for debugging and monitoring your program.
     */
    Serial.begin(57600);
    Serial.println("resetting");        // Send a message when the program starts
    
    /*
     * INITIALIZING THE LED STRIP:
     * This tells FastLED about your specific LED strip type and configuration.
     * WS2812 is a very common type of addressable LED.
     * RGB means the color order is Red-Green-Blue (some strips use different orders).
     */
    FastLED.addLeds<WS2812,DATA_PIN,RGB>(leds,NUM_LEDS);
    
    /*
     * BRIGHTNESS CONTROL:
     * setBrightness() controls the overall brightness of all LEDs.
     * 84 out of 255 is about 33% brightness - this helps prevent:
     * - Eye strain from super bright LEDs
     * - Excessive power consumption
     * - Potential damage to your power supply
     * 
     * Always start with lower brightness and increase as needed!
     */
    FastLED.setBrightness(84);
}

/*
 * FADE FUNCTION:
 * This function dims all LEDs in the strip by scaling their brightness.
 * It creates the "fading trail" effect behind the moving LED.
 */
void fadeall() { 
    /*
     * FOR LOOP THROUGH ALL LEDS:
     * This loop goes through every LED in the strip and dims it slightly.
     */
    for(int i = 0; i < NUM_LEDS; i++) { 
        /*
         * NSCALE8 EXPLANATION:
         * nscale8(250) scales the LED's brightness to 250/255 of its current value.
         * This means each LED becomes about 98% as bright as it was before.
         * 
         * - 255 would mean no change (100% brightness)
         * - 250 means 98% brightness (slight dimming)
         * - 200 would mean 78% brightness (faster dimming)
         * - 128 would mean 50% brightness (much faster dimming)
         * 
         * The smaller the number, the faster the trail fades away.
         */
        leds[i].nscale8(250); 
    } 
}

void loop() { 
    /*
     * STATIC VARIABLE EXPLANATION:
     * "static uint8_t hue = 0;" creates a variable that remembers its value
     * between different calls to loop(). 
     * 
     * Without "static": hue would reset to 0 every time loop() runs
     * With "static": hue keeps its value and can be incremented over time
     * 
     * uint8_t means "unsigned 8-bit integer" - a number from 0 to 255.
     * This is perfect for hue values, which range from 0-255 in FastLED.
     */
    static uint8_t hue = 0;
    
    Serial.print("x");      // Print a character to show the program is running
    
    /*
     * FORWARD MOVEMENT:
     * This for loop moves the LED from the beginning to the end of the strip.
     */
    for(int i = 0; i < NUM_LEDS; i++) {
        /*
         * HSV COLOR EXPLANATION:
         * CHSV creates a color using HSV (Hue, Saturation, Value) instead of RGB.
         * 
         * - Hue (hue++): The color on the rainbow spectrum (0-255)
         *   0=red, 42=orange, 85=green, 128=cyan, 170=blue, 213=purple, back to red
         * - Saturation (255): How "pure" the color is (255=vivid, 0=gray)
         * - Value (255): How bright the color is (255=full brightness, 0=black)
         * 
         * hue++ increments the hue each time, cycling through rainbow colors.
         * When hue reaches 255, it automatically wraps back to 0.
         */
        leds[i] = CHSV(hue++, 255, 255);
        
        FastLED.show();     // Display the current LED state
        
        /*
         * CREATING THE TRAIL EFFECT:
         * Instead of turning the LED completely off (like in FirstLight),
         * we call fadeall() to dim all LEDs slightly. This creates a
         * trailing effect behind the bright moving LED.
         */
        fadeall();
        
        /*
         * ANIMATION SPEED:
         * delay(10) makes each step take 10 milliseconds.
         * This is much faster than the FirstLight example (which used 100ms).
         * Try different values to change the speed!
         */
        delay(10);
    }
    
    Serial.print("x");      // Print another character to show direction change
    
    /*
     * BACKWARD MOVEMENT:
     * This for loop moves the LED from the end back to the beginning.
     * Notice how we start at (NUM_LEDS-1) and count down to 0.
     * 
     * Loop breakdown:
     * - int i = (NUM_LEDS)-1: Start at the last LED
     * - i >= 0: Keep going while we haven't reached the beginning
     * - i--: Decrement i (same as i = i - 1)
     */
    for(int i = (NUM_LEDS)-1; i >= 0; i--) {
        /*
         * SAME EFFECT, OPPOSITE DIRECTION:
         * The code inside this loop is identical to the forward loop,
         * but since we're counting backwards, the LED moves in reverse.
         */
        leds[i] = CHSV(hue++, 255, 255);    // Set current LED to current hue color
        FastLED.show();                      // Display the change
        fadeall();                           // Fade all LEDs for trailing effect
        delay(10);                           // Control animation speed
    }
    
    /*
     * LOOP CONTINUATION:
     * When both for loops complete, the main loop() function ends and
     * automatically starts over. This creates the continuous back-and-forth
     * scanning effect.
     * 
     * EXPERIMENT IDEAS:
     * 1. Change the fadeall() scale value (250) to make trails longer or shorter
     * 2. Adjust the delay(10) to change animation speed
     * 3. Try different saturation values in CHSV (255, 128, 0)
     * 4. Add a delay between forward and backward movement
     * 5. Change the brightness with FastLED.setBrightness()
     */
}
