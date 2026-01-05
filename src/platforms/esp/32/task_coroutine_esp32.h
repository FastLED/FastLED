/// @file task_coroutine_esp32.h
/// @brief ESP32 FreeRTOS TaskCoroutine implementation
///
/// This file provides the ESP32-specific implementation of ITaskCoroutine.

#pragma once

#include "platforms/itask_coroutine.h"
#include "fl/stl/string.h"
#include "fl/stl/functional.h"

namespace fl {
namespace platforms {

//=============================================================================
// TaskCoroutineESP32 - FreeRTOS-based implementation
//=============================================================================

class TaskCoroutineESP32 : public ITaskCoroutine {
public:
    TaskCoroutineESP32(fl::string name,
                       TaskFunction function,
                       size_t stack_size,
                       uint8_t priority);
    ~TaskCoroutineESP32() override;

    void stop() override;
    bool isRunning() const override;

private:
    void* mHandle;
    fl::string mName;
    TaskFunction mFunction;
};

} // namespace platforms
} // namespace fl
