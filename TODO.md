# Testing

  * Esp32 testing
    * https://github.com/marketplace/actions/esp32-qemu-runner will run a sketch for X seconds and see's if it crashes
      * There's specific tests we'd like to run with this including the WS2812 and APA102 tests to test the clockless and clocked drivers

# Feature Enhancements

  * I2S driver for ESP32 WS2812
    * https://github.com/hpwit/I2SClocklessLedDriver
      * Our copy is here: https://github.com/FastLED/FastLED/blob/master/src/platforms/esp/32/clockless_i2s_esp32.h
    * S3:
      * https://github.com/hpwit/I2SClockLessLedDriveresp32s3
    * Apparently, this driver allows MASSIVE parallelization for WS2812
    * Timing guide for reducing RMT frequency https://github.com/Makuna/NeoPixelBus/pull/795
    * ESp32 LED guide
      * web: https://components.espressif.com/components/espressif/led_strip
      * repo: https://github.com/espressif/idf-extra-components/tree/60c14263f3b69ac6e98ecae79beecbe5c18d5596/led_strip
      * adafruit conversation on RMT progress: https://github.com/adafruit/Adafruit_NeoPixel/issues/375


  * MIT Licensed SdFat library
    * https://github.com/greiman/SdFat
  * YVes LittleFS implementation for ESP
    * https://github.com/hpwit/ledOS/blob/main/src/fileSystem.h

  * NimBLE for Arduino
    * https://github.com/h2zero/NimBLE-Arduino?tab=readme-ov-file

  * Arduino test compile
    * https://github.com/hpwit/arduino-test-compile/blob/master/arduino-test-compile.sh


# Misc:

  * sutaburosu's guide to playing around with FastLED 4
    * https://github.com/sutaburosu/FastLED4-ESP32-playpen


# Corkscrew projection

  * Projection from Corkscrew (θ, h) to Cylindrical (θ, h)
    * Super sample cylindrical space
    * θ should in this case be normalized to [0, 1]?
    * Because cylindrical projection will be on a W X H grid, so θ -> [0, W-1]
  * This needs to be super sampled 2x2.
  * The further sampling will be done via the function currently in the XYPathRenderer
  * `Splat Rendering`
    * for "sub-pixel" (neighbor splatting?) rendering.
    * // Rasterizes point with a value For best visual results, you'll want to
    // rasterize tile2x2 tiles, which are generated for you by the XYPathRenderer
    // to represent sub pixel / neightbor splatting positions along a path.
    // TODO: Bring the math from XYPathRenderer::at_subpixel(float alpha)
    // into a general purpose function.


*Inputs*

  * Total Circumference / length of the Corkscrew
  * Total angle of the Corkscrew
    * Count the total number of segments in the vertical direction and times by 2pi, likewise with the ramainder.
  * Optional offset circumference, default 0.
    * Allows us to generate pixel perfect corkscrew with gaps acccounted for via circuference offseting.
    * Example (accounts for gap):
      * segment 0: offset circumference = 0, circumference = 100
      * segment 1: offset circumference = 100.5, circumference = 100
      * segment 2: offset circumference = 101, circumference = 100



*Outputs*

  * Width and and a Height of the cyclindracal map
    * Width is the circumference of one turn on the Corkscrew
    * Height is the total number of segments in the vertical direction
    * Some rounding up will be necessary for the projection.
  * The reference format will be a vector of vec2f {width, height} mapping each point on the corkscrew (r,c) to the cylindrical space {w,h}
  * Optional compact format is vec2u {width, height} vec2<uint8_t> for linear blend in each w and h direction.
    * This would keep the data computation in fixed integer space.
    * Is small enough to fit in a single uint32_t, upto 256x256 pixels (HUGE DISPLAY SEGMENT)
