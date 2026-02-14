// IWYU pragma: private

/// @file platforms/esp/memory_esp8266.hpp
/// ESP8266 memory statistics implementation

#include "fl/stl/stdint.h"
#include "platforms/is_platform.h"

namespace fl {
namespace platforms {

/// @brief Get free heap memory (ESP8266 implementation)
/// @return Number of free bytes in heap
inline size_t getFreeHeap() {
#if defined(FL_IS_ESP8266)
    // ESP8266 SDK function (declared in user_interface.h, included by Arduino core)
    extern "C" u32 system_get_free_heap_size(void);
    return system_get_free_heap_size();
#else
    return 0;
#endif
}

/// @brief Get total heap size (ESP8266 implementation)
/// @return Total heap size (not available on ESP8266)
inline size_t getHeapSize() {
    // Not available on ESP8266
    return 0;
}

/// @brief Get minimum free heap (ESP8266 implementation)
/// @return Minimum free heap (not available on ESP8266)
inline size_t getMinFreeHeap() {
    // Not available on ESP8266
    return 0;
}

} // namespace platforms
} // namespace fl
