// IWYU pragma: private

/// @file task_coroutine_null.cpp
/// @brief Concrete no-op implementation of TaskCoroutineNull

// Only compile this file when not using stub, ESP32, or Teensy implementation
#include "platforms/is_platform.h"
#include "platforms/arm/teensy/is_teensy.h"
#if !defined(FASTLED_STUB_IMPL) && !defined(FL_IS_ESP32) && !defined(FL_IS_TEENSY_4X)

#include "platforms/itask_coroutine.h"

namespace fl {
namespace platforms {

//=============================================================================
// TaskCoroutineNullImpl - Concrete no-op implementation
//=============================================================================

/// @brief Concrete no-op implementation of TaskCoroutineNull
///
/// This class provides no-op stubs for platforms without OS/RTOS support.
/// All operations are no-ops - tasks are never created or executed.
class TaskCoroutineNullImpl : public TaskCoroutineNull {
public:
    TaskCoroutineNullImpl(fl::string name,
                          TaskFunction function,
                          size_t /*stack_size*/,
                          u8 /*priority*/)
        : mName(fl::move(name))
        , mFunction(fl::move(function)) {
        // No OS support - task is not created
    }

    ~TaskCoroutineNullImpl() override {
        // No cleanup needed (no task was created)
    }

    void stop() override {
        // No-op: No task to stop
    }

    bool isRunning() const override {
        return false;  // Never running on platforms without OS support
    }

private:
    fl::string mName;
    TaskFunction mFunction;
};

//=============================================================================
// Static factory method
//=============================================================================

TaskCoroutinePtr TaskCoroutineNull::create(fl::string name,
                                            TaskFunction function,
                                            size_t stack_size,
                                            u8 priority) {
    return TaskCoroutinePtr(
        new TaskCoroutineNullImpl(fl::move(name), fl::move(function), stack_size, priority));  // ok bare allocation
}

//=============================================================================
// Factory function - creates platform-specific implementation
//=============================================================================

TaskCoroutinePtr createTaskCoroutine(fl::string name,
                                      ITaskCoroutine::TaskFunction function,
                                      size_t stack_size,
                                      u8 priority,
                                      int /*core_id*/) {
    return TaskCoroutineNull::create(fl::move(name), fl::move(function), stack_size, priority);
}

//=============================================================================
// Static exitCurrent implementation
//=============================================================================

void ITaskCoroutine::exitCurrent() {
    // No-op: No task exists on platforms without OS support
}

} // namespace platforms
} // namespace fl

#endif // !defined(FASTLED_STUB_IMPL) && !defined(FL_IS_ESP32) && !defined(FL_IS_TEENSY_4X)
