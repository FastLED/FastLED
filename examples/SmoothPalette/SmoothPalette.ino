#include <FastLED.h>

#define LED_PIN     3
#define NUM_LEDS    25
#define BRIGHTNESS  255
#define LED_TYPE    WS2811
#define COLOR_ORDER RGB
CRGB leds[NUM_LEDS];

#define UPDATES_PER_SECOND 40

// This example demonstrates use of the 8-bit interpolating color retrieval from
// compact palettes.  For basic compact palette use, consult the ColorPalette
// example.

// Smooth palette indexing uses the 12 most significant bits of a 16-bit integer.
// As such, the difference in index between the first color in the palette and
// the first interpolation step between this color and the next color is 16.
// In practice, this just means that when you would increment a 16-color palette
// index by, say, n, you would increment by n << 4 instead.

CRGBPalette16 palette;
TBlendType    currentBlending;
uint8_t motionSpeed;
uint16_t motionSpeedSmooth;
uint8_t useSmoothInterpolation;


void setup() {
  delay( 3000 ); // power-up safety delay
  FastLED.addLeds<LED_TYPE, LED_PIN, COLOR_ORDER>(leds, NUM_LEDS).setCorrection( TypicalLEDStrip );
  FastLED.setBrightness(  BRIGHTNESS );

  // Use a "stripe palette"; they effectively demonstrate the poor low-speed
  // animation quality of 4-bit palette interpolation.
  palette = RainbowStripeColors_p;

  //
  motionSpeed = 1;
  // With smooth interpolation we can achieve much slower animation motion
  // but retain high framerate.
  motionSpeedSmooth = 128;

}


void loop()
{

  // Every 20 seconds switch between 4-bit interpolation and 8-bit interpolation
  useSmoothInterpolation = (millis() / 20000) % 2;

  static uint16_t startIndex = 0;
  static uint16_t startIndexSmooth = 0;

  startIndex = startIndex + motionSpeed;
  startIndexSmooth = startIndexSmooth + motionSpeedSmooth;

  if (useSmoothInterpolation) {
    FillLEDsFromPaletteColorsSmooth( startIndexSmooth);
  }
  else {
    FillLEDsFromPaletteColors( startIndex);
  }

  FastLED.show();
  FastLED.delay(1000 / UPDATES_PER_SECOND);
}

void FillLEDsFromPaletteColors( uint8_t colorIndex)
{
  uint8_t brightness = 255;
  for( int i = 0; i < NUM_LEDS; i++) {
    leds[i] = ColorFromPalette( palette, colorIndex, brightness, BLEND);

    // this line determines how much of the palette is rendered into the LEDs
    // with 25 LEDs this implies a width of 75/256, or about 1/3 of the palette.
    colorIndex += 3;
  }
}

void FillLEDsFromPaletteColorsSmooth( uint16_t colorIndex)
{
  uint8_t brightness = 255;
  for( int i = 0; i < NUM_LEDS; i++) {
    leds[i] = ColorFromPaletteExtended( palette, colorIndex, brightness, BLEND);

    // this line determines how much of the palette is rendered into the LEDs
    // with 25 LEDs this implies a width of 19200/65536, or about 1/3 of the palette.
    colorIndex += 768;
  }
}
