#pragma once

#include "fl/stl/noexcept.h"

// IWYU pragma: private

namespace fl {
namespace task {
namespace detail {

// Publish only after a task subsystem is initialized. An unused subsystem
// does not become a link dependency of ordinary task::run()/FastLED.delay().
using TaskPump = void (*)();
void set_scheduler_pump(TaskPump pump) FL_NO_EXCEPT;
void set_executor_pump(TaskPump pump) FL_NO_EXCEPT;

} // namespace detail
} // namespace task
} // namespace fl
