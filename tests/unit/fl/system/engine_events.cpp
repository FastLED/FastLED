#include "FastLED.h"
#include "fl/channels/dither_frame.h"
#include "fl/channels/manager.h"
#include "fl/stl/function.h"
#include "fl/stl/scope_exit.h"
#include "fl/stl/shared_ptr.h"
#include "fl/stl/vector.h"
#include "fl/system/engine_events.h"
#include "test.h"

using namespace fl;

FL_TEST_FILE(FL_FILEPATH) {
namespace {

// Isolate the general listener list from the manager's normal frame hooks.
struct FrameListeners {
    bool restoreManager;
    const fl::u8 savedDitherFrame = fl::detail::gDitherFrame;
    FrameListeners() {
        auto& manager = ChannelManager::instance();
        restoreManager = EngineEvents::hasListener(&manager);
        EngineEvents::removeListener(&manager);
    }
    ~FrameListeners() {
        fl::detail::gDitherFrame = savedDitherFrame;
        if (restoreManager) {
            EngineEvents::addListener(&ChannelManager::instance());
        }
    }
};

struct RecordingListener : EngineEvents::Listener {
    fl::vector<int>& calls;
    int tag;
    fl::function<void()> onCall;

    RecordingListener(fl::vector<int>& output, int value)
        : calls(output), tag(value) {}

    void record(int hook) {
        calls.push_back(tag * 10 + hook);
        if (onCall) onCall();
    }
    void onBeginFrame() FL_NO_EXCEPT override { record(0); }
    void onEndFrame() FL_NO_EXCEPT override { record(1); }
    void onEndShowLeds() FL_NO_EXCEPT override { record(2); }
};

using FrameHook = void (*)();
const FrameHook hooks[] = {&EngineEvents::onBeginFrame,
                           &EngineEvents::onEndFrame,
                           &EngineEvents::onEndShowLeds};

} // anonymous namespace

FL_TEST_CASE("Frame hooks preserve a sole listener's dispatch snapshot #5384") {
    FrameListeners fixture;
    for (int hook = 0; hook < 3; ++hook) {
        fl::vector<int> calls;
        RecordingListener first(calls, 1), second(calls, 2);
        auto cleanup = fl::make_scope_exit([&]() {
            EngineEvents::removeListener(&first);
            EngineEvents::removeListener(&second);
        });
        hooks[hook](); // Empty general listener list.
        FL_CHECK(calls.empty());
        first.onCall = [&]() {
            EngineEvents::removeListener(&first);
            EngineEvents::addListener(&second);
        };
        EngineEvents::addListener(&first);

        hooks[hook]();
        FL_CHECK_EQ(calls, fl::vector<int>({10 + hook}));
        hooks[hook]();
        FL_CHECK_EQ(calls, fl::vector<int>({10 + hook, 20 + hook}));
    }
}

FL_TEST_CASE("Frame hooks observe replacement in a nested dispatch #5384") {
    FrameListeners fixture;
    for (int hook = 0; hook < 3; ++hook) {
        fl::vector<int> calls;
        RecordingListener first(calls, 1), second(calls, 2);
        auto cleanup = fl::make_scope_exit([&]() {
            EngineEvents::removeListener(&first);
            EngineEvents::removeListener(&second);
        });
        first.onCall = [&]() {
            EngineEvents::removeListener(&first);
            EngineEvents::addListener(&second);
            hooks[hook]();
        };
        EngineEvents::addListener(&first);

        hooks[hook]();
        FL_CHECK_EQ(calls, fl::vector<int>({10 + hook, 20 + hook}));
    }
}

FL_TEST_CASE("Multiple frame listeners retain priority and mutation snapshots #5384") {
    FrameListeners fixture;
    for (int hook = 0; hook < 3; ++hook) {
        fl::vector<int> calls;
        RecordingListener first(calls, 1), second(calls, 2), third(calls, 3);
        auto cleanup = fl::make_scope_exit([&]() {
            EngineEvents::removeListener(&first);
            EngineEvents::removeListener(&second);
            EngineEvents::removeListener(&third);
        });
        first.onCall = [&]() {
            EngineEvents::removeListener(&second);
            EngineEvents::addListener(&third);
        };
        EngineEvents::addListener(&second);
        EngineEvents::addListener(&first, 10);

        hooks[hook]();
        FL_CHECK_EQ(calls, fl::vector<int>({10 + hook, 20 + hook}));
        calls.clear();
        hooks[hook]();
        FL_CHECK_EQ(calls, fl::vector<int>({10 + hook, 30 + hook}));
    }
}

FL_TEST_CASE("Frame snapshots preserve callbacks across the stack limit #5384") {
    FrameListeners fixture;
    for (int hook = 0; hook < 3; ++hook) {
        fl::vector<int> calls;
        fl::vector<fl::shared_ptr<RecordingListener>> listeners;
        for (int i = 0; i < 17; ++i) {
            listeners.push_back(fl::make_shared<RecordingListener>(calls, i));
        }
        auto cleanup = fl::make_scope_exit([&]() {
            for (const auto& listener : listeners) {
                EngineEvents::removeListener(listener.get());
            }
        });
        listeners.front()->onCall = [&]() {
            EngineEvents::removeListener(listeners.back().get());
        };
        for (const auto& listener : listeners) {
            EngineEvents::addListener(listener.get());
        }

        hooks[hook]();
        FL_REQUIRE_EQ(calls.size(), 17u);
        for (int i = 0; i < 17; ++i) {
            FL_CHECK_EQ(calls[i], i * 10 + hook);
        }
        calls.clear();
        hooks[hook]();
        FL_REQUIRE_EQ(calls.size(), 16u);
        for (int i = 0; i < 16; ++i) {
            FL_CHECK_EQ(calls[i], i * 10 + hook);
        }
    }
}

} // FL_TEST_FILE
