// IWYU pragma: private

/// @file _build.cpp.hpp
/// @brief Unity build header for platforms/arm/lpc/drivers/ subdirectory.
///
/// Groups the per-driver `_build.cpp.hpp` unity headers so the parent
/// `platforms/arm/lpc/_build.cpp.hpp` only reaches one level deep, per
/// UnityBuildChecker convention.

// begin sub directory includes
#include "platforms/arm/lpc/drivers/sct_dma/_build.cpp.hpp"
