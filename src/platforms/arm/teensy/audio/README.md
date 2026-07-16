# FastLED Teensy I2S Audio

This directory contains the small, FastLED-owned subset of the PJRC Teensy
Audio implementation needed for I2S input. It deliberately does not use the
umbrella `Audio.h` header and does not carry playback, SD, or SerialFlash
components.

## Upstream provenance

The initial migration is derived from the Teensyduino 1.60 framework package:

- `cores/teensy4/AudioStream.h` and `AudioStream.cpp`
- `libraries/Audio/input_i2s.{h,cpp}`
- `libraries/Audio/input_i2s2.{h,cpp}`
- the I2S clock configuration portions of `output_i2s.cpp` and
  `output_i2s2.cpp`

Upstream copyright and MIT license notices are retained in each imported
source file. FastLED changes must remain documented next to the relevant
code so future upstream comparisons are practical.
