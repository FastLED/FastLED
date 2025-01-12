
// g++ --std=c++11 test.cpp

#include "test.h"
#include "fl/rectangular_draw_buffer.h"


using namespace fl;

TEST_CASE("Rectangular Buffer") {
    RectangularDrawBuffer buffer;

    SUBCASE("Empty buffer has no LEDs") {
        CHECK(buffer.getTotalBytes() == 0);
        CHECK(buffer.getMaxBytesInStrip() == 0);
    }

    SUBCASE("Add one strip of 10 RGB LEDs") {
        buffer.queue(DrawItem(1, 10, false));

        CHECK(buffer.getMaxBytesInStrip() == 30);
        CHECK(buffer.getTotalBytes() == 30);
    }

    SUBCASE("Add two strips of 10 RGB LEDs") {
        buffer.queue(DrawItem(1, 10, false));
        buffer.queue(DrawItem(2, 10, false));

        CHECK(buffer.getMaxBytesInStrip() == 30);
        CHECK(buffer.getTotalBytes() == 60);
    }

    SUBCASE("Add one strip of 10 RGBW LEDs") {
        buffer.queue(DrawItem(1, 10, true));

        uint32_t num_bytes = Rgbw::size_as_rgb(10) * 3;

        CHECK(buffer.getMaxBytesInStrip() == num_bytes);
        CHECK(buffer.getTotalBytes() == num_bytes);
    }

    SUBCASE("Add one strip of 10 RGBW LEDs and one strip of 10 RGB LEDs") {
        buffer.queue(DrawItem(1, 10, true));
        buffer.queue(DrawItem(2, 10, false));

        uint32_t max_size_strip_bytes = Rgbw::size_as_rgb(10) * 3;

        CHECK(buffer.getMaxBytesInStrip() == max_size_strip_bytes);
        CHECK(buffer.getTotalBytes() == max_size_strip_bytes * 2);
    }
}


TEST_CASE("Rectangular Buffer queue tests") {
    RectangularDrawBuffer buffer;

    SUBCASE("Queueing start and done") {
        CHECK(buffer.mQueueState == RectangularDrawBuffer::IDLE);
        buffer.onQueuingStart();
        CHECK(buffer.mQueueState == RectangularDrawBuffer::QUEUEING);
        buffer.onQueuingDone();
        CHECK(buffer.mQueueState == RectangularDrawBuffer::QUEUE_DONE);
        buffer.onQueuingStart();
        CHECK(buffer.mQueueState == RectangularDrawBuffer::QUEUEING);
    }

    SUBCASE("Queue and then draw") {
        buffer.onQueuingStart();
        buffer.queue(DrawItem(1, 10, false));
        buffer.queue(DrawItem(2, 10, false));
        buffer.onQueuingDone();

        CHECK(buffer.mPinToLedSegment.size() == 2);
        CHECK(buffer.mAllLedsBufferUint8.size() == 60);

        fl::Slice<uint8_t> slice1 = buffer.getLedsBufferBytesForPin(1, true);
        fl::Slice<uint8_t> slice2 = buffer.getLedsBufferBytesForPin(2, true);
        // Expect that the address of slice1 happens before slice2 in memory.
        CHECK(slice1.data() < slice2.data());
        // Check that the size of each slice is 30 bytes.
        CHECK(slice1.size() == 30);
        CHECK(slice2.size() == 30);
        // Check that the uint8_t buffer is zeroed out.
        for (size_t i = 0; i < slice1.size(); ++i) {
            REQUIRE(slice1[i] == 0);
            REQUIRE(slice2[i] == 0);
        }
        // now fill slice1 with 0x1, slice2 with 0x2
        for (size_t i = 0; i < slice1.size(); i += 3) {
            slice1[i] = 0x1;
            slice2[i] = 0x2;
        }
        // Check that the uint8_t buffer is filled with 0x1 and 0x2.
        auto& all_leds = buffer.mAllLedsBufferUint8;
        for (size_t i = 0; i < all_leds.size(); i += 3) {
            if (i < slice1.size()) {
                REQUIRE(all_leds[i] == 0x1);
            } else {
                REQUIRE(all_leds[i] == 0x2);
            }
        }

        // bonus, test the slice::pop_front() works as expected, this time fill with 0x3 and 0x4
        while (!slice1.empty()) {
            slice1[0] = 0x3;
            slice1.pop_front();
        }

        while (!slice2.empty()) {
            slice2[0] = 0x4;
            slice2.pop_front();
        }

        // Check that the uint8_t buffer is filled with 0x3 and 0x4.
        for (size_t i = 0; i < 60; ++i) {
            if (i < 30) {
                REQUIRE(all_leds[i] == 0x3);
            } else {
                REQUIRE(all_leds[i] == 0x4);
            }
        }
    }
}
