#pragma once

/// @file fl/yield.h
/// @brief Platform-aware cooperative yield for FastLED
///
/// fl::yield() gives other tasks/threads a chance to run:
/// - ESP32: calls vTaskDelay(0) to yield to FreeRTOS scheduler
/// - Multithreaded hosts: calls std::this_thread::yield()
/// - Single-threaded platforms: no-op
///
/// Safe to call from any thread/task.

namespace fl {

/// @brief Yield to the platform scheduler
///
/// On ESP32 (FreeRTOS), this calls vTaskDelay(0) which yields to
/// any equal-or-higher priority tasks waiting to run. This is
/// important for cooperative multitasking on ESP32 where WiFi,
/// Bluetooth, and other system tasks need CPU time.
///
/// On multithreaded host platforms, this calls std::this_thread::yield().
///
/// On single-threaded non-RTOS platforms, this is a no-op.
///
/// Safe to call from any thread or FreeRTOS task.
void yield();

} // namespace fl
