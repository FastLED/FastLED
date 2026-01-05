/// @file task_coroutine_null.h
/// @brief Null TaskCoroutine interface for platforms without OS support
///
/// This provides a no-op implementation of ITaskCoroutine for platforms
/// that don't have OS/RTOS support (e.g., bare-metal AVR, STM32 without FreeRTOS).
///
/// ## Design Pattern
///
/// Follows the single-dispatch interface pattern:
/// - TaskCoroutineNull: Abstract interface (pure virtual methods)
/// - TaskCoroutineNullImpl: Concrete no-op implementation (in .cpp file)
/// - createTaskCoroutine(): Factory function for instantiation
///
/// ## Usage
///
/// ```cpp
/// // Create via factory function (task won't actually run on null platform)
/// auto* task = fl::platforms::createTaskCoroutine(
///     "MyTask",
///     []() { /* task code - won't execute */ },
///     4096,  // stack_size (ignored)
///     1      // priority (ignored)
/// );
///
/// // Stop and cleanup
/// task->stop();  // No-op
/// delete task;
/// ```

#pragma once

#include "platforms/itask_coroutine.h"

namespace fl {
namespace platforms {

//=============================================================================
// TaskCoroutineNull - Abstract interface for null platform
//=============================================================================

/// @brief Null task coroutine interface for platforms without OS support
///
/// This is an abstract interface - use create() factory method or createTaskCoroutine() to instantiate.
///
/// The null implementation provides no-op stubs for all methods, allowing
/// code to compile on platforms without OS/RTOS support. Tasks are never
/// actually created or executed.
class TaskCoroutineNull : public ITaskCoroutine {
public:
    //=========================================================================
    // Factory Method
    //=========================================================================

    /// @brief Create a new null task coroutine instance
    /// @param name Task name (ignored)
    /// @param function Task function (ignored, never executed)
    /// @param stack_size Stack size in bytes (ignored)
    /// @param priority Task priority (ignored)
    /// @return Pointer to new TaskCoroutineNull instance (caller owns)
    ///
    /// Creates a no-op task coroutine. The task is never actually created
    /// or executed - all operations are no-ops.
    static TaskCoroutineNull* create(fl::string name,
                                      TaskFunction function,
                                      size_t stack_size = 4096,
                                      uint8_t priority = 5);

    //=========================================================================
    // Lifecycle
    //=========================================================================

    ~TaskCoroutineNull() override = default;

    //=========================================================================
    // ITaskCoroutine Interface Implementation
    //=========================================================================

    void stop() override = 0;
    bool isRunning() const override = 0;
};

} // namespace platforms
} // namespace fl
