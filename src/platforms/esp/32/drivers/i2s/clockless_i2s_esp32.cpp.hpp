// Thread-local storage implementation for ClocklessI2S RGBW conversion buffer

#ifdef ESP32

#include "fl/thread_local.h"
#include "fl/stl/vector.h"

namespace fl {

/// @brief Get thread-local RGBW conversion scratchpad
/// @details Returns a reference to a thread-local vector that persists across frames.
///          The buffer is automatically managed by the caller and remains allocated
///          to avoid reallocation overhead on subsequent frames.
/// @return Reference to thread-local uint8_t vector for RGBW-to-RGB conversion
fl::vector<uint8_t>& get_rgbw_scratchpad() {
    static ThreadLocal<fl::vector<uint8_t>> tls_buffer;
    return tls_buffer.access();
}

} // namespace fl

#endif // ifdef ESP32
