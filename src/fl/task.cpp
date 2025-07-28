#include "fl/task.h"
#include "fl/async.h"
#include "fl/time.h"

namespace fl {

namespace {
// Helper to generate a trace label from a TracePoint
fl::unique_ptr<fl::string> make_trace_label(const fl::TracePoint& trace) {
    // Basic implementation: "file:line"
    // More advanced version could strip common path prefixes
    return fl::make_unique<fl::string>(fl::string(fl::get<0>(trace)) + ":" + fl::to_string(fl::get<1>(trace)));
}
} // namespace

task::task(TaskType type, int interval_ms)
    : mType(type),
      mIntervalMs(interval_ms),
      mCanceled(false),
      mTraceLabel(nullptr),
      mHasThen(false),
      mHasCatch(false),
      mLastRunTime(UINT32_MAX) {  // Use UINT32_MAX to indicate "never run"
}

task::task(TaskType type, int interval_ms, const fl::TracePoint& trace)
    : mType(type),
      mIntervalMs(interval_ms),
      mCanceled(false),
      mTraceLabel(make_trace_label(trace)),
      mHasThen(false),
      mHasCatch(false),
      mLastRunTime(UINT32_MAX) {  // Use UINT32_MAX to indicate "never run"
}

task::task(TaskType type, int interval_ms, fl::unique_ptr<fl::string> trace_label)
    : mType(type),
      mIntervalMs(interval_ms),
      mCanceled(false),
      mTraceLabel(fl::move(trace_label)),
      mHasThen(false),
      mHasCatch(false),
      mLastRunTime(UINT32_MAX) {  // Use UINT32_MAX to indicate "never run"
}

fl::unique_ptr<task> task::every_ms(int interval_ms) {
    return fl::make_unique<task>(TaskType::kEveryMs, interval_ms);
}

fl::unique_ptr<task> task::every_ms(int interval_ms, const fl::TracePoint& trace) {
    return fl::make_unique<task>(TaskType::kEveryMs, interval_ms, trace);
}

fl::unique_ptr<task> task::at_framerate(int fps) {
    return fl::make_unique<task>(TaskType::kAtFramerate, 1000 / fps);
}

fl::unique_ptr<task> task::at_framerate(int fps, const fl::TracePoint& trace) {
    return fl::make_unique<task>(TaskType::kAtFramerate, 1000 / fps, trace);
}

fl::unique_ptr<task> task::before_frame() {
    return fl::make_unique<task>(TaskType::kBeforeFrame, 0);
}

fl::unique_ptr<task> task::before_frame(const fl::TracePoint& trace) {
    return fl::make_unique<task>(TaskType::kBeforeFrame, 0, trace);
}

fl::unique_ptr<task> task::after_frame() {
    return fl::make_unique<task>(TaskType::kAfterFrame, 0);
}

fl::unique_ptr<task> task::after_frame(const fl::TracePoint& trace) {
    return fl::make_unique<task>(TaskType::kAfterFrame, 0, trace);
}

task& task::then(fl::function<void()> on_then) & {
    mThenCallback = fl::move(on_then);
    mHasThen = true;
    return *this;
}

task&& task::then(fl::function<void()> on_then) && {
    return fl::move(then(fl::move(on_then)));
}

task& task::catch_(fl::function<void(const Error&)> on_catch) & {
    mCatchCallback = fl::move(on_catch);
    mHasCatch = true;
    return *this;
}

task&& task::catch_(fl::function<void(const Error&)> on_catch) && {
    return fl::move(catch_(fl::move(on_catch)));
}

task& task::cancel() & {
    mCanceled = true;
    return *this;
}

task&& task::cancel() && {
    return fl::move(cancel());
}

bool task::ready_to_run(uint32_t current_time) const {
    // For frame-based tasks, they're always ready
    if (mType == TaskType::kBeforeFrame || mType == TaskType::kAfterFrame) {
        return true;
    }
    
    // For time-based tasks, check if enough time has passed
    if (mIntervalMs <= 0) {
        return true;
    }
    
    // Use UINT32_MAX to indicate "never run" instead of 0 to handle cases where time() returns 0
    if (mLastRunTime == UINT32_MAX) {
        return true;
    }
    
    // Check if enough time has passed since last run
    return (current_time - mLastRunTime) >= static_cast<uint32_t>(mIntervalMs);
}

} // namespace fl
