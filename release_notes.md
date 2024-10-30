
FastLED 3.9.2
=============
* WS28XX family of led chipsets can now be overclocked
  * See also define `FASTLED_LED_OVERCLOCK`
    * Example: `#define FASTLED_OVERCLOCK 1.2` (gives 20% overclock).
    * You can set this define before you include `"FastLED.h"`
    * Slower chips like AVR which do software bitbanging will ignore this.
    * This discovery came from this reddit thread:
      * https://www.reddit.com/r/FastLED/comments/1gdqtw5/comment/luegowu
      * A special thanks to https://www.reddit.com/user/Tiny_Structure_7/ for discovering this!
    * See examples/Overclock.ino for a working demo.
  * You can either overclock globally or per led chipset on supported chipsets.
  * Real world tests
    * I (Zach Vorhies) have seen 25% overclock on my own test setup using cheap amazon WS2812.
    * u/Tiny_Structure_7 was able to overclock quality WS2812 LEDs 800khz -> 1.2mhz!!
    * Assuming 550 WS2812's can be driven at 60fps at normal clock.
      * 25% overclock: 687 @ 60fps
      * 50% overclock: 825 @ 60fps
      * Animartrix benchmark (ESP32S3)
        * 3.7.X: 34fps
        * 3.9.0: 59fps
        * 3.9.2: 70fps @ 20% overclock (after this the CPU becomes the bottleneck).
      * FastLED is now likely at the theoretical maximum speed and efficiency for frame draw (async) & dispatch (overclock).
  * Fixes `ESPAsyncWebServer.h` namespace collision with `fs.h` in FastLED, which has been renamed to `file_system.h`

FastLED 3.9.1
=============
* Bug fix for namespace conflicts
* One of our third_party libraries was causing a namespace conflict with ArduinoJson included by the user.
  * If you are affected then please upgrade.
* FastLED now supports it's own namespace, default is `fl`
  * Off by default, as old code wants FastLED stuff to be global.
  * Enable it by defining: `FASTLED_FORCE_NAMESPACE`


FastLED 3.9.0
=============
* Beta 4.0.0 release
* ESP32 RMT5 Driver Implemented.
  * Driver crashes on boot should now be solved.
  * Parallel AND async.
    * Drive up to 8 channels in parallel (more, for future boards) with graceful fallback
      if your sketch allocates some of them.
        * In the 3.7.X series the total number of RMT channels was limited to 4.
    * async mode means FastLED.show() returns immediately if RMT channels are ready for new
      data. This means you can compute the next frame while the current frame is being drawn.
  * Flicker with WIFI *should* be solved. The new RMT 5.1 driver features
    large DMA buffers and deep transaction queues to prevent underflow conditions.
  * Memory efficient streaming encoding. As a result the "one shot" encoder no longer
    exists for the RMT5 driver, but may be added back at a future date if people want it.
  * If for some reason the RMT5 driver doesn't work for you then use the following define `FASTLED_RMT5=0` to get back the old behavior.
* Improved color mixing algorithm, global brightness, and color scaling are now separate for non-AVR platforms. This only affects chipsets that have higher than RGB8 output, aka APA102, and clones
  right now.
  * APA102 and APA102HD now perform their own color mixing in psuedo 13 bit space.
    * If you don't like this behavior you can always go back by using setting `FASTLED_HD_COLOR_MIXING=0`.
* Binary size
  * Avr platforms now use less memory
  * 200 bytes in comparison to 3.7.8:
    * 3.7.8: attiny85 size was 9447 (limit is 9500 before the builder triggers a failure)
    * 3.8.0: attiny85 size is now 9296
    * This is only true for the WS2812 chipset. The APA102 chipset consumes significantly more memory.
* Compile support for ATtiny1604 and other Attiny boards
  * Many of these boards were failing a linking step due to a missing timer_millis value. This is now injected in via weak symbol for these boards, meaning that you won't get a linker error if you include code (like wiring.cpp) that defines this.
  * If you need a working timer value on AVR that increases via an ISR you can do so by defining `FASTLED_DEFINE_AVR_MILLIS_TIMER0_IMPL=1`
* Board support
  * nordicnrf52_dk now supported and tested (thanks https://github.com/paulhayes!)
* Some unannounced features.
* Happy coding!


For sketches that do a lot of heavy processing for each frame, FastLED is going to be **significantly** faster with this new release.

How much faster?

I benchmarked the animartrix sketch, which has heavy floating point requirements (you'll need a Teensy41 or an ESP32S3 to handle the processing requirements).

FastLED 3.7.X - 34fps
FastLED 3.9.0 - 59fps (+70% speedup!)

Why?

In FastLED 3.7.X, FastLED.show() was always a blocking operation. Now it's only blocking when the previous frame is waiting to complete it's render.

In the benchmark I measured:
12 ms - preparing the frame for draw.
17 ms - actually drawing the frame.

@ 22x22 WS2812 grid.

So for FastLED 3.7.X this meant that these two values would sum together. So 12ms + 17ms = 29ms = 34fps.
But in FastLED 3.9.0 the calculation works like this MAX(12, 17) = 17ms = 59fps. If you fall into this category, FastLED will now free up 17ms to do available work @ 60fps, which is a game changer.

As of today's release, nobody else is doing async drawing. FastLED is the only one to offer this feature.

FastLED 3.8.0
=============
* Attiny0/1 (commonly Attiny85) support added.
  * https://github.com/FastLED/FastLED/pull/1292 , https://github.com/FastLED/FastLED/pull/1183 , https://github.com/FastLED/FastLED/pull/1061
  * Special thanks to [@freemovers](https://github.com/freemovers), [@jasoncoon](https://github.com/jasoncoon), [@ngyl88](https://github.com/ngyl88) for the contribution.
  * Many common boards are now compiled in the Attiny family. See our repo for which ones are supported.
* Arduino nano compiling with new pin definitions.
  *  https://github.com/FastLED/FastLED/pull/1719
  *  Thanks to https://github.com/ngyl88 for the contribution!
* New STM32F1 boards compiling
  * bluepill
  * maple mini
* CPPCheck now passing for HIGH and MEDIUM severity on all platforms.


FastLED 3.7.7
=============
* WS2812 RGBW mode is now part of the API.
  * Api: `FastLED.addLeds<WS2812, DATA_PIN, GRB>(leds, NUM_LEDS).setRgbw(RgbwDefault());`
  * Only enabled on ESP32 boards, no-op on other platforms.
  * See [examples/RGBW/RGBW.ino](https://github.com/FastLED/FastLED/blob/master/examples/RGBW/RGBW.ino)
* WS2812 Emulated RGBW Controller
  * Works on all platforms (theoretically)
  * Has an extra side buffer to convert RGB -> RGBW data.
    * This data is sent to the real driver as if it were RGB data.
    * Some padding is added when source LED data is not a multiple of 3.
  * See [examples/RGBWEmulated/RGBWEmulated.ino](https://github.com/FastLED/FastLED/blob/master/examples/RGBW/RGBW.ino)
* New supported chipsets
  * UCS1912 (Clockless)
  * WS2815 (Clockless)
* New supported boards
  * xiaoblesense_adafruit
    * Fixes https://github.com/FastLED/FastLED/issues/1445
* [PixelIterator](src/pixel_iterator.h) has been introduced to reduce complexity of writing driver code
  * This is how RGBW mode was implemented.
  * This is a concrete class (no templates!) so it's suitable for driver code in cpp files.
  * PixelController<> can convert to a PixelIterator, see `PixelController<>::as_iterator(...)`
* Fixed APA102HD mode for user supplied function via the linker. Added test so that it won't break.


FastLED 3.7.6
=============
* WS2812 RGBW Mode enabled on ESP32 via experimental `FASTLED_EXPERIMENTAL_ESP32_RGBW_ENABLED`
* RPXXXX compiler fixes to solve asm segment overflow violation
* ESP32 binary size blew up in 3.7.5, in 3.7.6 it's back to the same size as 3.7.4
* APA102 & SK9822 have downgraded their default clock speed to improve "just works" experience
  * APA102 chipsets have downgraded their default clock from 24 mhz to 6mhz to get around the "long strip signal degradation bug"
    * https://www.pjrc.com/why-apa102-leds-have-trouble-at-24-mhz/
    * We are prioritizing "just works by default" rather than "optimized by default but only for short strips".
    * 6 Mhz is still blazingly fast compared to WS2812 and you can always bump it up to get more performance.
  * SK9822 have downgraded their default clock from 24 mhz -> 12 mhz out of an abundance of caution.
    * I don't see an analysis of whether SK9822 has the same issue as the APA102 for the clock signal degredation.
    * However, 12 mhz is still blazingly fast (>10x) compared to WS2812. If you need faster, bump it up.
* NRF52XXX platforms
  * Selecting an invalid pin will not spew pages and pages of template errors. Now it's been deprecated to a runtime message and assert.
* nrf52840 compile support now official.

FastLED 3.7.5
=============

* split the esp32-idf 4.x vs 5.x rmt driver. 5.x just redirects to 4.x by @zackees in https://github.com/FastLED/FastLED/pull/1682
* manually merged in stub from https://github.com/FastLED/FastLED/pull/1366 by @zackees in https://github.com/FastLED/FastLED/pull/1685
* manually merge changes from https://github.com/FastLED/FastLED/compare/master...ben-xo:FastLED:feature/avr-clockless-trinket-interrupts by @zackees in https://github.com/FastLED/FastLED/pull/1686
* Add simplex noise [revisit this PR in 2022] by @aykevl in https://github.com/FastLED/FastLED/pull/1252
* Add ColorFromPaletteExtended function for higher precision by @zackees in https://github.com/FastLED/FastLED/pull/1687
* correct RP2350 PIO count / fix double define SysTick by @FeuerSturm in https://github.com/FastLED/FastLED/pull/1689
* improved simplex noise by @zackees in https://github.com/FastLED/FastLED/pull/1690
* Fix shift count overflow on AVR in simplex snoise16 by @tttapa in https://github.com/FastLED/FastLED/pull/1692
* adds extended color palette for 256 by @zackees in https://github.com/FastLED/FastLED/pull/1697
* RP2350 board now compiles.



FastLED 3.7.4
=============
Board support added
  * https://github.com/FastLED/FastLED/pull/1681
    * Partial support for adafruit nrf sense
      * WS2812 compiles
      * APA102 does not
    * Hat tip to https://github.com/SamShort7 for the patch.
  * https://github.com/FastLED/FastLED/pull/1630
    * Adafruit Pixel Trinkey M0 support
    * Hat tip: https://github.com/BlitzCityDIY


FastLED 3.7.3
=============
Adds Arduino IDE 2.3.1+ support in the idf-5.1 toolchain
The following boards are now tested to compile and build
  * esp32dev
  * esp32c3
  * esp32s3
  * esp32c6
  * esp32s2


FastLED 3.7.2
=============
This is a feature enhancement release
  * https://github.com/FastLED/FastLED/commit/cbfede210fcf90bcec6bbc6eee7e9fbd6256fdd1
    * fill_gradient() now has higher precision for non __AVR__ boards.
		* Fixes: https://github.com/FastLED/FastLED/issues/1658
			* Thanks https://github.com/sutaburosu for the fix.


FastLED 3.7.1
=============
This is a bug fix release
  * https://github.com/FastLED/FastLED/commit/85650d9eda459df20ea966b85d48b84053c2c604
    * Addresses compiler issues related ESP32-S3 and the RMT legacy driver in ArduinoIDE 2.3.2 update which now includes the ESP-IDF 5.1.
    * Note that this is a compiler fix *only* and was simple. If the community reports additional problems we will release a bugfix to address it.
  * https://github.com/FastLED/FastLED/commit/e0a34180c5ad1512aa39f6b6c0987119535d39e8
    * Work around for ESP32 halt when writing WS2812 LEDS under massive load. It appears there was an underflow condition in a critical ISR to refill the RMT buffer that did not give back to a semaphore. Subsequent calls to `show()` would then block forever. We now given a max timeout so that in the worse case scenario there will be a momentary hang of `portMAX_DELAY`.


FastLED 3.7.0
=============
This release incorporates valuable improvements from FastLED contributors, tested and explored by the world-wide FastLED community of artists, creators, and developers.  Thank you for all of your time, energy, and help!  Here are some of the most significant changes in FastLED 3.7.0:
* Support for ESP-IDF version 5.x on ESP32 and ESP8266a
* Improved support for new boards including UNO r4, Adafruit Grand Central Metro M4, SparkFun Thing Plus, RP2040, Portenta C33, and others.  We also added a pointer to the PORTING.md document to help streamline additional porting; if you’re porting to a new microcontroller, PORTING.md is the place to start.
* New gamma correction capability for APA102 and SK9822 LEDs
* Bug fixes and performances improvements, including faster smaller code on AVR, fewer compiler warnings, and  faster build times
* Released May 2024, with heartfelt thanks to all the FastLED community members around the world!


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
