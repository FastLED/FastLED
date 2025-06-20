/// @file    DemoReel100.ino
/// @brief   FastLED "100 lines of code" demo reel, showing off some effects
/// @example DemoReel100.ino

/*
 * BEGINNER'S GUIDE TO FASTLED - DEMO REEL 100
 * 
 * This is a more advanced example that demonstrates multiple animation patterns
 * and automatic pattern switching. It's called "100 lines of code" because it
 * shows how much you can accomplish with relatively little code using FastLED.
 * 
 * What you'll learn:
 * - Function pointers (storing functions in variables)
 * - Arrays of functions
 * - Automatic pattern switching using timers
 * - Color palettes and palette-based effects
 * - Advanced FastLED functions like fill_rainbow(), beatsin16(), etc.
 * - Non-blocking timing with EVERY_N_MILLISECONDS()
 * - Color blending and mathematical color functions
 * - Random number generation for effects
 * 
 * ADVANCED CONCEPTS:
 * - Function arrays and pattern management
 * - Time-based animations without delay()
 * - Mathematical color generation
 * - Palette interpolation
 * - Beat-synchronized effects
 */

#include <FastLED.h>

// FastLED "100-lines-of-code" demo reel, showing just a few 
// of the kinds of animation patterns you can quickly and easily 
// compose using FastLED.  
//
// This example also shows one easy way to define multiple 
// animations patterns and have them automatically rotate.
//
// -Mark Kriegsman, December 2014

// Hardware configuration - adjust these for your setup
#define DATA_PIN    3           // Pin connected to the data line of your LED strip
//#define CLK_PIN   4           // Uncomment for SPI-based strips (4-wire)
#define LED_TYPE    WS2811      // Type of LED strip (WS2811, WS2812B, APA102, etc.)
#define COLOR_ORDER GRB         // Color order for your strip (GRB, RGB, BGR, etc.)
#define NUM_LEDS    64          // Number of LEDs in your strip
CRGB leds[NUM_LEDS];            // Array to hold the color data for each LED

// Animation configuration
#define BRIGHTNESS          96  // Overall brightness (0-255, start low!)
#define FRAMES_PER_SECOND  120  // How many times per second to update the LEDs

void setup() {
    /*
     * STARTUP DELAY:
     * 3-second delay gives you time to unplug if something goes wrong.
     * This is especially important with longer LED strips that draw more power.
     */
    delay(3000);
    
    /*
     * LED STRIP INITIALIZATION:
     * This tells FastLED about your specific hardware configuration.
     * The .setCorrection() helps colors look more natural by compensating
     * for the color characteristics of typical LED strips.
     */
    FastLED.addLeds<LED_TYPE,DATA_PIN,COLOR_ORDER>(leds, NUM_LEDS).setCorrection(TypicalLEDStrip);
    
    // Alternative for 4-wire (SPI) strips - uncomment if needed:
    // FastLED.addLeds<LED_TYPE,DATA_PIN,CLK_PIN,COLOR_ORDER>(leds, NUM_LEDS).setCorrection(TypicalLEDStrip);

    /*
     * MASTER BRIGHTNESS CONTROL:
     * This sets the overall brightness for all patterns.
     * Lower values prevent excessive power draw and eye strain.
     */
    FastLED.setBrightness(BRIGHTNESS);
}

/*
 * FUNCTION POINTER EXPLANATION:
 * A function pointer is like a variable that stores the address of a function.
 * Instead of storing a number or text, it stores "which function to call."
 * 
 * typedef void (*SimplePatternList[])(); means:
 * - void: the functions return nothing
 * - (*SimplePatternList[]): an array of function pointers
 * - (): the functions take no parameters
 * 
 * This allows us to store multiple animation functions in an array
 * and call them by index number.
 */
typedef void (*SimplePatternList[])();

/*
 * PATTERN ARRAY:
 * This array contains pointers to all our animation functions.
 * Each name (rainbow, confetti, etc.) is a function defined below.
 * We can call any pattern by using: gPatterns[index]()
 */
SimplePatternList gPatterns = { rainbow, rainbowWithGlitter, confetti, sinelon, juggle, bpm };

uint8_t gCurrentPatternNumber = 0;  // Index of which pattern is currently running
uint8_t gHue = 0;                   // Rotating "base color" used by many patterns

void loop()
{
    /*
     * PATTERN EXECUTION:
     * Call the current pattern function. This is like saying:
     * "Run whichever animation pattern is currently selected"
     * The function updates the 'leds' array with new colors.
     */
    gPatterns[gCurrentPatternNumber]();

    /*
     * DISPLAY THE RESULTS:
     * Send the updated 'leds' array to the actual LED strip.
     * This is when you see the visual changes.
     */
    FastLED.show();
    
    /*
     * FRAME RATE CONTROL:
     * FastLED.delay() is like delay(), but it also handles some internal
     * FastLED tasks. 1000/FRAMES_PER_SECOND calculates the delay needed
     * to achieve the target frame rate.
     * 
     * Example: 1000/120 = 8.33ms delay = about 120 FPS
     */
    FastLED.delay(1000/FRAMES_PER_SECOND);

    /*
     * NON-BLOCKING TIMERS:
     * These EVERY_N_* macros let us do things periodically without using delay().
     * This is important because delay() would stop the animation completely.
     * 
     * Think of these like alarms that go off at regular intervals.
     */
    EVERY_N_MILLISECONDS( 20 ) { 
        gHue++; // Slowly cycle the "base color" through the rainbow
    }
    
    EVERY_N_SECONDS( 10 ) { 
        nextPattern(); // Change to the next pattern every 10 seconds
    }
}

/*
 * ARRAY SIZE CALCULATION:
 * This macro calculates how many elements are in an array.
 * sizeof(A) = total bytes used by array
 * sizeof((A)[0]) = bytes used by one element
 * Dividing gives us the number of elements.
 */
#define ARRAY_SIZE(A) (sizeof(A) / sizeof((A)[0]))

/*
 * PATTERN SWITCHING FUNCTION:
 * This advances to the next pattern in our array.
 * The % operator (modulo) wraps around to 0 when we reach the end.
 */
void nextPattern()
{
    // Add one to current pattern number, wrap around at the end
    gCurrentPatternNumber = (gCurrentPatternNumber + 1) % ARRAY_SIZE( gPatterns);
}

// ========== ANIMATION PATTERN FUNCTIONS ==========
// Each function below creates a different visual effect

/*
 * RAINBOW PATTERN:
 * Creates a rainbow that spans the entire strip.
 * fill_rainbow() is a FastLED utility function that does the math for us.
 */
void rainbow() 
{
    /*
     * FILL_RAINBOW EXPLANATION:
     * - leds: the array to fill with colors
     * - NUM_LEDS: how many LEDs to fill
     * - gHue: starting hue (color) position
     * - 7: how much the hue changes between adjacent LEDs
     * 
     * A smaller last number (like 3) makes wider rainbow stripes
     * A larger number (like 15) makes narrower stripes
     */
    fill_rainbow( leds, NUM_LEDS, gHue, 7);
}

/*
 * RAINBOW WITH GLITTER:
 * Takes the rainbow pattern and adds random white sparkles.
 */
void rainbowWithGlitter() 
{
    rainbow();          // First, fill with rainbow
    addGlitter(80);     // Then add white sparkles
}

/*
 * ADD GLITTER FUNCTION:
 * Randomly adds white sparkles to the current pattern.
 */
void addGlitter( fract8 chanceOfGlitter) 
{
    /*
     * RANDOM GLITTER LOGIC:
     * random8() generates a random number from 0-255
     * If it's less than chanceOfGlitter, we add a sparkle
     * Higher chanceOfGlitter = more frequent sparkles
     */
    if( random8() < chanceOfGlitter) {
        /*
         * ADDING WHITE SPARKLES:
         * random16(NUM_LEDS) picks a random LED position
         * += means "add to the existing color" rather than replacing it
         * This creates white sparkles on top of the existing pattern
         */
        leds[ random16(NUM_LEDS) ] += CRGB::White;
    }
}

/*
 * CONFETTI PATTERN:
 * Random colored speckles that blink in and fade smoothly.
 */
void confetti() 
{
    /*
     * FADE TO BLACK:
     * fadeToBlackBy() dims all LEDs by a fixed amount each frame.
     * This creates the fading effect as old sparkles gradually disappear.
     * 10 is a gentle fade - try 20 or 50 for faster fading.
     */
    fadeToBlackBy( leds, NUM_LEDS, 10);
    
    /*
     * ADD NEW SPARKLES:
     * Pick a random position and add a random color
     */
    int pos = random16(NUM_LEDS);                               // Random position
    leds[pos] += CHSV( gHue + random8(64), 200, 255);          // Random color near current hue
}

/*
 * SINELON PATTERN:
 * A colored dot sweeping back and forth, with fading trails.
 * Uses mathematical sine waves for smooth motion.
 */
void sinelon()
{
    fadeToBlackBy( leds, NUM_LEDS, 20);     // Fade existing trails
    
    /*
     * BEATSIN16 EXPLANATION:
     * beatsin16() generates a sine wave that oscillates smoothly between two values.
     * - 13: beats per minute (controls speed)
     * - 0: minimum value (first LED)
     * - NUM_LEDS-1: maximum value (last LED)
     * 
     * This creates smooth back-and-forth motion instead of linear movement.
     */
    int pos = beatsin16( 13, 0, NUM_LEDS-1 );
    leds[pos] += CHSV( gHue, 255, 192);     // Add colored dot at calculated position
}

/*
 * BPM PATTERN:
 * Colored stripes pulsing at a defined Beats-Per-Minute (BPM).
 * Demonstrates color palettes and beat-synchronized effects.
 */
void bpm()
{
    uint8_t BeatsPerMinute = 62;            // Set the rhythm speed
    CRGBPalette16 palette = PartyColors_p;  // Use a predefined color palette
    
    /*
     * BEATSIN8 FOR BRIGHTNESS:
     * beatsin8() creates a sine wave for brightness that pulses with the beat.
     * The brightness oscillates between 64 and 255.
     */
    uint8_t beat = beatsin8( BeatsPerMinute, 64, 255);
    
    /*
     * PALETTE-BASED COLORING:
     * Instead of picking specific colors, we use a color palette.
     * This allows for smooth color transitions and professional-looking effects.
     */
    for( int i = 0; i < NUM_LEDS; i++) {
        /*
         * COLORFROMPALETTE EXPLANATION:
         * - palette: which color palette to use
         * - gHue+(i*2): palette position (changes with time and position)
         * - beat-gHue+(i*10): brightness (pulsing and varying by position)
         * 
         * This creates colorful stripes that pulse with the beat.
         */
        leds[i] = ColorFromPalette(palette, gHue+(i*2), beat-gHue+(i*10));
    }
}

/*
 * JUGGLE PATTERN:
 * Eight colored dots, weaving in and out of sync with each other.
 * Demonstrates multiple moving elements with different speeds.
 */
void juggle() {
    fadeToBlackBy( leds, NUM_LEDS, 20);     // Fade existing trails
    
    uint8_t dothue = 0;                     // Starting hue for the dots
    
    /*
     * MULTIPLE MOVING DOTS:
     * This loop creates 8 different dots, each moving at a slightly different speed.
     */
    for( int i = 0; i < 8; i++) {
        /*
         * BEATSIN16 WITH DIFFERENT SPEEDS:
         * Each dot uses (i+7) as its speed, so they move at different rates:
         * Dot 0: 7 BPM, Dot 1: 8 BPM, Dot 2: 9 BPM, etc.
         * This creates the "juggling" effect as they weave past each other.
         */
        leds[beatsin16( i+7, 0, NUM_LEDS-1 )] |= CHSV(dothue, 200, 255);
        
        /*
         * COLOR PROGRESSION:
         * Each dot gets a different color by incrementing dothue.
         * 32 * 8 = 256, so the colors span the full rainbow spectrum.
         */
        dothue += 32;
    }
}

/*
 * SUMMARY AND NEXT STEPS:
 * 
 * This example demonstrates many advanced FastLED concepts:
 * - Function pointers for pattern management
 * - Non-blocking timers for smooth animations
 * - Mathematical functions for natural motion
 * - Color palettes for professional effects
 * - Multiple animation layers and blending
 * 
 * EXPERIMENT IDEAS:
 * 1. Create your own pattern function and add it to gPatterns array
 * 2. Change the timing in EVERY_N_SECONDS() to switch patterns faster/slower
 * 3. Try different color palettes (RainbowColors_p, CloudColors_p, etc.)
 * 4. Adjust the BPM values to change animation speeds
 * 5. Combine elements from different patterns to create new effects
 * 6. Add user input to manually change patterns (buttons, serial commands, etc.)
 */
