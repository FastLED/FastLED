// @filter: (memory is high)

/// @file    MultiArrays.ino
/// @brief   Demonstrates how to use multiple LED strips, each with their own data
/// @example MultiArrays.ino

// MultiArrays - see https://github.com/FastLED/FastLED/wiki/Multiple-Controller-Examples for more info on
// using multiple controllers.  In this example, we're going to set up three NEOPIXEL strips on three
// different pins, each strip getting its own CRGB array to be played with

/*
 * SAFE PIN REFERENCE FOR MULTI-ARRAY PROJECTS
 *
 * When using multiple LED strips, choose pins carefully to avoid conflicts.
 * Below are tested safe pins for common boards with sufficient memory:
 *
 * ESP32-DevKitC (esp32dev) - Original ESP32 WROOM-32:
 *   Safe pins: 2, 4, 5, 12-19, 21-23, 25-27, 32, 33
 *   Best for LED output: 12, 13, 14, 15, 16, 17, 18, 19, 21, 22, 23, 25, 26, 27
 *   Avoid: 0 (boot button), 1 (TX), 3 (RX), 6-11 (flash), 34-39 (input only)
 *   Notes: GPIO 2 can cause boot issues if pulled high. Avoid strapping pins (0, 2, 5, 12, 15) if possible.
 *
 * ESP32-C3 (esp32-c3-devkitm-1):
 *   Safe pins: 0-10 (most are safe after boot)
 *   Best for LED output: 0, 1, 2, 3, 4, 5, 6, 7, 8, 10
 *   Avoid: 9 (boot strapping), 18, 19 (USB if using native USB)
 *   Notes: Fewer pins than ESP32, but most are GPIO capable. Pin 9 is strapping pin (keep low during boot).
 *
 * ESP32-S3 (esp32-s3-devkitc-1):
 *   Safe pins: 1-18, 21, 38-48 (many options available)
 *   Best for LED output: 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 21, 38, 39, 40, 41, 42
 *   Avoid: 0 (boot button), 19, 20 (USB JTAG by default), 26-32 (flash/PSRAM on some modules), 43, 44 (TX/RX)
 *   Notes: Very flexible GPIO. Avoid 19/20 for USB-JTAG. Pin 1 is safe (avoids GPIO19 USB conflict).
 *
 * ESP32-C6:
 *   Safe pins: 0-7, 15-23 (after boot)
 *   Best for LED output: 0, 1, 2, 3, 4, 5, 6, 7, 15, 16, 17, 18, 19, 20, 21, 22, 23
 *   Avoid: 8, 9 (strapping pins), 12, 13 (USB if using native USB)
 *   Notes: Similar to C3 but with more pins. Most GPIO are multi-function capable.
 *
 * ESP32-P4 (if using esp32p4-devkit):
 *   Safe pins: Varies by specific module - refer to your dev board documentation
 *   Notes: Industrial ESP32 variant. Check your specific module's datasheet for GPIO capabilities.
 *
 * RP2040 (Raspberry Pi Pico):
 *   Safe pins: GP0-GP22, GP26-GP28 (most GPIO pins)
 *   Best for LED output: GP2-GP22 (any of these work well)
 *   Avoid: GP23-GP25 (used for internal functions on Pico)
 *   Notes: Very flexible - nearly all GPIO can be used. GP25 is the onboard LED on Pico.
 *   No special strapping requirements. Consider leaving GP0/GP1 free for UART debugging.
 *
 * Teensy 4.0:
 *   Safe pins: 0-23, 24-27 (some constraints), 28-33
 *   Best for LED output: 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23
 *   Avoid: Pin 13 (onboard LED - can use but may blink), 0 (RX), consider avoiding for debugging
 *   Notes: All digital pins are 3.3V. Very fast GPIO. Most pins work great for LEDs.
 *
 * Teensy 4.1:
 *   Safe pins: 0-41, 54-57 (extensive GPIO)
 *   Best for LED output: 1-12, 14-27, 28-39 (many options)
 *   Avoid: Pin 13 (onboard LED), 0 (RX) if using Serial, 40-41 (I2C if using Wire)
 *   Notes: Similar to 4.0 but with many more pins. Excellent for large multi-strip projects.
 *
 * Teensy 3.6:
 *   Safe pins: 0-39, 54-57 (many GPIO available)
 *   Best for LED output: 2-12, 14-23, 24-33
 *   Avoid: Pin 13 (onboard LED), 0 (RX), 1 (TX) if using Serial
 *   Notes: 3.3V GPIO. Good memory for multi-strip projects. Predecessor to Teensy 4.x.
 *
 * General Recommendations:
 *   - Always test your specific board configuration before deploying
 *   - Start with 2-3 strips to verify pin assignments work correctly
 *   - Check for pin conflicts with other peripherals (SPI, I2C, UART)
 *   - Consider power requirements separately from data pin selection
 *   - For production, avoid pins needed for programming/debugging (TX, RX, USB pins)
 *   - When in doubt, refer to your board's pinout diagram
 *   - Use level shifters if mixing 3.3V boards with 5V LED strips (recommended for reliability)
 *
 * NOTE: This example uses platform-specific pin definitions to automatically
 * select safe pins for your board. Finding truly "universal" safe pins across
 * all platforms is challenging due to different architectures, boot requirements,
 * and hardware constraints (strapping pins, flash interfaces, USB, etc.).
 * The platform detection below handles these differences automatically.
 * If you need different pins, simply define PIN_RED, PIN_GREEN, and PIN_BLUE
 * before the platform detection code.
 */

#include <FastLED.h>

#define NUM_LEDS_PER_STRIP 60
CRGB redLeds[NUM_LEDS_PER_STRIP];
CRGB greenLeds[NUM_LEDS_PER_STRIP];
CRGB blueLeds[NUM_LEDS_PER_STRIP];

// Platform-specific safe pin definitions
// These pins are chosen to avoid strapping pins, flash pins, USB pins, etc.
#if defined(ESP32)
  // ESP32 variants - use universally safe pins that work across most ESP32 modules
  #if defined(CONFIG_IDF_TARGET_ESP32C3)
    // ESP32-C3: Avoid pin 9 (strapping), use safe GPIO
    #define PIN_RED   2
    #define PIN_GREEN 3
    #define PIN_BLUE  4
  #elif defined(CONFIG_IDF_TARGET_ESP32S3)
    // ESP32-S3: Use safe pins that avoid USB-JTAG (19, 20) and flash/PSRAM (26-32)
    #define PIN_RED   1
    #define PIN_GREEN 2
    #define PIN_BLUE  3
  #elif defined(CONFIG_IDF_TARGET_ESP32C6)
    // ESP32-C6: Avoid strapping pins 8, 9 and USB pins 12, 13
    #define PIN_RED   2
    #define PIN_GREEN 3
    #define PIN_BLUE  4
  #elif defined(CONFIG_IDF_TARGET_ESP32C5)
    // ESP32-C5: Avoid strapping pins 2, 7, 25, 27, 28, USB pins 13, 14, and flash pins 16-22
    #define PIN_RED   3
    #define PIN_GREEN 4
    #define PIN_BLUE  5
  #elif defined(CONFIG_IDF_TARGET_ESP32H2)
    // ESP32-H2: Avoid flash pins 15-21, strapping pins 2, 3, 8, 9, 25, USB-Serial-JTAG 26, 27
    #define PIN_RED   0
    #define PIN_GREEN 1
    #define PIN_BLUE  4
  #elif defined(CONFIG_IDF_TARGET_ESP32C2)
    // ESP32-C2: Only GPIO 0-20 available. Avoid flash pins 11-17, strapping pins 8, 9
    #define PIN_RED   0
    #define PIN_GREEN 1
    #define PIN_BLUE  2
  #else
    // ESP32 classic (WROOM-32, WROVER, etc): Avoid strapping and flash pins
    #define PIN_RED   13
    #define PIN_GREEN 14
    #define PIN_BLUE  15
  #endif
#elif defined(ARDUINO_ARCH_RP2040)
  // RP2040 (Raspberry Pi Pico): Most GPIO are safe, using mid-range pins
  #define PIN_RED   10
  #define PIN_GREEN 11
  #define PIN_BLUE  12
#elif defined(__IMXRT1062__)
  // Teensy 4.0/4.1: Most pins are safe, using common GPIO
  #define PIN_RED   2
  #define PIN_GREEN 3
  #define PIN_BLUE  4
#elif defined(__MK66FX1M0__) || defined(__MK64FX512__)
  // Teensy 3.6: Safe pins away from Serial
  #define PIN_RED   2
  #define PIN_GREEN 3
  #define PIN_BLUE  4
#else
  // Universal fallback for other platforms (AVR, etc.)
  // These are common digital pins on most Arduino boards
  #define PIN_RED   6
  #define PIN_GREEN 7
  #define PIN_BLUE  8
#endif

// For mirroring strips, all the "special" stuff happens just in setup.  We
// just addLeds multiple times, once for each strip
void setup() {
  // tell FastLED there's 60 NEOPIXEL leds on pin PIN_RED
  FastLED.addLeds<NEOPIXEL, PIN_RED>(redLeds, NUM_LEDS_PER_STRIP);

  // tell FastLED there's 60 NEOPIXEL leds on pin PIN_GREEN
  FastLED.addLeds<NEOPIXEL, PIN_GREEN>(greenLeds, NUM_LEDS_PER_STRIP);

  // tell FastLED there's 60 NEOPIXEL leds on pin PIN_BLUE
  FastLED.addLeds<NEOPIXEL, PIN_BLUE>(blueLeds, NUM_LEDS_PER_STRIP);

}

void loop() {
  for(int i = 0; i < NUM_LEDS_PER_STRIP; i++) {
    // set our current dot to red, green, and blue
    redLeds[i] = CRGB::Red;
    greenLeds[i] = CRGB::Green;
    blueLeds[i] = CRGB::Blue;
    FastLED.show();
    // clear our current dot before we move on
    redLeds[i] = CRGB::Black;
    greenLeds[i] = CRGB::Black;
    blueLeds[i] = CRGB::Black;
    delay(100);
  }

  for(int i = NUM_LEDS_PER_STRIP-1; i >= 0; i--) {
    // set our current dot to red, green, and blue
    redLeds[i] = CRGB::Red;
    greenLeds[i] = CRGB::Green;
    blueLeds[i] = CRGB::Blue;
    FastLED.show();
    // clear our current dot before we move on
    redLeds[i] = CRGB::Black;
    greenLeds[i] = CRGB::Black;
    blueLeds[i] = CRGB::Black;
    delay(100);
  }
}
