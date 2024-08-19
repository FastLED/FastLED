# Testing

  * Esp32 testing
    * https://github.com/marketplace/actions/esp32-qemu-runner will run a sketch for X seconds and see's if it crashes
      * There's specific tests we'd like to run with this including the WS2812 and APA102 tests to test the clockless and clocked drivers

# Feature Enhancements

  * I2S driver for ESP32 WS2812
    * Original repo is here: https://github.com/hpwit/I2SClockLessLedDriveresp32s3/blob/main/src/I2SClockLessLedDriveresp32s3.h
      * Our copy is here: https://github.com/FastLED/FastLED/blob/master/src/platforms/esp/32/clockless_i2s_esp32.h
    * Apparently, this driver allows MASSIVE parallelization for WS2812
