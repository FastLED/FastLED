#ifndef __INC_FASTLED_CONFIG_H
#define __INC_FASTLED_CONFIG_H

#include "FastLED.h"

/// @file fastled_config.h
/// Contains definitions that can be used to configure FastLED at compile time

/// @def FASTLED_FORCE_SOFTWARE_PINS
/// Use this option only for debugging pin access and forcing software pin access.  Forces use of `digitalWrite()`
/// methods for pin access vs. direct hardware port access.
/// @note Software pin access only works in Arduino-based environments.
// #define FASTLED_FORCE_SOFTWARE_PINS

/// @def FASTLED_FORCE_SOFTWARE_SPI
/// Use this option only for debugging bitbang'd SPI access or to work around bugs in hardware
/// SPI access.  Forces use of bit-banged SPI, even on pins that have hardware SPI available.
// #define FASTLED_FORCE_SOFTWARE_SPI

/// @def FASTLED_ALLOW_INTERRUPTS
/// Use this to force FastLED to allow interrupts in the clockless chipsets (or to force it to
/// disallow), overriding the default on platforms that support this.  Set the value to 1 to
/// allow interrupts or 0 to disallow them.
// #define FASTLED_ALLOW_INTERRUPTS 1
// #define FASTLED_ALLOW_INTERRUPTS 0

/// @def FASTLED_NOISE_ALLOW_AVERAGE_TO_OVERFLOW
/// Use this to allow some integer overflows/underflows in the inoise() functions.
/// The original implementions allowed this, and had some discontinuties in the noise
/// output.  It's technically an implementation bug, and was fixed, but you may wish
/// to preserve the old look and feel of the inoise() functions in your existing animations.  
/// The default is 0:  NO overflow, and 'continuous' noise output, aka the fixed way.
// #define FASTLED_NOISE_ALLOW_AVERAGE_TO_OVERFLOW 0
// #define FASTLED_NOISE_ALLOW_AVERAGE_TO_OVERFLOW 1

/// @def FASTLED_SCALE8_FIXED
/// Use this to toggle whether or not to use the "fixed" FastLED scale8().  The initial scale8()
/// had a problem where scale8(255,255) would give you 254.  This is now fixed, and that
/// fix is enabled by default.  However, if for some reason you have code that is not
/// working right as a result of this (e.g. code that was expecting the old scale8() behavior)
/// you can disable it here.
#define FASTLED_SCALE8_FIXED 1
// #define FASTLED_SCALE8_FIXED 0

/// @def FASTLED_BLEND_FIXED
/// Use this to toggle whether to use "fixed" FastLED pixel blending, including ColorFromPalette.
/// The prior pixel blend functions had integer-rounding math errors that led to
/// small errors being inadvertently added to the low bits of blended colors, including colors
/// retrieved from color palettes using LINEAR_BLEND.  This is now fixed, and the
/// fix is enabled by default.  However, if for some reason you wish to run with the old
/// blending, including the integer rounding and color errors, you can disable the bugfix here.
#define FASTLED_BLEND_FIXED 1
// #define FASTLED_BLEND_FIXED 0

/// @def FASTLED_NOISE_FIXED
/// Use this to toggle whether to use "fixed" FastLED 8-bit and 16-bit noise functions.
/// The prior noise functions had some math errors that led to "discontinuities" in the
/// output, which by definition should be smooth and continuous.  The bug led to 
/// noise function output that had "edges" and glitches in it.  This is now fixed, and the
/// fix is enabled by default.  However, if for some reason you wish to run with the old
/// noise code, including the glitches, you can disable the bugfix here.
#define FASTLED_NOISE_FIXED 1
//#define FASTLED_NOISE_FIXED 0

/// @def FASTLED_INTERRUPT_RETRY_COUNT
/// Use this to determine how many times FastLED will attempt to re-transmit a frame if interrupted
/// for too long by interrupts.
#ifndef FASTLED_INTERRUPT_RETRY_COUNT
#define FASTLED_INTERRUPT_RETRY_COUNT 2
#endif

/// @def FASTLED_USE_GLOBAL_BRIGHTNESS
/// Use this toggle to enable global brightness in contollers that support is (e.g. ADA102 and SK9822).
/// It changes how color scaling works and uses global brightness before scaling down color values.
/// This enables much more accurate color control on low brightness settings.
//#define FASTLED_USE_GLOBAL_BRIGHTNESS 1


// The defines are used for Doxygen documentation generation.
// They're commented out above and repeated here so the Doxygen parser
// will be able to find them. They will not affect your own configuration, 
// and you do *NOT* need to modify them.
#ifdef FASTLED_DOXYGEN
#define FASTLED_FORCE_SOFTWARE_PINS
#define FASTLED_FORCE_SOFTWARE_SPI
#define FASTLED_ALLOW_INTERRUPTS
#define FASTLED_NOISE_ALLOW_AVERAGE_TO_OVERFLOW 0
#define FASTLED_INTERRUPT_RETRY_COUNT 2
#define FASTLED_USE_GLOBAL_BRIGHTNESS 0
#endif

#endif
