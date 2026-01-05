/// @file task_coroutine_null.h
/// @brief Null TaskCoroutine implementation for platforms without OS support
///
/// This file provides inline no-op implementations of TaskCoroutine methods
/// for platforms that don't have OS/RTOS support.

#pragma once

// Note: This file is included from fl/task.h, so TaskCoroutine is already declared
// No need to include fl/task.h (would create circular dependency)

namespace fl {

//=============================================================================
// TaskCoroutine Implementation (Null/No-Op)
//=============================================================================

inline TaskCoroutine::TaskCoroutine(fl::string name,
                                     TaskFunction function,
                                     size_t /*stack_size*/,
                                     uint8_t /*priority*/)
    : mHandle(nullptr)
    , mName(fl::move(name))
    , mFunction(fl::move(function)) {
    // No OS support - task is not created
}

inline TaskCoroutine::~TaskCoroutine() {
    // No cleanup needed (no task was created)
}

inline void TaskCoroutine::stop() {
    // No-op: No task to stop
    mHandle = nullptr;
}

inline void TaskCoroutine::exitCurrent() {
    // No-op: No task exists on platforms without OS support
}

inline TaskCoroutine::Handle TaskCoroutine::createTaskImpl(const fl::string& /*name*/,
                                                            const TaskFunction& /*function*/,
                                                            size_t /*stack_size*/,
                                                            uint8_t /*priority*/) {
    // No OS support - return nullptr
    return nullptr;
}

inline void TaskCoroutine::deleteTaskImpl(Handle /*handle*/) {
    // No-op: No tasks to delete
}

inline void TaskCoroutine::exitCurrentImpl() {
    // No-op: No tasks exist
}

} // namespace fl
