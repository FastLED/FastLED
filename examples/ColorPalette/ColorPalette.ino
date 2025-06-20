/// @file    ColorPalette.ino
/// @brief   Demonstrates how to use @ref ColorPalettes
/// @example ColorPalette.ino

/*
 * BEGINNER'S GUIDE TO FASTLED - COLOR PALETTES
 * 
 * This example demonstrates one of FastLED's most powerful features: COLOR PALETTES.
 * Think of a palette like a painter's palette - instead of defining every single color
 * you want to use, you create a collection of colors that work well together.
 * 
 * WHY USE PALETTES?
 * 1. Memory efficient - Store 16 colors, get 256 smooth transitions
 * 2. Easy to change themes - Swap one palette for completely different look
 * 3. Professional results - Predefined palettes look great together
 * 4. Smooth gradients - Automatic interpolation between palette colors
 * 
 * What you'll learn:
 * - How to use predefined color palettes
 * - Creating custom palettes
 * - Different blending modes (LINEARBLEND vs NOBLEND)
 * - Using ColorFromPalette() to get colors
 * - Time-based palette switching
 * - Forward function declarations
 * - Static variables in functions
 * 
 * CONCEPTS COVERED:
 * - Palette interpolation and indexing
 * - Memory management with PROGMEM
 * - Function organization and declarations
 */

#include <FastLED.h>

// Hardware configuration
#define LED_PIN     5           // Pin connected to LED strip data line
#define NUM_LEDS    50          // Number of LEDs in your strip
#define BRIGHTNESS  64          // Overall brightness (0-255, keep lower for safety)
#define LED_TYPE    WS2811      // Type of LED strip
#define COLOR_ORDER GRB         // Color order for your specific strip
CRGB leds[NUM_LEDS];           // Array to hold LED color data

#define UPDATES_PER_SECOND 100  // How often to refresh the display

/*
 * PALETTE AND BLENDING EXPLANATION:
 * 
 * CRGBPalette16: A palette that holds 16 distinct colors but can be accessed
 * as if it has 256 colors through interpolation. This saves memory while
 * providing smooth color transitions.
 * 
 * TBlendType: Controls how colors are mixed when accessing between palette entries:
 * - LINEARBLEND: Smooth gradients between colors (recommended for most effects)
 * - NOBLEND: Sharp transitions, no mixing between colors
 */

// This example shows several ways to set up and use 'palettes' of colors
// with FastLED.
//
// These compact palettes provide an easy way to re-colorize your
// animation on the fly, quickly, easily, and with low overhead.
//
// USING palettes is MUCH simpler in practice than in theory, so first just
// run this sketch, and watch the pretty lights as you then read through
// the code.  Although this sketch has eight (or more) different color schemes,
// the entire sketch compiles down to about 6.5K on AVR.
//
// FastLED provides a few pre-configured color palettes, and makes it
// extremely easy to make up your own color schemes with palettes.
//
// Some notes on the more abstract 'theory and practice' of
// FastLED compact palettes are at the bottom of this file.

CRGBPalette16 currentPalette;   // The palette currently being used
TBlendType    currentBlending;  // How colors blend between palette entries

/*
 * FORWARD DECLARATIONS:
 * In C++, you must declare functions before you use them. These declarations
 * tell the compiler "these functions exist and will be defined later."
 * This is like a table of contents for your functions.
 */
extern CRGBPalette16 myRedWhiteBluePalette;                    // Custom palette in RAM
extern const TProgmemPalette16 myRedWhiteBluePalette_p FL_PROGMEM; // Custom palette in flash memory

// Function declarations - these tell the compiler what functions we'll define later
void ChangePalettePeriodically();        // Switches between different palettes over time
void FillLEDsFromPaletteColors(uint8_t colorIndex);  // Maps palette colors to LEDs
void SetupPurpleAndGreenPalette();       // Creates a custom purple/green palette
void SetupTotallyRandomPalette();        // Creates a palette with random colors
void SetupBlackAndWhiteStripedPalette(); // Creates a black/white striped palette

void setup() {
    /*
     * STARTUP SAFETY:
     * 3-second delay provides time to disconnect power if something goes wrong.
     */
    delay( 3000 );
    
    /*
     * LED STRIP INITIALIZATION:
     * Configure FastLED for your specific hardware and apply color correction
     * for more natural-looking colors.
     */
    FastLED.addLeds<LED_TYPE, LED_PIN, COLOR_ORDER>(leds, NUM_LEDS).setCorrection( TypicalLEDStrip );
    FastLED.setBrightness( BRIGHTNESS );
    
    /*
     * INITIAL PALETTE SETUP:
     * Start with a rainbow palette and linear blending for smooth transitions.
     * RainbowColors_p is one of FastLED's built-in palettes.
     */
    currentPalette = RainbowColors_p;
    currentBlending = LINEARBLEND;
}

void loop()
{
    /*
     * PALETTE MANAGEMENT:
     * Check if it's time to switch to a different palette.
     * This happens automatically based on elapsed time.
     */
    ChangePalettePeriodically();
    
    /*
     * ANIMATION INDEX:
     * static variables retain their value between function calls.
     * startIndex controls which part of the palette we're displaying.
     * Incrementing it creates the moving/shifting effect.
     */
    static uint8_t startIndex = 0;
    startIndex = startIndex + 1; /* motion speed - try different values! */
    
    /*
     * APPLY PALETTE TO LEDS:
     * Use the current palette to set colors for all LEDs.
     */
    FillLEDsFromPaletteColors( startIndex);
    
    /*
     * DISPLAY AND TIMING:
     * Show the results and maintain consistent frame rate.
     */
    FastLED.show();
    FastLED.delay(1000 / UPDATES_PER_SECOND);
}

/*
 * PALETTE-TO-LED MAPPING FUNCTION:
 * This function demonstrates how to get colors from a palette and apply them to LEDs.
 */
void FillLEDsFromPaletteColors( uint8_t colorIndex)
{
    uint8_t brightness = 255;   // Full brightness for all LEDs
    
    /*
     * LED LOOP:
     * For each LED, get a color from the palette and apply it.
     */
    for( int i = 0; i < NUM_LEDS; ++i) {
        /*
         * COLORFROMPALETTE EXPLANATION:
         * - currentPalette: which palette to use
         * - colorIndex: position in the palette (0-255)
         * - brightness: how bright this LED should be (0-255)
         * - currentBlending: how to blend between palette colors
         * 
         * colorIndex is incremented for each LED, creating a gradient effect
         * across the strip. Try different increment values (+= 1, += 5, += 10)
         */
        leds[i] = ColorFromPalette( currentPalette, colorIndex, brightness, currentBlending);
        colorIndex += 3;  // Move to next palette position (try different values!)
    }
}

// There are several different palettes of colors demonstrated here.
//
// FastLED provides several 'preset' palettes: RainbowColors_p, RainbowStripeColors_p,
// OceanColors_p, CloudColors_p, LavaColors_p, ForestColors_p, and PartyColors_p.
//
// Additionally, you can manually define your own color palettes, or you can write
// code that creates color palettes on the fly.  All are shown here.

/*
 * AUTOMATIC PALETTE SWITCHING:
 * This function changes palettes every 5-10 seconds, demonstrating
 * different palette types and blending modes.
 */
void ChangePalettePeriodically()
{
    /*
     * TIME-BASED SWITCHING:
     * Use millis() to get current time and create a "seconds" counter.
     * The % 60 operator gives us seconds from 0-59, creating a minute-long cycle.
     */
    uint8_t secondHand = (millis() / 1000) % 60;
    static uint8_t lastSecond = 99;  // Initialize to impossible value
    
    /*
     * CHANGE DETECTION:
     * Only update palette when the second changes, not every loop iteration.
     * This prevents constant palette switching.
     */
    if( lastSecond != secondHand) {
        lastSecond = secondHand;
        
        /*
         * PALETTE SCHEDULE:
         * Different palettes are activated at different times.
         * Notice how some use LINEARBLEND (smooth) vs NOBLEND (sharp transitions).
         */
        if( secondHand ==  0)  { currentPalette = RainbowColors_p;         currentBlending = LINEARBLEND; }
        if( secondHand == 10)  { currentPalette = RainbowStripeColors_p;   currentBlending = NOBLEND;  }
        if( secondHand == 15)  { currentPalette = RainbowStripeColors_p;   currentBlending = LINEARBLEND; }
        if( secondHand == 20)  { SetupPurpleAndGreenPalette();             currentBlending = LINEARBLEND; }
        if( secondHand == 25)  { SetupTotallyRandomPalette();              currentBlending = LINEARBLEND; }
        if( secondHand == 30)  { SetupBlackAndWhiteStripedPalette();       currentBlending = NOBLEND; }
        if( secondHand == 35)  { SetupBlackAndWhiteStripedPalette();       currentBlending = LINEARBLEND; }
        if( secondHand == 40)  { currentPalette = CloudColors_p;           currentBlending = LINEARBLEND; }
        if( secondHand == 45)  { currentPalette = PartyColors_p;           currentBlending = LINEARBLEND; }
        if( secondHand == 50)  { currentPalette = myRedWhiteBluePalette_p; currentBlending = NOBLEND;  }
        if( secondHand == 55)  { currentPalette = myRedWhiteBluePalette_p; currentBlending = LINEARBLEND; }
    }
}

/*
 * RANDOM PALETTE CREATION:
 * This function fills the palette with completely random colors.
 * Each time it's called, you get a completely different color scheme.
 */
void SetupTotallyRandomPalette()
{
    /*
     * PALETTE ARRAY ACCESS:
     * A CRGBPalette16 is like an array with 16 slots for colors.
     * We can set each slot individually to any color we want.
     */
    for( int i = 0; i < 16; ++i) {
        /*
         * RANDOM COLOR GENERATION:
         * CHSV(random hue, full saturation, random brightness)
         * This creates vivid colors with varying brightness levels.
         */
        currentPalette[i] = CHSV( random8(), 255, random8());
    }
}

/*
 * STRIPED PALETTE CREATION:
 * This function demonstrates programmatic palette creation using FastLED utilities.
 */
void SetupBlackAndWhiteStripedPalette()
{
    /*
     * FILL_SOLID USAGE:
     * fill_solid() sets all 16 palette entries to the same color.
     * This creates a solid foundation we can modify.
     */
    fill_solid( currentPalette, 16, CRGB::Black);
    
    /*
     * SELECTIVE COLOR PLACEMENT:
     * Set every 4th palette entry to white, creating a striped pattern.
     * When interpolated, this creates black-to-white-to-black gradients.
     */
    currentPalette[0] = CRGB::White;
    currentPalette[4] = CRGB::White;
    currentPalette[8] = CRGB::White;
    currentPalette[12] = CRGB::White;
}

/*
 * CUSTOM COLOR PALETTE:
 * This function shows how to create a palette with specific colors
 * arranged in a particular pattern.
 */
void SetupPurpleAndGreenPalette()
{
    /*
     * COLOR DEFINITION:
     * Define specific colors using HSV for consistency.
     * HUE_PURPLE and HUE_GREEN are FastLED constants.
     */
    CRGB purple = CHSV( HUE_PURPLE, 255, 255);
    CRGB green  = CHSV( HUE_GREEN, 255, 255);
    CRGB black  = CRGB::Black;
    
    /*
     * PALETTE CONSTRUCTOR:
     * CRGBPalette16() lets you specify all 16 colors explicitly.
     * This creates a repeating pattern of green-green-black-black-purple-purple-black-black.
     */
    currentPalette = CRGBPalette16(
                                   green,  green,  black,  black,
                                   purple, purple, black,  black,
                                   green,  green,  black,  black,
                                   purple, purple, black,  black );
}

/*
 * PROGMEM PALETTE EXAMPLE:
 * This demonstrates storing a palette in flash memory instead of RAM.
 * PROGMEM palettes use less RAM but are read-only.
 */
const TProgmemPalette16 myRedWhiteBluePalette_p FL_PROGMEM =
{
    CRGB::Red,
    CRGB::Gray, // 'white' is too bright compared to red and blue
    CRGB::Blue,
    CRGB::Black,
    
    CRGB::Red,
    CRGB::Gray,
    CRGB::Blue,
    CRGB::Black,
    
    CRGB::Red,
    CRGB::Red,
    CRGB::Gray,
    CRGB::Gray,
    CRGB::Blue,
    CRGB::Blue,
    CRGB::Black,
    CRGB::Black
};

// Additional notes on FastLED compact palettes:
//
// Normally, in computer graphics, the palette (or "color lookup table")
// has 256 entries, each containing a specific 24-bit RGB color.  You can then
// index into the color palette using a simple 8-bit (one byte) value.
// A 256-entry color palette takes up 768 bytes of RAM, which on Arduino
// is quite possibly "too many" bytes.
//
// FastLED does offer traditional 256-element palettes, for setups that
// can afford the 768-byte cost in RAM.
//
// However, FastLED also offers a compact alternative.  FastLED offers
// palettes that store 16 distinct entries, but can be accessed AS IF
// they actually have 256 entries; this is accomplished by interpolating
// between the 16 explicit entries to create fifteen intermediate palette
// entries between each pair.
//
// So for example, if you set the first two explicit entries of a compact 
// palette to Green (0,255,0) and Blue (0,0,255), and then retrieved 
// the first sixteen entries from the virtual palette (of 256), you'd get
// Green, followed by a smooth gradient from green-to-blue, and then Blue.

/*
 * SUMMARY AND EXPERIMENTATION IDEAS:
 * 
 * Now that you understand palettes, try these experiments:
 * 
 * 1. CREATE YOUR OWN PALETTE:
 *    - Copy SetupPurpleAndGreenPalette() and make your own colors
 *    - Try seasonal themes: autumn colors, ocean blues, sunset oranges
 * 
 * 2. MODIFY THE ANIMATION:
 *    - Change the colorIndex increment value (try 1, 5, 10, 20)
 *    - Modify startIndex increment for different speeds
 *    - Try negative increments for reverse animation
 * 
 * 3. EXPERIMENT WITH BLENDING:
 *    - Compare LINEARBLEND vs NOBLEND with the same palette
 *    - Create palettes specifically designed for each blend type
 * 
 * 4. COMBINE WITH OTHER EFFECTS:
 *    - Use palettes with sine waves for position
 *    - Apply brightness variations using beatsin8()
 *    - Combine multiple palette effects on one strip
 * 
 * 5. INTERACTIVE PALETTES:
 *    - Add buttons to manually switch palettes
 *    - Use sensors to influence palette selection
 *    - Create palettes that respond to music or sound
 */
