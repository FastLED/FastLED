#include "frame_tracker.h"
#include "fl/int.h"

namespace fl {

namespace { // anonymous namespace
long linear_map(long x, long in_min, long in_max, long out_min, long out_max) {
    const long run = in_max - in_min;
    if (run == 0) {
        return 0; // AVR returns -1, SAM returns 0
    }
    const long rise = out_max - out_min;
    const long delta = x - in_min;
    return (delta * rise) / run + out_min;
}
} // anonymous namespace

FrameTracker::FrameTracker(float fps) {
    // Convert fps to microseconds per frame interval
    mMicrosSecondsPerInterval = static_cast<fl::u32>(1000000.0f / fps + .5f);
}

void FrameTracker::get_interval_frames(fl::u32 now, fl::u32 *frameNumber,
                                       fl::u32 *nextFrameNumber,
                                       uint8_t *amountOfNextFrame) const {
    // Account for any pause time
    fl::u32 effectiveTime = now;

    // Convert milliseconds to microseconds for precise calculation
    fl::u64 microseconds = static_cast<fl::u64>(effectiveTime) * 1000ULL;

    // Calculate frame number with proper rounding
    *frameNumber = microseconds / mMicrosSecondsPerInterval;
    *nextFrameNumber = *frameNumber + 1;

    // Calculate interpolation amount if requested
    if (amountOfNextFrame != nullptr) {
        fl::u64 frame1_start = (*frameNumber * mMicrosSecondsPerInterval);
        fl::u64 frame2_start = (*nextFrameNumber * mMicrosSecondsPerInterval);
        fl::u32 rel_time = microseconds - frame1_start;
        fl::u32 frame_duration = frame2_start - frame1_start;
        uint8_t progress = uint8_t(linear_map(rel_time, 0, frame_duration, 0, 255));
        *amountOfNextFrame = progress;
    }
}

fl::u32 FrameTracker::get_exact_timestamp_ms(fl::u32 frameNumber) const {
    fl::u64 microseconds = frameNumber * mMicrosSecondsPerInterval;
    return static_cast<fl::u32>(microseconds / 1000) + mStartTime;
}

} // namespace fl
