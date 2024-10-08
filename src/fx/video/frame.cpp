#include "namespace.h"
#include "crgb.h"
#include "ptr.h"
#include "frame.h"
#include <string.h>

FASTLED_NAMESPACE_BEGIN

Frame::Frame(int bytes_per_frame)
    : mBytesPerFrame(bytes_per_frame), mSurface(new uint8_t[bytes_per_frame])
{
    // Initialize the surface with zeros
    memset(mSurface.get(), 0, bytes_per_frame);
}

void Frame::copy(const Frame& other) {
    memcpy(mSurface.get(), other.mSurface.get(), other.mBytesPerFrame);
}

FASTLED_NAMESPACE_END

