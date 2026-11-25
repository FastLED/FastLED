#pragma once

#include "fl/stl/vector.h"

namespace fl {
namespace detail {

class SpiFrameOutput {
public:
    using value_type = u8;

    explicit SpiFrameOutput(fl::vector_psram<u8>& bytes) FL_NO_EXCEPT
        : mBytes(bytes) {}

    void push_back(u8 byte) FL_NO_EXCEPT {
        if (mFailed) {
            return;
        }
        // A failed vector append leaves its size unchanged.
        const auto previousSize = mBytes.size();
        mBytes.push_back(byte);
        mFailed = mBytes.size() == previousSize;
    }

    bool failed() const FL_NO_EXCEPT { return mFailed; }

private:
    fl::vector_psram<u8>& mBytes;
    bool mFailed = false;
};

} // namespace detail
} // namespace fl
