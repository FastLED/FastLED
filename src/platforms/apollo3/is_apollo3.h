#pragma once

// ok no namespace fl

/// @file is_apollo3.h
/// Apollo3 platform detection header
///
/// This header detects Ambiq Apollo3 platforms by checking compiler-defined
/// macros and defines standardized FL_IS_APOLLO3 macro.
///
/// Defines:
/// - FL_IS_APOLLO3: Apollo3 platform (Ambiq Apollo3 Blue - SparkFun Artemis family)
///
/// Note: Apollo3 is also detected as an ARM platform via platforms/arm/is_arm.h

// ============================================================================
// FL_IS_APOLLO3 - Apollo3 platform detection
// ============================================================================
#if defined(ARDUINO_ARCH_APOLLO3) || defined(FASTLED_APOLLO3) || defined(APOLLO3)
#define FL_IS_APOLLO3
#endif
