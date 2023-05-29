FastLED 3.6.0
=============
This release incorporates valuable improvements from FastLED contributors, tested and explored by the world-wide FastLED community of artists, creators, and developers.  Thank you for all of your time, energy, and help!  Here are some of the most significant changes in FastLED 3.6.0: 
* Greatly improved support for ESP32 and ESP8266
* Expanded and improved board support including Teensy4, Adafruit M4 CAN Express and Grand Central M4, RP2040, ATtiny48/88, Arduino MKRZero, and various other AVR and ARM boards
* Added support for DP1903 LEDs
* Added fill_rainbow_circular and fill_palette_circular functions to draw a full rainbow or other color palette on a circular ring of LEDs
* Added a non-wrapping mode for ColorFromPalette, "LINEARBLEND_NOWRAP"
* No more "register" compiler warnings
* Bug fixes and performance improvements, including in lib8tion and noise functions
* We are expanding the FastLED team to help the library grow, evolve, and flourish
* Released May 2023, with deepest thanks to all the FastLED community members around the world!


FastLED 3.5.0
=============
This release incorporates dozens of valuable improvements from FastLED contributors, tested and explored by the world-wide FastLED community of artists, creators, and developers.  Thank you for all of your time, energy, and help!  Here are some of the most significant changes in FastLED 3.5.0: 
* Greatly improved ESP32 and ESP8266 support
* Improved board support for Teensy 4, Adafruit MatrixPortal M4, Arduino Nano Every, Particle Photon, and Seeed Wio Terminal
* Improved and/or sped up: sin8, cos8, blend8, blur2d, scale8, Perlin/simplex noise
* Improved HSV colors are smoother, richer, and brighter in fill_rainbow and elsewhere
* Modernized and cleaned up the FastLED examples
* Added github CI integration to help with automated testing
* Added a Code of Conduct from https://www.contributor-covenant.org/
* Released January 2022, with many thanks to FastLED contributors and the FastLED community!  


FastLED 3.4.0
=============

* Improved reliability on ESP32 when wifi is active
* Merged in contributed support for Adafruit boards: QT Py SAMD21, Circuit Playground Express,  Circuit Playground Bluefruit, and ItsyBitsy nRF52840 Express
* Merged in contributed support for SparkFun Artemis boards
* Merged in contributed support for Arduino Nano Every / Arduino Uno Wifi Rev. 2
* Merged in contributed support for Seeedstudio Odyssey and XIAO boards
* Merged in contributed support for AVR chips ATmega1284, ATmega4809, and LGT8F328
* XYMatrix example now supports 90-degree rotated orientation
* Moved source code files into "src" subdirectory
* Many small code cleanups and bug fixes
* Released December 2020, with many thanks to everyone contributing to FastLED!

We also want to note here that in 2020, Github named FastLED one of the 'Greatest Hits' of Open Source software, and preserved an archived copy of FastLED in the Arctic Code Vault, the Bodleian Library at Oxford University, the Bibliotheca Alexandrina (the Library of Alexandria), and the Stanford University Libraries.  https://archiveprogram.github.com/greatest-hits/



FastLED 3.3.3
=============

* Improved support for ESP32, Teensy4, ATmega16, nRF52, and ARM STM32.  
* Added animation examples: "TwinkleFox" holiday lights, "Pride2015" moving rainbows, and "Pacifica" gentle ocean waves 
* Fixed a few bugs including a rare divide-by-zero crash
* Cleaned up code and examples a bit
* Said our sad farwells to FastLED founder Daniel Garcia, who we lost in a tragic accident on September 2nd, 2019.  Dan's beautiful code and warm kindness have been at the heart of the library, and our community, for ten years.  FastLED will continue with help from all across the FastLED world, and Dan's spirit will be with us whenever the lights shine and glow.  Thank you, Dan, for everything.


FastLED 3.3.2
=============

* Fix APA102 compile error #870 
* Normalize pin definition macros so that we can have an .ino file that can be used to output what pin/port mappings should be for a platform
* Add defnition for ATmega32

FastLED 3.3.1
=============

* Fix teensy build issue 
* Bring in sam's RMT timing fix

FastLED 3.3.0
==============
* Preliminary Teensy 4 support
* Fix #861 - power computation for OctoWS2811
* keywords and other minor changes for compilers (#854, #845)
* Fix some nrf52 issues (#856), #840

FastLED 3.2.10
==============
* Adafruit Metro M4 Airlift support
* Arduino Nano 33 IOT preliminary definitions
* Bug fixes

FastLED 3.2.9
=============
* Update ItsyBitsy support
* Remove conflicting types courtesy of an esp8266 framework update
* Fixes to clockless M0 code to allow for more interrupt enabled environments
* ATTiny25 compilation fix
* Some STM32 fixes (the platform still seems unhappy, though)
* NRF52 support
* Updated ESP32 support - supporting up to 24-way parallel output



FastLED 3.2.6
=============

* typo fix

FastLED 3.2.5
=============

* Fix for SAMD51 based boards (a SAMD21 optimization broke the D51 builds, now D51 is a separate platform)

FastLED 3.2.4
=============

* fix builds for WAV boards

FastLED 3.2.2
=============

* Perf tweak for SAMD21
* LPD6803 support
* Add atmega328pb support
* Variety of minor bug/correctness/typo fixes
* Added SM16703, GE8822, GS1903

FastLED 3.2.1
=============
* ATmega644P support
* Adafruit Hallowwing (Thanks to Lady Ada)
* Improved STM 32 support
* Some user contributed cleanups
* ESP32 APA102 output fix

FastLED3.2
==========
* ESP32 support with improved output and parallel output options (thanks Sam Guyer!)
* various minor contributed fixes

FastLED 3.1.8
=============
* Added support for Adafruit Circuit Playground Express (Thanks to Lady Ada)
* Improved support for Adafruit Gemma and Trinket m0 (Thanks to Lady Ada)
* Added support for PJRC's WS2812Serial (Thanks to Paul Stoffregen)
* Added support for ATmega328 non-picopower hardware pins (Thanks to John Whittington)
* Fixes for ESP32 support (Thanks to Daniel Tullemans)
* 'Makefile' compilation fix (Thanks to Nico Hood)

FastLED 3.1.7 (skipped)
=======================

FastLED 3.1.6
=============
* Preliminary support for esp32
* Variety of random bug fixes
* 6-channel parallel output for the esp8266
* Race condition fixes for teensy hardware SPI
* Preliminary teensy 3.6 support
* Various fixes falling out from "fixing" scale 8 adjustments
* Add gemma m0 support (thanks @ladyada!)

FastLED 3.1.5
=============
* Fix due parallel output build issue

FastLED 3.1.4
=============
* fix digispark avr build issue

FastLED3.1.3
===============

* Add SK6822 timings
* Add ESP8266 support - note, only tested w/the arduino esp8266 build environment
* Improvements to hsv2rgb, palette, and noise performance
* Improvements to rgb2hsv accuracy
* Fixed noise discontinuity
* Add wino board support
* Fix scale8 (so now, scale8(255,255) == 255, not 254!)
* Add ESP8266 parallel output support


FastLED3.1.1
============
* Enabled RFDuino/nrf51822 hardware SPI support
* Fix edge case bug w/HSV palette blending
* Fix power management issue w/parallel output
* Use static_asserts for some more useful compile time errors around bad pins
* Roll power management into FastLED.show/delay directly
* Support for adafruit pixies on arduino type platforms that have SoftwareSerial
  * TODO: support hardware serial on platforms that have it available
* Add UCS2903 timings
* Preliminary CPixelView/CRGBSet code - more flexible treatment of groups of arrays
  * https://github.com/FastLED/FastLED/wiki/RGBSet-Reference


FastLED3.1.0
============
* Added support for the following platforms
  * Arduino Zero
  * Teensy LC
  * RFDuino/nrf51822
  * Spark Core
* Major internal code reoganization
* Started doxygen based documentation
* Lots of bug/performance fixes
* Parallel output on various arm platforms
* lots of new stuff

FastLED3.0.2
============
* possibly fix issues #67 and #90 by fixing gcc 4.8.x support

FastLED3.0.1
============
* fix issue #89 w/power management pin always being on

FastLED3.0
==========

* Added support for the following platforms:
  * Arduino due
  * Teensy 3.1
* Added the following LED chipsets:
  * USC1903_400
  * GW6205 / GW6205_400
  * APA102
  * APA104
  * LPD1886
  * P9813
  * SmartMatrix
* Added multiple examples:
  * ColorPalette - show off the color palette code
  * ColorTemperature - show off the color correction code
  * Fire2012
  * Fire2012WithPalette
  * Multiple led controller examples
  * Noise
  * NoisePlayground
  * NoisePlusPalette
  * SmartMatrix - show off SmartMatrix support
  * XYMatrix - show how to use a mtrix layout of leds
* Added color correction
* Added dithering
* Added power management support
* Added support for color palettes
* Added easing functions
* Added fast trig functions
* Added simplex noise functions
* Added color utility functions
* Fixed DMXSERIAL/DMXSIMPLE support
* Timing adjustments for existing SPI chipsets
* Cleaned up the code layout to make platform support easier
* Many bug fixes
* A number of performance/memory improvements
* Remove Squant (takes up space!)

FastLED2
========

## Full release of the library

## Release Candidate 6
* Rename library, offically, to FastLED, move to github
* Update keywords with all the new stuffs

## Release Candidate 5
* Gemma and Trinket: supported except for global "setBrightness"

## Release Candidate 4
* Added NEOPIXEL as a synonym for WS2811
* Fix WS2811/WS2812B timings, bring it in line to exactly 1.25ns/bit.  
* Fix handling of constant color definitions (damn you, gcc!)

## Release Candidate 3
* Fixed bug when Clock and Data were on the same port
* Added ability to set pixel color directly from HSV
* Added ability to retrieve current random16 seed

## Release Candidate 2
* mostly bug fixes
* Fix SPI macro definitions for latest teensy3 software update
* Teensy 2 compilation fix
* hsv2rgb_rainbow performance fix

## Release Candidate 1
* New unified/simplified API for adding/using controllers
* fleshout clockless chip support
* add hsv (spectrum and rainbow style colors)
* high speed memory management operations
* library for interpolation/easing functions
* various api changes, addition of clear and showColor functions
* scale value applied to all show methods
* bug fixes for SM16716
* performance improvements, lpd8806 exceeds 22Mbit now
* hardware def fixes
* allow alternate rgb color orderings
* high speed math methods
* rich CRGB structure

## Preview 3
* True hardware SPI support for teensy (up to 20Mbit output!)
* Minor bug fixes/tweaks

## Preview 2
* Rename pin class to FastPin
* Replace latch with select, more accurate description of what it does
* Enforce intra-frame timing for ws2801s
* SM16716 support
* Add #define FAST_SPI_INTERRUPTS_WRITE_PINS to make sure world is ok w/interrupts and SPI
* Add #define FASTLED_FORCE_SOFTWARE_SPI for those times when you absolutely don't want to use hardware SPI, ev
en if you're using the hardware SPI pins
* Add pin definitions for the arduino megas - should fix ws2811 support
* Add pin definitions for the leonardo - should fix spi support and pin mappings
* Add warnings when pin definitions are missing
* Added google+ community for fastspi users - https://plus.google.com/communities/109127054924227823508
# Add pin definitions for Teensy++ 2.0


## Preview 1
* Initial release
