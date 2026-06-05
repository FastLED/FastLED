// ok no namespace fl
#pragma once

// IWYU pragma: private

// LPC8xx (Cortex-M0+) shares the standard 32-bit ARM integer typedefs.
// Delegate to the shared ARM int.h so future divergence (e.g. if a specific
// LPC toolchain needs different short/long widths) has a single place to land.
#include "platforms/arm/int.h"
