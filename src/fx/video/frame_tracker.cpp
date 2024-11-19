#include "frame_tracker.h"

FASTLED_NAMESPACE_BEGIN

FrameTracker::FrameTracker(float fps) {
    // Convert fps to microseconds per frame interval
    mMicrosSecondsPerInterval = static_cast<uint32_t>(1000000.0f / fps + .5f);
}



void FrameTracker::get_interval_frames(uint32_t now, uint32_t* frameNumber, uint32_t* nextFrameNumber, uint8_t* amountOfNextFrame) const {
    // Account for any pause time
    uint32_t effectiveTime = now;


    // Convert milliseconds to microseconds for precise calculation
    uint64_t microseconds = static_cast<uint64_t>(effectiveTime) * 1000ULL;
    
    // Calculate frame number with proper rounding
    *frameNumber = microseconds / mMicrosSecondsPerInterval;
    *nextFrameNumber = *frameNumber + 1;

    // Calculate interpolation amount if requested
    if (amountOfNextFrame != nullptr) {
        uint64_t frame1_start = (*frameNumber * mMicrosSecondsPerInterval);
        uint64_t frame2_start = (*nextFrameNumber * mMicrosSecondsPerInterval);
        uint32_t rel_time = microseconds - frame1_start;
        uint32_t frame_duration = frame2_start - frame1_start;
        double progress = static_cast<double>(rel_time) / frame_duration;
        *amountOfNextFrame = static_cast<uint8_t>(progress * 255 + 0.5);
    }
}

uint32_t FrameTracker::get_exact_timestamp_ms(uint32_t frameNumber) const {
    uint64_t microseconds = frameNumber * mMicrosSecondsPerInterval;
    return static_cast<uint32_t>(microseconds / 1000) + mStartTime + mPauseOffset;
}

FASTLED_NAMESPACE_END
