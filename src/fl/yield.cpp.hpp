/// @file fl/yield.cpp.hpp
/// @brief Implementation of fl::yield()

#include "fl/yield.h"
#include "fl/sketch_macros.h"
#include "fl/stl/thread.h"

#if SKETCH_HAS_LOTS_OF_MEMORY
#include "fl/async.h"
#endif

#ifdef FL_IS_ESP32
// IWYU pragma: begin_keep
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
// IWYU pragma: end_keep
#endif

namespace fl {

void yield() {
#if SKETCH_HAS_LOTS_OF_MEMORY
    // Pump the scheduler so registered fl::task instances make progress
    // during yield. Reentrancy guard prevents infinite recursion when
    // a task callback calls yield().
    static bool s_in_yield = false;
    if (!s_in_yield) {
        s_in_yield = true;
        Scheduler::instance().update();
        s_in_yield = false;
    }
#endif

    // OS/RTOS yield
#ifdef FL_IS_ESP32
    // Yield to FreeRTOS scheduler - allows WiFi, BT, and other
    // system tasks to run. vTaskDelay(0) yields to any
    // equal-or-higher priority task that is ready.
    // Safe to call from any FreeRTOS task, not just main.
    vTaskDelay(0);
#elif FASTLED_MULTITHREADED
    // On multithreaded host/stub platforms, yield the OS thread.
    // Use std::this_thread::yield() directly (not fl::this_thread::yield())
    // because fl::this_thread::yield() calls fl::yield() on the main thread.
    std::this_thread::yield();  // okay std namespace
#endif
    // Single-threaded non-RTOS platforms: no-op
}

} // namespace fl
