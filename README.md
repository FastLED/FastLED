[![Gitter](https://badges.gitter.im/Join%20Chat.svg)](https://gitter.im/FastLED/public)
[![arduino-library-badge](https://www.ardu-badge.com/badge/FastLED.svg)](https://www.ardu-badge.com/FastLED)
![build status](https://github.com/FastLED/FastLED/workflows/build/badge.svg)

IMPORTANT NOTE: For AVR based systems, avr-gcc 4.8.x is supported and tested.  This means Arduino 1.6.5 and later.


FastLED 3.6
===========

This is a library for easily & efficiently controlling a wide variety of LED chipsets, like the ones
sold by Adafruit (NeoPixel, DotStar, LPD8806), Sparkfun (WS2801), and AliExpress.  In addition to writing to the
LEDs, this library also includes a number of functions for high-performing 8-bit math for manipulating
your RGB values, as well as low level classes for abstracting out access to pins and SPI hardware, while
still keeping things as fast as possible.

Quick note for people installing from GitHub repo zips, rename the folder "FastLED" before copying it to your Arduino/libraries folder.  GitHub likes putting `-branchname` into the name of the folder, which unfortunately makes Arduino cranky!

We have multiple goals with this library:

* Quick start for new developers - hook up your LEDs and go, no need to think about specifics of the LED chipsets being used
* Zero pain switching LED chipsets - you get some new LEDs that the library supports, just change the definition of LEDs you're using, et. voila!  Your code is running with the new LEDs.
* High performance - with features like zero cost global brightness scaling, high performance 8-bit math for RGB manipulation, and some of the fastest bit-bang'd SPI support around, FastLED wants to keep as many CPU cycles available for your LED patterns as possible

## Getting Help

If you need help with using the library, please consider going to the reddit community first, which is at http://fastled.io/r (or https://reddit.com/r/FastLED). There are thousands of great people in that group and many times you will get a quicker answer to your question there, as you will be likely to run into other people who have had the same issue.  If you run into bugs with the library (compilation failures, the library doing the wrong thing), or if you'd like to request that we support a particular platform or LED chipset, then please open an issue at http://fastled.io/issues and we will try to figure out what is going wrong.

## Simple Example

How quickly can you get up and running with the library?  Here's a simple blink program:
```cpp
#include <FastLED.h>
#define NUM_LEDS 60
CRGB leds[NUM_LEDS];
void setup() { FastLED.addLeds<NEOPIXEL, 6>(leds, NUM_LEDS); }
void loop() {
	leds[0] = CRGB::White; FastLED.show(); delay(30);
	leds[0] = CRGB::Black; FastLED.show(); delay(30);
}
```

## Supported LED Chipsets

Here's a list of all the LED chipsets are supported.  More details on the LED chipsets are included [on our wiki page](https://github.com/FastLED/FastLED/wiki/Chipset-reference)

* Adafruit's DotStars - aka APA102
* Adafruit's Neopixel - aka WS2812B (also WS2811/WS2812/WS2813, also supported in lo-speed mode) - a 3 wire addressable LED chipset
* TM1809/4 - 3 wire chipset, cheaply available on aliexpress.com
* TM1803 - 3 wire chipset, sold by RadioShack
* UCS1903 - another 3 wire LED chipset, cheap
* GW6205 - another 3 wire LED chipset
* LPD8806 - SPI based chipset, very high speed
* WS2801 - SPI based chipset, cheap and widely available
* SM16716 - SPI based chipset
* APA102 - SPI based chipset
* P9813 - aka Cool Neon's Total Control Lighting
* DMX - send rgb data out over DMX using Arduino DMX libraries
* SmartMatrix panels - needs the SmartMatrix library (https://github.com/pixelmatix/SmartMatrix)
* LPD6803 - SPI based chpiset, chip CMODE pin must be set to 1 (inside oscillator mode)

HL1606, and "595"-style shift registers are no longer supported by the library.  The older Version 1 of the library ("FastSPI_LED") has support for these, but is missing many of the advanced features of current versions and is no longer being maintained.

## Supported Platforms

Right now the library is supported on a variety of Arduino compatible platforms.  If it's ARM or AVR and uses the Arduino software (or a modified version of it to build) then it is likely supported.  Note that we have a long list of upcoming platforms to support, so if you don't see what you're looking for here, ask, it may be on the roadmap (or may already be supported).  N.B. at the moment we are only supporting the stock compilers that ship with the Arduino software.  Support for upgraded compilers, as well as using AVR Studio and skipping the Arduino entirely, should be coming in a near future release.

* Arduino & compatibles - straight up Arduino devices, Uno, Duo, Leonardo, Mega, Nano, etc...
* Arduino Yún
* Adafruit Trinket & Gemma - Trinket Pro may be supported, but haven't tested to confirm yet
* Teensy 2, Teensy++ 2, Teensy 3.0, Teensy 3.1/3.2, Teensy LC, Teensy 3.5, Teensy 3.6, and Teensy 4.0 - Arduino compatible from pjrc.com with some extra goodies (note the Teensy LC, 3.2, 3.5, 3.6, 4.0 are ARM, not AVR!)
* Arduino Due and the digistump DigiX
* RFDuino
* SparkCore
* Arduino Zero
* ESP8266 using the Arduino board definitions from http://arduino.esp8266.com/stable/package_esp8266com_index.json - please be sure to also read https://github.com/FastLED/FastLED/wiki/ESP8266-notes for information specific to the 8266.
* The wino board - http://wino-board.com
* ESP32 based boards

What types of platforms are we thinking about supporting in the future?  Here's a short list:  ChipKit32, Maple, Beagleboard

## What about that name?

Wait, what happened to FastSPI_LED and FastSPI_LED2?  The library was initially named FastSPI_LED because it was focused on very fast and efficient SPI access.  However, since then, the library has expanded to support a number of LED chipsets that don't use SPI, as well as a number of math and utility functions for LED processing across the board.  We decided that the name FastLED more accurately represents the totality of what the library provides, everything fast, for LEDs.

## For more information

Check out the official site http://fastled.io for links to documentation, issues, and news


*TODO* - get candy
