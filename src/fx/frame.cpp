
#include "frame.h"
#include "crgb.h"
#include "namespace.h"
#include "ptr.h"
#include <stdlib.h>
#include <string.h>

#ifdef ESP32
#include "esp_heap_caps.h"
#include "esp_system.h"
#endif

FASTLED_NAMESPACE_BEGIN

namespace {

#ifdef ESP32
// On esp32, attempt to always allocate in psram first.
void *DefaultAlloc(size_t size) {
    void *out = heap_caps_malloc(size, MALLOC_CAP_SPIRAM);
    if (out == nullptr) {
        // Fallback to default allocator.
        out = heap_caps_malloc(size, MALLOC_CAP_DEFAULT);
    }
    return out;
}
void DefaultFree(void *ptr) { heap_caps_free(ptr); }
#else
void *DefaultAlloc(size_t size) { return malloc(size); }
void DefaultFree(void *ptr) { free(ptr); }
#endif

void *(*Alloc)(size_t) = DefaultAlloc;
void (*Free)(void *) = DefaultFree;
} // namespace

void Frame::SetAllocator(void *(*alloc)(size_t), void (*free)(void *)) {
    Alloc = alloc;
    Free = free;
}

Frame::Frame(int pixels_count, bool has_alpha)
    : mPixelsCount(pixels_count), mRgb() {
    mRgb.reset(reinterpret_cast<CRGB *>(Alloc(pixels_count * sizeof(CRGB))));
    memset(mRgb.get(), 0, pixels_count * sizeof(CRGB));
    if (has_alpha) {
        mAlpha.reset(reinterpret_cast<uint8_t *>(Alloc(pixels_count)));
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
