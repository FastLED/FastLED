/// @file itask_coroutine.h
/// @brief Interface for platform-specific task coroutine implementations
///
/// This header defines the ITaskCoroutine interface that all platform-specific
/// task implementations must implement.

#pragma once

#include "fl/stl/string.h"
#include "fl/stl/function.h"

namespace fl {
namespace platforms {

//=============================================================================
// ITaskCoroutine - Interface for platform-specific task implementations
//=============================================================================

class ITaskCoroutine {
public:
    using TaskFunction = fl::function<void()>;

    virtual ~ITaskCoroutine() = default;
    virtual void stop() = 0;
    virtual bool isRunning() const = 0;
    static void exitCurrent();  // Platform-specific static method
};

// Factory function provided by each platform implementation
ITaskCoroutine* createTaskCoroutine(fl::string name,
                                     ITaskCoroutine::TaskFunction function,
                                     size_t stack_size,
                                     uint8_t priority);

} // namespace platforms
} // namespace fl
