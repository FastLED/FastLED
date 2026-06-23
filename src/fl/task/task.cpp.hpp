#include "fl/task/task.h"
#include "fl/stl/limits.h"
#include "fl/task/scheduler.h"
#include "fl/stl/sstream.h"
#include "fl/stl/unique_ptr.h"
#include "fl/stl/atomic.h"
#include "platforms/coroutine.h"
#include "fl/stl/noexcept.h"

namespace fl {
namespace task {

namespace {
// Generate trace label from TracePoint
string make_trace_label(const TracePoint& trace) {
    sstream ss;
    ss << fl::get<0>(trace) << ":" << fl::get<1>(trace);
    return ss.str();
}

// Task ID generator (atomic for thread safety)
int next_task_id() {
    static fl::atomic<int> id(0); // okay static in header
    return id.fetch_add(1) + 1;
}
} // namespace

} // namespace task
} // namespace fl

namespace fl {
namespace task {

//=============================================================================
// Coroutine - RAII wrapper around platform-specific implementation
//=============================================================================

class Coroutine {
public:
    using TaskFunction = fl::function<void()>;

    Coroutine(fl::string name, TaskFunction function, size_t stack_size = 4096, u8 priority = 5, int core_id = -1)
        : mImpl(platforms::createTaskCoroutine(fl::move(name), fl::move(function), stack_size, priority, core_id)) {
    }

    ~Coroutine() FL_NOEXCEPT = default;

    Coroutine(const Coroutine&) FL_NOEXCEPT = delete;
    Coroutine& operator=(const Coroutine&) FL_NOEXCEPT = delete;
    Coroutine(Coroutine&&) FL_NOEXCEPT = delete;
    Coroutine& operator=(Coroutine&&) FL_NOEXCEPT = delete;

    void stop() {
        if (mImpl) {
            mImpl->stop();
        }
    }

    bool isRunning() const {
        return mImpl ? mImpl->isRunning() : false;
    }

    static void exitCurrent() {
        platforms::ICoroutineTask::exitCurrent();
    }

private:
    platforms::TaskCoroutinePtr mImpl;
};

} // namespace task
} // namespace fl

namespace fl {
namespace task {

//=============================================================================
// ITaskImpl - Virtual Interface
//=============================================================================

class ITaskImpl {
public:
    virtual ~ITaskImpl() FL_NOEXCEPT = default;
    virtual void set_then(function<void()> on_then) = 0;
    virtual void set_catch(function<void(const Error&)> on_catch) = 0;
    virtual void set_canceled() = 0;
    virtual int id() const = 0;
    virtual void set_id(int id) = 0;
    virtual bool has_then() const = 0;
    virtual bool has_catch() const = 0;
    virtual string trace_label() const = 0;
    virtual TaskType type() const = 0;
    virtual int interval_ms() const = 0;
    virtual void set_interval_ms(int interval_ms) = 0;
    virtual u32 last_run_time() const = 0;
    virtual void set_last_run_time(u32 time) = 0;
    virtual bool ready_to_run(u32 current_time) const = 0;
    virtual bool ready_to_run_frame_task(u32 current_time) const = 0;
    virtual bool is_canceled() const = 0;
    virtual bool is_auto_registered() const = 0;
    virtual void execute_then() = 0;
    virtual void execute_catch(const Error& error) = 0;
    virtual void auto_register_with_scheduler() = 0;

    // Coroutine methods (no-op for non-coroutine tasks)
    virtual void stop() = 0;
    virtual bool isRunning() const = 0;
};

//=============================================================================
// TimeTask - Time-based task implementation
//=============================================================================

class TimeTask : public ITaskImpl {
public:
    TimeTask(TaskType type, int interval_ms, optional<TracePoint> trace = nullopt)
        : mTaskId(next_task_id())
        , mType(type)
        , mIntervalMs(interval_ms)
        , mTraceLabel(trace ? make_unique<string>(make_trace_label(*trace)) : nullptr)
        // Use (max)() to prevent macro expansion by Arduino.h's max macro
        , mLastRunTime((numeric_limits<u32>::max)()) {}

    void set_then(function<void()> on_then) override {
        mThenCallback = fl::move(on_then);
        mHasThen = true;
    }

    void set_catch(function<void(const Error&)> on_catch) override {
        mCatchCallback = fl::move(on_catch);
        mHasCatch = true;
    }

    void set_canceled() override {
        mCanceled = true;
        // Release callbacks immediately to free captured variables (e.g. promises
        // holding response objects). Without this, captures survive until the
        // scheduler erases the task on its next update pass.
        mThenCallback = {};
        mCatchCallback = {};
        mHasThen = false;
        mHasCatch = false;
    }

    int id() const override { return mTaskId; }
    void set_id(int id) override { mTaskId = id; }
    bool has_then() const override { return mHasThen; }
    bool has_catch() const override { return mHasCatch; }
    string trace_label() const override { return mTraceLabel ? *mTraceLabel : ""; }
    TaskType type() const override { return mType; }
    int interval_ms() const override { return mIntervalMs; }
    void set_interval_ms(int interval_ms) override { mIntervalMs = interval_ms; }
    u32 last_run_time() const override { return mLastRunTime; }
    void set_last_run_time(u32 time) override { mLastRunTime = time; }
    bool is_canceled() const override { return mCanceled; }
    bool is_auto_registered() const override { return mAutoRegistered; }

    bool ready_to_run(u32 current_time) const override {
        if (mType == TaskType::kBeforeFrame || mType == TaskType::kAfterFrame) {
            return false;  // Frame tasks not ready during regular updates
        }
        if (mIntervalMs <= 0) return true;
        // Use (max)() to prevent macro expansion by Arduino.h's max macro
        if (mLastRunTime == (fl::numeric_limits<u32>::max)()) return true;
        return (current_time - mLastRunTime) >= static_cast<u32>(mIntervalMs);
    }

    bool ready_to_run_frame_task(u32 /*current_time*/) const override {
        return mType == TaskType::kBeforeFrame || mType == TaskType::kAfterFrame;
    }

    void execute_then() override {
        if (mHasThen && mThenCallback) {
            mThenCallback();
        }
    }

    void execute_catch(const Error& error) override {
        if (mHasCatch && mCatchCallback) {
            mCatchCallback(error);
        }
    }

    void auto_register_with_scheduler() override {
        mAutoRegistered = true;
    }

    void stop() override {
        mRunning = false;
        mCanceled = true;  // Mark as canceled so scheduler removes it
    }

    bool isRunning() const override {
        return mRunning && !mCanceled;
    }

private:
    int mTaskId;
    TaskType mType;
    int mIntervalMs;
    bool mCanceled = false;
    bool mAutoRegistered = false;
    bool mRunning = true;  // Time tasks start running immediately
    unique_ptr<string> mTraceLabel;
    bool mHasThen = false;
    bool mHasCatch = false;
    u32 mLastRunTime;
    function<void()> mThenCallback;
    function<void(const Error&)> mCatchCallback;
};

//=============================================================================
// CoroutineTask - OS-level coroutine task
//=============================================================================

class CoroutineTask : public ITaskImpl {
public:
    CoroutineTask(const CoroutineConfig& config)
        : mTaskId(next_task_id())
        , mTraceLabel(config.trace ? make_unique<string>(make_trace_label(*config.trace)) : nullptr)
        , mCoroutine(make_unique<Coroutine>(config.name, config.func, config.stack_size, config.priority,
                                                 config.core_id.has_value() ? config.core_id.value() : -1)) {}

    void set_then(function<void()>) override { /* Coroutine tasks don't use then */ }
    void set_catch(function<void(const Error&)>) override { /* Coroutine tasks don't use catch */ }
    void set_canceled() override { mCanceled = true; }

    int id() const override { return mTaskId; }
    void set_id(int id) override { mTaskId = id; }
    bool has_then() const override { return false; }
    bool has_catch() const override { return false; }
    string trace_label() const override { return mTraceLabel ? *mTraceLabel : ""; }
    TaskType type() const override { return TaskType::kCoroutine; }
    int interval_ms() const override { return 0; }
    void set_interval_ms(int) override { /* Coroutine tasks don't use intervals */ }
    fl::u32 last_run_time() const override { return 0; }
    void set_last_run_time(fl::u32) override {}
    bool is_canceled() const override { return mCanceled; }
    bool is_auto_registered() const override { return mAutoRegistered; }

    bool ready_to_run(fl::u32) const override { return false; }
    bool ready_to_run_frame_task(fl::u32) const override { return false; }

    void execute_then() override {}
    void execute_catch(const Error&) override {}

    void auto_register_with_scheduler() override {
        mAutoRegistered = true;
    }

    void stop() override {
        if (mCoroutine) {
            mCoroutine->stop();
        }
    }

    bool isRunning() const override {
        return mCoroutine ? mCoroutine->isRunning() : false;
    }

private:
    int mTaskId;
    bool mCanceled = false;
    bool mAutoRegistered = false;
    unique_ptr<string> mTraceLabel;
    unique_ptr<Coroutine> mCoroutine;
};

//=============================================================================
// Handle - Public API Implementation
//=============================================================================

Handle::Handle(shared_ptr<ITaskImpl> impl) : mImpl(fl::move(impl)) {}

// Fluent API
Handle& Handle::then(function<void()> on_then) {
    if (mImpl) {
        mImpl->set_then(fl::move(on_then));
        if (!mImpl->is_auto_registered()) {
            mImpl->auto_register_with_scheduler();
            Scheduler::instance().add_task(*this);
        }
    }
    return *this;
}

Handle& Handle::catch_(function<void(const Error&)> on_catch) {
    if (mImpl) {
        mImpl->set_catch(fl::move(on_catch));
    }
    return *this;
}

Handle& Handle::cancel() {
    if (mImpl) {
        mImpl->set_canceled();
    }
    return *this;
}

// Getters
int Handle::id() const { return mImpl ? mImpl->id() : 0; }
bool Handle::has_then() const { return mImpl ? mImpl->has_then() : false; }
bool Handle::has_catch() const { return mImpl ? mImpl->has_catch() : false; }
string Handle::trace_label() const { return mImpl ? mImpl->trace_label() : ""; }
TaskType Handle::type() const { return mImpl ? mImpl->type() : TaskType::kEveryMs; }
int Handle::interval_ms() const { return mImpl ? mImpl->interval_ms() : 0; }
void Handle::set_interval_ms(int interval_ms) { if (mImpl) mImpl->set_interval_ms(interval_ms); }
fl::u32 Handle::last_run_time() const { return mImpl ? mImpl->last_run_time() : 0; }
void Handle::set_last_run_time(fl::u32 time) { if (mImpl) mImpl->set_last_run_time(time); }
bool Handle::ready_to_run(fl::u32 current_time) const { return mImpl ? mImpl->ready_to_run(current_time) : false; }
bool Handle::is_valid() const { return mImpl != nullptr; }
bool Handle::isCoroutine() const { return mImpl && mImpl->type() == TaskType::kCoroutine; }

// Coroutine control
void Handle::stop() { if (mImpl) mImpl->stop(); }
bool Handle::isRunning() const { return mImpl ? mImpl->isRunning() : false; }

// Free function builders (were static methods on class fl::task)
Handle every_ms(int interval_ms) {
    return Handle(fl::make_shared<TimeTask>(TaskType::kEveryMs, interval_ms));
}

Handle every_ms(int interval_ms, const TracePoint& trace) {
    return Handle(fl::make_shared<TimeTask>(TaskType::kEveryMs, interval_ms, trace));
}

Handle at_framerate(int fps) {
    return Handle(fl::make_shared<TimeTask>(TaskType::kAtFramerate, 1000 / fps));
}

Handle at_framerate(int fps, const TracePoint& trace) {
    return Handle(fl::make_shared<TimeTask>(TaskType::kAtFramerate, 1000 / fps, trace));
}

Handle before_frame() {
    return Handle(fl::make_shared<TimeTask>(TaskType::kBeforeFrame, 0));
}

Handle before_frame(const TracePoint& trace) {
    return Handle(fl::make_shared<TimeTask>(TaskType::kBeforeFrame, 0, trace));
}

Handle after_frame() {
    return Handle(fl::make_shared<TimeTask>(TaskType::kAfterFrame, 0));
}

Handle after_frame(const TracePoint& trace) {
    return Handle(fl::make_shared<TimeTask>(TaskType::kAfterFrame, 0, trace));
}

Handle after_frame(function<void()> on_then) {
    Handle t = after_frame();
    t.then(fl::move(on_then));
    return t;
}

Handle after_frame(function<void()> on_then, const TracePoint& trace) {
    Handle t = after_frame(trace);
    t.then(fl::move(on_then));
    return t;
}

Handle coroutine(const CoroutineConfig& config) {
    return Handle(fl::make_shared<CoroutineTask>(config));
}

// Static coroutine control
void exit_current() { Coroutine::exitCurrent(); }

// Internal methods for Scheduler (friend access only)
void Handle::_set_id(int id) { if (mImpl) mImpl->set_id(id); }
int Handle::_id() const { return mImpl ? mImpl->id() : 0; }
bool Handle::_is_canceled() const { return mImpl ? mImpl->is_canceled() : true; }
bool Handle::_ready_to_run(fl::u32 current_time) const { return mImpl ? mImpl->ready_to_run(current_time) : false; }
bool Handle::_ready_to_run_frame_task(fl::u32 current_time) const { return mImpl ? mImpl->ready_to_run_frame_task(current_time) : false; }
void Handle::_set_last_run_time(fl::u32 time) { if (mImpl) mImpl->set_last_run_time(time); }
bool Handle::_has_then() const { return mImpl ? mImpl->has_then() : false; }
void Handle::_execute_then() { if (mImpl) mImpl->execute_then(); }
void Handle::_execute_catch(const Error& error) { if (mImpl) mImpl->execute_catch(error); }
TaskType Handle::_type() const { return mImpl ? mImpl->type() : TaskType::kEveryMs; }
string Handle::_trace_label() const { return mImpl ? mImpl->trace_label() : ""; }

} // namespace task
} // namespace fl
