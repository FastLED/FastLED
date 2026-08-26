#pragma once

#include "fl/stl/allocator.h"
#include "fl/stl/compiler_control.h"
#include "fl/stl/noexcept.h"
#include "fl/stl/stdint.h"

namespace fl {
namespace third_party {

constexpr fl::size MP3_HELIX_STREAM_BUFFER_SIZE = 4096;
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
    virtual ~Mp3MemoryHook() FL_NO_EXCEPT = default;
    virtual bool allowAllocate(fl::size, Mp3MemoryTag) FL_NO_EXCEPT {
        return true;
    }
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
                          Mp3MemoryTag tag) FL_NO_EXCEPT {
    return static_cast<T*>(Mp3MemoryAllocate(sizeof(T) * count, tag));
}

template <typename T>
struct Mp3MemoryDeleter {
    Mp3MemoryDeleter() FL_NO_EXCEPT
        : mBytes(0), mTag(Mp3MemoryTag::DecoderState) {}
    Mp3MemoryDeleter(fl::size bytes, Mp3MemoryTag tag) FL_NO_EXCEPT
        : mBytes(bytes), mTag(tag) {}

    void operator()(T* ptr) const FL_NO_EXCEPT {
        Mp3MemoryFree(ptr, mBytes, mTag);
    }

    fl::size mBytes;
    Mp3MemoryTag mTag;
};

} // namespace third_party
} // namespace fl
