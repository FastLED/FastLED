#include "FastLED.h"
#include "platforms/esp/32/drivers/rmt/rmt_4/rmt4_frame_queue.h"
#include "test.h"

using namespace fl;

namespace {

class RejectQueueAllocation : public memory_resource {
protected:
    void *do_allocate(fl::size) FL_NO_EXCEPT override { return nullptr; }
    void do_deallocate(void *, fl::size) FL_NO_EXCEPT override {}
};

ChannelDataPtr makeFrame(int pin) {
    return ChannelData::create(
        pin, ChipsetTimingConfig(800, 450, 450, 50, "WS2812"));
}

} // namespace

FL_TEST_CASE("Rmt4FrameQueue leases buffers before returning from enqueue") {
    Rmt4FrameQueue queue;
    auto frame = makeFrame(5);
    FL_CHECK_TRUE(queue.empty());
    FL_CHECK_FALSE(queue.enqueue(ChannelDataPtr()));
    FL_REQUIRE_TRUE(queue.enqueue(frame));
    FL_CHECK_TRUE(frame->isInUse());
    FL_CHECK_FALSE(queue.enqueue(frame));
    FL_REQUIRE_EQ(queue.size(), 1u);
    FL_CHECK_EQ(queue[0].get(), frame.get());
    FL_CHECK_EQ(queue.at(0).get(), frame.get());
    queue.clear();
    FL_CHECK_TRUE(queue.empty());
    FL_CHECK_FALSE(frame->isInUse());
}

FL_TEST_CASE("Rmt4FrameQueue take transfers a lease without releasing it") {
    auto first = makeFrame(5);
    auto second = makeFrame(6);
    ChannelDataPtr taken;
    {
        Rmt4FrameQueue queue;
        FL_REQUIRE_TRUE(queue.enqueue(first));
        FL_REQUIRE_TRUE(queue.enqueue(second));
        taken = queue.take(0);
        FL_REQUIRE_TRUE(bool(taken));
        FL_CHECK_EQ(taken.get(), first.get());
        FL_CHECK_TRUE(first->isInUse());
        FL_REQUIRE_EQ(queue.size(), 1u);
        FL_CHECK_EQ(queue[0].get(), second.get());
    }
    FL_CHECK_TRUE(first->isInUse());
    FL_CHECK_FALSE(second->isInUse());
    taken->setInUse(false);
}

FL_TEST_CASE("Rmt4FrameQueue append preserves leases and supports queued strips") {
    Rmt4FrameQueue destination;
    auto first = makeFrame(5);
    FL_REQUIRE_TRUE(destination.enqueue(first));
    {
        Rmt4FrameQueue source;
        for (int pin = 6; pin < 25; ++pin) {
            FL_REQUIRE_TRUE(source.enqueue(makeFrame(pin)));
        }
        destination.appendFrom(source);
        FL_CHECK_TRUE(source.empty());
    }
    FL_CHECK_EQ(destination.size(), 20u);
    for (size_t i = 0; i < destination.size(); ++i) {
        FL_CHECK_TRUE(destination[i]->isInUse());
        FL_CHECK_EQ(destination[i]->getPin(), int(i) + 5);
    }
    destination.appendFrom(destination);
    FL_CHECK_EQ(destination.size(), 20u);
    destination.clear();
    FL_CHECK_FALSE(first->isInUse());
}

FL_TEST_CASE("Rmt4FrameQueue adopts a transferred lease") {
    Rmt4FrameQueue source;
    Rmt4FrameQueue destination;
    auto frame = makeFrame(5);
    FL_REQUIRE_TRUE(source.enqueue(frame));
    auto transferred = source.take(0);
    FL_CHECK_TRUE(frame->isInUse());
    destination.adopt(fl::move(transferred));
    FL_CHECK_TRUE(source.empty());
    FL_CHECK_TRUE(frame->isInUse());
    FL_CHECK_EQ(destination.size(), 1u);
    destination.clear();
    FL_CHECK_FALSE(frame->isInUse());
}

FL_TEST_CASE("Rmt4FrameQueue failed growth never leases an unqueued frame") {
    RejectQueueAllocation resource;
    Rmt4FrameQueue queue(&resource);
    for (int pin = 0; pin < 16; ++pin) {
        FL_REQUIRE_TRUE(queue.enqueue(makeFrame(pin)));
    }
    auto rejected = makeFrame(16);
    FL_CHECK_FALSE(queue.enqueue(rejected));
    FL_CHECK_FALSE(rejected->isInUse());
    FL_CHECK_EQ(queue.size(), 16u);
}

FL_TEST_CASE("Rmt4FrameQueue failed batch transfer preserves both owners") {
    RejectQueueAllocation resource;
    Rmt4FrameQueue destination(&resource);
    for (int pin = 0; pin < 16; ++pin) {
        FL_REQUIRE_TRUE(destination.enqueue(makeFrame(pin)));
    }
    Rmt4FrameQueue source;
    auto frame = makeFrame(16);
    FL_REQUIRE_TRUE(source.enqueue(frame));
    FL_CHECK_FALSE(destination.appendFrom(source));
    FL_CHECK_EQ(destination.size(), 16u);
    FL_REQUIRE_EQ(source.size(), 1u);
    FL_CHECK_EQ(source[0].get(), frame.get());
    FL_CHECK_TRUE(frame->isInUse());
    source.clear();
    FL_CHECK_FALSE(frame->isInUse());
}

FL_TEST_CASE("Rmt4FrameQueue failed adoption releases its transferred lease") {
    RejectQueueAllocation resource;
    Rmt4FrameQueue destination(&resource);
    for (int pin = 0; pin < 16; ++pin) {
        FL_REQUIRE_TRUE(destination.enqueue(makeFrame(pin)));
    }
    Rmt4FrameQueue source;
    auto frame = makeFrame(16);
    FL_REQUIRE_TRUE(source.enqueue(frame));
    FL_CHECK_FALSE(destination.adopt(source.take(0)));
    FL_CHECK_TRUE(source.empty());
    FL_CHECK_FALSE(frame->isInUse());
    FL_CHECK_EQ(destination.size(), 16u);
}
