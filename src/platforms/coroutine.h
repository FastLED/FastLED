// ok no namespace fl
#pragma once

/// @file platforms/coroutine.h
/// @brief Coroutine system interfaces — all declarations, no implementations
///
/// This is the single entry point for coroutine interfaces.
/// Consumers include this header; implementations are in coroutine.impl.cpp.hpp.

#include "fl/stl/string.h"
#include "fl/stl/function.h"
#include "fl/stl/unique_ptr.h"

#include "platforms/coroutine_runtime.h"

namespace fl {
namespace platforms {

//=============================================================================
// ICoroutineTask - Interface for platform-specific task implementations
//=============================================================================

class ICoroutineTask {
public:
    using TaskFunction = fl::function<void()>;

    virtual ~ICoroutineTask() = default;
    virtual void stop() = 0;
    virtual bool isRunning() const = 0;
    static void exitCurrent();  // Platform-specific static method
};

/// @brief Owning smart pointer for task coroutines
using TaskCoroutinePtr = fl::unique_ptr<ICoroutineTask>;

// Factory function provided by each platform implementation
TaskCoroutinePtr createTaskCoroutine(fl::string name,
                                      ICoroutineTask::TaskFunction function,
                                      size_t stack_size,
                                      u8 priority,
                                      int core_id = -1);

} // namespace platforms
} // namespace fl
