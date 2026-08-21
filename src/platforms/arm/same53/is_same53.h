#pragma once

// IWYU pragma: private

// ok no namespace fl

/// @file is_same53.h
/// @brief Microchip SAME53 platform detection.

// Teknic's ClearCore Arduino core identifies the MCU with
// __SAME53N19A__. Accept the vendor-prefixed spelling as well so the
// platform remains detectable with toolchains that expose the full part name.
#if defined(__SAME53N19A__) || defined(__ATSAME53N19A__)
#define FL_IS_SAME53
#endif
