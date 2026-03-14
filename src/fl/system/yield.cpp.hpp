/// @file fl/yield.cpp.hpp
/// @brief Implementation of fl::yield()

#include "fl/system/yield.h"
#include "fl/system/sketch_macros.h"
#include "fl/stl/thread.h"

#if SKETCH_HAS_LOTS_OF_MEMORY
#include "fl/stl/async.h"
#endif

#ifdef FL_IS_ESP32
// IWYU pragma: begin_keep
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
// IWYU pragma: end_keep
#endif

namespace fl {

static void sys_yield() {
    // Pure OS-level yield — no FastLED subsystem pumping.
#ifdef FL_IS_ESP32
    vTaskDelay(0);
#elif FASTLED_MULTITHREADED
    std::this_thread::yield();  // okay std namespace
#endif
    // Single-threaded non-RTOS platforms: no-op
}

void yield() {
    sys_yield();
}

} // namespace fl
