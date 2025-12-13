#pragma once

// ok no namespace fl

/// @file is_rp.h
/// @brief RP platform family detection macros for FastLED
///
/// This header provides detection for the entire Raspberry Pi RP platform family.
/// It includes both RP2040 and RP2350 detection headers and defines FL_IS_RP
/// if any RP platform is detected.

// Include platform-specific detection headers
#include "is_rp2040.h"
#include "is_rp2350.h"

// Define FL_IS_RP if any RP platform is detected
#if defined(FL_IS_RP2040) || defined(FL_IS_RP2350)
#define FL_IS_RP
#endif
