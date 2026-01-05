#include "fl/task.h"
#include "fl/numeric_limits.h"
#include "fl/async.h"
#include "fl/stl/sstream.h"
#include "fl/stl/unique_ptr.h"
#include "fl/stl/atomic.h"
#include "platforms/itask_coroutine.h"

namespace fl {

namespace {
// Generate trace label from TracePoint
string make_trace_label(const TracePoint& trace) {
    sstream ss;
    ss << fl::get<0>(trace) << ":" << fl::get<1>(trace);
    return ss.str();
}

// Task ID generator (atomic for thread safety)
int next_task_id() {
    static fl::atomic<int> id(0);
    return id.fetch_add(1) + 1;
}
} // namespace

} // namespace fl

namespace fl {

//=============================================================================
// TaskCoroutine - RAII wrapper around platform-specific implementation
//=============================================================================

class TaskCoroutine {
public:
    using TaskFunction = fl::function<void()>;

    TaskCoroutine(fl::string name, TaskFunction function, size_t stack_size = 4096, uint8_t priority = 5)
        : mImpl(platforms::createTaskCoroutine(fl::move(name), fl::move(function), stack_size, priority)) {
    }

    ~TaskCoroutine() {
        if (mImpl) {
            delete mImpl;
        }
    }

    TaskCoroutine(const TaskCoroutine&) = delete;
    TaskCoroutine& operator=(const TaskCoroutine&) = delete;
    TaskCoroutine(TaskCoroutine&&) = delete;
    TaskCoroutine& operator=(TaskCoroutine&&) = delete;

    void stop() {
        if (mImpl) {
            mImpl->stop();
        }
    }

    bool isRunning() const {
        return mImpl ? mImpl->isRunning() : false;
    }

    static void exitCurrent() {
        platforms::ITaskCoroutine::exitCurrent();
    }

private:
    platforms::ITaskCoroutine* mImpl;
};

} // namespace fl

// Platform dispatch for TaskCoroutine inline implementations (outside fl namespace)
#include "platforms/coroutine.h"  // allow-include-after-namespace

namespace fl {

//=============================================================================
// ITaskImpl - Virtual Interface
//=============================================================================

class ITaskImpl {
public:
    virtual ~ITaskImpl() = default;
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
    virtual uint32_t last_run_time() const = 0;
    virtual void set_last_run_time(uint32_t time) = 0;
    virtual bool ready_to_run(uint32_t current_time) const = 0;
    virtual bool ready_to_run_frame_task(uint32_t current_time) const = 0;
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
        , mLastRunTime(numeric_limits<uint32_t>::max()) {}

    void set_then(function<void()> on_then) override {
        mThenCallback = fl::move(on_then);
        mHasThen = true;
    }

    void set_catch(function<void(const Error&)> on_catch) override {
        mCatchCallback = fl::move(on_catch);
        mHasCatch = true;
    }

    void set_canceled() override { mCanceled = true; }

    int id() const override { return mTaskId; }
    void set_id(int id) override { mTaskId = id; }
    bool has_then() const override { return mHasThen; }
    bool has_catch() const override { return mHasCatch; }
    string trace_label() const override { return mTraceLabel ? *mTraceLabel : ""; }
    TaskType type() const override { return mType; }
    int interval_ms() const override { return mIntervalMs; }
    uint32_t last_run_time() const override { return mLastRunTime; }
    void set_last_run_time(uint32_t time) override { mLastRunTime = time; }
    bool is_canceled() const override { return mCanceled; }
    bool is_auto_registered() const override { return mAutoRegistered; }

    bool ready_to_run(uint32_t current_time) const override {
        if (mType == TaskType::kBeforeFrame || mType == TaskType::kAfterFrame) {
            return false;  // Frame tasks not ready during regular updates
        }
        if (mIntervalMs <= 0) return true;
        if (mLastRunTime == fl::numeric_limits<uint32_t>::max()) return true;
        return (current_time - mLastRunTime) >= static_cast<uint32_t>(mIntervalMs);
    }

    bool ready_to_run_frame_task(uint32_t /*current_time*/) const override {
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
    uint32_t mLastRunTime;
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
        , mCoroutine(make_unique<TaskCoroutine>(config.name, config.function, config.stack_size, config.priority)) {}

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
    uint32_t last_run_time() const override { return 0; }
    void set_last_run_time(uint32_t) override {}
    bool is_canceled() const override { return mCanceled; }
    bool is_auto_registered() const override { return mAutoRegistered; }

    bool ready_to_run(uint32_t) const override { return false; }
    bool ready_to_run_frame_task(uint32_t) const override { return false; }

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
    unique_ptr<TaskCoroutine> mCoroutine;
};

//=============================================================================
// task - Public API Implementation
//=============================================================================

task::task(shared_ptr<ITaskImpl> impl) : mImpl(fl::move(impl)) {}

// Static builders
task task::every_ms(int interval_ms) {
    return task(fl::make_shared<TimeTask>(TaskType::kEveryMs, interval_ms));
}

task task::every_ms(int interval_ms, const TracePoint& trace) {
    return task(fl::make_shared<TimeTask>(TaskType::kEveryMs, interval_ms, trace));
}

task task::at_framerate(int fps) {
    return task(fl::make_shared<TimeTask>(TaskType::kAtFramerate, 1000 / fps));
}

task task::at_framerate(int fps, const TracePoint& trace) {
    return task(fl::make_shared<TimeTask>(TaskType::kAtFramerate, 1000 / fps, trace));
}

task task::before_frame() {
    return task(fl::make_shared<TimeTask>(TaskType::kBeforeFrame, 0));
}

task task::before_frame(const TracePoint& trace) {
    return task(fl::make_shared<TimeTask>(TaskType::kBeforeFrame, 0, trace));
}

task task::after_frame() {
    return task(fl::make_shared<TimeTask>(TaskType::kAfterFrame, 0));
}

task task::after_frame(const TracePoint& trace) {
    return task(fl::make_shared<TimeTask>(TaskType::kAfterFrame, 0, trace));
}

task task::after_frame(function<void()> on_then) {
    task t = task::after_frame();
    t.then(fl::move(on_then));
    return t;
}

task task::after_frame(function<void()> on_then, const TracePoint& trace) {
    task t = task::after_frame(trace);
    t.then(fl::move(on_then));
    return t;
}

task task::coroutine(const CoroutineConfig& config) {
    return task(fl::make_shared<CoroutineTask>(config));
}

// Fluent API
task& task::then(function<void()> on_then) {
    if (mImpl) {
        mImpl->set_then(fl::move(on_then));
        if (!mImpl->is_auto_registered()) {
            mImpl->auto_register_with_scheduler();
            fl::Scheduler::instance().add_task(*this);
        }
    }
    return *this;
}

task& task::catch_(function<void(const Error&)> on_catch) {
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

// Getters
int task::id() const { return mImpl ? mImpl->id() : 0; }
bool task::has_then() const { return mImpl ? mImpl->has_then() : false; }
bool task::has_catch() const { return mImpl ? mImpl->has_catch() : false; }
string task::trace_label() const { return mImpl ? mImpl->trace_label() : ""; }
TaskType task::type() const { return mImpl ? mImpl->type() : TaskType::kEveryMs; }
int task::interval_ms() const { return mImpl ? mImpl->interval_ms() : 0; }
uint32_t task::last_run_time() const { return mImpl ? mImpl->last_run_time() : 0; }
void task::set_last_run_time(uint32_t time) { if (mImpl) mImpl->set_last_run_time(time); }
bool task::ready_to_run(uint32_t current_time) const { return mImpl ? mImpl->ready_to_run(current_time) : false; }
bool task::is_valid() const { return mImpl != nullptr; }
bool task::isCoroutine() const { return mImpl && mImpl->type() == TaskType::kCoroutine; }

// Coroutine control
void task::stop() { if (mImpl) mImpl->stop(); }
bool task::isRunning() const { return mImpl ? mImpl->isRunning() : false; }
void task::exitCurrent() { TaskCoroutine::exitCurrent(); }

// Internal methods for Scheduler (friend access only)
void task::_set_id(int id) { if (mImpl) mImpl->set_id(id); }
int task::_id() const { return mImpl ? mImpl->id() : 0; }
bool task::_is_canceled() const { return mImpl ? mImpl->is_canceled() : true; }
bool task::_ready_to_run(uint32_t current_time) const { return mImpl ? mImpl->ready_to_run(current_time) : false; }
bool task::_ready_to_run_frame_task(uint32_t current_time) const { return mImpl ? mImpl->ready_to_run_frame_task(current_time) : false; }
void task::_set_last_run_time(uint32_t time) { if (mImpl) mImpl->set_last_run_time(time); }
bool task::_has_then() const { return mImpl ? mImpl->has_then() : false; }
void task::_execute_then() { if (mImpl) mImpl->execute_then(); }
void task::_execute_catch(const Error& error) { if (mImpl) mImpl->execute_catch(error); }
TaskType task::_type() const { return mImpl ? mImpl->type() : TaskType::kEveryMs; }
string task::_trace_label() const { return mImpl ? mImpl->trace_label() : ""; }

} // namespace fl
