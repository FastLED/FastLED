#include "fl/channels/presentation_timing.h"
#include "fl/channels/data.h"
#include "fl/channels/channel.h"
#include "fl/channels/config.h"
#include "fl/channels/driver.h"
#include "fl/channels/manager.h"
#include "fl/stl/vector.h"
#include "test.h"

FL_TEST_FILE(FL_FILEPATH) {

using namespace fl;

class FakeTimedDriver final : public IChannelDriver {
  public:
    explicit FakeTimedDriver(const char* driverName = "FAKE_TIMED_4517")
        : name(driverName) {}
    const char* name;
    ChannelDataPtr queued;
    PresentationToken copiedToken;
    fl::vector<PresentationEvent> events;
    size_t next = 0;
    bool measured = true;
    int pollCount = 0;

    void enqueue(ChannelDataPtr data) FL_NO_EXCEPT override {
        queued = data;
        copiedToken = data->presentationToken();
    }
    void show() FL_NO_EXCEPT override {}
    DriverState poll() FL_NO_EXCEPT override {
        ++pollCount;
        return DriverState::READY;
    }
    PresentationTimingCapability presentationTimingCapability() const FL_NO_EXCEPT override {
        return measured ? PresentationTimingCapability::MeasuredVisibility
                        : PresentationTimingCapability::Unknown;
    }
    bool takePresentationEvent(PresentationEvent& event) FL_NO_EXCEPT override {
        if (next == events.size()) {
            events.clear();
            next = 0;
            return false;
        }
        event = events[next++];
        return true;
    }
    fl::string getName() const FL_NO_EXCEPT override {
        return fl::string::from_literal(name);
    }
    Capabilities getCapabilities() const FL_NO_EXCEPT override {
        return Capabilities(true, false);
    }
    bool canHandle(const ChannelDataPtr&) const FL_NO_EXCEPT override { return true; }
};

struct RemovalState {
    fl::shared_ptr<FakeTimedDriver> driver;
    int events = 0;
    int destructions = 0;
};

class SelfRemovingTimingSink final : public IPresentationTimingSink {
  public:
    explicit SelfRemovingTimingSink(fl::shared_ptr<RemovalState> state)
        : mState(fl::move(state)) {}
    ~SelfRemovingTimingSink() FL_NO_EXCEPT override { ++mState->destructions; }
    void onPresentationEvent(const PresentationEvent&) FL_NO_EXCEPT override {
        ++mState->events;
        auto& manager = ChannelManager::instance();
        manager.setPresentationTimingSink(fl::shared_ptr<IPresentationTimingSink>());
        manager.removeDriver(mState->driver);
    }
  private:
    fl::shared_ptr<RemovalState> mState;
};

class RecordingTimingSink final : public IPresentationTimingSink {
  public:
    fl::vector<PresentationEvent> received;
    void onPresentationEvent(const PresentationEvent& event) FL_NO_EXCEPT override {
        received.push_back(event);
    }
};

FL_TEST_CASE("presentation timing distinguishes frame count from emitted dwell") {
    // Eight accepted frames: four bright, four dark. Count average is 1/2,
    // but bright frames remain visible 9x longer than dark ones.
    FakeTimedDriver driver;
    auto data = ChannelData::create(1, ChipsetTimingConfig(350, 700, 600, 50));
    PresentationTimeline timeline;
    u32 timestamp = 100;
    u32 brightDwell = 0;
    u32 totalDwell = 0;
    for (u32 i = 0; i < 8; ++i) {
        const PresentationToken token(1, i + 1);
        data->setPresentationToken(token);
        driver.enqueue(data);
        driver.show();
        PresentationEvent ended;
        driver.events.push_back(PresentationEvent::latched(driver.copiedToken, timestamp));
        const u32 dwell = (i & 1) == 0 ? 9 : 1;
        timestamp += dwell;
        driver.events.push_back(PresentationEvent::ended(driver.copiedToken, timestamp));
        PresentationEvent observed;
        FL_REQUIRE(driver.takePresentationEvent(observed));
        FL_CHECK(timeline.observe(observed, &ended));
        FL_REQUIRE(driver.takePresentationEvent(observed));
        FL_CHECK(timeline.observe(observed, &ended));
        FL_CHECK_EQ(ended.durationUs, dwell);
        totalDwell += ended.durationUs;
        if ((i & 1) == 0) brightDwell += ended.durationUs;
    }
    FL_CHECK_EQ(brightDwell * 10, totalDwell * 9); // 36/40, not 4/8
}

FL_TEST_CASE("presentation timing drops replacement and handles wrap") {
    PresentationTimeline timeline;
    PresentationEvent ended;
    const PresentationToken first(7, 11);
    const PresentationToken dropped(7, 12);
    const PresentationToken second(7, 13);
    FL_CHECK(timeline.observe(PresentationEvent::latched(first, 0xfffffff0u), &ended));
    FL_CHECK(timeline.observe(PresentationEvent::dropped(dropped, 0xfffffff5u), &ended));
    FL_CHECK_FALSE(ended.hasDuration);
    FL_CHECK(timeline.observe(PresentationEvent::latched(second, 0x10u), &ended));
    FL_CHECK(ended.hasDuration);
    FL_CHECK(ended.token == first);
    FL_CHECK_EQ(ended.durationUs, 32u);
    FL_CHECK(timeline.observe(PresentationEvent::ended(second, 0x30u), &ended));
    FL_CHECK_EQ(ended.durationUs, 32u);
    FL_CHECK_FALSE(timeline.observe(PresentationEvent::ended(first, 0x40u), &ended));
}

FL_TEST_CASE("fixed cadence preserves count-weighted emitted mean") {
    PresentationTimeline timeline;
    u32 brightDwell = 0;
    u32 totalDwell = 0;
    for (u32 frame = 1; frame <= 8; ++frame) {
        const PresentationToken token(4, frame);
        PresentationEvent completed;
        const u32 start = frame * 20;
        FL_CHECK(timeline.observe(PresentationEvent::latched(token, start), &completed));
        FL_CHECK(timeline.observe(PresentationEvent::ended(token, start + 20), &completed));
        totalDwell += completed.durationUs;
        if ((frame & 1) != 0) brightDwell += completed.durationUs;
    }
    FL_CHECK_EQ(brightDwell * 2, totalDwell);
}

FL_TEST_CASE("first channel id zero is a valid presentation source") {
    PresentationTimeline timeline;
    PresentationEvent completed;
    const PresentationToken firstChannel(0, 1);
    FL_CHECK(firstChannel.valid());
    FL_CHECK_FALSE(timeline.observe(PresentationEvent::ended(PresentationToken(9, 1), 5),
                                    &completed));
    FL_CHECK(timeline.observe(PresentationEvent::latched(firstChannel, 10), &completed));
    FL_CHECK(timeline.observe(PresentationEvent::ended(firstChannel, 25), &completed));
    FL_CHECK_EQ(completed.durationUs, 15u);
}

FL_TEST_CASE("presentation timeline rejects replay and out-of-order latch") {
    PresentationTimeline timeline;
    PresentationEvent completed;
    const PresentationToken first(1, 10);
    FL_CHECK(timeline.observe(PresentationEvent::latched(first, 10), &completed));
    FL_CHECK_FALSE(timeline.observe(PresentationEvent::latched(first, 11), &completed));
    FL_CHECK(timeline.observe(PresentationEvent::ended(first, 20), &completed));
    FL_CHECK_FALSE(timeline.observe(PresentationEvent::latched(first, 21), &completed));
    FL_CHECK_FALSE(timeline.observe(PresentationEvent::latched(PresentationToken(1, 9), 21), &completed));
    FL_CHECK(timeline.observe(PresentationEvent::latched(PresentationToken(1, 11), 30), &completed));

    PresentationTimeline wrapping;
    FL_CHECK(wrapping.observe(PresentationEvent::latched(PresentationToken(2, 0xffffffffu),
                                                        100), &completed));
    FL_CHECK(wrapping.observe(PresentationEvent::ended(PresentationToken(2, 0xffffffffu),
                                                      110), &completed));
    FL_CHECK(wrapping.observe(PresentationEvent::latched(PresentationToken(2, 1),
                                                        120), &completed));
    FL_CHECK_FALSE(wrapping.observe(PresentationEvent::latched(
                       PresentationToken(2, 0xffffffffu), 130), &completed));
}

FL_TEST_CASE("dropped submissions never latch but earlier queued frame may") {
    PresentationTimeline timeline;
    PresentationEvent completed;
    const PresentationToken older(6, 11);
    const PresentationToken dropped(6, 12);
    const PresentationToken newer(6, 13);
    FL_CHECK(timeline.observe(PresentationEvent::dropped(dropped, 5), &completed));
    FL_CHECK(timeline.observe(PresentationEvent::latched(older, 10), &completed));
    FL_CHECK(timeline.observe(PresentationEvent::ended(older, 20), &completed));
    FL_CHECK_FALSE(timeline.observe(PresentationEvent::latched(dropped, 21), &completed));
    FL_CHECK(timeline.observe(PresentationEvent::latched(newer, 30), &completed));

    PresentationTimeline sparse;
    FL_CHECK(sparse.observe(PresentationEvent::dropped(PresentationToken(8, 12), 1),
                            &completed));
    FL_CHECK(sparse.observe(PresentationEvent::dropped(PresentationToken(8, 14), 2),
                            &completed));
    FL_CHECK(sparse.observe(PresentationEvent::latched(PresentationToken(8, 11), 10),
                            &completed));
    FL_CHECK(sparse.observe(PresentationEvent::latched(PresentationToken(8, 13), 20),
                            &completed));
    FL_CHECK_FALSE(sparse.observe(PresentationEvent::latched(PresentationToken(8, 14), 30),
                                  &completed));
    FL_CHECK(sparse.observe(PresentationEvent::latched(PresentationToken(8, 15), 30),
                            &completed));
}

FL_TEST_CASE("too many future drops fail closed without forgetting a tombstone") {
    PresentationTimeline timeline;
    PresentationEvent completed;
    for (u32 frame = 1; frame <= 8; ++frame) {
        FL_CHECK(timeline.observe(PresentationEvent::dropped(PresentationToken(9, frame),
                                                              frame), &completed));
    }
    FL_CHECK_FALSE(timeline.observe(PresentationEvent::dropped(PresentationToken(9, 9),
                                                               9), &completed));
    FL_CHECK_FALSE(timeline.observe(PresentationEvent::latched(PresentationToken(9, 1),
                                                               10), &completed));
}

FL_TEST_CASE("fake async driver reports only measured latch, drop and end") {
    auto driver = fl::make_shared<FakeTimedDriver>();
    auto& manager = ChannelManager::instance();
    manager.addDriver(999, driver);
    auto sink = fl::make_shared<RecordingTimingSink>();
    manager.setPresentationTimingSink(sink);

    auto data = ChannelData::create(1, ChipsetTimingConfig(350, 700, 600, 50));
    data->setPresentationToken(PresentationToken(1, 101));
    driver->enqueue(data);
    driver->show();
    manager.poll();
    FL_CHECK(sink->received.empty()); // Queue/show/READY are not visibility.
    data->setPresentationToken(PresentationToken(1, 102));
    FL_CHECK_EQ(driver->copiedToken.frame, 101u); // Async snapshot is stable.
    const PresentationToken a(1, 101), b(2, 8), c(1, 102);
    driver->events.push_back(PresentationEvent::latched(a, 100));
    driver->events.push_back(PresentationEvent::latched(b, 105));
    driver->events.push_back(PresentationEvent::dropped(c, 108));
    driver->events.push_back(PresentationEvent::ended(a, 120));
    driver->events.push_back(PresentationEvent::ended(b, 130));
    driver->measured = false;
    manager.poll();
    FL_CHECK(sink->received.empty()); // Unknown-capability events must not be forwarded.
    driver->measured = true;
    manager.poll();
    FL_CHECK_EQ(sink->received.size(), 5u);
    if (sink->received.size() == 5u) {
        FL_CHECK(sink->received[0].token == a);
        FL_CHECK(sink->received[1].token == b);
        FL_CHECK(sink->received[2].kind == PresentationKind::Dropped);
        FL_CHECK(sink->received[3].token == a);
        FL_CHECK(sink->received[4].token == b);
    }

    manager.setPresentationTimingSink(fl::shared_ptr<IPresentationTimingSink>());
    manager.removeDriver(driver);
}

FL_TEST_CASE("presentation poll bounds drain and survives callback reconfiguration") {
    auto& manager = ChannelManager::instance();
    auto driver = fl::make_shared<FakeTimedDriver>("FAKE_TIMED_BUDGET_4517");
    manager.addDriver(1001, driver);
    auto sink = fl::make_shared<RecordingTimingSink>();
    manager.setPresentationTimingSink(sink);
    for (u32 frame = 1; frame <= 65; ++frame) {
        driver->events.push_back(PresentationEvent::dropped(PresentationToken(3, frame), frame));
    }
    manager.poll();
    FL_CHECK_EQ(sink->received.size(), 64u);
    manager.poll();
    FL_CHECK_EQ(sink->received.size(), 65u);
    manager.setPresentationTimingSink(fl::shared_ptr<IPresentationTimingSink>());
    manager.removeDriver(driver);

    auto first = fl::make_shared<FakeTimedDriver>("FAKE_TIMED_REMOVE_4517");
    auto second = fl::make_shared<FakeTimedDriver>("FAKE_TIMED_AFTER_4517");
    manager.addDriver(1002, first);
    manager.addDriver(1001, second);
    auto state = fl::make_shared<RemovalState>();
    state->driver = first;
    manager.setPresentationTimingSink(fl::make_shared<SelfRemovingTimingSink>(state));
    first->events.push_back(PresentationEvent::latched(PresentationToken(5, 1), 100));
    manager.poll();
    FL_CHECK_EQ(state->events, 1);
    FL_CHECK_EQ(state->destructions, 1);
    FL_CHECK_EQ(second->pollCount, 1);
    manager.removeDriver(second);
}

FL_TEST_CASE("unknown timing driver has no fabricated observations") {
    FakeTimedDriver driver;
    PresentationEvent event;
    FL_CHECK(driver.IChannelDriver::presentationTimingCapability() ==
             PresentationTimingCapability::Unknown);
    FL_CHECK_FALSE(driver.IChannelDriver::takePresentationEvent(event));
    PresentationTimeline timeline;
    FL_CHECK_FALSE(timeline.observe(PresentationEvent(), &event));
}

FL_TEST_CASE("channel assigns distinct stable tokens to accepted submissions") {
    auto driver = fl::make_shared<FakeTimedDriver>();
    auto& manager = ChannelManager::instance();
    manager.addDriver(100000, driver);
    CRGB leds[1] = {CRGB::Red};
    ChannelOptions options;
    ChannelConfig config(1, ChipsetTimingConfig(350, 700, 600, 50),
                         fl::span<CRGB>(leds, 1), RGB, options);
    auto channel = Channel::create(config);
    channel->showLeds(255);
    FL_CHECK_EQ(driver->copiedToken.channelId, channel->id());
    FL_CHECK_EQ(driver->copiedToken.frame, 1u);
    channel->showLeds(255);
    FL_CHECK_EQ(driver->copiedToken.frame, 2u);
    manager.removeDriver(driver);
}

} // FL_TEST_FILE
