
// g++ --std=c++11 test.cpp

#include "fl/rectangular_draw_buffer.h"
#include "rgbw.h"
#include "test.h"

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
};

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
        CHECK(buffer.mAllLedsBufferUint8Size == 60);

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
        uint8_t *all_leds = buffer.mAllLedsBufferUint8.get();
        uint32_t n_bytes = buffer.mAllLedsBufferUint8Size;
        for (size_t i = 0; i < n_bytes; i += 3) {
            if (i < slice1.size()) {
                REQUIRE(all_leds[i] == 0x1);
            } else {
                REQUIRE(all_leds[i] == 0x2);
            }
        }

        // bonus, test the slice::pop_front() works as expected, this time fill
        // with 0x3 and 0x4
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

    SUBCASE("Test that the order that the pins are added are preserved") {
        buffer.onQueuingStart();
        buffer.queue(DrawItem(2, 10, false));
        buffer.queue(DrawItem(1, 10, false));
        buffer.queue(DrawItem(3, 10, false));
        buffer.onQueuingDone();

        CHECK(buffer.mPinToLedSegment.size() == 3);
        CHECK(buffer.mAllLedsBufferUint8Size == 90);

        fl::Slice<uint8_t> slice1 = buffer.getLedsBufferBytesForPin(2, true);
        fl::Slice<uint8_t> slice2 = buffer.getLedsBufferBytesForPin(1, true);
        fl::Slice<uint8_t> slice3 = buffer.getLedsBufferBytesForPin(3, true);

        // Expect that the address of slice1 happens before slice2 in memory.
        CHECK(slice1.data() < slice2.data());
        CHECK(slice2.data() < slice3.data());

        // Check that the end byte of slice1 is the first byte of slice2
        CHECK(slice1.data() + slice1.size() == slice2.data());
        // Check that the end byte of slice2 is the first byte of slice3
        CHECK(slice2.data() + slice2.size() == slice3.data());
        // Check that the ptr of the first byte of slice1 is the same as the ptr
        // of the first byte of the buffer
        CHECK(slice1.data() == buffer.mAllLedsBufferUint8.get());
        // check that the start address is aligned to 4 bytes
        CHECK((reinterpret_cast<uintptr_t>(slice1.data()) & 0x3) == 0);
    }

    SUBCASE("Complex test where all strip data is confirmed to be inside the "
            "buffer block") {
        buffer.onQueuingStart();
        buffer.queue(DrawItem(1, 10, true));
        buffer.queue(DrawItem(2, 11, false));
        buffer.queue(DrawItem(3, 12, true));
        buffer.queue(DrawItem(4, 13, false));
        buffer.queue(DrawItem(5, 14, true));
        buffer.queue(DrawItem(6, 15, false));
        buffer.queue(DrawItem(7, 16, true));
        buffer.queue(DrawItem(8, 17, false));
        buffer.queue(DrawItem(9, 18, true));
        buffer.onQueuingDone();
        CHECK(buffer.mPinToLedSegment.size() == 9);

        uint32_t expected_max_strip_bytes = Rgbw::size_as_rgb(18) * 3;
        uint32_t actual_max_strip_bytes = buffer.getMaxBytesInStrip();
        CHECK(actual_max_strip_bytes == expected_max_strip_bytes);

        uint32_t expected_total_bytes = expected_max_strip_bytes * 9;
        uint32_t actual_total_bytes = buffer.getTotalBytes();
        CHECK(actual_total_bytes == expected_total_bytes);

        uint8_t pins[] = {1, 2, 3, 4, 5, 6, 7, 8, 9};
        for (uint8_t pin : pins) {
            fl::Slice<uint8_t> slice =
                buffer.getLedsBufferBytesForPin(pin, true);
            CHECK(slice.size() == expected_max_strip_bytes);
            const uint8_t *first_address = &slice.front();
            const uint8_t *last_address = &slice.back();
            // check that they are both in the buffer
            CHECK(first_address >= buffer.mAllLedsBufferUint8.get());
            CHECK(first_address <= buffer.mAllLedsBufferUint8.get() +
                                       buffer.mAllLedsBufferUint8Size);
            CHECK(last_address >= buffer.mAllLedsBufferUint8.get());
            CHECK(last_address <= buffer.mAllLedsBufferUint8.get() +
                                      buffer.mAllLedsBufferUint8Size);
        }
    }

    SUBCASE("I2S test where we load 16 X 256 leds") {
        buffer.onQueuingStart();
        for (int i = 0; i < 16; i++) {
            buffer.queue(DrawItem(i, 256, false));
        }
        buffer.onQueuingDone();
        CHECK(buffer.mPinToLedSegment.size() == 16);
        for (uint32_t i = 0; i < buffer.mAllLedsBufferUint8Size; ++i) {
            buffer.mAllLedsBufferUint8[i] = i % 256;
        }
    }
};
