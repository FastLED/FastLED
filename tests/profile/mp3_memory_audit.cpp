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
        const unsigned index = static_cast<unsigned>(tag);
        if (index >= kTagCount) {
            mValid = false;
            return;
        }
        ++mAllocations;
        mCurrent += bytes;
        if (mCurrent > mPeak) {
            mPeak = mCurrent;
        }
        mTaggedCurrent[index] += bytes;
        if (mTaggedCurrent[index] > mTaggedPeak[index]) {
            mTaggedPeak[index] = mTaggedCurrent[index];
        }
    }

    void onFree(void*, fl::size bytes,
                Mp3MemoryTag tag) FL_NO_EXCEPT override {
        const unsigned index = static_cast<unsigned>(tag);
        if (index >= kTagCount || bytes > mCurrent ||
            bytes > mTaggedCurrent[index]) {
            mValid = false;
            return;
        }
        mCurrent -= bytes;
        mTaggedCurrent[index] -= bytes;
    }

    fl::size current() const { return mCurrent; }
    fl::size peak() const { return mPeak; }
    fl::size allocations() const { return mAllocations; }
    bool valid() const { return mValid; }
    bool balanced() const {
        if (mCurrent != 0) {
            return false;
        }
        for (unsigned index = 0; index < kTagCount; ++index) {
            if (mTaggedCurrent[index] != 0) {
                return false;
            }
        }
        return true;
    }
    fl::size taggedPeak(Mp3MemoryTag tag) const {
        const unsigned index = static_cast<unsigned>(tag);
        return index < kTagCount ? mTaggedPeak[index] : 0;
    }

private:
    static constexpr unsigned kTagCount =
        static_cast<unsigned>(Mp3MemoryTag::Count);
    fl::size mCurrent = 0;
    fl::size mPeak = 0;
    fl::size mAllocations = 0;
    fl::size mTaggedCurrent[kTagCount] = {};
    fl::size mTaggedPeak[kTagCount] = {};
    bool mValid = true;
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
    ClearMp3MemoryHook();
    return initialized && hook.valid() && hook.balanced();
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
