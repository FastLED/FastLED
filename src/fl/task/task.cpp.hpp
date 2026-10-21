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
string make_trace_label(const TracePoint& trace) FL_NO_EXCEPT {
    sstream ss;
    ss << fl::get<0>(trace) << ":" << fl::get<1>(trace);
    return ss.str();
}

// Task ID generator (atomic for thread safety)
int next_task_id() FL_NO_EXCEPT {
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
        FL_NO_EXCEPT : mImpl(platforms::createTaskCoroutine(fl::move(name), fl::move(function), stack_size, priority, core_id)) {
    }

    ~Coroutine() FL_NO_EXCEPT = default;

    Coroutine(const Coroutine&) FL_NO_EXCEPT = delete;
    Coroutine& operator=(const Coroutine&) FL_NO_EXCEPT = delete;
    Coroutine(Coroutine&&) FL_NO_EXCEPT = delete;
    Coroutine& operator=(Coroutine&&) FL_NO_EXCEPT = delete;

    void stop() FL_NO_EXCEPT {
        if (mImpl) {
            mImpl->stop();
        }
    }

    bool isRunning() const FL_NO_EXCEPT {
        return mImpl ? mImpl->isRunning() : false;
    }

    static void exitCurrent() FL_NO_EXCEPT {
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
    virtual ~ITaskImpl() FL_NO_EXCEPT = default;
    virtual void set_then(function<void()> on_then) FL_NO_EXCEPT = 0;
    virtual void set_catch(function<void(const Error&)> on_catch) FL_NO_EXCEPT = 0;
    virtual void set_canceled() FL_NO_EXCEPT = 0;
    virtual int id() const FL_NO_EXCEPT = 0;
    virtual void set_id(int id) FL_NO_EXCEPT = 0;
    virtual bool has_then() const FL_NO_EXCEPT = 0;
    virtual bool has_catch() const FL_NO_EXCEPT = 0;
    virtual string trace_label() const FL_NO_EXCEPT = 0;
    virtual TaskType type() const FL_NO_EXCEPT = 0;
    virtual int interval_ms() const FL_NO_EXCEPT = 0;
    virtual void set_interval_ms(int interval_ms) FL_NO_EXCEPT = 0;
    virtual u32 last_run_time() const FL_NO_EXCEPT = 0;
    virtual void set_last_run_time(u32 time) FL_NO_EXCEPT = 0;
    virtual bool ready_to_run(u32 current_time) const FL_NO_EXCEPT = 0;
    virtual bool ready_to_run_frame_task(u32 current_time) const FL_NO_EXCEPT = 0;
    virtual bool is_canceled() const FL_NO_EXCEPT = 0;
    virtual bool is_auto_registered() const FL_NO_EXCEPT = 0;
    virtual void execute_then() FL_NO_EXCEPT = 0;
    virtual void execute_catch(const Error& error) FL_NO_EXCEPT = 0;
    virtual void auto_register_with_scheduler() FL_NO_EXCEPT = 0;

    // Coroutine methods (no-op for non-coroutine tasks)
    virtual void stop() FL_NO_EXCEPT = 0;
    virtual bool isRunning() const FL_NO_EXCEPT = 0;
};

//=============================================================================
// TimeTask - Time-based task implementation
//=============================================================================

class TimeTask : public ITaskImpl {
public:
    TimeTask(TaskType type, int interval_ms, optional<TracePoint> trace = nullopt)
        FL_NO_EXCEPT : mTaskId(next_task_id())
        , mType(type)
        , mIntervalMs(interval_ms)
        , mTraceLabel(trace ? make_unique<string>(make_trace_label(*trace)) : nullptr)
        // Use (max)() to prevent macro expansion by Arduino.h's max macro
        , mLastRunTime((numeric_limits<u32>::max)()) {}

    void set_then(function<void()> on_then) FL_NO_EXCEPT override {
        mThenCallback = fl::move(on_then);
        mHasThen = true;
    }

    void set_catch(function<void(const Error&)> on_catch) FL_NO_EXCEPT override {
        mCatchCallback = fl::move(on_catch);
        mHasCatch = true;
    }

    void set_canceled() FL_NO_EXCEPT override {
        mCanceled = true;
        // Release callbacks immediately to free captured variables (e.g. promises
        // holding response objects). Without this, captures survive until the
        // scheduler erases the task on its next update pass.
        mThenCallback = {};
        mCatchCallback = {};
        mHasThen = false;
        mHasCatch = false;
    }

    int id() const FL_NO_EXCEPT override { return mTaskId; }
    void set_id(int id) FL_NO_EXCEPT override { mTaskId = id; }
    bool has_then() const FL_NO_EXCEPT override { return mHasThen; }
    bool has_catch() const FL_NO_EXCEPT override { return mHasCatch; }
    string trace_label() const FL_NO_EXCEPT override { return mTraceLabel ? *mTraceLabel : ""; }
    TaskType type() const FL_NO_EXCEPT override { return mType; }
    int interval_ms() const FL_NO_EXCEPT override { return mIntervalMs; }
    void set_interval_ms(int interval_ms) FL_NO_EXCEPT override { mIntervalMs = interval_ms; }
    u32 last_run_time() const FL_NO_EXCEPT override { return mLastRunTime; }
    void set_last_run_time(u32 time) FL_NO_EXCEPT override { mLastRunTime = time; }
    bool is_canceled() const FL_NO_EXCEPT override { return mCanceled; }
    bool is_auto_registered() const FL_NO_EXCEPT override { return mAutoRegistered; }

    bool ready_to_run(u32 current_time) const FL_NO_EXCEPT override {
        if (mType == TaskType::kBeforeFrame || mType == TaskType::kAfterFrame) {
            return false;  // Frame tasks not ready during regular updates
        }
        if (mIntervalMs <= 0) return true;
        // Use (max)() to prevent macro expansion by Arduino.h's max macro
        if (mLastRunTime == (fl::numeric_limits<u32>::max)()) return true;
        return (current_time - mLastRunTime) >= static_cast<u32>(mIntervalMs);
    }

    bool ready_to_run_frame_task(u32 /*current_time*/) const FL_NO_EXCEPT override {
        return mType == TaskType::kBeforeFrame || mType == TaskType::kAfterFrame;
    }

    void execute_then() FL_NO_EXCEPT override {
        if (mHasThen && mThenCallback) {
            mThenCallback();
        }
    }

    void execute_catch(const Error& error) FL_NO_EXCEPT override {
        if (mHasCatch && mCatchCallback) {
            mCatchCallback(error);
        }
    }

    void auto_register_with_scheduler() FL_NO_EXCEPT override {
        mAutoRegistered = true;
    }

    void stop() FL_NO_EXCEPT override {
        mRunning = false;
        mCanceled = true;  // Mark as canceled so scheduler removes it
    }

    bool isRunning() const FL_NO_EXCEPT override {
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
        FL_NO_EXCEPT : mTaskId(next_task_id())
        , mTraceLabel(config.trace ? make_unique<string>(make_trace_label(*config.trace)) : nullptr)
        , mCoroutine(make_unique<Coroutine>(config.name, config.func, config.stack_size, config.priority,
                                                 config.core_id.has_value() ? config.core_id.value() : -1)) {}

    void set_then(function<void()>) FL_NO_EXCEPT override { /* Coroutine tasks don't use then */ }
    void set_catch(function<void(const Error&)>) FL_NO_EXCEPT override { /* Coroutine tasks don't use catch */ }
    void set_canceled() FL_NO_EXCEPT override { mCanceled = true; }

    int id() const FL_NO_EXCEPT override { return mTaskId; }
    void set_id(int id) FL_NO_EXCEPT override { mTaskId = id; }
    bool has_then() const FL_NO_EXCEPT override { return false; }
    bool has_catch() const FL_NO_EXCEPT override { return false; }
    string trace_label() const FL_NO_EXCEPT override { return mTraceLabel ? *mTraceLabel : ""; }
    TaskType type() const FL_NO_EXCEPT override { return TaskType::kCoroutine; }
    int interval_ms() const FL_NO_EXCEPT override { return 0; }
    void set_interval_ms(int) FL_NO_EXCEPT override { /* Coroutine tasks don't use intervals */ }
    fl::u32 last_run_time() const FL_NO_EXCEPT override { return 0; }
    void set_last_run_time(fl::u32) FL_NO_EXCEPT override {}
    bool is_canceled() const FL_NO_EXCEPT override { return mCanceled; }
    bool is_auto_registered() const FL_NO_EXCEPT override { return mAutoRegistered; }

    bool ready_to_run(fl::u32) const FL_NO_EXCEPT override { return false; }
    bool ready_to_run_frame_task(fl::u32) const FL_NO_EXCEPT override { return false; }

    void execute_then() FL_NO_EXCEPT override {}
    void execute_catch(const Error&) FL_NO_EXCEPT override {}

    void auto_register_with_scheduler() FL_NO_EXCEPT override {
        mAutoRegistered = true;
    }

    void stop() FL_NO_EXCEPT override {
        if (mCoroutine) {
            mCoroutine->stop();
        }
    }

    bool isRunning() const FL_NO_EXCEPT override {
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

Handle::Handle(shared_ptr<ITaskImpl> impl) FL_NO_EXCEPT : mImpl(fl::move(impl)) {}

// Fluent API
Handle& Handle::then(function<void()> on_then) FL_NO_EXCEPT {
    if (mImpl) {
        mImpl->set_then(fl::move(on_then));
        if (!mImpl->is_auto_registered()) {
            mImpl->auto_register_with_scheduler();
            Scheduler::instance().add_task(*this);
        }
    }
    return *this;
}

Handle& Handle::catch_(function<void(const Error&)> on_catch) FL_NO_EXCEPT {
    if (mImpl) {
        mImpl->set_catch(fl::move(on_catch));
    }
    return *this;
}

Handle& Handle::cancel() FL_NO_EXCEPT {
    if (mImpl) {
        mImpl->set_canceled();
    }
    return *this;
}

// Getters
int Handle::id() const FL_NO_EXCEPT { return mImpl ? mImpl->id() : 0; }
bool Handle::has_then() const FL_NO_EXCEPT { return mImpl ? mImpl->has_then() : false; }
bool Handle::has_catch() const FL_NO_EXCEPT { return mImpl ? mImpl->has_catch() : false; }
string Handle::trace_label() const FL_NO_EXCEPT { return mImpl ? mImpl->trace_label() : ""; }
TaskType Handle::type() const FL_NO_EXCEPT { return mImpl ? mImpl->type() : TaskType::kEveryMs; }
int Handle::interval_ms() const FL_NO_EXCEPT { return mImpl ? mImpl->interval_ms() : 0; }
void Handle::set_interval_ms(int interval_ms) FL_NO_EXCEPT { if (mImpl) mImpl->set_interval_ms(interval_ms); }
fl::u32 Handle::last_run_time() const FL_NO_EXCEPT { return mImpl ? mImpl->last_run_time() : 0; }
void Handle::set_last_run_time(fl::u32 time) FL_NO_EXCEPT { if (mImpl) mImpl->set_last_run_time(time); }
bool Handle::ready_to_run(fl::u32 current_time) const FL_NO_EXCEPT { return mImpl ? mImpl->ready_to_run(current_time) : false; }
bool Handle::is_valid() const FL_NO_EXCEPT { return mImpl != nullptr; }
bool Handle::isCoroutine() const FL_NO_EXCEPT { return mImpl && mImpl->type() == TaskType::kCoroutine; }

// Coroutine control
void Handle::stop() FL_NO_EXCEPT { if (mImpl) mImpl->stop(); }
bool Handle::isRunning() const FL_NO_EXCEPT { return mImpl ? mImpl->isRunning() : false; }

// Free function builders (were static methods on class fl::task)
Handle every_ms(int interval_ms) FL_NO_EXCEPT {
    return Handle(fl::make_shared<TimeTask>(TaskType::kEveryMs, interval_ms));
}

Handle every_ms(int interval_ms, const TracePoint& trace) FL_NO_EXCEPT {
    return Handle(fl::make_shared<TimeTask>(TaskType::kEveryMs, interval_ms, trace));
}

Handle at_framerate(int fps) FL_NO_EXCEPT {
    return Handle(fl::make_shared<TimeTask>(TaskType::kAtFramerate, 1000 / fps));
}

Handle at_framerate(int fps, const TracePoint& trace) FL_NO_EXCEPT {
    return Handle(fl::make_shared<TimeTask>(TaskType::kAtFramerate, 1000 / fps, trace));
}

Handle before_frame() FL_NO_EXCEPT {
    return Handle(fl::make_shared<TimeTask>(TaskType::kBeforeFrame, 0));
}

Handle before_frame(const TracePoint& trace) FL_NO_EXCEPT {
    return Handle(fl::make_shared<TimeTask>(TaskType::kBeforeFrame, 0, trace));
}

Handle after_frame() FL_NO_EXCEPT {
    return Handle(fl::make_shared<TimeTask>(TaskType::kAfterFrame, 0));
}

Handle after_frame(const TracePoint& trace) FL_NO_EXCEPT {
    return Handle(fl::make_shared<TimeTask>(TaskType::kAfterFrame, 0, trace));
}

Handle after_frame(function<void()> on_then) FL_NO_EXCEPT {
    Handle t = after_frame();
    t.then(fl::move(on_then));
    return t;
}

Handle after_frame(function<void()> on_then, const TracePoint& trace) FL_NO_EXCEPT {
    Handle t = after_frame(trace);
    t.then(fl::move(on_then));
    return t;
}

Handle coroutine(const CoroutineConfig& config) FL_NO_EXCEPT {
    return Handle(fl::make_shared<CoroutineTask>(config));
}

// Static coroutine control
void exit_current() FL_NO_EXCEPT { Coroutine::exitCurrent(); }

// Internal methods for Scheduler (friend access only)
void Handle::_set_id(int id) FL_NO_EXCEPT { if (mImpl) mImpl->set_id(id); }
int Handle::_id() const FL_NO_EXCEPT { return mImpl ? mImpl->id() : 0; }
bool Handle::_is_canceled() const FL_NO_EXCEPT { return mImpl ? mImpl->is_canceled() : true; }
bool Handle::_ready_to_run(fl::u32 current_time) const FL_NO_EXCEPT { return mImpl ? mImpl->ready_to_run(current_time) : false; }
bool Handle::_ready_to_run_frame_task(fl::u32 current_time) const FL_NO_EXCEPT { return mImpl ? mImpl->ready_to_run_frame_task(current_time) : false; }
void Handle::_set_last_run_time(fl::u32 time) FL_NO_EXCEPT { if (mImpl) mImpl->set_last_run_time(time); }
bool Handle::_has_then() const FL_NO_EXCEPT { return mImpl ? mImpl->has_then() : false; }
void Handle::_execute_then() FL_NO_EXCEPT { if (mImpl) mImpl->execute_then(); }
void Handle::_execute_catch(const Error& error) FL_NO_EXCEPT { if (mImpl) mImpl->execute_catch(error); }
TaskType Handle::_type() const FL_NO_EXCEPT { return mImpl ? mImpl->type() : TaskType::kEveryMs; }
string Handle::_trace_label() const FL_NO_EXCEPT { return mImpl ? mImpl->trace_label() : ""; }

} // namespace task
} // namespace fl
