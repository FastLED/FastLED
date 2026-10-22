#pragma once

// IWYU pragma: private

#include "fl/channels/data.h"
#include "fl/stl/cstddef.h"
#include "fl/stl/move.h"
#include "fl/stl/noexcept.h"
#include "fl/stl/vector.h"

namespace fl {

// Owns buffer leases for frames waiting to start. The driver must serialize
// access to this queue and to the ChannelData in-use flags externally.
class Rmt4FrameQueue {
public:
    Rmt4FrameQueue() FL_NO_EXCEPT = default;
    explicit Rmt4FrameQueue(memory_resource *resource) FL_NO_EXCEPT
        : mFrames(resource) {}
    ~Rmt4FrameQueue() FL_NO_EXCEPT { clear(); }

    Rmt4FrameQueue(const Rmt4FrameQueue &) FL_NO_EXCEPT = delete;
    Rmt4FrameQueue &operator=(const Rmt4FrameQueue &) FL_NO_EXCEPT = delete;

    bool empty() const FL_NO_EXCEPT { return mFrames.empty(); }
    size_t size() const FL_NO_EXCEPT { return mFrames.size(); }

    const ChannelDataPtr &operator[](size_t index) const FL_NO_EXCEPT {
        return mFrames[index];
    }

    const ChannelDataPtr &at(size_t index) const FL_NO_EXCEPT {
        return mFrames[index];
    }

    bool enqueue(const ChannelDataPtr &frame) FL_NO_EXCEPT {
        if (!frame || frame->isInUse()) {
            return false;
        }
        const size_t previous = mFrames.size();
        mFrames.push_back(frame);
        if (mFrames.size() == previous) {
            return false;
        }
        frame->setInUse(true);
        return true;
    }

    // The caller transfers an already-held lease, not a second reference to a
    // lease still owned by another queue or active transmission.
    bool adopt(ChannelDataPtr frame) FL_NO_EXCEPT {
        if (!frame) { return false; }
        const size_t previous = mFrames.size();
        mFrames.push_back(fl::move(frame));
        if (mFrames.size() == previous) {
            frame->setInUse(false);
            return false;
        }
        return true;
    }

    // Completion or the next owner must release the returned lease.
    ChannelDataPtr take(size_t index) FL_NO_EXCEPT {
        if (index >= mFrames.size()) {
            return ChannelDataPtr();
        }
        ChannelDataPtr frame = fl::move(mFrames[index]);
        mFrames.erase(mFrames.begin() + index);
        return frame;
    }

    bool appendFrom(Rmt4FrameQueue &source) FL_NO_EXCEPT {
        if (this == &source) {
            return true;
        }
        const size_t required = mFrames.size() + source.size();
        if (required < mFrames.size()) { return false; }
        mFrames.reserve(required);
        if (mFrames.capacity() < required) {
            return false;
        }
        for (size_t i = 0; i < source.size(); ++i) {
            mFrames.push_back(fl::move(source.mFrames[i]));
        }
        source.mFrames.clear();
        return true;
    }

    void clear() FL_NO_EXCEPT {
        for (size_t i = 0; i < mFrames.size(); ++i) {
            mFrames[i]->setInUse(false);
        }
        mFrames.clear();
    }

private:
    vector_inlined<ChannelDataPtr, 16> mFrames;
};

} // namespace fl
