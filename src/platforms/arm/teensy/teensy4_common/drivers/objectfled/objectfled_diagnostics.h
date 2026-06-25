#pragma once

// IWYU pragma: private

#include "platforms/arm/teensy/is_teensy.h"

#include "fl/stl/json.h"
#include "fl/stl/noexcept.h"
#include "fl/stl/stdint.h"

namespace fl {

#if defined(FL_IS_TEENSY_4X) && defined(FASTLED_OBJECTFLED_DIAGNOSTICS) && FASTLED_OBJECTFLED_DIAGNOSTICS

void objectFledDiagnosticsReset() FL_NO_EXCEPT;
void objectFledDiagnosticsRecord(const char* stage, const u8* pins = nullptr,
                                 u32 pin_count = 0) FL_NO_EXCEPT;
fl::json objectFledDiagnosticsToJson() FL_NO_EXCEPT;

#else

inline void objectFledDiagnosticsReset() FL_NO_EXCEPT {}

inline void objectFledDiagnosticsRecord(const char*, const u8* = nullptr,
                                        u32 = 0) FL_NO_EXCEPT {}

inline fl::json objectFledDiagnosticsToJson() FL_NO_EXCEPT {
    fl::json result = fl::json::object();
    result.set("enabled", false);
    return result;
}

#endif

} // namespace fl
