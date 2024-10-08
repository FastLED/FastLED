
#include "namespace.h"
#include "crgb.h"
#include "ptr.h"
#include <string.h>
#include <stdlib.h>
#include "frame.h"


FASTLED_NAMESPACE_BEGIN

namespace {
    void* DefaultAlloc(size_t size) {
        return malloc(size);
    }

    void DefaultFree(void* ptr) {
        free(ptr);
    }

    void* (*Alloc)(size_t) = DefaultAlloc;
    void (*Free)(void*) = DefaultFree;
}  // namespace

void Frame::SetAllocator(void* (*alloc)(size_t), void (*free)(void*)) {
    Alloc = alloc;
    Free = free;
}

Frame::Frame(int pixels_count, bool has_alpha)
    : mPixelsCount(pixels_count), mRgb() {
    mRgb.reset(reinterpret_cast<CRGB*>(Alloc(pixels_count * sizeof(CRGB))));
    memset(mRgb.get(), 0, pixels_count * sizeof(CRGB));
    if (has_alpha) {
        mAlpha.reset(reinterpret_cast<uint8_t*>(Alloc(pixels_count)));
        memset(mAlpha.get(), 0, pixels_count);
    }
}

Frame::~Frame() {
    if (mRgb) {
        Free(mRgb.release());
    }
    if (mAlpha) {
        Free(mAlpha.release());
    }
}

FASTLED_NAMESPACE_END
