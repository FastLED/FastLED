FastLED 3.9.14
==============
* Attiny13 and Attiny4343 now works
  * https://github.com/FastLED/FastLED/pull/1874
  * Thanks https://github.com/sutaburosu!
* Arduino GIGA Now working
  * Thank you [@RubixCubix!](https://github.com/RubiCubix)
* Fix for mqtt build modes: https://github.com/FastLED/FastLED/issues/1884

FastLED 3.9.13
==============
* HD107(s) and HD mode are now availabe in FastLED.
  * See example HD107.ino
  * Exactly the same as the AP102 chipset, but with turbo 40-mhz.
  * Keep in mind APA102 is under clocked by FastLED for long strip stability, due to a bug in the chipset. See more: https://forum.makerforums.info/t/hi-all-i-have-800-strip-lengths-of-apa-102-leds-running-off-a/58899/23
* WS2816 has improved support for the ObjectFLED and Esp32 RMT5 drivers.
  * Big thanks to https://github.com/kbob for all the PR's he's submitting to do this.
* ESP32 Legacy RMT Driver
  * Long standing espressif bug for RMT under high load has finally been fixed.
  * Big thanks to https://github.com/Jueff for fixing it.
  * A regression was fixed in getting the cpu clock cycles.
  
![image](https://github.com/user-attachments/assets/9684ab7d-2eaa-40df-a00d-0dff18098917)


FastLED 3.9.12
==============
* WS2816 (high definition) chipset now supported.
  * Thank you https://github.com/kbob for the code to do this!
  * This is a 16-bit per channel LED that runs on the WS2812 protocol.
  * 4-bit internal gamma correction on the chipset.
    * 8-bit gamma correction from software + hardware is possible, but not implemented yet.
    * Therefore this is a beta release of the driver that may see a color correction enhancement down the line.
  * See example: https://github.com/FastLED/FastLED/blob/master/examples/WS2816/WS2816.ino
* Apollo3 SPE LoRa Thing Plus expLoRaBLE now supported
* ESP32-C3 - WS2812 Flicker when using WIFI / Interrupts is now fixed.
  * This has always been a problem since before 3.9.X series.
  * ESP32-C3 now is more stable than ESP32-S3 for the RMT controller because they can allocate much more memory per channel.
  * If you are on the ESP32-S3, please try out the SPI controller if driving one strip, or use the new I2S driver if driving lots of strips.
* ObjectFLED is now automatic for Teensy 4.0/4.1 for WS2812.
  * To disable use `#define FASTLED_NOT_USES_OBJECTFLED` before `#include "FastLED.h"`
* Fixes for RGBW emulated mode for SAMD (digit, due) chipsets.
* AVR platforms will see a 22% shrinkage when using the APA102 and APA102-HD chipset.
  * Uno Firmware (bytes) w/ APA102-HD (bytes):
    * 3.9.11: 11787
    * 3.9.12: 9243 (-22%)


FastLED 3.9.11
==============
* Bug fix for the Teensy and ESP32S3 massive parallel drivers.
  * Teensy ObjectFLED: Each led strip can now be a different length, see [examples](https://github.com/FastLED/FastLED/blob/master/examples/TeensyMassiveParallel/TeensyMassiveParallel.ino)
  * ESP32 S3 I2S:
    * The FastLED.addLeds(...) style api now works..
      * Please note at this time that all 16 strips must be used. Not sure why this is. If anyone has clarification please reach out.
    * RGBW support has been added externally via RGBW -> RGB data spoofing (same thing RGBW Emulated mode uses).
    * Fixed compiliation issue for Arduino 2.3.4, which is missing some headers. In this case the driver will issue a warning that it will unavailable. 
* Cross platform improvements for
  * `FASTLED_DBG`
  * `FASTLED_WARN`
  * `FASTLED_ASSERT`


FastLED 3.9.10
==============
* ESP32
  * RMT5 driver has been fixed for ESP32-S3. Upto 4 RMT workers may work in parallel.
    * Rebased espressifs led_strip to v3.0.0
    * Unresolved issues:
      * DMA does not work for ESP32-S3 for my test setup with XIAO ESP32-S3
        * This appears to be an espressif bug as using dma is not tested in their examples and does not work with the stock driver, or there is something I don't understand.
        * Therefore DMA is disable for now, force it on with
          * `#define FASTLED_RMT_USE_DMA` 
          * `#include "FastLED.h"`
          * If anyone knows what's going on, please file a bug with FastLED [issues](https://github.com/FastLED/FastLED/issues/new) page.
  * New WS2812 SPI driver for ESP32
    * Enables the ESP32C2 device, as it does not have a I2S or RMT drivers.
    * SPI is backed by DMA and is apparently more stable than the RMT driver.
      * Unfortunately, the driver only works with the WS2812 protocol.
    * I was able to test that ESP32-S3 was able to use two spi channels in parallel.
    * You can enable this default via
      * `#define FASTLED_ESP32_USE_CLOCKLESS_SPI`
      * `#include "FastLED.h"
    * Advanced users can enable both the RMT5 and SPI drivers if they are willing to manually construct the SPI driver and at it to the FastLED singleton object via `FastLED.addLeds<...>'
    * If RMT is not present (ESP32C2) then the ClocklessSpiWS2812 driver will be enabled and selected automatically.
* Teensy
  * Massive Parallel - ObjectFLED clockless driver.
    * Stability improvements with timing.
    * Resolves issue with using ObjectFLED mode with Teensy Audio DMA.
    * ObjectFLED driver is now rebased to version 1.1.0


FastLED 3.9.9 - I2S For ESP32-S3
=============
* ESP32
  * Yves's amazing I2S driver for ESP32S3 is available through fastled!
    * 12 way parallel, I2S/LCD protocol.
    * https://github.com/hpwit/I2SClockLessLedDriveresp32s3
    * 12
    * See the Esp32-S3-I2SDemo: https://github.com/FastLED/FastLED/blob/master/examples/Esp32S3I2SDemo/Esp32S3I2SDemo.ino
      * Be mindful of the requirements, this driver requires psram to be enabled, which requires platformio or esp-idf to work. Instructions are in the example.
      * There's no standard FastLED.add<....> api for this driver yet... But hopefully soon.
  * RMT Green light being stuck on / Performance issues on the Wroom
    * Traced it back to RMT disable/delete which puts the pin in floating input mode, which can false signal led colors. If you are affected by this, a weak pulldown resistor will also solve the issue.
    * Fixed: FastLED no longer attempts to disable rmt between draws - once RMT mode is enabled it stay enabled.
    * MAY fix wroom. If this doesn't fix it, just downgrade to RMT4 (sorry), or switch to a higher end chipset. I tested the driver at 6.5ms for 256 * 4 way parallel, which is the max performance on ESP32S3. It was flawless for me.
  * Some internal cleanup. We are now header-stable with 4.0 release: few namespace / header changes from this release forward.

Special thanks to Yves and the amazing work with the 12 way parallel driver. He's pushing the limits on what the ESP32-S3 is capabable of. No joke.

If you are an absolute performance freak, check out Yves's advanced version of this driver with ~8x multiplexing through "turbo" I2S:

https://github.com/hpwit/I2SClockLessLedVirtualDriveresp32s3

FastLED 3.9.8 - FastLED now supports 27.5k pixels and more, on the Teensy 4.x
=============
* FastLED 3.9.8 is the 7th beta release of FastLED 4.0
* We are introducing the new beta release of a *Massive Parallel mode* for Teensy 4.0/4.1 for you to try out!
  * Made possible by Kurt Funderburg's excellent ObjectFLED driver!
    * Check out his stand alone driver: https://github.com/KurtMF/ObjectFLED
    * And give him a star on his repo, this is INCREDIBLE WORK!
  * This will allow you to drive
    * Teensy 4.1: 50 strips of WS2812 - 27,500 pixels @ 60fps!!
      * ~36k pixels at 30% overclock (common)
      * ~46k pixels at 70% overclock (highest end WS2812)
    * Teensy 4.0: 40 strips of WS2812 - 22,000 pixels @ 60fps.
  * The Teensy 4.x series is a **absolute** LED driving beast!
  * This driver is async, so you can prepare the next frame while the current frame draws.
  * Sketch Example: [https://github.com/FastLED/FastLED/blob/master/examples/TeensyMassiveParallel/TeensyMassiveParallel.ino](https://github.com/FastLED/FastLED/blob/master/examples/TeensyMassiveParallel/TeensyMassiveParallel.ino)
  * It's very simple to turn on:
    * `#define FASTLED_USES_OBJECTFLED`
    * `#include "FastLED.h"` - that's it! No other changes necessary!
  * Q/A:
    * Is anything else supported other than WS2812? - Not at this moment. As far as I know, all strips on this bulk controller **must** use the same
      timings. Because of the popularity of WS2812, it is enabled for this controller first. I will add support for other controllers based on the number of feature requests for other WS281x chipsets.
    * Is overclocking supported? Yes, and it binds to the current overclock `#define FASTLED_OVERCLOCK 1.2` @ a 20% overlock.
    * Have you tested this? Very lightly in FastLED, but Kurt has done his own tests and FastLED just provides some wrappers to map it to our familiar and easy api.
    * How does this compare to the stock LED driver on Teensy for just one strip? Better and way less random light flashes. For some reason the stock Teensy WS2812 driver seems to produce glitches, but with the ObjectFLED driver seems to fix this.
    * Will this become the default driver on Teensy 4.x? Yes, in the next release, unless users report problems.
    * Is RGBW supported? Yes - all FastLED RGBW modes are supported.
    * Can other non WS281x chipsets be supported? It appears so, as ObjectFLED does have flexible timings that make it suitable for other clockless chipsets.
    * Does this consume a lot of memory? Yes. ObjectFLED expects a rectangular pixel buffer and this will be generated automatically. This buffer will then be converted into a DMA memory block. However, this shouldn't be that big of a problem as the Teensy 4.x features a massive amount of memory.
* Other Changes
  * ESP32 - bug fixes for RMT5 no recycle mode. This is now the default and addresses the "green led stuck on" issue that some people are facing with ESP-WROOM-32. We also saw it in one bug report for ESP32-S3, so we are going to just enable it everywhere.
    * If you absolutely need the extra controllers because you have more strips than RMT controllers, then you can re-enable recycle mode with:
      * `#define FASTLED_RMT5_RECYCLE=1` before `#include "FastLED.h"`
* Arduino Cloud compile fixes
  * ESP328622 has an additional compile fix for the in-place new operator. Arduino Cloud compiler uses an ancient gcc compiler version which is missing the __has_include that we use to determine if FastLED needs to define a missing in-place new operator.
* Internal stuff
  * `FASTLED_ASSERT(true/false, MSG)` now implemented on ESP32, other platforms will just call `FASTLED_WARN(MSG)` and not abort. Use it via `#include fl/assert.h`. Be careful because on ESP32 it will absolutely abort the program, even in release. This may change later.


FastLED 3.9.7
=============
* ESP32:
  * Okay final fix for the green led that's been stuck on. It turns out in 3.9.6 I made a mistake and swapped the RMT recycle vs no recycle. This should now be corrected. To get the old behavior back use `#define FASTLED_RMT5_RECYCLE=1`. The new behavior may become the default if it turns out this is more stable.
* Arduino Cloud Compiler: This should now work ancient compiler toolchains that they use for some of the older ESP boards. Despite the fact that two bugs were fixed in the last release, another one cropped up in 3.9.6 for extremely old idf toolchians which defines digitalRead/digitalWrite not as functions, but as macros.


FastLED 3.9.6
=============
* ESP32:
  * Sticky first green LED on the chain has been fixed. It turned out to be aggressive RMT recycling. We've disabled this for now and filed a bug:
      * https://github.com/FastLED/FastLED/issues/1786
      * https://github.com/FastLED/FastLED/issues/1761
      * https://github.com/FastLED/FastLED/issues/1774
* Bug fix for FastLED 3.9.5
  * Fixes using namespace fl in `FastLED.h` in the last release (oops!)
* Fixes for Arduino Cloud compiler and their ancient version of esp-idf for older chips.
  * Handle missing `IRAM_ATTR`
  * inplace new operator now is smarter about when to be defined by us.

FastLED 3.9.5
=============

* Esp32:
  * There's a bug in the firmware of some ESP32's where the first LED is green/blue/red, though we haven't be able to reproduce it.
  * This may be manifesting because of our RMT recycling. We offer a new RMT5 variant that may fix this.
    * Here's how you enable it: use `#define FASTLED_RMT5_RECYCLE=0` before you `#include "FastLED.h"`
    * If this works then please let us know either on reddit or responding to our bug entries:
      * https://github.com/FastLED/FastLED/issues/1786
      * https://github.com/FastLED/FastLED/issues/1761
      * https://github.com/FastLED/FastLED/issues/1774
* ESP32C6
  * This new board had some pins marked as invalid. This has been fixed.
* ESP32S2
  * The correct SPI chipset (FSPI, was VSPI) is now used when `FASTLED_ALL_PINS_HARDWARE_SPI` is active.
* The previous headers that were in src/ now have a stub that will issue a deprecation warning and instructions to fix, please migrated before 4.0 as the deprecated headers will go away.
* Many many strict compiler warnings are now treated as errors during unit test. Many fixes in the core have been applied.
* CLEDController::setEnabled(bool) now allows controllers to be selectively disabled/enabled. This is useful if you want to have multiple controller types mapped to the same pin and select which ones are active during runtime, or to shut them off for whatever reason.
* Attiny88 is now under test.
* CLEDController::clearLeds() again calls showLeds(0)
* Completely remove Json build artifacts for avr, fixes compiler error for ancient avr-gcc versions.
* Namespaces: `fl` - the new FastLED namespace
  * Much of the new code in 3.9.X has been moved into the `fl` namespace. This is now located in the `fl/` directory. These files have mandatory namespaces but most casual users won't care because because all the files in the `fl/` directory are for internal core use.
  * Namespaces for the core library are now enabled in internal unit tests to ensure they work correctly for the power users that need them. Enabling them requires a build-level define. (i.e. every build system except ArduinoIDE supports this) you can use it putting in this build flag: `-DFASTLED_NAMESPACE=1`. This will force it on for the entire FastLED core.
  * We are doing this because we keep getting conflicts with our files and classes conflict with power users who have lots of code.The arduino build system likes to put all the headers into the global space so the chance of collisions goes up dramatically with the number of dependencies one has and we are tired of playing wack a mole with fixing this.
    * Example: https://github.com/FastLED/FastLED/issues/1775
* Stl-like Containers: We have some exciting features coming up for you. In this release we are providing some of the containers necessary for complex embedded black-magic.
  * `fl::Str`: a copy on write String with inlined memory, which overflows to the heap after 64 characters. Lightning fast to copy around and keep your characters on the stack and prevent heap allocation. Check it out in `fl/str.h`. If 64 characters is too large for your needs then you can change it with a build-level define.
  * `fl/vector.h`:
    * `fl::FixedVector`: Inlined vector which won't ever overflow.
    * `fl::HeapVector`: Do you need overflow in your vector or a drop in replacement for `std::vector`? Use this.
    * `fl::SortedHeapVector`: If you want to have your items sorted, use this. Inserts are O(n) always right now, however with deferred sorting, it could be much faster. Use `fl::SortedHeapVector::setMaxSize(int)` to keep it from growing.
  * `fl/map.h`
    * `fl::SortedHeapMap`: Almost a drop in replacement for `std::map`. It differs from the `fl::SortedHeapVector` because this version works on key/value pairs. Like `std::map` this takes a comparator which only applies to the keys.
    * `fl::FixedMap`: Constant size version of `fl::SortedHeapMap` but keeps all the elements inlined and never overflows to the heap.
  * `fl/set.h`
    * `fl::FixedSet`: Similar to an `std::set`. Never overflows and all the memory is inlined. Ever operation is O(N) but the inlined nature means it will beat out any other set as long as you keep it small.
  * `fl/scoped_ptr.h`:
    * `fl::scoped_ptr.h`:
      * Similar to `std::unique_ptr`, this allows you to manage a pointer type and have it automatically destructed.
    * `fl::scoped_array.h`: Same thing but for arrays. Supports `operator[]` for array like access.
  * `fl/slice.h`: Similar to an `std::span`, this class will allow you to pass around arrays of contigious memory. You can `pop_front()` and `pop_back()`, but it doesn't own the memory so nothing will get deleted.
  * `fl/ptr.h`
    * `fl::Ptr<T>`, a ref counted intrusive shared pointer. "Intrusive" means the referent is inside the class the pointer refers to, which prevents an extra allocation on the heap. It's harder to use than `std::shared_ptr` because it's extremely strict and will not auto-covert a raw pointer into this Ptr type without using `Ptr<T>::TakeOwnership(T*)`. This is done to prevent objects from double deletion. It can also take in pointers to stack/static objects with `Ptr<T>::NoTracking(T*)`, which will disable reference counter but still allow you use
* Blur effects no longer link to the int XY(int x, int y) function which is assumed to exist in your sketch. This has been the bane of existance for those that encounter it. Now all functions that linked to XY() now take in a `fl::XYMap` which is the class
  form of this. This also means that you can apply blur effects with multiple led panels, where XY() assumed you just had only one array of leds.
* Sensors
  * PIR (passife infrared) sensors are one of the staples of LED effects. They are extremely good at picking up movement anywhere and are extremely cheap. They are also extremely easy to use with only one pin, besides the power rails. I've used them countless times for nearly all my LED effects. Therefore I've added two PIR sensors for you to play around with.
    * `sensors/pir.h`
      * `fl::Pir`: This is a basic PIR that will tell you if the sensor is curently triggered. It doesn't do much else.
      * `fl::AdvancedPir`: An extended version of `fl::Pir` which gives transition effects as it turns on and off. Here is what the
        the constructor looks like: `fl::PirAdvanced(int pin, uint32_t latchMs = 5000, uint32_t risingTime = 1000, uint32_t fallingTime = 1000)`.
        You will give it the pin, an optional latch time (how long it stays on for), the rising time (how long to go from off to on) and the falling
        time which is how long it takes to go from on to off. By default it will ramp on for one second, stay on for 5 seconds at full brightness, then
        start turning off for one second. All you have to do is give it the current `millis()` value.
      * To see it in action check out `examples/fx/NoiseRing`
* AVR
  * The Atmega family and 32u now has a maximum of 16 controllers that can be active, up from 8, due to these models having more memory. Someone actually needed this, suprisingly.
* The 4.0 release is getting closer. We have some exciting stuff on the horizon that I can't wait to show you! Happy Coding! ~Zach

FastLED 3.9.4
=============
* Fixes some name collisions from users including a lot of libraries.
* Other misc fixes.


FastLED 3.9.3
=============
* Beta Release 3 for FastLED 4.0.0
* ESP32C6 now supported with RMT5 driver without workaround. This chip does not use DMA and so must go through the non DMA path for RMT.
* RMT5 tweaks for ESP32
  * For non DMA memory boards like the ESP32, ESP32C3, ESP32C6 RMT will now double it's memory but only allow 4 RMT workers.
  * This was the behavior for the RMT4.X drivers.
  * This is done to reduce LED corruption when WIFI is enabled.
* WS2812 now allows user overrides of it's timing values T1, T2, T3. This is to help debug timing issues on the new V5B of
  this chipset. You can define FASTLED_WS2812_T1, FASTLED_WS2812_T2, FASTLED_WS2812_T3 before you include FastLED.

FastLED 3.9.2
=============
* Beta release 2 for FastLED 4.0.0
  * In this version we introduce the pre-release of our WS2812 overclocking
  * We have compile fixes for 3.9.X
* WS28XX family of led chipsets can now be overclocked
  * See also define `FASTLED_OVERCLOCK`
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


Example of how to enable overclocking.

```
#define FASTLED_OVERCLOCK 1.2 // 20% overclock ~ 960 khz.
#include "FastLED.h"
```


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
