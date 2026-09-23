/// @file presentation_timing.h
/// @brief Optional, measured per-channel LED presentation observations.
#pragma once

#include "fl/stl/noexcept.h"
#include "fl/stl/stdint.h"
#include "fl/stl/static_assert.h"
#include "led_sysdefs.h"

#ifndef FL_PRESENTATION_TIMING
#if defined(FASTLED_TESTING) && !FL_PLATFORM_HAS_TINY_MEMORY
#define FL_PRESENTATION_TIMING 1
#else
#define FL_PRESENTATION_TIMING 0
#endif
#endif

#if FL_PLATFORM_HAS_TINY_MEMORY && FL_PRESENTATION_TIMING
#error "presentation timing is unavailable on TINY memory tiers"
#endif

namespace fl {

/// Value identity; drivers copy it at enqueue and never retain a Channel*.
struct PresentationToken {
    i32 channelId = 0;
    u32 frame = 0;

    constexpr PresentationToken() FL_NO_EXCEPT = default;
    constexpr PresentationToken(i32 id, u32 sequence) FL_NO_EXCEPT
        : channelId(id), frame(sequence) {}
    constexpr bool operator==(const PresentationToken& other) const FL_NO_EXCEPT {
        return channelId == other.channelId && frame == other.frame;
    }
    constexpr bool operator!=(const PresentationToken& other) const FL_NO_EXCEPT {
        return !(*this == other);
    }
    constexpr bool valid() const FL_NO_EXCEPT { return channelId >= 0 && frame != 0; }
};
FL_STATIC_ASSERT(sizeof(PresentationToken) == 8, "presentation token must remain compact");

enum class PresentationKind : u8 {
    Latched, ///< Frame actually became visible, not merely queued or DMA-complete.
    Ended,   ///< This frame ceased to be visible at timestampUs.
    Dropped, ///< Queued frame never became visible; timestamp is diagnostic only.
};

enum class PresentationTimingCapability : u8 {
    Unknown,            ///< Fixed-cadence assumption only; no visibility evidence.
    MeasuredVisibility, ///< Driver can report actual latch and end/drop events.
};

struct PresentationEvent {
    PresentationToken token;
    u32 timestampUs = 0; ///< Driver clock; wrap-safe intervals must be < 2^31 us.
    u32 durationUs = 0;  ///< Filled by PresentationTimeline for an ended frame.
    PresentationKind kind = PresentationKind::Dropped;
    bool hasDuration = false;

    constexpr PresentationEvent() FL_NO_EXCEPT = default;
    constexpr PresentationEvent(PresentationToken frameToken, u32 us,
                                PresentationKind eventKind) FL_NO_EXCEPT
        : token(frameToken), timestampUs(us), durationUs(0),
          kind(eventKind), hasDuration(false) {}

    static constexpr PresentationEvent latched(PresentationToken token, u32 us) FL_NO_EXCEPT {
        return PresentationEvent(token, us, PresentationKind::Latched);
    }
    static constexpr PresentationEvent ended(PresentationToken token, u32 us) FL_NO_EXCEPT {
        return PresentationEvent(token, us, PresentationKind::Ended);
    }
    static constexpr PresentationEvent dropped(PresentationToken token, u32 us) FL_NO_EXCEPT {
        return PresentationEvent(token, us, PresentationKind::Dropped);
    }
};

/// One-channel value-only timeline. A successor latch closes the preceding
/// frame; a queued/dropped frame cannot consume visible time. No heap/ISR work.
/// The caller supplies one tracker per channel, or demultiplexes by channelId.
class PresentationTimeline {
  public:
    bool observe(const PresentationEvent& event, PresentationEvent* completed) FL_NO_EXCEPT {
        if (completed != nullptr) *completed = PresentationEvent();
        if (!event.token.valid()) return false;
        if (mInvalid) return false;
        if (mBound && event.token.channelId != mChannelId) return false;
        if (event.kind == PresentationKind::Dropped) {
            if (mVisible && event.token == mCurrent) return false;
            if (!recordDrop(event.token.frame)) return false;
            mChannelId = event.token.channelId;
            mBound = true;
            return true;
        }
        if (event.kind == PresentationKind::Ended) {
            if (!mVisible || event.token != mCurrent) return false;
            if (!close(event.timestampUs, completed)) return false;
            mVisible = false;
            return true;
        }
        if (mHaveLastFrame) {
            // Serial-number arithmetic permits a sequence rollover, but
            // rejects duplicates, replay, and jumps >= half the u32 range.
            const u32 forward = event.token.frame - mLastFrame;
            if (forward == 0 || forward >= 0x80000000u) return false;
        }
        if (wasDropped(event.token.frame)) return false;
        if (mVisible) {
            if (!close(event.timestampUs, completed)) return false;
        }
        mChannelId = event.token.channelId;
        mBound = true;
        mCurrent = event.token;
        mLastFrame = event.token.frame;
        mHaveLastFrame = true;
        pruneDrops();
        mStartUs = event.timestampUs;
        mVisible = true;
        return true;
    }

  private:
    static const u8 kMaxPendingDrops = 8;

    bool wasDropped(u32 frame) const FL_NO_EXCEPT {
        for (u8 i = 0; i < mDroppedCount; ++i) {
            if (mDroppedFrames[i] == frame) return true;
        }
        return false;
    }

    bool recordDrop(u32 frame) FL_NO_EXCEPT {
        if (mHaveLastFrame) {
            const u32 forward = frame - mLastFrame;
            if (forward == 0) return false; // Already visible, not dropped.
            if (forward >= 0x80000000u) return true; // Older than last latch.
        }
        if (wasDropped(frame)) return true;
        if (mDroppedCount == kMaxPendingDrops) {
            // Never forget a tombstone and then allow that frame to latch.
            // A consumer must replace this timeline after a queue overflow.
            mInvalid = true;
            return false;
        }
        mDroppedFrames[mDroppedCount++] = frame;
        return true;
    }

    void pruneDrops() FL_NO_EXCEPT {
        for (u8 i = 0; i < mDroppedCount;) {
            const u32 forward = mDroppedFrames[i] - mLastFrame;
            if (forward == 0 || forward >= 0x80000000u) {
                mDroppedFrames[i] = mDroppedFrames[--mDroppedCount];
            } else {
                ++i;
            }
        }
    }

    bool close(u32 endUs, PresentationEvent* completed) const FL_NO_EXCEPT {
        const u32 delta = endUs - mStartUs;
        if (delta >= 0x80000000u) return false;
        if (completed != nullptr) {
            *completed = PresentationEvent::ended(mCurrent, endUs);
            completed->durationUs = delta;
            completed->hasDuration = true;
        }
        return true;
    }
    i32 mChannelId = 0;
    bool mBound = false;
    PresentationToken mCurrent;
    u32 mLastFrame = 0;
    u32 mDroppedFrames[kMaxPendingDrops] = {};
    u32 mStartUs = 0;
    u8 mDroppedCount = 0;
    bool mVisible = false;
    bool mHaveLastFrame = false;
    bool mInvalid = false;
};

/// Optional task-context consumer. ChannelManager holds shared ownership,
/// so an asynchronous driver never retains a dangling observer pointer.
class IPresentationTimingSink {
  public:
    virtual ~IPresentationTimingSink() FL_NO_EXCEPT = default;
    virtual void onPresentationEvent(const PresentationEvent& event) FL_NO_EXCEPT = 0;
};

} // namespace fl
