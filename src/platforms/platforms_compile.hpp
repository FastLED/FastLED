// Hierarchical include file for platforms/ directory
#pragma once

#ifdef FASTLED_ALL_SRC

// PLATFORMS MODULE IMPLEMENTATIONS (HIERARCHICAL)
#include "platforms/arm/arm_compile.hpp"  // 1 files
#include "platforms/avr/avr_compile.hpp"  // 1 files
#include "platforms/esp/esp_compile.hpp"  // 7 files
#include "platforms/shared/shared_compile.hpp"  // 14 files
#include "platforms/wasm/wasm_compile.hpp"  // 9 files

#endif // FASTLED_ALL_SRC
