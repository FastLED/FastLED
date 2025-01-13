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