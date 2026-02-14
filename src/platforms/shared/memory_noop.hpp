// IWYU pragma: private

/// @file platforms/shared/memory_noop.hpp
/// No-op memory statistics implementation for platforms without heap tracking

#include "fl/stl/stdint.h"

namespace fl {
namespace platforms {

/// @brief Get free heap memory (no-op implementation)
/// @return Always returns 0 (heap tracking not available)
inline size_t getFreeHeap() {
    return 0;
}

/// @brief Get total heap size (no-op implementation)
/// @return Always returns 0 (heap size not available)
inline size_t getHeapSize() {
    return 0;
}

/// @brief Get minimum free heap (no-op implementation)
/// @return Always returns 0 (heap tracking not available)
inline size_t getMinFreeHeap() {
    return 0;
}

} // namespace platforms
} // namespace fl
