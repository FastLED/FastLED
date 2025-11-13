#pragma once

// ok no namespace fl

// Backward compatibility shim for fastpin_avr_legacy.h
//
// This file has been decomposed into family-specific headers for better maintainability.
// The 500+ line god-header was difficult for both humans and AI to work with.
//
// New structure:
//   - atmega/m328p/fastpin_m328p.h       (Arduino UNO, Nano)
//   - atmega/m32u4/fastpin_m32u4.h       (Leonardo, Pro Micro, Teensy 2.0)
//   - atmega/m2560/fastpin_m2560.h       (Arduino MEGA)
//   - atmega/common/fastpin_legacy_other.h (Other ATmega variants)
//   - attiny/pins/fastpin_attiny.h       (All ATtiny variants)
//
// This shim provides backward compatibility by forwarding to the new dispatcher.
// It will be removed in FastLED v4.0.

#warning "fastpin_avr_legacy.h is deprecated. The god-header has been split into family-specific files. This backward-compat shim will be removed in FastLED v4.0."

#include "atmega/common/fastpin_avr_legacy_dispatcher.h"
