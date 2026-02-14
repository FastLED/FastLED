#pragma once

#include "fl/int.h"

/// @file memory.h
/// Platform-abstracted memory query functions
///
/// This header provides cross-platform memory introspection capabilities.
/// Each platform implements its own version of these functions based on
/// available system APIs.

namespace fl {

/// @brief Heap memory information
/// @details Provides separate tracking for SRAM and PSRAM (if available)
struct HeapInfo {
    fl::size free_sram;   ///< Free SRAM in bytes (internal fast memory)
    fl::size free_psram;  ///< Free PSRAM in bytes (external slower memory, 0 if not available)

    /// @brief Total free heap (SRAM + PSRAM)
    fl::size total() const { return free_sram + free_psram; }

    /// @brief Check if PSRAM is available
    bool has_psram() const { return free_psram > 0; }
};

/// @brief Query available heap memory
/// @return HeapInfo struct with free SRAM and PSRAM bytes
///
/// Platform implementations:
/// - ESP32: Reports both SRAM (heap_caps_get_free_size(MALLOC_CAP_INTERNAL))
///          and PSRAM (heap_caps_get_free_size(MALLOC_CAP_SPIRAM)) if available
/// - ESP8266: Reports SRAM only (no PSRAM support)
/// - AVR: Reports available SRAM between heap and stack (no PSRAM)
/// - ARM: Platform-dependent (SRAM only, no PSRAM)
/// - Native/Stub: Returns {0, 0} (heap size not available in standard C++)
/// - WASM: Returns {0, 0} (JavaScript heap is managed externally)
///
/// Usage:
/// @code
/// fl::HeapInfo heap = fl::getFreeHeap();
/// if (heap.free_sram < 1024) {
///     // Low SRAM warning
/// }
/// if (heap.has_psram()) {
///     // PSRAM is available
/// }
/// @endcode
HeapInfo getFreeHeap();

} // namespace fl
