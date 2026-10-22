#pragma once

// IWYU pragma: private

#include "fl/stl/compiler_control.h"
#include "fl/stl/cstddef.h"
#include "fl/stl/noexcept.h"
#include "fl/stl/stdint.h"
#include "platforms/esp/is_esp.h"

#ifdef FL_IS_ESP32
// Optional application-provided C bridge. A weak reference does not extract
// an Arduino adapter from an archive. Return size_t(-1) to use native output;
// otherwise return the number of accepted bytes. The provider owns its stream
// and must preserve complete-buffer writes. No C++ stream ABI is assumed.
extern "C" fl::size fastled_esp_console_write(const fl::u8* bytes,
                                              fl::size size) FL_NO_EXCEPT FL_LINK_WEAK;
#endif

namespace fl {
namespace detail {
inline fl::size tryConsoleWrite(const fl::u8* bytes, fl::size size) FL_NO_EXCEPT {
#ifdef FL_IS_ESP32
    if (fastled_esp_console_write) {
        return fastled_esp_console_write(bytes, size);
    }
#else
    (void)bytes;
    (void)size;
#endif
    return static_cast<fl::size>(-1);
}
} // namespace detail
} // namespace fl
