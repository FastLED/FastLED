#include "fl/system/engine_events.h"
#include "fl/stl/span.h"
#include "fl/stl/vector.h"
#include "test.h"

FL_TEST_FILE(FL_FILEPATH) {

namespace {

class RecordingListener : public fl::EngineEvents::Listener {
public:
    int id = 0;
    fl::vector<int>* calls = nullptr;

    ~RecordingListener() override { fl::EngineEvents::removeListener(this); }
    void onBeginFrame() override { calls->push_back(id); }
    void onEndShowLeds() override { calls->push_back(id); }
};

class MutatingListener : public RecordingListener {
public:
    RecordingListener* removed = nullptr;
    fl::span<RecordingListener> additions;
    bool mutated = false;

    void mutate() {
        if (mutated) {
            return;
        }
        mutated = true;
        // Objects stay alive until dispatch finishes: only registration changes.
        fl::EngineEvents::removeListener(removed);
        for (auto& listener : additions) {
            fl::EngineEvents::addListener(&listener, 100);
        }
    }

    void onBeginFrame() override {
        RecordingListener::onBeginFrame();
        mutate();
    }
    void onEndShowLeds() override {
        RecordingListener::onEndShowLeds();
        mutate();
    }
};

} // namespace

FL_TEST_CASE("EngineEvents spills with stable priorities and rejects duplicates") {
    fl::vector<int> calls;
    RecordingListener listeners[20];
    for (int i = 0; i < 20; ++i) {
        listeners[i].id = i;
        listeners[i].calls = &calls;
        fl::EngineEvents::addListener(&listeners[i], i % 3);
    }
    // Duplicate registration neither adds an invocation nor changes priority.
    fl::EngineEvents::addListener(&listeners[0], 100);
    fl::EngineEvents::onBeginFrame();
    FL_REQUIRE_EQ(calls.size(), 20u);
    fl::size index = 0;
    for (int priority = 2; priority >= 0; --priority) {
        for (int i = priority; i < 20; i += 3) {
            FL_CHECK_EQ(calls[index++], i);
        }
    }
}

FL_TEST_CASE("EngineEvents snapshots survive listener removal and registry growth") {
    bool endShow = false;
    FL_SUBCASE("begin frame") { endShow = false; }
    FL_SUBCASE("end show") { endShow = true; }

    fl::vector<int> calls;
    RecordingListener listeners[19];
    RecordingListener additions[20];
    for (int i = 0; i < 19; ++i) {
        listeners[i].id = i;
        listeners[i].calls = &calls;
        fl::EngineEvents::addListener(&listeners[i]);
    }
    for (int i = 0; i < 20; ++i) {
        additions[i].id = 20 + i;
        additions[i].calls = &calls;
    }
    MutatingListener mutator;
    mutator.id = 19;
    mutator.calls = &calls;
    mutator.removed = &listeners[18];
    mutator.additions = additions;
    fl::EngineEvents::addListener(&mutator, 50);

    const auto dispatch = [endShow]() {
        if (endShow) {
            fl::EngineEvents::onEndShowLeds();
        } else {
            fl::EngineEvents::onBeginFrame();
        }
    };
    dispatch();
    FL_REQUIRE_EQ(calls.size(), 20u);
    FL_CHECK_EQ(calls[0], 19);
    for (int i = 0; i < 19; ++i) {
        FL_CHECK_EQ(calls[i + 1], i);
    }
    FL_CHECK_FALSE(fl::EngineEvents::hasListener(&listeners[18]));

    calls.clear();
    dispatch();
    FL_REQUIRE_EQ(calls.size(), 39u);
    for (int i = 0; i < 20; ++i) {
        FL_CHECK_EQ(calls[i], 20 + i);
    }
    FL_CHECK_EQ(calls[20], 19);
    for (int i = 0; i < 18; ++i) {
        FL_CHECK_EQ(calls[i + 21], i);
    }
}

} // FL_TEST_FILE
