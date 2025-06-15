/// @file    NoisePlusPalette.ino
/// @brief   Demonstrates how to mix noise generation with color palettes on a 2D LED matrix
/// @example NoisePlusPalette.ino
///
/// OVERVIEW:
/// This sketch demonstrates combining Perlin noise with color palettes to create
/// dynamic, flowing color patterns on an LED matrix. The noise function creates
/// natural-looking patterns that change over time, while the color palettes
/// determine which colors are used to visualize the noise values.

#include <FastLED.h>  // Main FastLED library for controlling LEDs

#if !SKETCH_HAS_LOTS_OF_MEMORY
// Don't compile this for AVR microcontrollers (like Arduino Uno) because they typically 
// don't have enough memory to handle this complex animation.
// Instead, we provide empty setup/loop functions so the sketch will compile but do nothing.
void setup() {}
void loop() {}
#else  // For all other platforms with more memory (ESP32, Teensy, etc.)

// LED hardware configuration
#define LED_PIN     3        // Data pin connected to the LED strip
#define BRIGHTNESS  96       // Default brightness level (0-255)
#define LED_TYPE    WS2811   // Type of LED strip being used
#define COLOR_ORDER GRB      // Color order of the LEDs (varies by strip type)

// Matrix dimensions - defines the size of our virtual LED grid
const uint8_t kMatrixWidth  = 16;  // Number of columns in the matrix
const uint8_t kMatrixHeight = 16;  // Number of rows in the matrix

// LED strip layout configuration
const bool kMatrixSerpentineLayout = true;  // If true, every other row runs backwards
                                           // This is common in matrix setups to allow
                                           // for easier wiring


// HOW THIS EXAMPLE WORKS:
//
// This example combines two features of FastLED to produce a remarkable range of
// effects from a relatively small amount of code.  This example combines FastLED's 
// color palette lookup functions with FastLED's Perlin noise generator, and
// the combination is extremely powerful.
//
// You might want to look at the "ColorPalette" and "Noise" examples separately
// if this example code seems daunting.
//
// 
// The basic setup here is that for each frame, we generate a new array of 
// 'noise' data, and then map it onto the LED matrix through a color palette.
//
// Periodically, the color palette is changed, and new noise-generation parameters
// are chosen at the same time.  In this example, specific noise-generation
// values have been selected to match the given color palettes; some are faster, 
// or slower, or larger, or smaller than others, but there's no reason these 
// parameters can't be freely mixed-and-matched.
//
// In addition, this example includes some fast automatic 'data smoothing' at 
// lower noise speeds to help produce smoother animations in those cases.
//
// The FastLED built-in color palettes (Forest, Clouds, Lava, Ocean, Party) are
// used, as well as some 'hand-defined' ones, and some procedurally generated
// palettes.


// Calculate the total number of LEDs in our matrix
#define NUM_LEDS (kMatrixWidth * kMatrixHeight)

// Find the larger dimension (width or height) for our noise array size
#define MAX_DIMENSION ((kMatrixWidth>kMatrixHeight) ? kMatrixWidth : kMatrixHeight)

// Array to hold all LED color values - one CRGB struct per LED
CRGB leds[kMatrixWidth * kMatrixHeight];

// The 16-bit version of our coordinates for the noise function
// Using 16 bits gives us more resolution and smoother animations
static uint16_t x;  // x-coordinate in the noise space
static uint16_t y;  // y-coordinate in the noise space
static uint16_t z;  // z-coordinate (time dimension) in the noise space

// ANIMATION PARAMETERS:

// We're using the x/y dimensions to map to the x/y pixels on the matrix.  We'll
// use the z-axis for "time".  speed determines how fast time moves forward.  Try
// 1 for a very slow moving effect, or 60 for something that ends up looking like
// water.
uint16_t speed = 20; // Speed is set dynamically once we've started up
                     // Higher values = faster animation

// Scale determines how far apart the pixels in our noise matrix are.  Try
// changing these values around to see how it affects the motion of the display.  The
// higher the value of scale, the more "zoomed out" the noise will be.  A value
// of 1 will be so zoomed in, you'll mostly see solid colors.
uint16_t scale = 30; // Scale is set dynamically once we've started up
                     // Higher values = more "zoomed out" pattern

// This is the array that we keep our computed noise values in
// Each position stores an 8-bit (0-255) noise value
uint8_t noise[MAX_DIMENSION][MAX_DIMENSION];

// The current color palette we're using to map noise values to colors
CRGBPalette16 currentPalette( PartyColors_p );  // Start with party colors

// If colorLoop is set to 1, we'll cycle through the colors in the palette
// This creates an additional animation effect on top of the noise movement
uint8_t colorLoop = 1;  // 0 = no color cycling, 1 = cycle colors


// Forward declare our functions so that we have maximum compatibility
// with other build tools outside of ArduinoIDE. The *.ino files are
// special in that Arduino will generate function prototypes for you.
// For non-Arduino environments, we need these declarations.
void SetupRandomPalette();                    // Creates a random color palette
void SetupPurpleAndGreenPalette();            // Creates a purple and green striped palette
void SetupBlackAndWhiteStripedPalette();      // Creates a black and white striped palette
void ChangePaletteAndSettingsPeriodically();  // Changes palettes and settings over time
void mapNoiseToLEDsUsingPalette();            // Maps noise data to LED colors using the palette
uint16_t XY( uint8_t x, uint8_t y);           // Converts x,y coordinates to LED array index

void setup() {
  delay(3000);  // 3 second delay for recovery and to give time for the serial monitor to open
  
  // Initialize the LED strip:
  // - LED_TYPE specifies the chipset (WS2811, WS2812B, etc.)
  // - LED_PIN is the data pin number
  // - COLOR_ORDER specifies the RGB color ordering for your strip
  FastLED.addLeds<LED_TYPE,LED_PIN,COLOR_ORDER>(leds,NUM_LEDS);
  // NOTE - This does NOT have a ScreenMap (because it's a legacy sketch)
  // so it won't look that good on the web-compiler. But adding it is ONE LINE!
  
  // Set the overall brightness level (0-255)
  FastLED.setBrightness(BRIGHTNESS);

  // Initialize our noise coordinates to random values
  // This ensures the pattern starts from a different position each time
  x = random16();  // Random x starting position
  y = random16();  // Random y starting position
  z = random16();  // Random time starting position
}



// Fill the x/y array of 8-bit noise values using the inoise8 function.
void fillnoise8() {
  // If we're running at a low "speed", some 8-bit artifacts become visible
  // from frame-to-frame.  In order to reduce this, we can do some fast data-smoothing.
  // The amount of data smoothing we're doing depends on "speed".
  uint8_t dataSmoothing = 0;
  if( speed < 50) {
    // At lower speeds, apply more smoothing
    // This formula creates more smoothing at lower speeds:
    // speed=10 → smoothing=160, speed=30 → smoothing=80
    dataSmoothing = 200 - (speed * 4);
  }
  
  // Loop through each pixel in our noise array
  for(int i = 0; i < MAX_DIMENSION; i++) {
    // Calculate the offset for this pixel in the x dimension
    int ioffset = scale * i;
    
    for(int j = 0; j < MAX_DIMENSION; j++) {
      // Calculate the offset for this pixel in the y dimension
      int joffset = scale * j;
      
      // Generate the noise value for this pixel using 3D Perlin noise
      // The noise function takes x, y, and z (time) coordinates
      uint8_t data = inoise8(x + ioffset, y + joffset, z);

      // The range of the inoise8 function is roughly 16-238.
      // These two operations expand those values out to roughly 0..255
      // You can comment them out if you want the raw noise data.
      data = qsub8(data, 16);                // Subtract 16 (with underflow protection)
      data = qadd8(data, scale8(data, 39));  // Add a scaled version of the data to itself

      // Apply data smoothing if enabled
      if( dataSmoothing ) {
        uint8_t olddata = noise[i][j];       // Get the previous frame's value
        
        // Blend between old and new data based on smoothing amount
        // Higher dataSmoothing = more of the old value is kept
        uint8_t newdata = scale8(olddata, dataSmoothing) + 
                          scale8(data, 256 - dataSmoothing);
        data = newdata;
      }
      
      // Store the final noise value in our array
      noise[i][j] = data;
    }
  }
  
  // Increment z to move through the noise space over time
  z += speed;
  
  // Apply slow drift to X and Y, just for visual variation
  // This creates a gentle shifting of the entire pattern
  x += speed / 8;   // X drifts at 1/8 the speed of z
  y -= speed / 16;  // Y drifts at 1/16 the speed of z in the opposite direction
}



// Map the noise data to LED colors using the current color palette
void mapNoiseToLEDsUsingPalette()
{
  // Static variable that slowly increases to cycle through colors when colorLoop is enabled
  static uint8_t ihue=0;
  
  // Loop through each pixel in our LED matrix
  for(int i = 0; i < kMatrixWidth; i++) {
    for(int j = 0; j < kMatrixHeight; j++) {
      // We use the value at the (i,j) coordinate in the noise
      // array for our brightness, and the flipped value from (j,i)
      // for our pixel's index into the color palette.
      // This creates interesting patterns with two different noise mappings.

      uint8_t index = noise[j][i];  // Color index from the flipped coordinate
      uint8_t bri =   noise[i][j];  // Brightness from the normal coordinate

      // If color cycling is enabled, add a slowly-changing base value to the index
      // This makes the colors shift/rotate through the palette over time
      if( colorLoop) { 
        index += ihue;  // Add the slowly increasing hue offset
      }

      // Brighten up the colors, as the color palette itself often contains the 
      // light/dark dynamic range desired
      if( bri > 127 ) {
        // If brightness is in the upper half, make it full brightness
        bri = 255;
      } else {
        // Otherwise, scale it to the full range (0-127 becomes 0-254)
        bri = dim8_raw( bri * 2);
      }

      // Get the final color by looking up the palette color at our index
      // and applying the brightness value
      CRGB color = ColorFromPalette( currentPalette, index, bri);
      
      // Set the LED color in our array, using the XY mapping function
      // to convert from x,y coordinates to the 1D array index
      leds[XY(i,j)] = color;
    }
  }
  
  // Increment the hue value for the next frame (for color cycling)
  ihue+=1;
}

void loop() {
  // The main program loop that runs continuously
  
  // Periodically choose a new palette, speed, and scale
  // This creates variety in the animation over time
  ChangePaletteAndSettingsPeriodically();

  // Generate new noise data for this frame
  fillnoise8();
  
  // Convert the noise data to colors in the LED array
  // using the current palette
  mapNoiseToLEDsUsingPalette();

  // Send the color data to the actual LEDs
  FastLED.show();
  
  // No delay is needed here as the calculations already take some time
  // Adding a delay would slow down the animation
  // delay(10);
}



// PALETTE MANAGEMENT:
//
// There are several different palettes of colors demonstrated here.
//
// FastLED provides several 'preset' palettes: RainbowColors_p, RainbowStripeColors_p,
// OceanColors_p, CloudColors_p, LavaColors_p, ForestColors_p, and PartyColors_p.
//
// Additionally, you can manually define your own color palettes, or you can write
// code that creates color palettes on the fly.

// This controls how long each palette is displayed before changing
// 1 = 5 sec per palette
// 2 = 10 sec per palette
// etc.
#define HOLD_PALETTES_X_TIMES_AS_LONG 1  // Multiplier for palette duration

// Periodically change the palette, speed, and scale settings
void ChangePaletteAndSettingsPeriodically()
{
  // Calculate which "second hand" we're on (0-59) based on elapsed time
  // We divide by HOLD_PALETTES_X_TIMES_AS_LONG to slow down the changes
  uint8_t secondHand = ((millis() / 1000) / HOLD_PALETTES_X_TIMES_AS_LONG) % 60;
  static uint8_t lastSecond = 99;  // Track the last second to detect changes
  
  // Only update when the second hand changes
  if( lastSecond != secondHand) {
    lastSecond = secondHand;
    
    // Every 5 seconds, change to a different palette and settings
    // Each palette has specific speed and scale settings that work well with it
    
    if( secondHand ==  0)  { currentPalette = RainbowColors_p;         speed = 20; scale = 30; colorLoop = 1; }
    if( secondHand ==  5)  { SetupPurpleAndGreenPalette();             speed = 10; scale = 50; colorLoop = 1; }
    if( secondHand == 10)  { SetupBlackAndWhiteStripedPalette();       speed = 20; scale = 30; colorLoop = 1; }
    if( secondHand == 15)  { currentPalette = ForestColors_p;          speed =  8; scale =120; colorLoop = 0; }
    if( secondHand == 20)  { currentPalette = CloudColors_p;           speed =  4; scale = 30; colorLoop = 0; }
    if( secondHand == 25)  { currentPalette = LavaColors_p;            speed =  8; scale = 50; colorLoop = 0; }
    if( secondHand == 30)  { currentPalette = OceanColors_p;           speed = 20; scale = 90; colorLoop = 0; }
    if( secondHand == 35)  { currentPalette = PartyColors_p;           speed = 20; scale = 30; colorLoop = 1; }
    if( secondHand == 40)  { SetupRandomPalette();                     speed = 20; scale = 20; colorLoop = 1; }
    if( secondHand == 45)  { SetupRandomPalette();                     speed = 50; scale = 50; colorLoop = 1; }
    if( secondHand == 50)  { SetupRandomPalette();                     speed = 90; scale = 90; colorLoop = 1; }
    if( secondHand == 55)  { currentPalette = RainbowStripeColors_p;   speed = 30; scale = 20; colorLoop = 1; }
  }
}



// This function generates a random palette that's a gradient
// between four different colors.  The first is a dim hue, the second is 
// a bright hue, the third is a bright pastel, and the last is 
// another bright hue.  This gives some visual bright/dark variation
// which is more interesting than just a gradient of different hues.
void SetupRandomPalette()
{
  // Create a new palette with 4 random colors that blend together
  currentPalette = CRGBPalette16( 
                      CHSV( random8(), 255, 32),    // Random dim hue (low value)
                      CHSV( random8(), 255, 255),   // Random bright hue (full saturation & value)
                      CHSV( random8(), 128, 255),   // Random pastel (medium saturation, full value)
                      CHSV( random8(), 255, 255));  // Another random bright hue
  
  // The CRGBPalette16 constructor automatically creates a 16-color gradient
  // between these four colors, evenly distributed
}


// This function sets up a palette of black and white stripes,
// using code.  Since the palette is effectively an array of
// sixteen CRGB colors, the various fill_* functions can be used
// to set them up.
void SetupBlackAndWhiteStripedPalette()
{
  // 'black out' all 16 palette entries...
  fill_solid( currentPalette, 16, CRGB::Black);
  
  // and set every fourth one to white to create stripes
  // Positions 0, 4, 8, and 12 in the 16-color palette
  currentPalette[0] = CRGB::White;
  currentPalette[4] = CRGB::White;
  currentPalette[8] = CRGB::White;
  currentPalette[12] = CRGB::White;
  
  // The palette interpolation will create smooth transitions between these colors
}

// This function sets up a palette of purple and green stripes.
void SetupPurpleAndGreenPalette()
{
  // Define our colors using HSV color space for consistency
  CRGB purple = CHSV( HUE_PURPLE, 255, 255);  // Bright purple
  CRGB green  = CHSV( HUE_GREEN, 255, 255);   // Bright green
  CRGB black  = CRGB::Black;                  // Black
  
  // Create a 16-color palette with a specific pattern:
  // green-green-black-black-purple-purple-black-black, repeated twice
  // This creates alternating green and purple stripes with black in between
  currentPalette = CRGBPalette16( 
    green,  green,  black,  black,   // First 4 colors
    purple, purple, black,  black,   // Next 4 colors
    green,  green,  black,  black,   // Repeat the pattern
    purple, purple, black,  black ); // Last 4 colors
}


//
// Mark's xy coordinate mapping code.  See the XYMatrix for more information on it.
//
// This function converts x,y coordinates to a single array index
// It handles both regular and serpentine matrix layouts
uint16_t XY( uint8_t x, uint8_t y)
{
  uint16_t i;
  
  // For a regular/sequential layout, it's just y * width + x
  if( kMatrixSerpentineLayout == false) {
    i = (y * kMatrixWidth) + x;
  }
  
  // For a serpentine layout (zigzag), odd rows run backwards
  if( kMatrixSerpentineLayout == true) {
    if( y & 0x01) {  // Check if y is odd (bitwise AND with 1)
      // Odd rows run backwards
      uint8_t reverseX = (kMatrixWidth - 1) - x;
      i = (y * kMatrixWidth) + reverseX;
    } else {
      // Even rows run forwards
      i = (y * kMatrixWidth) + x;
    }
  }
  
  return i;
}


#endif  // End of the non-AVR code section
