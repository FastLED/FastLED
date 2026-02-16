#pragma once

// IWYU pragma: private

// ok no namespace fl

#include "is_avr.h"
#include "platforms/math8_config.h"
#include "lib8tion/lib8static.h"
#include "lib8tion/intmap.h"
#include "fl/compiler_control.h"
#include "fl/force_inline.h"

// Select appropriate AVR implementation based on MUL instruction availability
#ifdef FL_IS_AVR_ATTINY
// ATtiny platforms without MUL instruction
// IWYU pragma: begin_keep
#include "attiny/math/math8_attiny.h"
// IWYU pragma: end_keep
#else
// AVR platforms with MUL instruction (ATmega, etc.)
// IWYU pragma: begin_keep
#include "atmega/common/math8_avr.h"
// IWYU pragma: end_keep
#endif

// All function implementations are now in math8_attiny.h or math8_avr.h
// This file is just a router to select the appropriate implementation
