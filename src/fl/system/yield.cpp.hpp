/// @file fl/yield.cpp.hpp
/// @brief Implementation of fl::yield()

#include "fl/system/yield.h"
#include "fl/system/sketch_macros.h"
#include "fl/stl/thread.h"

#if SKETCH_HAS_LARGE_MEMORY
#include "fl/task/executor.h"
#endif

#ifdef FL_IS_ESP32
// IWYU pragma: begin_keep
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "fl/stl/noexcept.h"
// IWYU pragma: end_keep
#endif

namespace fl {

static void sys_yield() FL_NO_EXCEPT {
    // Pure OS-level yield — no FastLED subsystem pumping.
#ifdef FL_IS_ESP32
    vTaskDelay(0);
#elif FASTLED_MULTITHREADED
    std::this_thread::yield();  // okay std namespace
#endif
    // Single-threaded non-RTOS platforms: no-op
}

void yield() FL_NO_EXCEPT {
    sys_yield();
}

} // namespace fl
