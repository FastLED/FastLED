

// g++ --std=c++11 test.cpp

#include "test.h"

#include "test.h"
#include "fx/frame.h"
#include "fx/video/frame_interpolator.h"
#include "fl/namespace.h"

#include "fl/namespace.h"
FASTLED_USING_NAMESPACE


#if 0

TEST_CASE("FrameInterpolator::selectFrames") {
    SUBCASE("Empty interpolator") {
        FrameInterpolator interpolator(5, -1);
        const Frame *selected1;
        const Frame *selected2;
        CHECK_FALSE(interpolator.selectFrames(0, &selected1, &selected2));
    }

    SUBCASE("2 frame interpolator before") {
        // Create an interpolator with capacity for 2 frames
        FrameInterpolator interpolator(2, -1);

        // Create some test frames with different timestamps
        FrameRef frame1 = FrameRef::New(10, false); // 10 pixels, no alpha
        FrameRef frame2 = FrameRef::New(10, false);



        // Add frames with timestamps
        CHECK(interpolator.push_front(frame1, 0, 1000));
        CHECK(interpolator.push_front(frame2, 1, 2000));

        const Frame *selected1;
        const Frame *selected2;

        // Falls between two frames->
        bool selected = interpolator.selectFrames(0, &selected1, &selected2);
        CHECK(selected);
        CHECK(selected1);
        CHECK(selected2);
        // Now check that it's the same frame.
        CHECK(selected1 == selected2);
        // now check that the timestamp of the first frame is less than the
        // timestamp of the second frame
        CHECK(selected1->getTimestamp() == 1000);
        CHECK(selected2->getTimestamp() == 1000);
    }

    SUBCASE("2 frame interpolator between") {
        // Create an interpolator with capacity for 2 frames
        FrameInterpolator interpolator(2, -1);

        // Create some test frames with different timestamps
        FrameRef frame1 = FrameRef::New(10, false); // 10 pixels, no alpha
        FrameRef frame2 = FrameRef::New(10, false);

        // Add frames with timestamps
        //CHECK(interpolator.addWithTimestamp(frame1, 0));
        //CHECK(interpolator.addWithTimestamp(frame2, 1000));
        CHECK(interpolator.push_front(frame1, 0, 1000));
        CHECK(interpolator.push_front(frame2, 1, 2000));
        

        const Frame *selected1;
        const Frame *selected2;

        // Falls between two frames->
        bool selected = interpolator.selectFrames(500, &selected1, &selected2);
        CHECK(selected);
        CHECK(selected1);
        CHECK(selected2);
        // now check that the frames are different
        CHECK(selected1 != selected2);
        // now check that the timestamp of the first frame is less than the
        // timestamp of the second frame
        CHECK(selected1->getTimestamp() == 0);
        CHECK(selected2->getTimestamp() == 1000);
    }

    SUBCASE("2 frame interpolator after") {
        // Create an interpolator with capacity for 2 frames
        FrameInterpolator interpolator(2, -1);

        // Create some test frames with different timestamps
        FrameRef frame1 = FrameRef::New(10, false); // 10 pixels, no alpha
        FrameRef frame2 = FrameRef::New(10, false);

        // Add frames with timestamps
        CHECK(interpolator.push_front(frame1, 0, 0));
        CHECK(interpolator.push_front(frame2, 1, 1000));

        const Frame *selected1;
        const Frame *selected2;

        // Falls between two frames->
        bool selected = interpolator.selectFrames(1500, &selected1, &selected2);
        CHECK(selected);
        CHECK(selected1);
        CHECK(selected2);
        // now check that the frames are different
        CHECK(selected1 == selected2);
        // now check that the timestamp of the first frame is less than the
        // timestamp of the second frame
        CHECK(selected1->getTimestamp() == 1000);
        CHECK(selected2->getTimestamp() == 1000);
    }
}

TEST_CASE("FrameInterpolator::addWithTimestamp") {
    SUBCASE("add first frame") {
        FrameInterpolator interpolator(5, -1);
        FrameRef frame = FrameRef::New(10, false);
        CHECK(interpolator.push_front(frame, 0, 1000));
        FrameInterpolator::FrameBuffer* frames = interpolator.getFrames();
        CHECK_EQ(frames->size(), 1);
        CHECK_EQ(frames->front().frame->getTimestamp(), 1000);
    }

    SUBCASE("add second frame which is before first frame and should be rejected") {
        FrameInterpolator interpolator(5, -1);
        FrameRef frame1 = FrameRef::New(10, false);
        FrameRef frame2 = FrameRef::New(10, false);
        CHECK(interpolator.push_front(frame1, 0, 1000));
        CHECK_FALSE(interpolator.push_front(frame2, 1, 500));
        FrameInterpolator::FrameBuffer* frames = interpolator.getFrames();
        CHECK_EQ(frames->size(), 1);
        CHECK_EQ(frames->front().frame->getTimestamp(), 1000);
    }

    
    SUBCASE("add second frame which has the same timestamp as first frame and should be rejected") {
        FrameInterpolator interpolator(5, -1);
        FrameRef frame1 = FrameRef::New(10, false);
        FrameRef frame2 = FrameRef::New(10, false);
        CHECK(interpolator.push_front(frame1, 0, 1000));
        CHECK_FALSE(interpolator.push_front(frame2, 1, 1000));
        FrameInterpolator::FrameBuffer* frames = interpolator.getFrames();
        CHECK_EQ(frames->size(), 1);
        CHECK_EQ(frames->front().frame->getTimestamp(), 1000);
    }

    SUBCASE("add second frame which is after first frame and should be accepted") {
        FrameInterpolator interpolator(5, -1);
        FrameRef frame1 = FrameRef::New(10, false);
        FrameRef frame2 = FrameRef::New(10, false);
        CHECK(interpolator.push_front(frame1, 0, 1000));
        CHECK(interpolator.push_front(frame2, 1, 1500));
        FrameInterpolator::FrameBuffer* frames = interpolator.getFrames();
        CHECK_EQ(frames->size(), 2);
        CHECK_EQ(frames->front().frame->getTimestamp(), 1500);
        CHECK_EQ(frames->back().frame->getTimestamp(), 1000);
    }

}

TEST_CASE("FrameInterpolator::addWithTimestamp and overflow") {
    SUBCASE("add two frames and check time") {
        FrameInterpolator interpolator(2, -1);
        FrameRef frame = FrameRef::New(10, false);
        CHECK(interpolator.push_front(frame, 0, 1000));
        CHECK(interpolator.push_front(frame, 1, 2000));
        CHECK(interpolator.push_front(frame, 2, 3000));
        FrameInterpolator::FrameBuffer* frames = interpolator.getFrames();
        CHECK_EQ(frames->size(), 2);
        CHECK_EQ(frames->front().frame->getTimestamp(), 3000);
        CHECK_EQ(frames->back().frame->getTimestamp(), 2000);
    }

    SUBCASE("add two frames and check that Frame object was recycled") {
        FrameInterpolator interpolator(2, -1);
        FrameInterpolator::FrameBuffer* frames = interpolator.getFrames();
        CHECK_EQ(2, frames->capacity());
        CHECK_EQ(0, frames->size());
        FrameRef frame = FrameRef::New(2, false);
        CHECK(interpolator.push_front(frame, 0, 1000));
        CHECK_EQ(2, frames->capacity());
        CHECK_EQ(1, frames->size());

        CHECK(interpolator.push_front(frame, 1, 2000));
        CHECK_EQ(2, frames->capacity());
        CHECK_EQ(2, frames->size());
        CHECK(frames->full());

        Frame* frameThatShouldBeRecycled = frames->back().frame.get();
        CHECK(interpolator.push_front(frame, 2, 3000));
        CHECK_EQ(frames->size(), 2);
        CHECK_EQ(frames->front().frame->getTimestamp(), 3000);
        // check pointers are equal
        CHECK(frames->front().frame == frameThatShouldBeRecycled);
    }
}


TEST_CASE("FrameInterpolator::draw") {
    SUBCASE("Empty interpolator") {
        FrameInterpolator interpolator(5, -1);
        FrameRef frame = FrameRef::New(10, false);
        FrameRef dst = FrameRef::New(10, false);
        CHECK_FALSE(interpolator.draw(0, dst.get()));
    }

    SUBCASE("Add one frame and check that we will draw with that") {
        FrameInterpolator interpolator(5, -1);
        FrameRef frame = FrameRef::New(10, false);
        CHECK(interpolator.push_front(frame, 0, 1000));
        FrameRef dst = FrameRef::New(10, false);
        CHECK(interpolator.draw(0, dst.get()));
        CHECK_EQ(dst->getTimestamp(), 1000);
        CHECK(interpolator.draw(2000, dst.get()));
        CHECK_EQ(dst->getTimestamp(), 1000);
    }

    SUBCASE("Add two frames and check behavior for drawing before, between and after") {
        FrameInterpolator interpolator(5, -1);
        FrameRef frame1 = FrameRef::New(10, false);
        FrameRef frame2 = FrameRef::New(10, false);
        CHECK(interpolator.push_front(frame1, 0, 1000));
        CHECK(interpolator.push_front(frame2, 1, 2000));
        FrameRef dst = FrameRef::New(10, false);
        CHECK(interpolator.draw(0, dst.get()));
        CHECK_EQ(dst->getTimestamp(), 1000);
        CHECK(interpolator.draw(1500, dst.get()));
        CHECK_EQ(dst->getTimestamp(), 1500);
        CHECK(interpolator.draw(2500, dst.get()));
        CHECK_EQ(dst->getTimestamp(), 2000);
    }

    SUBCASE("Add three frames and check behavior for drawing before, between 0&1, between 1&2 and after") {
        FrameInterpolator interpolator(5, -1);
        FrameRef frame1 = FrameRef::New(10, false);
        FrameRef frame2 = FrameRef::New(10, false);
        FrameRef frame3 = FrameRef::New(10, false);
        CHECK(interpolator.push_front(frame1, 0, 1000));
        CHECK(interpolator.push_front(frame2, 1, 2000));
        CHECK(interpolator.push_front(frame3, 2, 3000));
        FrameRef dst = FrameRef::New(10, false);
        CHECK(interpolator.draw(0, dst.get()));
        CHECK_EQ(dst->getTimestamp(), 1000);
        // now check what happens when we draw exactly at 1000
        CHECK(interpolator.draw(1000, dst.get()));
        CHECK_EQ(dst->getTimestamp(), 1000);
        CHECK(interpolator.draw(1500, dst.get()));
        CHECK_EQ(dst->getTimestamp(), 1500);
        // now check what happens when we draw exactly at 2000
        CHECK(interpolator.draw(2000, dst.get()));
        CHECK_EQ(dst->getTimestamp(), 2000);
        CHECK(interpolator.draw(2500, dst.get()));
        CHECK_EQ(dst->getTimestamp(), 2500);
        CHECK(interpolator.draw(3500, dst.get()));
        CHECK_EQ(dst->getTimestamp(), 3000);
    }

    SUBCASE("Check that the draw command interpolates between two added frames when queried from the middle") {
        FrameInterpolator interpolator(5, -1);
        FrameRef frame1 = FrameRef::New(10, false);
        FrameRef frame2 = FrameRef::New(10, false);
        // frame 1 is all red
        for (int i = 0; i < 10; i++) {
            frame1->rgb()[i] = CRGB::Red;
        }
        // frame 2 is all blue
        for (int i = 0; i < 10; i++) {
            frame2->rgb()[i] = CRGB::Blue;
        }
        CHECK(interpolator.push_front(frame1, 0, 1000));
        CHECK(interpolator.push_front(frame2, 1, 2000));
        FrameRef dst = FrameRef::New(10, false);
        CHECK(interpolator.draw(1500, dst.get()));
        CHECK_EQ(dst->getTimestamp(), 1500);
        // now check that the frame is interpolated between red and blue
        for (int i = 0; i < 10; i++) {
            CHECK(dst->rgb()[i] == CRGB(128, 0, 127));
        }
    }
}

#endif
