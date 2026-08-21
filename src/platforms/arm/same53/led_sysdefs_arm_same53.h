// IWYU pragma: private

// ok no namespace fl

#pragma once

// ClearCore exposes connector IDs instead of Arduino's digitalPinToPort map.
#define FASTLED_NO_PINMAP

// SAME53 and SAMD51 are both Cortex-M4F devices running at 120 MHz. The
// interrupt and clockless timing definitions are shared, while pin routing is
// kept board-specific in fastpin_arm_same53.h.
#include "platforms/arm/d51/led_sysdefs_arm_d51.h"
