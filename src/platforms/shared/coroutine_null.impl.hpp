// IWYU pragma: private

/// @file coroutine_null.impl.hpp
/// @brief Null (no-op) coroutine implementations for platforms without OS support

#include "platforms/coroutine.h"
#include "fl/stl/noexcept.h"

namespace fl {
namespace platforms {

//=============================================================================
// TaskCoroutineNull - No-op implementation
//=============================================================================

/// @brief No-op task coroutine for platforms without OS/RTOS support.
/// All operations are no-ops — tasks are never created or executed.
class TaskCoroutineNull : public ICoroutineTask {
public:
    TaskCoroutineNull(fl::string name, TaskFunction function,
                      size_t /*stack_size*/, u8 /*priority*/)
        : mName(fl::move(name)), mFunction(fl::move(function)) {}

    ~TaskCoroutineNull() override = default;
    void stop() override {}
    bool isRunning() const override { return false; }

private:
    fl::string mName;
    TaskFunction mFunction;
};

//=============================================================================
// Factory function - creates platform-specific implementation
//=============================================================================

TaskCoroutinePtr createTaskCoroutine(fl::string name,
                                      ICoroutineTask::TaskFunction function,
                                      size_t stack_size,
                                      u8 priority,
                                      int /*core_id*/) FL_NOEXCEPT {
    return TaskCoroutinePtr(
        new TaskCoroutineNull(fl::move(name), fl::move(function), stack_size, priority));  // ok bare allocation
}

//=============================================================================
// Static exitCurrent implementation
//=============================================================================

void ICoroutineTask::exitCurrent() FL_NOEXCEPT {
    // No-op: No task exists on platforms without OS support
}

} // namespace platforms
} // namespace fl
