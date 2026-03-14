#pragma once

// IWYU pragma: private

/// @file coroutine_runtime_wasm.impl.hpp
/// @brief WASM coroutine runtime — pumps cooperative coroutine runner
///
/// Uses JSPI-based context switching (via CoroutinePlatformWasm) to provide
/// cooperative coroutines in WASM. The generic CoroutineRunner handles
/// scheduling; this file just wires the platform and runtime together.

#include "platforms/wasm/is_wasm.h"

#ifdef FL_IS_WASM

// IWYU pragma: begin_keep
#include "platforms/wasm/coroutine_platform_wasm.hpp"
#include "platforms/coroutine_runtime.h"
#include "platforms/coroutine.h"
#include "fl/stl/singleton.h"
#include "fl/stl/string.h"
#include "fl/stl/unique_ptr.h"
#include "fl/system/log.h"
// IWYU pragma: end_keep

namespace fl {
namespace platforms {

//=============================================================================
// Coroutine Runtime — pumps cooperative coroutine runner
//=============================================================================

class CoroutineRuntimeWasm : public ICoroutineRuntime {
public:
    void pumpCoroutines(fl::u32 us) override {
        if (CoroutineContext::isInsideCoroutine()) {
            // Called from within a coroutine — yield back to runner
            CoroutineContext::suspend();
        } else {
            // Called from main thread — give time to background coroutines
            CoroutineRunner::instance().run(us);
        }
    }
};

//=============================================================================
// Platform Registration — register fiber platform at startup
//=============================================================================

namespace {
struct WasmPlatformRegistrar {
    WasmPlatformRegistrar() {
        ICoroutinePlatform::setInstance(
            &fl::Singleton<CoroutinePlatformWasm>::instance());
    }
};
static WasmPlatformRegistrar sWasmPlatformRegistrar;
}  // namespace

ICoroutineRuntime& ICoroutineRuntime::instance() {
    return fl::Singleton<CoroutineRuntimeWasm>::instance();
}

//=============================================================================
// Task Coroutine — cooperative fiber-based
//=============================================================================

class TaskCoroutineWasm : public ICoroutineTask {
public:
    static TaskCoroutinePtr create(fl::string name,
                                    TaskFunction function,
                                    size_t stack_size = 4096,
                                    u8 priority = 5) {
        auto* ctx = CoroutineContext::create(fl::move(function), stack_size);
        if (!ctx) {
            FL_WARN("TaskCoroutineWasm: Failed to create context for '"
                    << name << "'");
            return nullptr;
        }

        TaskCoroutinePtr task(new TaskCoroutineWasm());  // ok bare allocation
        auto* impl = static_cast<TaskCoroutineWasm*>(task.get());
        impl->mName = fl::move(name);
        impl->mContext.reset(ctx);

        // Register with the global runner
        CoroutineRunner::instance().enqueue(ctx);

        return task;
    }

    ~TaskCoroutineWasm() override { stop(); }

    void stop() override {
        if (!mContext) return;
        mContext->stop_and_complete();
        CoroutineRunner::instance().remove(mContext.get());
    }

    bool isRunning() const override {
        if (!mContext) return false;
        return !mContext->is_completed();
    }

private:
    TaskCoroutineWasm() = default;

    fl::string mName;
    fl::unique_ptr<CoroutineContext> mContext;
};

//=============================================================================
// Factory function
//=============================================================================

TaskCoroutinePtr createTaskCoroutine(fl::string name,
                                      ICoroutineTask::TaskFunction function,
                                      size_t stack_size,
                                      u8 priority,
                                      int /*core_id*/) {
    return TaskCoroutineWasm::create(fl::move(name), fl::move(function),
                                     stack_size, priority);
}

//=============================================================================
// Static exitCurrent — suspend back to runner and mark completed
//=============================================================================

void ICoroutineTask::exitCurrent() {
    CoroutineContext* ctx = CoroutineContext::runningCoroutine();
    if (ctx) {
        ctx->set_should_stop(true);
    }
    CoroutineContext::suspend();
    // Should never resume — runner marks completed and removes on should_stop.
    while (true) {}
}

}  // namespace platforms
}  // namespace fl

#endif  // FL_IS_WASM
