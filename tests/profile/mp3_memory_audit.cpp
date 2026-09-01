// ok standalone

#include <stdint.h>

#include "fl/codec/mp3.h"
#include "fl/codec/mp3_memory.h"
#include "fl/stl/stdio.h"

// A second minimp3 instantiation, this one with the float DSP path, so the
// ledger can carry a measured working-RAM figure for it rather than an
// arithmetic claim. The production wrapper supplies the fixed-point row now
// that fixed point is what ships; float survives as the reference build the
// fixed-vs-float gates compare against, and it needs its own namespace because
// the two cannot share a translation unit.
#define MINIMP3_NAMESPACE minimp3_float_audit
#define MINIMP3_FLOAT_POINT 1
#define MINIMP3_IMPLEMENTATION
#define MINIMP3_NO_STDIO
#include "third_party/minimp3/minimp3.h" // ok cpp include
#undef MINIMP3_NO_STDIO
#undef MINIMP3_IMPLEMENTATION
#undef MINIMP3_FLOAT_POINT
#undef MINIMP3_NAMESPACE

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

/* Mirrors Mp3Minimp3Decoder's allocation shape exactly -- decoder state,
   scratch arena and caller-owned PCM, each under its accounting tag -- so the
   two minimp3 rows in the ledger are measured the same way and differ only
   where the decoders genuinely differ (the fixed build's larger scratch). */
class Minimp3FloatProbeDecoder {
public:
    ~Minimp3FloatProbeDecoder() { reset(); }

    bool init() FL_NO_EXCEPT {
        // Mp3MemoryAllocateArray is only explicitly instantiated for the
        // production decoder's types, so this goes through the untyped
        // allocator. Both decoder structures are PODs, which is what makes the
        // cast sound; the accounting tags are what the audit reads.
        mDecoder = static_cast<fl::minimp3_float_audit::mp3dec_t*>(
            Mp3MemoryAllocate(sizeof(fl::minimp3_float_audit::mp3dec_t),
                              Mp3MemoryTag::DecoderState));
        mScratch = static_cast<fl::minimp3_float_audit::mp3dec_scratch_t*>(
            Mp3MemoryAllocate(sizeof(fl::minimp3_float_audit::mp3dec_scratch_t),
                              Mp3MemoryTag::Scratch));
        mPcm = static_cast<fl::i16*>(
            Mp3MemoryAllocate(sizeof(fl::i16) * 2304, Mp3MemoryTag::PcmOutput));
        if (!mDecoder || !mScratch || !mPcm) {
            reset();
            return false;
        }
        fl::minimp3_float_audit::mp3dec_init(mDecoder);
        return true;
    }

    void reset() FL_NO_EXCEPT {
        if (mDecoder) {
            Mp3MemoryFree(mDecoder,
                          sizeof(fl::minimp3_float_audit::mp3dec_t),
                          Mp3MemoryTag::DecoderState);
            mDecoder = nullptr;
        }
        if (mScratch) {
            Mp3MemoryFree(mScratch,
                          sizeof(fl::minimp3_float_audit::mp3dec_scratch_t),
                          Mp3MemoryTag::Scratch);
            mScratch = nullptr;
        }
        if (mPcm) {
            Mp3MemoryFree(mPcm, sizeof(fl::i16) * 2304,
                          Mp3MemoryTag::PcmOutput);
            mPcm = nullptr;
        }
    }

private:
    fl::minimp3_float_audit::mp3dec_t* mDecoder = nullptr;
    fl::minimp3_float_audit::mp3dec_scratch_t* mScratch = nullptr;
    fl::i16* mPcm = nullptr;
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
    const bool minimp3_fixed = measureBackend<Mp3Minimp3Decoder>(
        "minimp3-fixed", MP3_MINIMP3_STREAM_BUFFER_SIZE);
    const bool minimp3_float = measureBackend<Minimp3FloatProbeDecoder>(
        "minimp3-float", MP3_MINIMP3_STREAM_BUFFER_SIZE);
    if (!minimp3_fixed || !minimp3_float) {
        return 1;
    }
    fl::printf(
        "PROFILE_RESULT:{\"variant\":\"memory\",\"target\":\"mp3_memory_"
        "audit\",\"total_calls\":1,\"total_time_ns\":1,"
        "\"ns_per_call\":1.0,\"calls_per_sec\":1000000000.0}\n");
    return 0;
}
