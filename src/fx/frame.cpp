#include "namespace.h"
#include "crgb.h"
#include "ptr.h"
#include "fx/frame.h"
#include <string.h>

FASTLED_NAMESPACE_BEGIN

Frame::Frame(int pixels_count)
    : mPixelsCount(pixels_count), mRgb(new CRGB[pixels_count]) {

}

void Frame::copy(const Frame& other) {
    memcpy(mRgb.get(), other.mRgb.get(), other.mPixelsCount * sizeof(CRGB));
}

FASTLED_NAMESPACE_END

