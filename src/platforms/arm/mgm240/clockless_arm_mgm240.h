#pragma once

// Use the GIGA ARM clockless controller as base - it's well-suited for Cortex-M33
// The GIGA uses ARM DWT cycle counter which is standard on Cortex-M33 (MGM240)
#include "platforms/arm/giga/clockless_arm_giga.h"