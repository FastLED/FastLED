#pragma once

// IWYU pragma: private

// ok no namespace fl  — preprocessor-only header (no declarations to scope).

// ESP-IDF esp_cache_msync() flag compatibility shim.
//
// `ESP_CACHE_MSYNC_FLAG_DIR_C2M` (cache-to-memory direction) was added in
// ESP-IDF 5.2. Prior versions of `esp_cache_msync()` defaulted to the C2M
// direction without taking an explicit flag, so backfilling the macro to
// `0` (no extra flag bits) preserves the same runtime behavior under the
// new call signature.
//
// Centralized here so every driver that calls `esp_cache_msync(... , ...,
// ESP_CACHE_MSYNC_FLAG_DIR_C2M)` gets the same shim. Per-driver duplicates
// drift out of sync the first time a unity-build include order shifts and
// one file is preprocessed before the file that owned the shim — that
// failure mode is what motivated this file (see FastLED #2619).

#include "platforms/esp/esp_version.h"

#if ESP_IDF_VERSION < ESP_IDF_VERSION_VAL(5, 2, 0)
    #ifndef ESP_CACHE_MSYNC_FLAG_DIR_C2M
        #define ESP_CACHE_MSYNC_FLAG_DIR_C2M 0  // Default direction on IDF < 5.2
    #endif
#endif
