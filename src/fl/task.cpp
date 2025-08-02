#include "fl/task.h"
#include "fl/async.h"
#include "fl/time.h"
#include "fl/sstream.h"

namespace fl {

namespace {
// Helper to generate a trace label from a TracePoint
fl::string make_trace_label(const fl::TracePoint& trace) {
    // Basic implementation: "file:line"
    // More advanced version could strip common path prefixes
    fl::sstream ss;
    ss << fl::get<0>(trace) << ":" << fl::get<1>(trace);
    return ss.str();
}
} // namespace

// TaskImpl implementation
TaskImpl::TaskImpl(TaskType type, int interval_ms)
    : mType(type),
      mIntervalMs(interval_ms),
      mCanceled(false),
      mTraceLabel(),
      mHasThen(false),
      mHasCatch(false),
      mLastRunTime(UINT32_MAX) {  // Use UINT32_MAX to indicate "never run"
}

TaskImpl::TaskImpl(TaskType type, int interval_ms, const fl::TracePoint& trace)
    : mType(type),
      mIntervalMs(interval_ms),
      mCanceled(false),
      mTraceLabel(fl::make_unique<string>(make_trace_label(trace))),  // Optional. Put it in the heap.
      mHasThen(false),
      mHasCatch(false),
      mLastRunTime(UINT32_MAX) {  // Use UINT32_MAX to indicate "never run"
}

// TaskImpl static builders
fl::shared_ptr<TaskImpl> TaskImpl::create_every_ms(int interval_ms) {
    return fl::make_shared<TaskImpl>(TaskType::kEveryMs, interval_ms);
}

fl::shared_ptr<TaskImpl> TaskImpl::create_every_ms(int interval_ms, const fl::TracePoint& trace) {
    return fl::make_shared<TaskImpl>(TaskType::kEveryMs, interval_ms, trace);
}

fl::shared_ptr<TaskImpl> TaskImpl::create_at_framerate(int fps) {
    return fl::make_shared<TaskImpl>(TaskType::kAtFramerate, 1000 / fps);
}

fl::shared_ptr<TaskImpl> TaskImpl::create_at_framerate(int fps, const fl::TracePoint& trace) {
    return fl::make_shared<TaskImpl>(TaskType::kAtFramerate, 1000 / fps, trace);
}

fl::shared_ptr<TaskImpl> TaskImpl::create_before_frame() {
    return fl::make_shared<TaskImpl>(TaskType::kBeforeFrame, 0);
}

fl::shared_ptr<TaskImpl> TaskImpl::create_before_frame(const fl::TracePoint& trace) {
    return fl::make_shared<TaskImpl>(TaskType::kBeforeFrame, 0, trace);
}

fl::shared_ptr<TaskImpl> TaskImpl::create_after_frame() {
    return fl::make_shared<TaskImpl>(TaskType::kAfterFrame, 0);
}


fl::shared_ptr<TaskImpl> TaskImpl::create_after_frame(const fl::TracePoint& trace) {
    return fl::make_shared<TaskImpl>(TaskType::kAfterFrame, 0, trace);
}

// TaskImpl fluent API
void TaskImpl::set_then(fl::function<void()> on_then) {
    mThenCallback = fl::move(on_then);
    mHasThen = true;
}

void TaskImpl::set_catch(fl::function<void(const Error&)> on_catch) {
    mCatchCallback = fl::move(on_catch);
    mHasCatch = true;
}

void TaskImpl::set_canceled() {
    mCanceled = true;
}

void TaskImpl::auto_register_with_scheduler() {
    // Auto-registration is now handled at the task wrapper level
    // Mark as registered to prevent duplicate registrations
    mAutoRegistered = true;
}

bool TaskImpl::ready_to_run(uint32_t current_time) const {
    // For frame-based tasks, they're only ready when explicitly called in frame context
    // Regular scheduler updates should NOT run frame tasks
    if (mType == TaskType::kBeforeFrame || mType == TaskType::kAfterFrame) {
        return false;  // Changed: frame tasks are not ready during regular updates
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

// New method to check if frame tasks are ready (used only during frame events)
bool TaskImpl::ready_to_run_frame_task(uint32_t current_time) const {
    // Only frame-based tasks are ready in frame context
    FL_UNUSED(current_time);
    if (mType == TaskType::kBeforeFrame || mType == TaskType::kAfterFrame) {
        return true;
    }
    // Non-frame tasks are not run in frame context
    return false;
}

void TaskImpl::execute_then() {
    if (mHasThen && mThenCallback) {
        mThenCallback();
    }
}

void TaskImpl::execute_catch(const Error& error) {
    if (mHasCatch && mCatchCallback) {
        mCatchCallback(error);
    }
}

// task wrapper implementation
task::task(shared_ptr<TaskImpl> impl) : mImpl(fl::move(impl)) {}

// task static builders
task task::every_ms(int interval_ms) {
    return task(TaskImpl::create_every_ms(interval_ms));
}

task task::every_ms(int interval_ms, const fl::TracePoint& trace) {
    return task(TaskImpl::create_every_ms(interval_ms, trace));
}

task task::at_framerate(int fps) {
    return task(TaskImpl::create_at_framerate(fps));
}

task task::at_framerate(int fps, const fl::TracePoint& trace) {
    return task(TaskImpl::create_at_framerate(fps, trace));
}

task task::before_frame() {
    return task(TaskImpl::create_before_frame());
}

task task::before_frame(const fl::TracePoint& trace) {
    return task(TaskImpl::create_before_frame(trace));
}

task task::after_frame() {
    return task(TaskImpl::create_after_frame());
}

task task::after_frame(const fl::TracePoint& trace) {
    return task(TaskImpl::create_after_frame(trace));
}

task task::after_frame(function<void()> on_then) {
    task out = task::after_frame();
    out.then(on_then);
    return out;
}

task task::after_frame(function<void()> on_then, const fl::TracePoint& trace) {
    task out = task::after_frame(trace);
    out.then(on_then);
    return out;
}

// task fluent API
task& task::then(fl::function<void()> on_then) {
    if (mImpl) {
        mImpl->set_then(fl::move(on_then));
        
        // Auto-register with scheduler when callback is set
        if (!mImpl->is_auto_registered()) {
            mImpl->auto_register_with_scheduler();
            fl::Scheduler::instance().add_task(*this);
        }
    }
    return *this;
}

task& task::catch_(fl::function<void(const Error&)> on_catch) {
    if (mImpl) {
        mImpl->set_catch(fl::move(on_catch));
    }
    return *this;
}

task& task::cancel() {
    if (mImpl) {
        mImpl->set_canceled();
    }
    return *this;
}

// task getters
int task::id() const {
    return mImpl ? mImpl->id() : 0;
}

bool task::has_then() const {
    return mImpl ? mImpl->has_then() : false;
}

bool task::has_catch() const {
    return mImpl ? mImpl->has_catch() : false;
}

string task::trace_label() const {
    return mImpl ? mImpl->trace_label() : "";
}

TaskType task::type() const {
    return mImpl ? mImpl->type() : TaskType::kEveryMs;
}

int task::interval_ms() const {
    return mImpl ? mImpl->interval_ms() : 0;
}

uint32_t task::last_run_time() const {
    return mImpl ? mImpl->last_run_time() : 0;
}

void task::set_last_run_time(uint32_t time) {
    if (mImpl) {
        mImpl->set_last_run_time(time);
    }
}

bool task::ready_to_run(uint32_t current_time) const {
    return mImpl ? mImpl->ready_to_run(current_time) : false;
}

} // namespace fl
