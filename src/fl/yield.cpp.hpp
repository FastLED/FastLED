/// @file fl/yield.cpp.hpp
/// @brief Implementation of fl::yield()

#include "fl/yield.h"
#include "fl/stl/thread.h"

#ifdef FL_IS_ESP32
// IWYU pragma: begin_keep
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
// IWYU pragma: end_keep
#endif

namespace fl {

void yield() {
#ifdef FL_IS_ESP32
    // Yield to FreeRTOS scheduler - allows WiFi, BT, and other
    // system tasks to run. vTaskDelay(0) yields to any
    // equal-or-higher priority task that is ready.
    // Safe to call from any FreeRTOS task, not just main.
    vTaskDelay(0);
#elif FASTLED_MULTITHREADED
    // On multithreaded host/stub platforms, yield the OS thread.
    fl::this_thread::yield();
#endif
    // Single-threaded non-RTOS platforms: no-op
}

} // namespace fl
