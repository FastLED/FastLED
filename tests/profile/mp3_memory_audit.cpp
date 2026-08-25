// ok standalone

#include "fl/codec/mp3.h"
#include "fl/codec/mp3_memory.h"
#include "fl/stl/stdio.h"

using namespace fl;
using namespace fl::third_party;

namespace {

class AccountingHook : public Mp3MemoryHook {
public:
    void onAllocate(void*, fl::size bytes,
                    Mp3MemoryTag tag) FL_NO_EXCEPT override {
        ++mAllocations;
        mCurrent += bytes;
        if (mCurrent > mPeak) {
            mPeak = mCurrent;
        }
        const unsigned index = static_cast<unsigned>(tag);
        mTaggedCurrent[index] += bytes;
        if (mTaggedCurrent[index] > mTaggedPeak[index]) {
            mTaggedPeak[index] = mTaggedCurrent[index];
        }
    }

    void onFree(void*, fl::size bytes,
                Mp3MemoryTag tag) FL_NO_EXCEPT override {
        mCurrent -= bytes;
        mTaggedCurrent[static_cast<unsigned>(tag)] -= bytes;
    }

    fl::size current() const { return mCurrent; }
    fl::size peak() const { return mPeak; }
    fl::size allocations() const { return mAllocations; }
    fl::size taggedPeak(Mp3MemoryTag tag) const {
        return mTaggedPeak[static_cast<unsigned>(tag)];
    }

private:
    fl::size mCurrent = 0;
    fl::size mPeak = 0;
    fl::size mAllocations = 0;
    fl::size mTaggedCurrent[4] = {};
    fl::size mTaggedPeak[4] = {};
};

template <typename Decoder>
bool measureBackend(const char* backend, fl::size stream_bytes) {
    AccountingHook hook;
    SetMp3MemoryHook(&hook);
    void* stream =
        Mp3MemoryAllocate(stream_bytes, Mp3MemoryTag::StreamBuffer);
    bool initialized = false;
    {
        Decoder decoder;
        initialized = decoder.init();
        fl::printf(
            "MP3_MEMORY:backend=%s current=%zu peak=%zu allocations=%zu "
            "decoder-state=%zu scratch=%zu stream-buffer=%zu pcm-output=%zu\n",
            backend, hook.current(), hook.peak(), hook.allocations(),
            hook.taggedPeak(Mp3MemoryTag::DecoderState),
            hook.taggedPeak(Mp3MemoryTag::Scratch),
            hook.taggedPeak(Mp3MemoryTag::StreamBuffer),
            hook.taggedPeak(Mp3MemoryTag::PcmOutput));
    }
    Mp3MemoryFree(stream, stream_bytes, Mp3MemoryTag::StreamBuffer);
    const bool balanced = hook.current() == 0;
    ClearMp3MemoryHook();
    return initialized && balanced;
}

} // anonymous namespace

int main() {
    const bool helix = measureBackend<Mp3HelixDecoder>(
        "helix", MP3_HELIX_STREAM_BUFFER_SIZE);
    const bool minimp3 = measureBackend<Mp3Minimp3Decoder>(
        "minimp3-float", MP3_MINIMP3_STREAM_BUFFER_SIZE);
    if (!helix || !minimp3) {
        return 1;
    }
    fl::printf(
        "PROFILE_RESULT:{\"variant\":\"memory\",\"target\":\"mp3_memory_"
        "audit\",\"total_calls\":1,\"total_time_ns\":1,"
        "\"ns_per_call\":1.0,\"calls_per_sec\":1000000000.0}\n");
    return 0;
}
