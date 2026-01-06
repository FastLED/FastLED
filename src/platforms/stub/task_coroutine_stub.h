/// @file task_coroutine_stub.h
/// @brief Host-platform TaskCoroutine interface using std::thread
///
/// This provides REAL coroutine execution on host platforms for unit testing.
/// Uses std::thread instead of FreeRTOS tasks.
///
/// **Embedded Behavior**: Like embedded systems (ESP32/Arduino), coroutine threads
/// are "daemon" threads that don't block process exit. Threads are detached but
/// contexts are tracked via shared_ptr for optional cleanup.
///
/// ## Design Pattern
///
/// Follows the single-dispatch interface pattern:
/// - TaskCoroutineStub: Abstract interface (pure virtual methods)
/// - TaskCoroutineStubImpl: Concrete implementation (in .cpp file)
/// - createTaskCoroutine(): Factory function for instantiation
///
/// ## Usage
///
/// ```cpp
/// // Create via factory function
/// auto* task = fl::platforms::createTaskCoroutine(
///     "MyTask",
///     []() { /* task code */ },
///     4096,  // stack_size (ignored on host)
///     1      // priority (ignored on host)
/// );
///
/// // Stop and cleanup
/// task->stop();
/// delete task;
/// ```

#pragma once

#include "platforms/itask_coroutine.h"

namespace fl {
namespace platforms {

//=============================================================================
// TaskCoroutineStub - Abstract interface for stub platform
//=============================================================================

/// @brief Host-platform task coroutine interface using std::thread
///
/// This is an abstract interface - use create() factory method or createTaskCoroutine() to instantiate.
///
/// The stub implementation uses std::thread for coroutine execution, providing
/// real concurrency for host-based unit tests without requiring FreeRTOS.
class TaskCoroutineStub : public ITaskCoroutine {
public:
    //=========================================================================
    // Factory Method
    //=========================================================================

    /// @brief Create a new task coroutine instance
    /// @param name Task name (for debugging)
    /// @param function Task function to execute
    /// @param stack_size Stack size in bytes (ignored on host)
    /// @param priority Task priority (ignored on host)
    /// @return Pointer to new TaskCoroutineStub instance (caller owns)
    ///
    /// Creates a new std::thread-based task coroutine. The task starts
    /// immediately and runs until completion or stop() is called.
    static TaskCoroutineStub* create(fl::string name,
                                      TaskFunction function,
                                      size_t stack_size = 4096,
                                      uint8_t priority = 5);

    //=========================================================================
    // Lifecycle
    //=========================================================================

    ~TaskCoroutineStub() override = default;

    //=========================================================================
    // ITaskCoroutine Interface Implementation
    //=========================================================================

    void stop() override = 0;
    bool isRunning() const override = 0;
};

//=============================================================================
// Global cleanup function for DLL mode
//=============================================================================

/// @brief Clean up all coroutine threads before DLL unload
///
/// This function must be called before the DLL unloads to ensure all
/// coroutine threads are properly joined. Otherwise, detached threads
/// will continue executing code from the unloaded DLL, causing access violations.
///
/// This is only necessary in DLL mode - in normal builds, threads can
/// safely continue running as daemon threads.
void cleanup_coroutine_threads();

} // namespace platforms
} // namespace fl
