// IWYU pragma: private

/// @file _build.cpp.hpp
/// @brief Unity build header for platforms/arm/lpc/ implementations

// begin current directory includes
#include "platforms/arm/lpc/io_lpc.cpp.hpp"
#include "platforms/arm/lpc/platform_time_lpc.cpp.hpp"
#include "platforms/arm/lpc/runtime_lpc.cpp.hpp"
#include "platforms/arm/lpc/rx_sct_capture.cpp.hpp"

// SCT+DMA channels-API engine (#3459). Self-gated to LPC845; other LPC
// variants compile out cleanly. Routed through drivers/_build.cpp.hpp
// per unity-build 1-level-deep convention.

// begin sub directory includes
#include "platforms/arm/lpc/drivers/_build.cpp.hpp"
