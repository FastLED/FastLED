/// @file task_coroutine_freertos.h
/// @brief ESP32 FreeRTOS TaskCoroutine implementation (inline methods)
///
/// This file provides inline implementations of TaskCoroutine methods for ESP32.
/// It's included from fl/task.h AFTER the TaskCoroutine class is declared.

#pragma once

// Note: This file is included from fl/task.h, so TaskCoroutine is already declared
// No need to include fl/task.h (would create circular dependency)

namespace fl {

//=============================================================================
// TaskCoroutine Implementation (ESP32/FreeRTOS)
//=============================================================================

inline TaskCoroutine::TaskCoroutine(fl::string name,
                                     TaskFunction function,
                                     size_t stack_size,
                                     uint8_t priority)
    : mHandle(nullptr)
    , mName(fl::move(name))
    , mFunction(fl::move(function)) {
    mHandle = createTaskImpl(mName, mFunction, stack_size, priority);
}

inline TaskCoroutine::~TaskCoroutine() {
    stop();
}

inline void TaskCoroutine::stop() {
    if (mHandle) {
        deleteTaskImpl(mHandle);
        mHandle = nullptr;
    }
}

inline void TaskCoroutine::exitCurrent() {
    exitCurrentImpl();
}

} // namespace fl
