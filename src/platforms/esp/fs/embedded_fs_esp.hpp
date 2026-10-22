#pragma once

// IWYU pragma: private
// Keep this fragment in the isolated embedded-storage translation unit.
#include "platforms/is_platform.h"
#include "fl/stl/has_include.h"

#if defined(FL_IS_ESP32) && FL_HAS_INCLUDE("esp_littlefs.h")
#include "platforms/esp/fs/embedded_fs_native.hpp"
#elif defined(FL_IS_ESP8266)
// Retain the existing ESP8266 opt-in and Arduino filesystem implementation.
#include "platforms/arduino/fs/embedded_fs_arduino.hpp"
#else
#include "platforms/shared/embedded_fs_noop.hpp"
#endif

#if defined(FL_IS_ESP8266)
namespace fl {
namespace platforms {
inline FsImplPtr makeEmbeddedFs(bool format) FL_NO_EXCEPT {
    return makeArduinoEmbeddedFs(format);
}
}
}
#endif
