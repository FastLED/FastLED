// ok no namespace fl
#pragma once

#ifndef __INC_FASTLED_PLATFORMS_DELAYCYCLES_H
#define __INC_FASTLED_PLATFORMS_DELAYCYCLES_H

/// @file platforms/delaycycles.h
/// Central distribution header for platform-specific cycle-accurate delay implementations
/// This header handles all platform detection and includes the appropriate
/// platform-specific delaycycles headers with NOP macros and delaycycles<> specializations.

#include "platforms/cycle_type.h"
#include "fl/force_inline.h"

// ============================================================================
// Platform-specific cycle delay includes (delaycycles)
// ============================================================================
// These provide NOP macros and delaycycles template implementations

#if defined(__AVR__)
#include "platforms/avr/delay_cycles.h"
#elif defined(ESP32) && !defined(ESP32C3) && !defined(ESP32C6)
#include "platforms/shared/delay_cycles_generic.h"
#include "platforms/esp/32/core/delaycycles.h"
#elif defined(ESP32C3) || defined(ESP32C6)
#include "platforms/shared/delay_cycles_generic.h"
#include "platforms/esp/32/core/delaycycles_riscv.h"
#elif defined(ARDUINO_ARCH_RP2040)
#include "platforms/shared/delay_cycles_generic.h"
#include "platforms/arm/rp/rp2040/delaycycles.h"
#elif defined(NRF52_SERIES)
#include "platforms/shared/delay_cycles_generic.h"
#include "platforms/arm/nrf52/delaycycles.h"
#elif defined(ARDUINO_ARCH_SAMD)
#include "platforms/shared/delay_cycles_generic.h"
#include "platforms/arm/d21/delaycycles.h"
#elif defined(__ARM_ARCH_7M__) || defined(__ARM_ARCH_7EM__)
#include "platforms/shared/delay_cycles_generic.h"
#include "platforms/arm/stm32/delaycycles.h"
#elif defined(ARDUINO_ARCH_RENESAS) || defined(ARDUINO_ARCH_RENESAS_UNO) || defined(ARDUINO_ARCH_RENESAS_PORTENTA)
#include "platforms/shared/delay_cycles_generic.h"
#include "platforms/arm/renesas/delaycycles.h"
#else
#include "platforms/shared/delay_cycles_generic.h"
#include "platforms/delaycycles_generic.h"
#endif

// ============================================================================
// ESP32 NOP workaround
// ============================================================================
// ESP32 core has its own definition of NOP, so undef it first
#ifdef ESP32
#undef NOP
#undef NOP2
#endif

#endif  // __INC_FASTLED_PLATFORMS_DELAYCYCLES_H
