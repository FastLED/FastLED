#pragma once

// ⚠️⚠️⚠️ DEPRECATED: WASM-SPECIFIC ACTIVE STRIP DATA HEADER ⚠️⚠️⚠️
//
// This file is now a compatibility wrapper. The actual ActiveStripData
// implementation has been moved to:
//   src/platforms/shared/active_strip_data/active_strip_data.h
//
// This allows for better testability and platform independence.
//
// WASM-specific functionality is handled in the corresponding .cpp file.
//
// ⚠️⚠️⚠️ NEW CODE SHOULD INCLUDE THE SHARED HEADER DIRECTLY ⚠️⚠️⚠️

#include "platforms/shared/active_strip_data/active_strip_data.h"
