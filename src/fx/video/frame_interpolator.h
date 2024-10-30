#pragma once

#include "fx/detail/data_stream.h"
#include "fx/frame.h"
#include "namespace.h"
#include "fx/detail/circular_buffer.h"
#include "fx/video/high_precision_interval.h"

FASTLED_NAMESPACE_BEGIN

FASTLED_SMART_REF(FrameInterpolator);

// Holds onto frames and allow interpolation. This allows
// effects to have high effective frame rate and also
// respond to things like sound which can modify the timing.
class FrameInterpolator : public Referent {
public:
    typedef CircularBuffer<FrameRef> FrameBuffer;
    FrameInterpolator(size_t nframes, float fpsVideo);

    HighPrecisionInterval& getInterval() { return mInterval; }
    const HighPrecisionInterval& getInterval() const { return mInterval; }

    // Will search through the array, select the two frames that are closest to the current time
    // and then interpolate between them, storing the results in the provided frame.
    // The destination frame will have "now" as the current timestamp if and only if
    // there are two frames that can be interpolated. Else it's set to the timestamp of the
    // frame that was selected.
    // Returns true if the interpolation was successful, false otherwise. If false then
    // the destination frame will not be modified.
    // Note that this adjustable_time is allowed to go pause or go backward in time. 
    bool draw(uint32_t adjustable_time, Frame* dst);
    bool draw(uint32_t adjustable_time, CRGB* leds, uint8_t* alpha, uint32_t* precise_timestamp = nullptr);

    // Frame's resources are copied into the internal data structures.
    bool add(const Frame& frame);

    // Used for recycling externally.
    bool pop_back(FrameRef* dst) { return mFrames.pop_back(dst); }
    bool push_front(FrameRef frame, uint32_t timestamp) {
        if (mFrames.full()) {
            return false;
        }
        if (!mFrames.empty() && timestamp <= mFrames.front()->getTimestamp()) {
            return false;
        }
        frame->setTimestamp(timestamp);
        return mFrames.push_front(frame);
    }

    bool pushNewest(FrameRef frame, uint32_t timestamp) {
        frame->setTimestamp(timestamp);
        return push_front(frame, timestamp);
    }
    bool popOldest(FrameRef* dst) { return mFrames.pop_back(dst); }

    bool addWithTimestamp(const Frame& frame, uint32_t timestamp);

    // Clear all frames
    void clear() {
        mFrames.clear();
        mInterval.reset(0);
    }

    // Selects the two frames that are closest to the current time. Returns false on failure.
    // frameMin will be before or equal to the current time, frameMax will be after.
    bool selectFrames(uint32_t now, const Frame** frameMin, const Frame** frameMax) const;
    bool full() const { return mFrames.full(); }


    FrameBuffer* getFrames() { return &mFrames; }

    bool needsRefresh(uint32_t now, uint32_t* precise_timestamp) const {
        return mInterval.needsRefresh(now, precise_timestamp);
    }

    void reset(uint32_t startTime) { mInterval.reset(startTime); }
    void incrementFrameCounter() { mInterval.incrementIntervalCounter(); }

    void pause(uint32_t now) { mInterval.pause(now); }
    void resume(uint32_t now) { mInterval.resume(now); }
    bool isPaused() const { return mInterval.isPaused(); }

private:
    FrameBuffer mFrames;
    HighPrecisionInterval mInterval;
};

FASTLED_NAMESPACE_END

