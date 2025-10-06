/// BasicTest example to demonstrate massive parallel output with FastLED using
/// ObjectFLED for Teensy 4.0/4.1.
///
/// This mode will support upto 42 parallel strips of WS2812 LEDS! ~7x that of OctoWS2811!
///
/// The theoritical limit of Teensy 4.0, if frames per second is not a concern, is
/// more than 200k pixels. However, realistically, to run 42 strips at 550 pixels
/// each at 60fps, is 23k pixels.
///
/// @author Kurt Funderburg
/// @reddit: reddit.com/u/Tiny_Structure_7
/// The FastLED code was written by Zach Vorhies

#if defined(__IMXRT1062__) // Teensy 4.0/4.1 only.
#include "./TeensyMassiveParallel.h"
#else
#include "platforms/sketch_fake.hpp"
#endif
