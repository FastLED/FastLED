/// @file itask_coroutine.h
/// @brief Interface for platform-specific task coroutine implementations
///
/// This header defines the ITaskCoroutine interface that all platform-specific
/// task implementations must implement.

#pragma once

#include "fl/stl/string.h"
#include "fl/stl/function.h"
#include "fl/stl/unique_ptr.h"

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

/// @brief Owning smart pointer for task coroutines
using TaskCoroutinePtr = fl::unique_ptr<ITaskCoroutine>;

// Factory function provided by each platform implementation
TaskCoroutinePtr createTaskCoroutine(fl::string name,
                                      ITaskCoroutine::TaskFunction function,
                                      size_t stack_size,
                                      u8 priority,
                                      int core_id = -1);

//=============================================================================
// TaskCoroutineNull - No-op implementation for platforms without OS support
//=============================================================================

/// @brief Null task coroutine interface for platforms without OS support
///
/// Provides no-op stubs for all methods, allowing code to compile on
/// platforms without OS/RTOS support. Tasks are never created or executed.
class TaskCoroutineNull : public ITaskCoroutine {
public:
    static TaskCoroutinePtr create(fl::string name,
                                    TaskFunction function,
                                    size_t stack_size = 4096,
                                    u8 priority = 5);

    ~TaskCoroutineNull() override = default;

    void stop() override = 0;
    bool isRunning() const override = 0;
};

} // namespace platforms
} // namespace fl
