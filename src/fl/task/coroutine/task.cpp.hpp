// ok no header - public task API remains in fl/task/task.h

#include "fl/task/task.h"
#include "fl/stl/limits.h"
#include "fl/task/scheduler.h"
#include "fl/stl/sstream.h"
#include "fl/stl/unique_ptr.h"
#include "fl/stl/atomic.h"
#include "platforms/coroutine.h"
#include "fl/stl/noexcept.h"

#include "fl/task/detail/task_impl.h"

namespace fl {
namespace task {

class Coroutine {
public:
    using TaskFunction = fl::function<void()>;

    Coroutine(fl::string name, TaskFunction function, size_t stack_size = 4096, u8 priority = 5, int core_id = -1)
        : mImpl(platforms::createTaskCoroutine(fl::move(name), fl::move(function), stack_size, priority, core_id)) {
    }

    ~Coroutine() FL_NO_EXCEPT = default;

    Coroutine(const Coroutine&) FL_NO_EXCEPT = delete;
    Coroutine& operator=(const Coroutine&) FL_NO_EXCEPT = delete;
    Coroutine(Coroutine&&) FL_NO_EXCEPT = delete;
    Coroutine& operator=(Coroutine&&) FL_NO_EXCEPT = delete;

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

class CoroutineTask : public ITaskImpl {
public:
    CoroutineTask(const CoroutineConfig& config)
        : mTaskId(detail::next_task_id())
        , mTraceLabel(config.trace ? make_unique<string>(detail::make_trace_label(*config.trace)) : nullptr)
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

Handle coroutine(const CoroutineConfig& config) FL_NO_EXCEPT {
    return Handle(fl::make_shared<CoroutineTask>(config));
}

// Static coroutine control
void exit_current() FL_NO_EXCEPT { Coroutine::exitCurrent(); }

} // namespace task
} // namespace fl
