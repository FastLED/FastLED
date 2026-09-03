#pragma once

#include "fl/stl/allocator.h"
#include "fl/stl/compiler_control.h"
#include "fl/stl/noexcept.h"
#include "fl/stl/stdint.h"

namespace fl {
namespace third_party {

// Internal codec-allocation contract. These declarations are visible through
// mp3.h only because decoder private storage contains typed deleters. They are
// not a consumer allocation API and may change with the decoder backends.
constexpr fl::size MP3_MINIMP3_STREAM_BUFFER_SIZE = 4096;

enum class Mp3MemoryTag : fl::u8 {
    DecoderState = 0,
    Scratch = 1,
    StreamBuffer = 2,
    PcmOutput = 3,
    Count = 4,
};

const char* Mp3MemoryTagName(Mp3MemoryTag tag) FL_NO_EXCEPT;

#if defined(FASTLED_TESTING)
class Mp3MemoryHook {
public:
    virtual ~Mp3MemoryHook() FL_NO_EXCEPT;
    virtual bool allowAllocate(fl::size, Mp3MemoryTag) FL_NO_EXCEPT;
    virtual void onAllocate(void* ptr, fl::size bytes,
                            Mp3MemoryTag tag) FL_NO_EXCEPT = 0;
    virtual void onFree(void* ptr, fl::size bytes,
                        Mp3MemoryTag tag) FL_NO_EXCEPT = 0;
};

void SetMp3MemoryHook(Mp3MemoryHook* hook) FL_NO_EXCEPT;
void ClearMp3MemoryHook() FL_NO_EXCEPT;
#endif

FL_NO_INLINE void* Mp3MemoryAllocate(
    fl::size bytes, Mp3MemoryTag tag) FL_NO_EXCEPT;
void Mp3MemoryFree(void* ptr, fl::size bytes,
                   Mp3MemoryTag tag) FL_NO_EXCEPT;

template <typename T>
T* Mp3MemoryAllocateArray(fl::size count,
                          Mp3MemoryTag tag) FL_NO_EXCEPT;

template <typename T>
struct Mp3MemoryDeleter {
    Mp3MemoryDeleter() FL_NO_EXCEPT;
    Mp3MemoryDeleter(fl::size bytes, Mp3MemoryTag tag) FL_NO_EXCEPT;

    void operator()(T* ptr) const FL_NO_EXCEPT;

    fl::size mBytes;
    Mp3MemoryTag mTag;
};

} // namespace third_party
} // namespace fl
