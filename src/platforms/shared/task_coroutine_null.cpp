/// @file task_coroutine_null.cpp
/// @brief Concrete no-op implementation of TaskCoroutineNull

// Only compile this file when not using stub or ESP32 implementation
#if !defined(FASTLED_STUB_IMPL) && !defined(ESP32)

#include "task_coroutine_null.h"

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
                          uint8_t /*priority*/)
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

TaskCoroutineNull* TaskCoroutineNull::create(fl::string name,
                                              TaskFunction function,
                                              size_t stack_size,
                                              uint8_t priority) {
    return new TaskCoroutineNullImpl(fl::move(name), fl::move(function), stack_size, priority);
}

//=============================================================================
// Factory function - creates platform-specific implementation
//=============================================================================

ITaskCoroutine* createTaskCoroutine(fl::string name,
                                     ITaskCoroutine::TaskFunction function,
                                     size_t stack_size,
                                     uint8_t priority) {
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

#endif // !defined(FASTLED_STUB_IMPL) && !defined(ESP32)
