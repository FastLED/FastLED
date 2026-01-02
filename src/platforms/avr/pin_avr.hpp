#pragma once

/// @file platforms/avr/pin_avr.hpp
/// AVR (Arduino Uno, Mega, etc.) pin implementation (header-only)
///
/// Provides zero-overhead native AVR register-based GPIO implementation.
/// Uses direct register manipulation for all platforms (Arduino and non-Arduino).
///
/// IMPORTANT: Translates fl:: enum classes to native AVR register operations.

// ok no namespace fl
#include "pin_avr_native.hpp"
