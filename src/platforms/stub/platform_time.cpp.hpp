#pragma once

// Only compile for stub platform (exclude WASM which has its own implementation)
#if defined(FASTLED_STUB_IMPL) && !defined(__EMSCRIPTEN__)

#include "platforms/time_platform.h"
#include "fl/stl/thread.h"
#include "fl/compiler_control.h"
#include "fl/stl/function.h"
#include <chrono>  // okay banned header (platform-specific time implementation)

// Forward declare delay override (defined in time_stub.cpp.hpp)
extern fl::function<void(uint32_t)> g_delay_override;

namespace {
    // Shared start time for millis/micros consistency
    FL_DISABLE_WARNING_PUSH
    FL_DISABLE_WARNING_GLOBAL_CONSTRUCTORS
    static const auto start_time = std::chrono::system_clock::now();  // okay std namespace
    FL_DISABLE_WARNING_POP
}

namespace fl {
namespace platforms {

void delay(fl::u32 ms) {
    // Check for test override first (for fast testing)
    if (g_delay_override) {
        g_delay_override(ms);
        return;
    }
    fl::this_thread::sleep_for(fl::chrono::milliseconds(ms));
}

void delayMicroseconds(fl::u32 us) {
    // No override for microseconds (precise hardware timing)
    fl::this_thread::sleep_for(fl::chrono::microseconds(us));
}

fl::u32 millis() {
    auto current = std::chrono::system_clock::now();  // okay std namespace
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(  // okay std namespace
        current - start_time);
    return static_cast<fl::u32>(elapsed.count());
}

fl::u32 micros() {
    auto current = std::chrono::system_clock::now();  // okay std namespace
    auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(  // okay std namespace
        current - start_time);
    return static_cast<fl::u32>(elapsed.count());
}

}  // namespace platforms
}  // namespace fl

#endif  // FASTLED_STUB_IMPL && !__EMSCRIPTEN__
