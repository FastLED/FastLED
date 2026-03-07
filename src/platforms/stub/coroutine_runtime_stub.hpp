#pragma once

// IWYU pragma: private

// Only compile for stub platform (exclude WASM which has its own implementation)
#include "platforms/wasm/is_wasm.h"

#if defined(FASTLED_STUB_IMPL) && !defined(FL_IS_WASM)

// IWYU pragma: begin_keep
#include "platforms/coroutine_runtime.h"
#include "platforms/stub/coroutine_runner.h"  // ok platform headers
#include "platforms/stub/time_stub.h"  // ok platform headers
#include "fl/singleton.h"
#include "fl/stl/thread.h"
#include "fl/stl/function.h"
#include "fl/stl/chrono.h"
#include "fl/compiler_control.h"
// IWYU pragma: end_keep

// Forward declare delay override (defined in time_stub.cpp.hpp)
extern fl::function<void(fl::u32)> g_delay_override;

namespace fl {
namespace platforms {

namespace {
    FL_DISABLE_WARNING_PUSH
    FL_DISABLE_WARNING_GLOBAL_CONSTRUCTORS
    static const auto s_stub_start_time = std::chrono::system_clock::now();  // okay std namespace
    FL_DISABLE_WARNING_POP
}

class CoroutineRuntimeStub : public ICoroutineRuntime {
public:
    void pumpEventQueueWithSleep(fl::u32 us) override {
        fl::detail::CoroutineRunner::instance().run(us);
    }

    bool hasPlatformBackgroundEvents() override {
        return true;
    }

    void platformDelay(fl::u32 ms) override {
        // Check for test override first (for fast testing)
        if (g_delay_override) {
            g_delay_override(ms);
            return;
        }
        fl::this_thread::sleep_for(fl::chrono::milliseconds(ms));  // ok sleep for
    }

    void platformDelayMicroseconds(fl::u32 us) override {
        fl::this_thread::sleep_for(fl::chrono::microseconds(us));  // ok sleep for
    }

    fl::u32 platformMillis() override {
        auto current = std::chrono::system_clock::now();  // okay std namespace
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(  // okay std namespace
            current - s_stub_start_time);
        return static_cast<fl::u32>(elapsed.count());
    }

    fl::u32 platformMicros() override {
        auto current = std::chrono::system_clock::now();  // okay std namespace
        auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(  // okay std namespace
            current - s_stub_start_time);
        return static_cast<fl::u32>(elapsed.count());
    }

    void osYield() override {
        std::this_thread::yield();  // okay std namespace
    }
};

ICoroutineRuntime& ICoroutineRuntime::instance() {
    return fl::Singleton<CoroutineRuntimeStub>::instance();
}

}  // namespace platforms
}  // namespace fl

#endif  // FASTLED_STUB_IMPL && !FL_IS_WASM
