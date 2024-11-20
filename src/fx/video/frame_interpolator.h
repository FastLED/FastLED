#pragma once

#include "fx/detail/data_stream.h"
#include "fx/frame.h"
#include "namespace.h"
#include "fx/detail/circular_buffer.h"
#include "fx/video/frame_tracker.h"

FASTLED_NAMESPACE_BEGIN

FASTLED_SMART_REF(FrameInterpolator);

// Holds onto frames and allow interpolation. This allows
// effects to have high effective frame rate and also
// respond to things like sound which can modify the timing.
class FrameInterpolator : public Referent {
public:

    struct Pair {
        uint32_t frameNumber;  // Lit bit faster to inline.
        FrameRef frame;
    };
    typedef CircularBuffer<Pair> FrameBuffer;
    FrameInterpolator(size_t nframes, float fpsVideo);

    // Will search through the array, select the two frames that are closest to the current time
    // and then interpolate between them, storing the results in the provided frame.
    // The destination frame will have "now" as the current timestamp if and only if
    // there are two frames that can be interpolated. Else it's set to the timestamp of the
    // frame that was selected.
    // Returns true if the interpolation was successful, false otherwise. If false then
    // the destination frame will not be modified.
    // Note that this adjustable_time is allowed to go pause or go backward in time. 
    bool draw(uint32_t adjustable_time, Frame* dst);  // Frame has alpha.
    bool draw(uint32_t adjustable_time, CRGB* leds, uint8_t* alpha = nullptr);

    // Used for recycling externally.
    bool pop_back(FrameRef* dst) {
        Pair pair;
        if (!mFrames.pop_back(&pair)) {
            return false;
        }
        *dst = pair.frame;
        return true;
    }

    // push the frame if and only if
    // 1. The frame is the next frame in the sequence.
    // 2. The frame is the newest frame.
    bool push_front(FrameRef frame, uint32_t frameNumber, uint32_t timestamp) {
        if (mFrames.full()) {
            // we are in an invalid state. We don't want to pop off frames unless
            // the caller supplies us with a destination to put the frame so the memory
            // block can be reused.
            return false;
        }
        if (mFrames.empty()) {
            frame->setFrameNumberAndTime(frameNumber, timestamp);
            Pair new_entry = {frameNumber, frame};
            mFrames.push_front(new_entry);
            return true;
        }
        if (!mFrames.empty()) {
            auto& front = mFrames.front();
            // auto& front_frame = front.frame;
            uint32_t front_frameNumber = front.frameNumber;
            if (front_frameNumber+1 == frameNumber) {
                // we got a new front frame and we have the capacity for it.
                Pair new_entry = {frameNumber, frame};
                frame->setFrameNumberAndTime(frameNumber, timestamp);
                mFrames.push_front(new_entry);
                return true;
            }
            // how did we end up here? This is an invalid state.
            return false;
        }
        return false;
    }

    // Clear all frames
    void clear() {
        mFrames.clear();
    }

    bool empty() const { return mFrames.empty(); }

    bool has(uint32_t frameNum) const {
        for (uint32_t i = 0; i < mFrames.size(); ++i) {
            if (mFrames[i].frameNumber == frameNum) {
                return true;
            }
        }
        return false;
    }

    FrameRef get(uint32_t frameNum) const {
        for (uint32_t i = 0; i < mFrames.size(); ++i) {
            if (mFrames[i].frameNumber == frameNum) {
                return mFrames[i].frame;
            }
        }
        return FrameRef();
    }

    bool full() const { return mFrames.full(); }
    FrameBuffer* getFrames() { return &mFrames; }

    bool needsFrame(uint32_t now, uint32_t* currentFrameNumber, uint32_t* nextFrameNumber) const {
        mFrameTracker.get_interval_frames(now, currentFrameNumber, nextFrameNumber);
        return !has(*currentFrameNumber) || !has(*nextFrameNumber);
    }

    bool get_newest_frame_number(uint32_t* frameNumber) const {
        if (mFrames.empty()) {
            return false;
        }
        auto& front = mFrames.front();
        *frameNumber = front.frameNumber;
        return true;
    }

    uint32_t get_exact_timestamp_ms(uint32_t frameNumber) const {
        return mFrameTracker.get_exact_timestamp_ms(frameNumber);
    }

    FrameTracker& getFrameTracker() { return mFrameTracker; }

private:
    FrameBuffer mFrames;
    FrameTracker mFrameTracker;
};

FASTLED_NAMESPACE_END

